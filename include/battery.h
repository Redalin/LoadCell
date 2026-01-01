// Header file for battery measurement functions
#ifndef BATTERY_H
#define BATTERY_H

float readVBAT();   
// Reads and returns the battery voltage in volts

float readAveragedMilliVolts(int pin, int samples);

#endif  // BATTERY_H