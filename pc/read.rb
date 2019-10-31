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

start_track  = 0
end_track    = 159
revolutions  = 1

usb = LIBUSB::Context.new
device = usb.devices(idVendor: 0x1209, idProduct: 0x0001).first
device.open_interface(0) do |handle|
  f = File.open(ARGV[0], 'w+b')
  header = [
    'SCP',
    0x19, # Version 1.9
    0x04, # Amiga disk
    revolutions,
    start_track,
    end_track,
    1, # Flags (INDEX)
    0, # 16-bit cells
    0, # 2 heads
    0, # 25ns resolution
  ]
  f.write(header.pack('a3CCCCCCCCC'))
  offset = 0x02B0
  relative_track = 0

  # Enable drive
  send_command(handle, COMMAND_ENABLE_DRIVE)
  sleep 0.5

  # Zero head
  send_command(handle, COMMAND_ZERO_HEAD)

  (start_track..end_track).each do |track|
    STDERR.puts "Reading Track #{track}"

    track_start_offset = offset
    f.seek(0x10 + relative_track * 4)
    f.write([track_start_offset].pack('L<'))
    relative_track += 1
  
    # Seek track
    send_command(handle, COMMAND_SEEK_HEAD, track)
    sleep 0.01

    track_data = nil
    index_data = nil
    loop do
      # Read 8M (40MHz) cycles per revolution
      # Read one additional revolution to allow index alignment
      send_command(handle, COMMAND_READ_RAW, revolutions)

      # Receive data stream
      track_data = read_data(handle)
      index_data = read_data(handle)
      status     = read_data(handle)
      if status == "\1\0"
        break
      else
        STDERR.puts "Buffer overrun. Retrying..."
        sleep 1
      end
    end

    # Unpack little endian track data
    track_data_n = track_data.unpack('S<*')
    # Convert cumulative times to deltas, allowing for overflow
    track_data_n = track_data_n.each_cons(2).map do |a,b|
      b > a ? b - a : 65536 + b - a
    end

    # Write raw track data to file
    encoded_data = track_data_n.pack('S>*')
    f.seek(track_start_offset + 4 + 12 * revolutions)
    f.write(encoded_data)

    # Increment the offset ready for the start of the next track
    offset = track_start_offset + 4 + 12 * revolutions + encoded_data.bytesize

    # Unpack little endian index data
    index_data_n = index_data.unpack('L<*').each_slice(2).to_a

    # Calculate track header data
    processed_index_data = index_data_n[0,revolutions+1].each_cons(2).map do |a,b|
      {
        :duration => b[0] - a[0],
        :bitcells => b[1] - a[1],
        :start    => 4 + 12 * revolutions + a[1] * 2
      }
    end

    # Write track header data
    f.seek(track_start_offset)
    f.write('TRK')
    f.write(track.chr)
    processed_index_data.each do |rev|
      f.write([rev[:duration], rev[:bitcells], rev[:start]].pack('L<L<L<'))
    end
  end

  # Disable drive
  send_command(handle, COMMAND_DISABLE_DRIVE)

  # Calculate checksum
  f.seek(0x10)
  checksum = f.read.unpack('C*').sum & 0xffffffff
  f.seek(0x0C)
  f.write([checksum].pack('L<'))

  # Close file
  f.close
end
