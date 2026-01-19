// Base code from https://randomnerdtutorials.com/arduino-load-cell-hx711/

#ifndef SCALE_H
#define SCALE_H

#include "HX711.h"

void initScale();
void scaleTare(); // Tare the scale
float scaleRead(); // Read from the scale
float scaleDummyRead();  // For testing without scale
float scaleCalibrate(); // Calibrate the scale



#endif  // SCALE_H