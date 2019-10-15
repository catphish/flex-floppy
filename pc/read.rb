require "libusb"

class ReadError < StandardError; end

usb = LIBUSB::Context.new
device = usb.devices(idVendor: 0x1209, idProduct: 0x0001).first
device.open_interface(0) do |handle|
  # 1 - Enable up the drive
  handle.bulk_transfer(:endpoint => 0x1, :dataOut => "\1")
  sleep 0.5
  (0..0).each do |track|
    track_data = "".force_encoding('BINARY')
    STDERR.puts "Reading Track #{track}"
    # 4 - Stream data
    handle.bulk_transfer(:endpoint => 0x1, :dataOut => [4, track, 40].pack('CCC'), :timeout => 10000)
    loop do
      # Receive data
      data = handle.bulk_transfer(:endpoint => 0x81, :dataIn => 1024*1024, :timeout => 10000)
      raise ReadError if data[-2, 2] == "\0\1"
      if data[-2, 2] == "\0\0"
        track_data << data[0, data.bytesize-2]
        break
      else
        track_data << data
      end
    end
    STDOUT.write ['T', track, track_data.bytesize].pack('aCN')
    STDOUT.write track_data
  end
  handle.bulk_transfer(:endpoint => 0x1, :dataOut => "\2", :timeout => 10000)
end
