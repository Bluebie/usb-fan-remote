#!/usr/local/bin/ruby
Dir.chdir File.dirname(__FILE__)
require_relative './fan-remote.rb'

remote = FanRemote.first.reset
remote.broadcasting_gap = 0.1

ack = lambda do | color, loops = 3 |
  remote.indicator_brightness = 1.0
  loops.times do
    remote.indicator = color
    sleep(0.01)
    remote.indicator = false
    sleep(0.1)
  end
end

PWMRate = 8.0 # seconds
SpeedSettings = [:off, :low, :medium, :high]

if ARGV[0] == 'set'
  if ARGV[1] == 'speed' || ARGV[1] == 'mode' || ARGV[1] == 'fan'
    speed = ARGV[2]
    if speed =~ /^[0-9.]+%$/
      # do a little blink notice of startup
      ack[ :aqua ]
      
      speed = speed.to_f / 100.0 # convert speed request in to floating value 0.0-1.0
      converted_speed = (speed * (SpeedSettings.length - 1))
      low_speed = SpeedSettings[converted_speed.floor] # lower speed setting
      high_speed = SpeedSettings[converted_speed.ceil] # get the higher speed setting
      proportion = converted_speed % 1.0 # how much time should we spend in the higher speed setting?
      
      
      # enter loop start sending codes to fan
      begin
        puts "Entering continuous control..."
        remote.indicator_brightness = 1.0
        loop do
          remote.speed = high_speed
          #remote.indicator = :red
          sleep PWMRate * proportion
          remote.speed = low_speed
          #remote.indicator = :green
          sleep PWMRate * (1.0 - proportion)
        end
      rescue Interrupt => e
        # user asked program to quit, so lets go out in style...
        puts "Turning off fan..."
        remote.speed = :off
        ack[ :red, 2 ]
      end
      
    else
      ack[ :blue ]
      remote.speed = speed.to_sym
    end
  elsif ARGV[1] == 'light' or ARGV[1] == 'lamp'
    remote.lamp = ['on', 'yes', 'lit', 'bright', 'true', 'enabled'].include?(ARGV[2])
    sleep(0.15) # takes about this long for the fan to react - so this syncs up the indicator blinks
    ack[ :yellow ]
  elsif ARGV[1] == 'indicator'
    if ARGV[2] == 'stream'
      loop do
        color, brightness = $stdin.gets.strip.split(':')
        remote.indicator = color != 'off' ? color.to_sym : false
        remote.indicator_brightness = brightness.to_f if brightness and brightness.length > 0
      end
    else
      remote.indicator = ARGV[2].to_sym
    end
  else
    puts "Unknown setting"
  end
else
  puts "Unknown command"
end
