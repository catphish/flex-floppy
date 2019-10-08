require "libusb"

class ReadError < StandardError; end

usb = LIBUSB::Context.new
device = usb.devices(idVendor: 0x1209, idProduct: 0x0001).first
device.open_interface(0) do |handle|
  # 1 - Enable up the drive
  handle.bulk_transfer(:endpoint => 0x1, :dataOut => "\1")
  sleep 0.5
  (0..159).each do |track|
    track_data = "".force_encoding('BINARY')
    STDERR.puts "Reading Track #{track}"
    # 4 - Stream data
    handle.bulk_transfer(:endpoint => 0x1, :dataOut => [4, track].pack('CC'), :timeout => 10000)
    loop do
      # Receive 64 bytes
      data = handle.bulk_transfer(:endpoint => 0x81, :dataIn => 64, :timeout => 10000)
      break if data == "\0"
      raise ReadError if data == "\1"
      track_data << data
    end
    STDOUT.write ['T', track, track_data.bytesize].pack('aCN')
    STDOUT.write track_data
  end
  handle.bulk_transfer(:endpoint => 0x1, :dataOut => "\2", :timeout => 10000)
end
