#include "scale.h"
#include "config.h"
// Mutex to protect concurrent access to the HX711 from different tasks/callbacks
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "webpage.h"
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

// Calibrate specific scale. which=1 or 2. Returns measured reading (NaN on error)
float scaleCalibrate(int which) {
    float result = NAN;
    if (scaleMutex) xSemaphoreTake(scaleMutex, portMAX_DELAY);
    if (which == 1) {
        if (scale.wait_ready_timeout(1000)) {
            scale.set_scale(1.0); // remove existing calibration
            Serial.println("Tare... remove any weights from scale 1.");
            delay(500);
            scale.tare();
            Serial.println("Put a known weight on scale 1.");
            delay(2000);
            result = scale.get_units(10);
            Serial.print("Calibrate result (1): "); Serial.println(result);
        } else {
            Serial.println("HX711 (1) not found for calibrate.");
        }
    } else if (which == 2) {
        if (scale2.wait_ready_timeout(1000)) {
            scale2.set_scale(1.0);
            Serial.println("Tare... remove any weights from scale 2.");
            delay(500);
            scale2.tare();
            Serial.println("Put a known weight on scale 2.");
            delay(2000);
            result = scale2.get_units(10);
            Serial.print("Calibrate result (2): "); Serial.println(result);
        } else {
            Serial.println("HX711 (2) not found for calibrate.");
        }
    } else {
        // invalid which - do nothing
    }
    if (scaleMutex) xSemaphoreGive(scaleMutex);
    return result;
}

// Tare one or both scales. which=1 or 2, otherwise both.
void scaleTare(int which) {
    if (scaleMutex) xSemaphoreTake(scaleMutex, portMAX_DELAY);
    if (which == 1) {
        Serial.println("Tare scale 1...");
        if (scale.wait_ready_timeout(500)) scale.tare(); else Serial.println("HX711 not found (scale 1).");
    } else if (which == 2) {
        Serial.println("Tare scale 2...");
        if (scale2.wait_ready_timeout(500)) scale2.tare(); else Serial.println("HX711 not found (scale 2).");
    } else {
        Serial.println("Tare both scales...");
        if (scale.wait_ready_timeout(500)) scale.tare(); else Serial.println("HX711 not found (scale 1).");
        if (scale2.wait_ready_timeout(500)) scale2.tare(); else Serial.println("HX711 not found (scale 2).");
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

// Compatibility: tare both scales
void scaleTareAll() { scaleTare(0); }

// Async calibrate task wrapper
struct CalArgs { int which; uint32_t clientId; };

static void calibrateTask(void *pvParameters) {
    CalArgs *args = (CalArgs*)pvParameters;
    int which = args->which;
    uint32_t clientId = args->clientId;
    // call synchronous calibrate (it will use delays) from this task
    float result = scaleCalibrate(which);
    // report back
    sendCalibrationResult(which, result, clientId);
    free(args);
    vTaskDelete(NULL);
}

// start async calibration (non-blocking)
void scaleCalibrateAsync(int which, uint32_t clientId) {
    CalArgs *args = (CalArgs*)malloc(sizeof(CalArgs));
    if (!args) return;
    args->which = which; args->clientId = clientId;
    xTaskCreatePinnedToCore(calibrateTask, "calib", 4096, args, tskIDLE_PRIORITY+1, NULL, 0);
}