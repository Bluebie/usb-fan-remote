// USB Lamp by Jenna Fox
// a little programmable led toy, in the vein of blinkm, 
// with a USB interface, and an educational slant
#import <stdlib.h>
#import <avr/io.h>
#import <util/delay.h>
#import <math.h>
#import <nice-things.c>

#define InfraredOutputPin 1 /* pin where LED connects (high = light on) - do not change without updating timer code in send_infrared() */

typedef unsigned int pulseCodeEntry;
#define MaxCodeLength 128
#define MaxCodeLengthBytes (MaxCodeLength * sizeof(pulseCodeEntry))

#define IndicatorOff 0
#define IndicatorRed 1
#define IndicatorYellow 2
#define IndicatorGreen 3
#define IndicatorAqua 4
#define IndicatorBlue 5
#define IndicatorPurple 6

byte broadcasting_gap = 50; // 50ms between each code send
byte broadcasting_frequency = 57; // 36khz default
byte broadcasting_duty = 25; // a little less than half
byte indicator_state = 0; // indicator light - off, red, yellow, green, aqua, blue, purple
byte indicator_brightness = 255; // brightness of indicator, in the range of 0-255
byte infrared_updates_scheduled = 0; // how many more updates should we send to the receiver
pulseCodeEntry pulse_code[MaxCodeLength] = {0}; // storage for current pulse code
byte pulse_code_length = 0; // length of actual pulse code
byte indicator_as_debug_light = false; // switch off indicator when code is done sending - for debug

// usbdrv.h dependancies
#import <avr/wdt.h>
#import <avr/eeprom.h>
#import <avr/pgmspace.h>
#import <requests.h>
#import <usbdrv.h>
#import <usb-nibbles.c>
#import <avr/interrupt.h>

// make and install with ./upload.rb usb-lamp.c 16.5mhz

// configured to oscillate to ground when LED active - connect LEDs from VCC to InfraredOutputPin
void send_infrared(void) {
  //counter value 25
  //counter prescale /8
  //on cpu speed of 8mhz
  
  // setup our simple IO
  inputs(pin(InfraredOutputPin));
  pinOff(InfraredOutputPin);
  
  // setup timer0 as clear on match mode, driving pin 1 at 40khz
  // setup timer with div 8 if running at 8mhz
  TCCR0A = 0b00000011; // enable Fast PWM mode 7
  
#if F_CPU == 8000000
  TCCR0B = 0b00000010; // scaling is clk/8
#elif F_CPU ==  1000000
  TCCR0B = 0b00000001; // scaling is clk/1
#elif F_CPU == 16500000
  TCCR0B = 0b00000010;
#else
# error send_infrared library only works at 8mhz or 1mhz clock speeds (i.e. on attiny85)
#endif
  
  TCCR0B |= 0b00001000; // enable WGM02 - part of the Fast PWM mode selection
  OCR0A = broadcasting_frequency; // defines speed: 1mhz / 25 = 37khz
  OCR0B = broadcasting_duty; // how much is the pulse on for? a little less than half the time
  
  // iterate through our pulses
  byte i = 0;
  //TCCR0A ^= 0b00100000; // toggle output to LED driving pin
  TCCR0A |= 0b00100000; // enable output to LED driving pin
  while (i < pulse_code_length) {
    DDRB ^= pin(InfraredOutputPin);
    microdelay(pulse_code[i] - 6); // 6 is just more trial and error arbitrary tuning - I bet it only works at 8mhz tho!
    
    i++;
  }
  
  TCCR0A = 0; // turn off timer things
  TCCR0B = 0;
  inputs(pin(InfraredOutputPin));
  pinOff(InfraredOutputPin);
}

// update the state of the indicator lights
// red is positive pb3, negative gnd
// green is positibe pb4, negative gnd
// blue is positive pb4, negative pb3
byte brightness_iter = 0;
void update_indicator(void) {
  static byte indicator_toggle;
  byte color = ((indicator_state - indicator_toggle) / 2) % 3; // is one of 0, 1, 2 (red, green blue)
  // indicator toggle oscillates on each call between 0 and 1
  // in doing so, it rattles the color value for in-between states yellow, aqua, and purple
  
  if (indicator_state != 0 && brightness_iter <= indicator_brightness) { // the light is turned on at all
    if (color == 0) { // red
      inputs(pin(4)); // turn off other lights
      pinOff(4);
      pinOn(3);
      outputs(pin(3));
    } else if (color == 1) { // green  
      pinOff(3);
      pinOn(4);
      outputs(pin(4));
    } else if (color == 2) { // blue
      pinsOff(pin(3) | pin(4));
      outputs(pin(4));
    }
  } else {
    inputs(pin(3) | pin(4)); // disconnect blue/green pair from circuit
    pinsOff(pin(3) | pin(4)); // stop powering anything
  }
  
  indicator_toggle = !indicator_toggle;
  brightness_iter += 1;
}


// use indicator to emit debugging info
void debug_indicator(byte mode) {
  if (indicator_as_debug_light == false) return;
  indicator_brightness = 255;
  indicator_state = mode;
  update_indicator();
}


int main(void) {
  uchar calibrationValue;
  
  wdt_disable(); // turn off watchdog timer if it was on
  
  calibrationValue = eeprom_read_byte(0); /* calibration value from last time */
  if (calibrationValue != 0xff) {
    OSCCAL = calibrationValue;
  }
  
  // setup usb things
  inputs(pin(0) | pin(2));
  pinsOff(pin(0) | pin(2));
  outputs(pin(InfraredOutputPin));
  pinOff(InfraredOutputPin);
  
  
  usbInit();
  usbDeviceDisconnect();
  delay(100); // wait 100ms to force device re-enumeration - makes sure pc and device agree we are starting from scratch here
  usbDeviceConnect();
  
  sei(); // interrupts are a go!
  
  // setup indicator lights
  outputs(pin(4) | pin(3));
  
  byte gaps;
  while(true) {
    usbPoll();
    
    // update the LED state
    update_indicator();
    
    if (infrared_updates_scheduled > 0) {
      debug_indicator(IndicatorRed);
      send_infrared();
      infrared_updates_scheduled -= 1;
      debug_indicator(IndicatorOff);
      
      gaps = broadcasting_gap / 4; // we want at least a 80ms gap after this broadcast
      while (gaps > 0) { // this could take longer, doing usb stuff..
        usbPoll(); // IDK how long this takes - it's probably pretty random really.
        delay(4); // a roughly 4ms gap
        update_indicator();
        gaps -= 1;
      }
    }
  }
}

