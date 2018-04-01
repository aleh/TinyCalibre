# TinyCalibre

Using the RC oscillator with AVR microcontrollers can be handy when you aim for minimalistic design (using ATtiny without a crystal) and don't need high frequencies (i.e. 8 MHz is fine). If your design needs to communicate with the outside world or measure time, then you need at least some precision for your clock. 

The factory calibration byte is good, but you'll need a new one in case you plan to use the micro with voltages different from the ones used for factory calibration. 

For example, ATtiny85's I have are calibrated for 5V VCC, so I need a different calibration byte when I use them with a 3V battery.

## How it works

In order to calibrate your microcontroller with this firmware you'll need an external clock source. This can be an Arduino with a crystal, your scope's test signal or a 32KHz signal from a real time clock module, such as DS1307-based TinyRTC. A TinyRTC module is what I ended up using (make sure to enable the square wave signal on the external pin there, fresh modules have it disabled).

Steps:

- Adjust the sketch for the frequency of your clock source and the pins you plan to use on your board.
- Flash this firmware to your target device.
- In case you use an external programmer adjust the fuse bits to not erase the EEPROM when programming.
- Make sure your board is using the voltage you want (i.e. disconnect the programmer, USB-to-serial converter, or anything else that can influence it).
- Connect your clock source to the pin you've selected in the code. 
- If you use an indicator LED (see the code), then it will shortly blink when the calibration begins and will light the LED for a second when it ends successfully (will blink multiple times in case clock signal is not available or does not match the frequency expectations).
- Now the calibration byte should be stored in the EEPROM at the address you specified (the byte itself and its invertion is stored for control). 
- Flash your own firmware, read the calibration byte from the EEPROM and write it to the OSCCAL register on startup:

        #include <avr/eeprom.h>
        // ...
        bool readCalibrationByte(uint8_t& b) {
          b = eeprom_read_byte((uint8_t*)yourAddress);
          return b == (uint8_t)(~eeprom_read_byte((uint8_t*)(yourAddress + 1)));
        }
        // ...
        void setup() {
          // ...
          uint8_t calibrationByte;
          if (readCalibrationByte(calibrationByte)) {
            OSCCAL = calibrationByte;
          }
          // ...

Enjoy!

---
