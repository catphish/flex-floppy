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
  handle.bulk_transfer(:endpoint => 0x82, :dataIn => 1024*1024, :timeout => 5000)
end

def write_data(handle, data)
  handle.bulk_transfer(:endpoint => 0x01, :dataOut => data, :timeout => 5000)
end

f = File.open(ARGV[0], 'rb')
f.seek(0x10)
track_offsets = f.read(168*4).unpack('L<*')

usb = LIBUSB::Context.new
device = usb.devices(idVendor: 0x1209, idProduct: 0x0001).first
device.open_interface(0) do |handle|
  # Enable drive
  send_command(handle, COMMAND_ENABLE_DRIVE)
  sleep 0.5

  # Zero head
  send_command(handle, COMMAND_ZERO_HEAD)

  track_offsets.each do |track_offset|
    next if track_offset == 0
    f.seek(track_offset)
    track_header = f.read(16).unpack('a3CL<L<L<')
    t,track,duration,bitcells,data_offset = track_header
    if track > 159
      STDERR.puts "Refusing to write track #{track}"
      next
    else
      STDERR.puts "Writing Track #{track}"
    end

    f.seek(track_offset + data_offset)
    data = f.read(bitcells*2).unpack('S>*')
    data = data.pack('S<*') + "\0\0"

    # Seek track
    send_command(handle, COMMAND_SEEK_HEAD, track)
    sleep 0.01

    loop do
      # Begin write
      send_command(handle, COMMAND_WRITE_RAW)

      # Send data
      write_data(handle, data)
      status = read_data(handle)
      if status == "\1\0"
        break
      else
        STDERR.puts "Buffer underrun. Retrying..."
        sleep 1
      end
    end
  end

  # Disable drive
  send_command(handle, COMMAND_DISABLE_DRIVE)

end
