#include "scale.h"
#include "config.h"
// Mutex to protect concurrent access to the HX711 from different tasks/callbacks
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
static SemaphoreHandle_t scaleMutex = NULL;
HX711 scale;
HX711 scale2;

void initScale() {
    // Initialization code for the scale

    // HX711 pins and calibration are defined in include/config.h

    // create mutex if not already created
    if (scaleMutex == NULL) {
        scaleMutex = xSemaphoreCreateMutex();
    }

    if (scaleMutex) xSemaphoreTake(scaleMutex, portMAX_DELAY);
    // init primary scale
    scale.begin(LOADCELL_DOUT_PIN, LOADCELL_SCK_PIN);
    scale.set_scale(CALIBRATION_FACTOR);
    if (scale.wait_ready_timeout(500)) {
        scale.tare();  // Reset the scale to 0 on initialization
    } else {
        Serial.println("HX711 not found during init (will retry later)");
    }
    // init second scale
    scale2.begin(LOADCELL2_DOUT_PIN, LOADCELL2_SCK_PIN);
    scale2.set_scale(LOADCELL2_CALIBRATION_FACTOR);
    if (scale2.wait_ready_timeout(500)) {
        scale2.tare();
    } else {
        Serial.println("HX711 (2) not found during init (will retry later)");
    }
    if (scaleMutex) xSemaphoreGive(scaleMutex);
    Serial.println("Scale initialized.");
}

void scaleCalibrate() {
    // Code to read from the scale can be added here
        // if (scale.wait_ready_timeout(1000)) {
    if (scaleMutex) xSemaphoreTake(scaleMutex, portMAX_DELAY);
    if (scale.wait_ready_timeout(1000)) {
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
    if (scaleMutex) xSemaphoreGive(scaleMutex);
    delay(1000);
}

void scaleTare() {
    // Code to tare the scale can be added here
    if (scaleMutex) xSemaphoreTake(scaleMutex, portMAX_DELAY);
    Serial.println("Tare... remove any weights from the scales.");
    if (scale.wait_ready_timeout(500)) {
        scale.tare();
    } else {
        Serial.println("HX711 not found (scale 1).");
    }
    if (scale2.wait_ready_timeout(500)) {
        scale2.tare();
    } else {
        Serial.println("HX711 not found (scale 2).");
    }
    Serial.println("Tare done...");
    if (scaleMutex) xSemaphoreGive(scaleMutex);
}   

void scaleRead() {

    if (scaleMutex) xSemaphoreTake(scaleMutex, portMAX_DELAY);
    if (scale.wait_ready_timeout(200)) {
        float reading = scale.get_units();
        // Serial.print(reading);
        // Serial.println("g");
    } else {
        Serial.println("HX711 not found.");
    }
    if (scaleMutex) xSemaphoreGive(scaleMutex);
}

float scaleGetUnits() {
    float result = NAN;
    if (scaleMutex) xSemaphoreTake(scaleMutex, portMAX_DELAY);
    if (scale.wait_ready_timeout(200)) {
        // average a few readings for stability
        float reading = scale.get_units(5);
        // Serial.print("Scale reading: ");
        // Serial.print(reading);
        // Serial.println("g");
        result = reading;
    } else {
        Serial.println("HX711 not ready");
    }
    if (scaleMutex) xSemaphoreGive(scaleMutex);
    return result;
}

// Return units for primary scale (same as scaleGetUnits)
float scaleGetUnits1() {
    float result = NAN;
    if (scaleMutex) xSemaphoreTake(scaleMutex, portMAX_DELAY);
    if (scale.wait_ready_timeout(200)) {
        result = scale.get_units(5);
    }
    if (scaleMutex) xSemaphoreGive(scaleMutex);
    return result;
}

// Return units for secondary scale
float scaleGetUnits2() {
    float result = NAN;
    if (scaleMutex) xSemaphoreTake(scaleMutex, portMAX_DELAY);
    if (scale2.wait_ready_timeout(200)) {
        result = scale2.get_units(5);
    }
    if (scaleMutex) xSemaphoreGive(scaleMutex);
    return result;
}

// Tare both scales
void scaleTareAll() {
    if (scaleMutex) xSemaphoreTake(scaleMutex, portMAX_DELAY);
    if (scale.wait_ready_timeout(200)) scale.tare();
    if (scale2.wait_ready_timeout(200)) scale2.tare();
    if (scaleMutex) xSemaphoreGive(scaleMutex);
}