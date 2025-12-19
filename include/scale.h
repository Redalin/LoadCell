// Base code from https://randomnerdtutorials.com/arduino-load-cell-hx711/

#ifndef SCALE_H
#define SCALE_H

#include "HX711.h"

void initScale();
// Tare a specific scale: which=1 or 2, which<=0 => both
void scaleTare(int which);
// Calibrate a specific scale: which=1 or 2. Returns measured reading (NaN on error)
float scaleCalibrate(int which);
void scaleCalibrateAsync(int which, uint32_t clientId = 0);
// Read from a specific scale (which=1 or 2). Default to scale 1
float scaleGetUnits(int which = 1);
float scaleGetUnits1();
float scaleGetUnits2();
float scaleDummyRead();  // For testing without scale
void scaleTareAll();  // Tare both scales


#endif  // LITTLEFS-CONF_H