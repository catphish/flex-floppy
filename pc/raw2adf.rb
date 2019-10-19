class Sector

  def initialize
    @current_byte = 0
    @current_bit  = 0
    @mfm_data = []
  end

  def process_bit(b)
    @current_byte <<= 1
    @current_byte += b
    @current_bit += 1
    if @current_bit == 8
      @mfm_data << (@current_byte & 0x55555555)
      @current_byte = 0
      @current_bit = 0
      return true if @mfm_data.size == 1080
    end
    false
  end

  def track
    @mfm_data[1] << 1 | @mfm_data[5]
  end

  def sector
    @mfm_data[2] << 1 | @mfm_data[6]
  end

  def data
    512.times.map do |n|
      @mfm_data[56+n] << 1 | @mfm_data[568+n]
    end.pack('C512')
  end

  def verify
    # TODO: Checksum verify
    true
  end

end

class Track

  def initialize
    @syncbyte = 0
    @sectors = []
  end

  def process_bits(bits)
    bits.each do |b|
      if @syncbyte == 0x44894489
        # Already in sync, pass bits to current sector
        if @current_sector.process_bit(b)
          @syncbyte = 0
          if @current_sector.verify
            @sectors[@current_sector.sector] = @current_sector
          end
          if @sectors.compact.size == 11
            return true
          end
        end
      else
        @syncbyte = ((@syncbyte << 1) & 0xffffffff) + b;
        if @syncbyte == 0x44894489
          # Now in sync. Populate sector
          @current_sector = Sector.new
        end
      end
    end
    false
  end

  def data
    @sectors.map(&:data).join
  end

end

while STDIN.read(1)
  track_number, track_bytes = STDIN.read(5).unpack('CN')
  STDERR.puts "Decoding track #{track_number}"
  timings = STDIN.read(track_bytes).unpack('n*')

  track = Track.new

  timings.each do |timing|
    if timing > 160 && timing < 800
      bits = (timing - 80) / 160
      break if track.process_bits([0]*bits + [1])
    end
  end
  STDOUT.write track.data
end