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
    STDERR.puts "Writing Track #{track}"
    # 4 - Stream data
    handle.bulk_transfer(:endpoint => 0x1, :dataOut => [6, track].pack('CC'), :timeout => 10000)
    write_data = [0x1,0x80].pack('CC')*409600
    write_data += [0x2,0x80].pack('CC')*1024
    write_data += [0x0, 0x0].pack('CC')
    handle.bulk_transfer(:endpoint => 0x1, :dataOut => write_data, :timeout => 10000)
    data = handle.bulk_transfer(:endpoint => 0x81, :dataIn => 64, :timeout => 10000)
    puts data.inspect
  end
  handle.bulk_transfer(:endpoint => 0x1, :dataOut => "\2", :timeout => 10000)
end
