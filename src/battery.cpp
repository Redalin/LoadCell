#include "battery.h"
#include "config.h"
#include <Arduino.h>

// Global battery voltage variable (defined here, declared as extern in config.h)
float vbat = 0.0;

// Optional: smoothing
float readAveragedMilliVolts(int pin, int samples) {
  float sum = 0;
  for (int i = 0; i < samples; ++i) {
    sum += analogReadMilliVolts(pin);
    delayMicroseconds(200); // small settle time
  }
  return sum / samples;
}


// read VBAT helper (uses analogRead)
float readVBAT() {  
  // Ensure correct attenuation for your expected pin voltage
  // If the ADC pin sees < 1.1V (as with your divider), ADC_0db is okay.
  // Using ADC_11db is also fine; analogReadMilliVolts accounts for it.
  // analogSetPinAttenuation(VBAT_PIN, ADC_0db);  // or ADC_11db


  // Read calibrated millivolts at the pin
  float mv = readAveragedMilliVolts(VBAT_PIN, 16);  // mV at ADC pin
  float measured = mv / 1000.0f;                       // V at ADC pin
  float actual = measured * VBAT_DIVIDER;              // V at battery
  debug("VBAT raw: "); debug(mv);
  debug(" measured: "); debug(String(measured, 2));
  debugln("v");
  return actual;
  
}