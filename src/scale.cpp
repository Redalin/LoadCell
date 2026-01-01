#include "scale.h"
#include "config.h"
// Mutex to protect concurrent access to the HX711 from different tasks/callbacks
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "webpage.h"
#include "display-oled.h"

static SemaphoreHandle_t scaleMutex = NULL;
HX711 scale;
String scaleMessage = "";

void initScale() {
    // Initialization code for the scale
    // HX711 pins and calibration are defined in include/config.h

    // create mutex if not already created
    if (scaleMutex == NULL) {
        scaleMutex = xSemaphoreCreateMutex();
    }

    if (scaleMutex) xSemaphoreTake(scaleMutex, portMAX_DELAY);
    // mutex for performance in case called from multiple tasks

    scale.begin(LOADCELL_DOUT_PIN, LOADCELL_SCK_PIN);

    // Check if tare button is pressed during init to perform Calibration
    int tareBtnState = digitalRead(TARE_BUTTON_PIN);
    if (tareBtnState == LOW) {
        scaleMessage = "Calibration Mode!";
        displayText(scaleMessage);
        Serial.println(scaleMessage);

        float calib = scaleCalibrate();
        if (!isnan(calib)) {
            scaleMessage = "Calibration complete";
            displayText(scaleMessage);
            Serial.println(scaleMessage);
            delay(2000);
        } else {
            Serial.println("Calibration failed during init.");
        }
    }

    scale.set_scale(CALIBRATION_FACTOR);
    if (scale.wait_ready_timeout(500)) {
        scale.tare();  // Reset the scale to 0 on initialization
    } else {
        Serial.println("HX711 not found during init (will retry later)");
    }
    if (scaleMutex) xSemaphoreGive(scaleMutex);
    Serial.println("Scale initialized.");
}

// Calibrate scale
float scaleCalibrate() {
    float result = NAN;
    if (scaleMutex) xSemaphoreTake(scaleMutex, portMAX_DELAY);
    if (scale.wait_ready_timeout(1000)) {
        scale.set_scale(1.0); // remove existing calibration
        scaleMessage = "Calibrating...";
        displayText(scaleMessage);
        Serial.println(scaleMessage);
        delay(500);

        scaleMessage = "Remove any weights from scale.";
        displayText(scaleMessage);
        Serial.println(scaleMessage);
        delay(500);

        scaleMessage = "3";
        displayText(scaleMessage);
        Serial.println(scaleMessage);
        delay(500);

        scaleMessage = "2"; 
        displayText(scaleMessage);
        Serial.println(scaleMessage);
        delay(500);    

        scaleMessage = "1";
        displayText(scaleMessage);
        Serial.println(scaleMessage);
        delay(500);

        scale.tare();
        
        scaleMessage = "Tare done.\nPlace known weight.";
        displayText(scaleMessage);
        Serial.println(scaleMessage);
        delay(2000);

        result = scale.get_units(10);
        scaleMessage = "Calibration: " + String(result, 2);
        displayText(scaleMessage);
        Serial.print(scaleMessage);
    } else {
        Serial.println("HX711 not found for calibrate.");
    }
    if (scaleMutex) xSemaphoreGive(scaleMutex);
    return result;
}

// Tare the single scale on child node
void scaleTare() {
    if (scaleMutex) xSemaphoreTake(scaleMutex, portMAX_DELAY);
    Serial.println("Tare scale...");
    if (scale.wait_ready_timeout(500)) scale.tare(); else Serial.println("HX711 not found.");
    Serial.println("Tare done...");
    if (scaleMutex) xSemaphoreGive(scaleMutex);
}


// Read from the single scale (child nodes only)
float scaleRead() {
    float result = NAN;
    if (scaleMutex) xSemaphoreTake(scaleMutex, portMAX_DELAY);
    // Child nodes only have one scale, so ignore 'which' parameter
    if (scale.wait_ready_timeout(200)) {
        result = scale.get_units(5);
    }
    if (scaleMutex) xSemaphoreGive(scaleMutex);
    return result;
}

// Dummy units for testing without scale
float scaleDummyRead() {
    static float dummyWeight = 0.0;
    dummyWeight += 10.0;
    if (dummyWeight > 1000.0) dummyWeight = 0.0;
    return dummyWeight;
}

// Compatibility helper: tare both scales
void scaleTareAll() {
    scaleTare();
}

// Async calibrate task wrapper
struct CalArgs { uint32_t clientId; };

static void calibrateTask(void *pvParameters) {
    CalArgs *args = (CalArgs*)pvParameters;
    uint32_t clientId = args->clientId;
    // call synchronous calibrate (it will use delays) from this task
    float result = scaleCalibrate();
    // report back
    sendCalibrationResult(result, clientId);
    free(args);
    vTaskDelete(NULL);
}

// start async calibration (non-blocking)
void scaleCalibrateAsync(uint32_t clientId) {
    CalArgs *args = (CalArgs*)malloc(sizeof(CalArgs));
    if (!args) return;
    args->clientId = clientId;
    xTaskCreatePinnedToCore(calibrateTask, "calib", 4096, args, tskIDLE_PRIORITY+1, NULL, 0);
}