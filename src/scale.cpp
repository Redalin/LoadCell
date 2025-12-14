#include "scale.h"


HX711 scale;

void initScale() {
    // Initialization code for the scale

    // HX711 circuit wiring
    const int LOADCELL_DOUT_PIN = 14;
    const int LOADCELL_SCK_PIN = 13;

    //Set the CALIBRATION FACTOR
    #define CALIBRATION_FACTOR 2051.7

    scale.begin(LOADCELL_DOUT_PIN, LOADCELL_SCK_PIN);

    scale.set_scale(CALIBRATION_FACTOR);
}

void scaleCalibrate() {
    // Code to read from the scale can be added here
        // if (scale.wait_ready_timeout(1000)) {
    if (scale.is_ready()) {
        scale.set_scale();    
        Serial.println("Tare... remove any weights from the scale.");
        delay(5000);
        scale.tare();
        Serial.println("Tare done...");
        Serial.print("Place a known weight on the scale...");
        delay(5000);
        long reading = scale.get_units(10);
        Serial.print("Result: ");
        Serial.println(reading);
    } else {
        Serial.println("HX711 not found.");
    }
    delay(1000);
}

void scaleTare() {
    // Code to tare the scale can be added here
    if (scale.is_ready()) {
        Serial.println("Tare... remove any weights from the scale.");
        // remove the delay to speed up tare
        // delay(5000); 
        scale.tare();
        Serial.println("Tare done...");
    } else {
        Serial.println("HX711 not found.");
    }
}   

void scaleRead() {

    if (scale.is_ready()) {
        
        float reading = scale.get_units();
        Serial.print(reading);
        Serial.println("g");
    } else {
        Serial.println("HX711 not found.");
    }
}

float scaleGetUnits() {
    if (scale.is_ready()) {
        // average a few readings for stability
        return scale.get_units(5);
    }
    return NAN;
}