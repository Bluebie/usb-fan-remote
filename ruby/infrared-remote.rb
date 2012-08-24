require 'libusb'
require 'psych'

class InfraredRemote
  attr_accessor :code_send_repeats
  
  # get a usb pixel
  def self.[] number
    self.new(devices[number])
  end
  
  # returns an array of pixel devices found on this computer
  def self.devices
    usb = LIBUSB::Context.new
    usb.devices.select { |device|
      device.manufacturer == 'creativepony.com' and device.product == 'Simple Remote'
    }.map { |device| self.new(device) }
  end
  
  # shortcut to the first control on the system - generally there should only be one!
  def self.first
    self.devices.first
  end
  
  def initialize device
    device = device.open
    @device = device
    @code_send_repeats = 10
    @usb_timeout = 60 # milliseconds
    @indicator_gamma = 2.2 # used by indicator_brightness to transform brightness to luminance and viceversa
    
    ObjectSpace.define_finalizer(self, self.class.finalize(device))
  end
  
  def self.finalize(device)
    device.usb_close
  end
  
  # set device in to default configuration
  def default
    # setup default configuration
    @code_send_repeats = 10
    self.set_timer( frequency: 36_000, duty: 0.45 )
    self.indicator = false
    self.broadcasting_gap = 0.1 # seconds between each pulse
    usb_set_indicator_as_debug_light( wValue: 0 ) # turn off debugging light mode
    self # return self for nifty chaining
  end
  alias_method :reset, :default
  
  # this is pretty vague - the device promises it will have a gap at least this long, but could be much longer
  def broadcasting_gap
    usb_get_broadcasting_gap(dataIn: 1).unpack('C').first / 1000.0
  end
  def broadcasting_gap= seconds
    raise "broadcasting gap cannot be greater than 0.255" unless seconds <= 0.255
    usb_set_broadcasting_gap(wValue: (seconds * 1000.0).to_i)
  end
  
  def cpu_frequency
    usb_get_cpu_frequency(dataIn: 2).unpack('v').first * 1000
  end
  
  # set the IR frequency - the precision is pretty bad, but give it a number like 37_000 and it gets as close as it can
  attr_reader :infrared_duty
  def frequency
    cpu_frequency.to_f / get_timer_config[:timer_width].to_f / 8
  end
  
  def frequency= hz
    update_timer_config( frequency: hz )
  end
  
  attr_reader :infrared_duty
  def infrared_duty= floatvalue
    set_timer( duty: floatvalue )
  end
  
  # get or set indicator mode - the indicator can be any of the specified colours, or false to disable it
  IndicatorCodes = [false, :red, :yellow, :green, :aqua, :blue, :violet]
  def indicator
    IndicatorCodes[usb_get_indicator(dataIn: 1).unpack('C').first]
  end
  def indicator= color
    color = :violet if color == :purple
    color = false if color == :black
    raise "Unknown Color" unless IndicatorCodes.include? color
    usb_set_indicator(wValue: IndicatorCodes.index(color))
  end
  alias_method :indicator_color, :indicator
  alias_method :indicator_color=, :indicator=
  
  # get and set indicator luminance or brightness - luminance is literal and brightness is perceptual - use 0.0-1.0 floats
  def indicator_luminance
    usb_get_indicator_luminance(dataIn: 1).unpack('C').first.to_f / 255.0
  end
  def indicator_luminance= value
    usb_set_indicator_luminance(wValue: (value * 255.0).round % 256)
  end
  def indicator_brightness
    self.indicator_luminance ** (1.0 / @indicator_gamma)
  end
  def indicator_brightness= value
    self.indicator_luminance = (value.to_f ** @indicator_gamma)
  end
  
  # use indicator light for debugging
  def debug
    usb_set_indicator_as_debug_light wValue: 1
    self
  end
  
  # send the currently loaded pulse code
  def send! count = @code_send_repeats
    usb_send_pulse_code(wValue: count)
  end
  
  def pronto code, repeats = 0
    code = code.split(' ').map { |i| i.to_i(16) }
    raw, freq, seq_1_length, seq_2_length = code.shift(4)
    seq_1 = pronto_convert(code.shift(seq_1_length))
    seq_2 = pronto_convert(code.shift(seq_2_length))
    
    frequency = 1000000 / (freq * 0.241246)
    self.set_timer(frequency: frequency, duty: 0.3333) # setup device configuration from pronto code preamble
    self.pulse_code = seq_1
    regular_gap = self.broadcasting_gap
    self.send! 1
    
    if repeats > 0
      sleep((seq_1.reduce(:+) / 1000.0) + regular_gap)
      self.pulse_code = seq_2
      self.send! repeats
    end
  end
  
  def pronto_convert sequence
    multiply = (9000.0 / 343.0)
    sequence.map do |num|
      (num * multiply).round
    end
  end
  
  
  # install the available usb requests
  dir = File.dirname(__FILE__)
  Psych.load(open "#{dir}/requests.yaml").each do |name, number|
    define_method "usb_#{name}" do |*args|
      usb_send({ bRequest: number }.merge(*args))
    end
  end
  
  # send code sequence @code_send_repeats many times - if another send_pulse_code request comes in, it'll replace this one mid way through
  #def send_pulse_code code_array
  #  pulse_code = code_array
  #  send!
    #usb_send_pulse_code(wValue: @code_send_repeats) # request device send the code sequence
  #end
  
  # set pulse code array
  def pulse_code= code_array
    usb_set_pulse_code(dataOut: code_array.pack('v*')) # load this code sequence in to the device
  end
  
  # get pulse code array
  def pulse_code
    # 64 is just the max number of entries which maybe in there times two
    length = usb_get_pulse_code_length(dataIn:1).unpack('C').first # find out the length of the currently loaded pulse code
    usb_get_pulse_code(dataIn: length * 2).unpack('v*')
  end
  
  protected #############################################################
  
  def get_timer_config; Hash[[:timer_width, :timer_duty].zip(usb_get_broadcasting_frequency(dataIn: 2).unpack('C'))]; end
  
  def set_timer update = {}
    status_quo = get_timer_config
    
    # figure out what the original LED duty was, convert to float
    duty = update[:duty].to_f || ( status_quo[:timer_duty].to_f / status_quo[:timer_width].to_f )
    
    status_quo[:timer_width] = (cpu_frequency.to_f / update[:frequency].to_f / 8.0).round if update.key? :frequency
    status_quo[:timer_duty] = (duty * status_quo[:timer_width]).round # redo duty to match new frequency
    usb_set_broadcasting_frequency( wValue: status_quo[:timer_width].round, wIndex: status_quo[:timer_duty].round )
  end
  
  def usb_send opts 
    #req_num, value = 0, index = 0, data = nil
    #@device.claim_interface(0)
    result = @device.control_transfer({ wIndex: 0, wValue: 0, timeout: @usb_timeout, bmRequestType: usb_request_type(opts) }.merge opts)
    #@device.release_interface(0)
    return result
  end
  
  def usb_request_type opts
    c = LIBUSB::Call
    value = c::RequestTypes[:REQUEST_TYPE_VENDOR] | c::RequestRecipients[:RECIPIENT_DEVICE]
    value |= c::EndpointDirections[:ENDPOINT_OUT] if opts.has_key? :dataOut
    value |= c::EndpointDirections[:ENDPOINT_IN] if opts.has_key? :dataIn
    return value
  end
end
