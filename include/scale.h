// Base code from https://randomnerdtutorials.com/arduino-load-cell-hx711/

#ifndef SCALE_H
#define SCALE_H

#include "HX711.h"

void initScale();
// Tare the single scale (child nodes only have one scale)
void scaleTare();
void scaleTareAll();  // Tare all connected scales (compatibility function)

// Calibrate the scale (child nodes only have one scale)
float scaleCalibrate();
void scaleCalibrateAsync( uint32_t clientId );
float scaleRead(); // Read from the scale
float scaleDummyRead();  // For testing without scale



#endif  // SCALE_H