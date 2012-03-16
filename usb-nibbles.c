
// usb driver related functions

// taken from avr-usb-rgb-led project
PROGMEM char usbDescriptorHidReport[22] = {    /* USB report descriptor */
    0x06, 0x00, 0xff,              // USAGE_PAGE (Generic Desktop)
    0x09, 0x01,                    // USAGE (Vendor Usage 1)
    0xa1, 0x01,                    // COLLECTION (Application)
    0x15, 0x00,                    //   LOGICAL_MINIMUM (0)
    0x26, 0xff, 0x00,              //   LOGICAL_MAXIMUM (255)
    0x75, 0x08,                    //   REPORT_SIZE (8)
    0x95, 0x01,                    //   REPORT_COUNT (1)
    0x09, 0x00,                    //   USAGE (Undefined)
    0xb2, 0x02, 0x01,              //   FEATURE (Data,Var,Abs,Buf)
    0xc0                           // END_COLLECTION
};


/* ------------------------------------------------------------------------- */
/* ------------------------ interface to USB driver ------------------------ */
/* ------------------------------------------------------------------------- */
static unsigned int usb_return_int = 0;
static byte pulse_code_index = 0;

usbMsgLen_t usbFunctionSetup(uchar data[8]) {
	usbRequest_t  *rq = (usbRequest_t *)data;

  if ((rq->bmRequestType & USBRQ_TYPE_MASK) == USBRQ_TYPE_VENDOR) {
	  if (rq->bRequest == get_cpu_frequency) {
	    usb_return_int = F_CPU / 1000; // cpu frequency in killohertz
      usbMsgPtr = (uchar*) &usb_return_int; // set this as the response
      return sizeof(usb_return_int); // send back that response
      
    } else if (rq->bRequest == set_indicator) {
      indicator_state = rq->wValue.bytes[0];
      
    } else if (rq->bRequest == get_indicator) {
      usbMsgPtr = &indicator_state;
      return sizeof(indicator_state);
      
    } else if (rq->bRequest == set_indicator_luminance) {
      indicator_brightness = rq->wValue.bytes[0];
      
    } else if (rq->bRequest == get_indicator_luminance) {
      usbMsgPtr = &indicator_brightness;
      return sizeof(indicator_brightness);
      
    } else if (rq->bRequest == set_pulse_code) {
      pulse_code_index = 0;
      pulse_code_length = rq->wLength.word / 2;
      if (pulse_code_length > MaxCodeLengthBytes) pulse_code_length = MaxCodeLengthBytes;
      return USB_NO_MSG; // tell vusb to use usbFunctionWrite to receive this data
      
    } else if (rq->bRequest == get_pulse_code) {
      usbMsgPtr = (uchar*) &pulse_code;
      return sizeof(pulseCodeEntry) * pulse_code_length;
      
    } else if (rq->bRequest == get_pulse_code_length) {
      usbMsgPtr = (uchar*) &pulse_code_length;
      return sizeof(pulse_code_length);
      
    } else if (rq->bRequest == send_pulse_code) {
      infrared_updates_scheduled = rq->wValue.bytes[0];
      
    } else if (rq->bRequest == get_broadcasting_gap) {
      usbMsgPtr = &broadcasting_gap;
      return sizeof(broadcasting_gap);
      
    } else if (rq->bRequest == get_broadcasting_frequency) {
      byte* two_bytes = (byte*) &usb_return_int;
      two_bytes[0] = broadcasting_frequency;
      two_bytes[1] = broadcasting_duty;
      usbMsgPtr = (uchar*) &usb_return_int;
      return sizeof(usb_return_int);
      
    } else if (rq->bRequest == set_broadcasting_gap) {
      // set the interval between code broadcasts (sort of milliseconds - lower is more often updates)
      broadcasting_gap = rq->wValue.word;
      
    } else if (rq->bRequest == set_broadcasting_frequency) {
      broadcasting_frequency = rq->wValue.word; // set the PWM control for frequency (lower is faster)
      broadcasting_duty = rq->wIndex.word; // use this as the duty for the LED (the portion which it is on for)
      
    } else if (rq->bRequest == set_indicator_as_debug_light) { // special debug mode
      indicator_as_debug_light = rq->wValue.bytes[0];
		}
  }
  
  return 0;   /* default for not implemented requests: return no data back to host */
}

// receive in chunks the data to fill in our pulse_code
uchar usbFunctionWrite(byte data[], byte data_length) {
    if (pulse_code_index + data_length > MaxCodeLengthBytes) {
      data_length = MaxCodeLengthBytes - pulse_code_index;
    }
    
    while (data_length > 0) {
      ((byte*) pulse_code)[pulse_code_index] = data[0];
      pulse_code_index += 1;
      data_length -= 1;
      data += 1;
    }
    
    return (pulse_code_index >= pulse_code_length);
}

/* ------------------------------------------------------------------------- */
/* ------------------------ Oscillator Calibration ------------------------- */
/* ------------------------------------------------------------------------- */

/* Calibrate the RC oscillator to 8.25 MHz. The core clock of 16.5 MHz is
 * derived from the 66 MHz peripheral clock by dividing. Our timing reference
 * is the Start Of Frame signal (a single SE0 bit) available immediately after
 * a USB RESET. We first do a binary search for the OSCCAL value and then
 * optimize this value with a neighboorhod search.
 * This algorithm may also be used to calibrate the RC oscillator directly to
 * 12 MHz (no PLL involved, can therefore be used on almost ALL AVRs), but this
 * is wide outside the spec for the OSCCAL value and the required precision for
 * the 12 MHz clock! Use the RC oscillator calibrated to 12 MHz for
 * experimental purposes only!
 */
static void calibrateOscillator(void) {
   uchar       step = 128;
   uchar       trialValue = 0, optimumValue;
   int         x, optimumDev, targetValue = (unsigned)(1499 * (double)F_CPU / 10.5e6 + 0.5);

   /* do a binary search: */
   do {
       OSCCAL = trialValue + step;
       x = usbMeasureFrameLength();    /* proportional to current real frequency */
       if(x < targetValue)             /* frequency still too low */
           trialValue += step;
       step >>= 1;
   } while(step > 0);
   /* We have a precision of +/- 1 for optimum OSCCAL here */
   /* now do a neighborhood search for optimum value */
   optimumValue = trialValue;
   optimumDev = x; /* this is certainly far away from optimum */
   for(OSCCAL = trialValue - 1; OSCCAL <= trialValue + 1; OSCCAL++){
       x = usbMeasureFrameLength() - targetValue;
       if(x < 0)
           x = -x;
       if(x < optimumDev){
           optimumDev = x;
           optimumValue = OSCCAL;
       }
   }
   OSCCAL = optimumValue;
}
/*
Note: This calibration algorithm may try OSCCAL values of up to 192 even if
the optimum value is far below 192. It may therefore exceed the allowed clock
frequency of the CPU in low voltage designs!
You may replace this search algorithm with any other algorithm you like if
you have additional constraints such as a maximum CPU clock.
For version 5.x RC oscillators (those with a split range of 2x128 steps, e.g.
ATTiny25, ATTiny45, ATTiny85), it may be useful to search for the optimum in
both regions.
*/

void usbEventResetReady(void) {
   calibrateOscillator();
   eeprom_write_byte(0, OSCCAL);   /* store the calibrated value in EEPROM */
}

