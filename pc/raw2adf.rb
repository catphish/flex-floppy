class Track
  def initialize
    @status = -1
    @syncbyte = 0
  end

  def process_bit(b)
    if @status == -1
      @syncbyte = ((@syncbyte << 1) & 0xffffffff) + b;
      if @syncbyte == 0x44894489
        @status = 0
      end
    else
      # Append one sector of MFM bits
    end
  end
end

while STDIN.read(1)
  track_number, track_bytes = STDIN.read(5).unpack('CN')
  puts "Decoding track #{track_number}"
  timings = STDIN.read(track_bytes).unpack('S<*')

  track = Track.new

  previous_timing = timings.shift
  timings.each do |timing|
    delta = timing - previous_timing
    if delta < 0
      delta = delta + 65536
    end
    
    bits = (delta - 80) / 160
    bits.times do |n|
      track.process_bit(0)
    end
    track.process_bit(1)

    previous_timing = timing
  end
end