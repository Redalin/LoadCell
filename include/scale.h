// Base code from https://randomnerdtutorials.com/arduino-load-cell-hx711/

#ifndef SCALE_H
#define SCALE_H

#include "HX711.h"

void initScale();
// Tare a specific scale: which=1 or 2, which<=0 => both
void scaleTare(int which);
// Calibrate a specific scale (async): which=1 or 2. clientId optional to direct response
void scaleCalibrateAsync(int which, uint32_t clientId = 0);
void scaleRead();
// Return the most recent weight in units (grams by default)
float scaleGetUnits();
float scaleGetDummyUnits();  // For testing without scale

// Per-scale getters
float scaleGetUnits1();
float scaleGetUnits2();
// Compatibility helper: tare both scales
void scaleTareAll();


#endif  // LITTLEFS-CONF_H