//
// TinyCalibre. Finds the best calibration byte for the internal RC oscillator based on the external calibration clock.
// Copyright (C) 2018, Aleh Dzenisiuk. All rights reserved.
// 

#include <a21.hpp>

#define TRACE_VIA_SERIAL 0

// The pin the calibration signal will be applied to.
typedef a21::FastPin<1> clockPin;

// The pin we'll toggle from LOW to HIGH when the calibration is successfully completed, e.g. a LED. 
typedef a21::FastPin<2> indicatorPin;

// The frequency of the calibration signal.
const uint16_t clockFrequency = 32768;

// Sometimes F_CPU is different from the effective frequency, like when you switch your 16MHz Arduino 
// to use the RC oscillator and IDE still thinks it's 16MHz or when the clock is divided by 8 using fuse bits. 
// This is the divisor.
const uint16_t F_CPU_DIV = 2;

// Where in the EEPROM the calibration byte should be stored.
// We use 2 bytes: the first one is the calibration byte itself and the next one is its inversion.
const uint16_t eepromAddress = 0;

// This is how you should read the value of the calibrated byte on startup.
bool readCalibrationByte(uint8_t& b) {
  b = a21::EEPROM::read(eepromAddress);
  return b == (uint8_t)(~a21::EEPROM::read(eepromAddress + 1));
}

// How many milliseconds should we count the external clock ticks to get some precision 
// (assuming we can err for 2ms when measuring this should be at least 200).
const uint32_t measurementDuration = 500;

// How many clock flips should we count to get the target duration.
const uint32_t clockFlipCount = measurementDuration * clockFrequency / 1000;

// We should not wait more than this amount of CPU cycles for a clock to flip from one state to another.
// Used to stop time measurement early in case no clock signal is applied.
const uint32_t clockChangeTimeout = 1 + F_CPU / clockFrequency;

void setup() {

  clockPin::setInput(false);
  
  indicatorPin::setOutput();

  #if TRACE_VIA_SERIAL

  Serial.begin(115200);
  Serial.print("TinyCalibre. F_CPU divisor is ");
  Serial.println(F_CPU_DIV);
  
  Serial.print("Default OSCCAL: 0x");
  Serial.println(OSCCAL, HEX);
  
  uint8_t b;
  if (readCalibrationByte(b)) {
    Serial.print("Calibrated OSCCAL (from EEPROM): 0x");
    Serial.println(b, HEX);
  } else {
    Serial.println("No calibrated OSCCAL in the EEPROM");
  }    

  // Make sure everything is transferred before we begin messing with the clock.
  Serial.flush();

  #endif
}

// Waits for the external clock signal to flip to the needed value.
bool waitForClock(bool value) {
    
  for (uint32_t i = 0; i < clockChangeTimeout; i++) {
    if (clockPin::read() == value)
      return true;
  }
  
  return false;
}

// Measures how many milliseconds the external clock will flip this number of times.
bool measureClockFlips(uint16_t flipCount, uint32_t& total_ms) {
  
    if (!waitForClock(true))
      return false;

    uint32_t startTime = millis();
    
    for (uint16_t count = 0; count < flipCount; count++) {  
      if (!waitForClock(false) || !waitForClock(true))
        return false;
    }

    total_ms = F_CPU_DIV * (millis() - startTime);
    
    return true;
}

bool timeForValue(uint8_t value, uint32_t& time) {
  
  uint8_t originalValue = OSCCAL;
  OSCCAL = value;
  
  bool measured = measureClockFlips(clockFlipCount, time);
  
  OSCCAL = originalValue;
  
  return measured;
}

bool calibrate(uint8_t& value) {

  Serial.println("Calibrating...");
  Serial.flush();
  
  uint8_t lowValue = 0x00;
  uint8_t highValue = 0x7F;

  uint32_t time;
  uint8_t m;

  while (lowValue < highValue) {

    uint8_t nm = (lowValue + highValue) / 2;
    if (nm == m)
      break;
      
    m = nm;
    if (!timeForValue(m, time))
      return false;
      
    Serial.print("\t0x");
    Serial.print(m, HEX);
    Serial.print(" - ");
    Serial.print(time);
    Serial.println();
    Serial.flush();

    if (time == measurementDuration) {
      break;
    } else if (time < measurementDuration) {
      lowValue = m;
    } else {
      highValue = m;
    }
  }

  value = m;
  uint16_t error = fabs((int16_t)measurementDuration - time);

  return error * 100l <= 1 * measurementDuration;
}

void updateCalibrationByte(uint8_t b) {
  a21::EEPROM::update(eepromAddress, b);
  a21::EEPROM::update(eepromAddress + 1, ~b);
}

void loop() {

  indicatorPin::setHigh();
  delay(100);
  indicatorPin::setLow();  

  uint8_t value;
  if (!calibrate(value)) {
    
    #if TRACE_VIA_SERIAL
    Serial.print("Failed to calibrate. Make sure there is ");
    Serial.print(clockFrequency);
    Serial.print("Hz calibration signal on pin ");
    Serial.print(clockPin::Pin);
    Serial.println();
    Serial.flush();
    #endif
    
    for (uint8_t i = 0; i < 5; i++) {
      indicatorPin::setHigh();
      delay(200);
      indicatorPin::setLow();
      delay(200);
    }
    
    delay(1000);
    
  } else {

    updateCalibrationByte(value);

    #if TRACE_VIA_SERIAL
    Serial.print("Calibrated successfully. New OSCCAL: 0x");
    Serial.print(value, HEX);
    Serial.println();
    Serial.flush();

    OSCCAL = value;
    Serial.println("Switched to it. The baud rate should be still OK and this text not garbled.");
    Serial.flush();
    #endif

    indicatorPin::setHigh();
    delay(1000);
    indicatorPin::setLow();

    delay(5000);
  }
}

