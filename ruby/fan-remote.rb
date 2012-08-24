require_relative './infrared-remote.rb'

class FanRemote < InfraredRemote
  # set if light is switched on or off
  def lamp= light_on
    self.pulse_code = case light_on
      when true
        [BIG, lil, BIG, lil, lil, BIG, BIG, lil, BIG, lil, lil, BIG, BIG, lil, lil, BIG, lil, BIG, lil, BIG, lil, BIG, lil]
      when false
        [BIG, lil, BIG, lil, lil, BIG, BIG, lil, BIG, lil, lil, BIG, lil, BIG, BIG, lil, lil, BIG, lil, BIG, lil, BIG, lil]
      end
    send!
  end
  alias_method :light=, :lamp=
  
  def speed= mode # takes in :off, :low, :medium, :high
    self.pulse_code = case mode
      when :off
        [BIG, lil, BIG, lil, lil, BIG, BIG, lil, BIG, lil, lil, BIG, lil, BIG, lil, BIG, lil, BIG, lil, BIG, lil, BIG, BIG]
      when :high
        [BIG, lil, BIG, lil, lil, BIG, BIG, lil, BIG, lil, lil, BIG, lil, BIG, lil, BIG, lil, BIG, lil, BIG, BIG, lil, lil]
      when :medium
        [BIG, lil, BIG, lil, lil, BIG, BIG, lil, BIG, lil, lil, BIG, lil, BIG, lil, BIG, lil, BIG, BIG, lil, lil, BIG, lil]
      when :low
        [BIG, lil, BIG, lil, lil, BIG, BIG, lil, BIG, lil, lil, BIG, lil, BIG, lil, BIG, BIG, lil, lil, BIG, lil, BIG, lil]
      end
    
    send!
  end
  
  private
  BIG = 1300
  def lil
    500
  end
end