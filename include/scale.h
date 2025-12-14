// Base code from https://randomnerdtutorials.com/arduino-load-cell-hx711/

#ifndef SCALE_H
#define SCALE_H

#include "HX711.h"

void initScale();
void scaleCalibrate();
void scaleTare();
void scaleRead();
// Return the most recent weight in units (grams by default)
float scaleGetUnits();

// Additional APIs for second load cell
float scaleGetUnits1();
float scaleGetUnits2();
// Tare both scales
void scaleTareAll();


#endif  // LITTLEFS-CONF_H