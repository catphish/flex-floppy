require "libusb"

COMMAND_ENABLE_DRIVE  = 0x01
COMMAND_DISABLE_DRIVE = 0x02
COMMAND_ZERO_HEAD     = 0x11
COMMAND_SEEK_HEAD     = 0x12
COMMAND_READ_RAW      = 0x21
COMMAND_WRITE_RAW     = 0x31

class ReadError < StandardError; end

def send_command(handle, command, value = 0)
  handle.control_transfer(:bmRequestType => 0x41, :bRequest => command, :wIndex => 0, :wValue => value, :timeout => 2000)
end

def read_data(handle)
  handle.bulk_transfer(:endpoint => 0x81, :dataIn => 1024*1024, :timeout => 5000)
end

def write_data(handle, data)
  handle.bulk_transfer(:endpoint => 0x01, :dataOut => data, :timeout => 5000)
end

usb = LIBUSB::Context.new
device = usb.devices(idVendor: 0x1209, idProduct: 0x0001).first
device.open_interface(0) do |handle|
  # Enable drive
  send_command(handle, COMMAND_ENABLE_DRIVE)
  sleep 0.5

  # Zero head
  send_command(handle, COMMAND_ZERO_HEAD)

  while(header = STDIN.read(6))
    _, track, length = header.unpack('CCN')
    STDERR.puts "Writing Track #{track}"

    data = STDIN.read(length)
    data = data.unpack('n*').pack('S<*')

    # Seek track
    send_command(handle, COMMAND_SEEK_HEAD, track)
    sleep 0.01

    loop do
      # Begin write
      send_command(handle, COMMAND_WRITE_RAW)

      # Send data
      write_data(handle, data + "\0\0")
      status = read_data(handle)
      if status == "\1\0"
        break
      else
        sleep 0.5
        STDERR.puts "Buffer underrun. Retrying..."
      end
    end
  end

  # Disable drive
  send_command(handle, COMMAND_DISABLE_DRIVE)

end
