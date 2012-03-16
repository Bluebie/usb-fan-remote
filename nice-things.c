#import <util/delay_basic.h>
#import <avr/io.h>

// For the niceness
typedef unsigned char byte;
typedef unsigned char boolean;

// make bit & value, eg bit(5) #=> 0b00010000
#define bit(number) _BV(number)
#define pin(number) _BV(number)

// USI serial aliases
#define USIOutputPort    PORTE
#define USIInputPort     PINE
#define USIDirectionPort DDRE
#define USIClockPin      PE4
#define USIDataInPin     PE5
#define USIDataOutPin    PE6

// booleans
#define    on 255
#define   off 0
#define  true 1
#define false 0
#define   yes true
#define    no false

// ensure a value is within the bounds of a minimum and maximum (inclusive)
#define constrainUpper(value, max) (value > max ? max : value)
#define constrainLower(value, min) (value < min ? min : value)
#define constrain(value, min, max) constrainLower(constrainUpper(value, max), min)

// set a pin on DDRB to be an input or an output - i.e. becomeOutput(pin(3));
#define inputs(pinmap) DDRB &= ~(pinmap)
#define outputs(pinmap) DDRB |= (pinmap)

// turn some pins on or off
#define pinsOn(pinmap) PORTB |= (pinmap)
#define pinsOff(pinmap) PORTB &= ~(pinmap)
#define pinsToggle(pinmap) PORTB ^= pinmap

// turn a single pin on or off
#define pinOn(pin) pinsOn(bit(pin))
#define pinOff(pin) pinsOff(bit(pin))
// TODO: Should be called pinToggle
#define toggle(pin) pinsToggle(bit(pin))

// delay a number of microseconds - or as close as we can get
#define microdelay(microseconds) _delay_loop_2(((microseconds) * (F_CPU / 100000)) / 40)

// delay in milliseconds - a custom implementation to avoid util/delay's tendancy to import floating point math libraries
inline void delay(unsigned int ms) {
  while (ms > 0) {
    // delay for one millisecond (250*4 cycles, multiplied by cpu mhz)
    // subtract number of time taken in while loop and decrement and other bits
    _delay_loop_2((25 * F_CPU / 100000));
    ms--;
  }
}



// digital read returns 0 or 1
#define get(pin) ((PINB >> pin) & 0b00000001)
#define getBitmap(bitmap) (PINB & bitmap)
inline void set(byte pin, byte state) {
  if (state) { pinOn(pin); } else { pinOff(pin); }
  // alternatly:
  // PORTB = (PORTB & ~(bit(pin)) | ((state & 1) << pin);
}

