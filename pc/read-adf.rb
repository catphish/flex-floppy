require "libusb"
require 'thread'

class ReadError < StandardError; end

usb = LIBUSB::Context.new
device = usb.devices(idVendor: 0x1209, idProduct: 0x0001).first
device.open_interface(0) do |handle|
  total_bytes = 0
  start_time = Time.now
  
  Thread.new do
    loop do
      sleep 1
      puts (total_bytes / (Time.now.to_f - start_time.to_f)).to_i
    end
  end

  loop do
    # Receive 64 bytes
    data = handle.bulk_transfer(:endpoint => 0x82, :dataIn => 64, :timeout => 10000)
    #puts data.bytesize
    total_bytes = total_bytes + data.bytesize
  end
end
