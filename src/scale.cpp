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
int lastTareButtonState = HIGH; // default state of the tare button


void initScale() {
    // Initialization code for the scale
    // HX711 pins and calibration are defined in include/config.h

    // create mutex if not already created
    if (scaleMutex == NULL) {
        scaleMutex = xSemaphoreCreateMutex();
    }

    scale.begin(LOADCELL_DOUT_PIN, LOADCELL_SCK_PIN);

    // Check if tare button is pressed during init to perform Calibration
    int tareBtnState = digitalRead(tareButtonPin);
    if (tareBtnState == LOW) {
        scaleMessage = "Calibration Mode!";
        displayText(scaleMessage, vbat);
        Serial.println(scaleMessage);
        float calib = scaleCalibrate();
        if (!isnan(calib)) {
            scaleMessage = "Calibration done =) \n Resuming normal operation.";
            displayText(scaleMessage, vbat);
            Serial.println(scaleMessage);
            delay(1000);
        } else {
            Serial.println("Calibration failed during init.");
        }
    }

    scale.set_scale(calibrationFactor);
    if (scale.wait_ready_timeout(500)) {
        scaleTare();  // Set the scale to 0 on startup
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
        displayText(scaleMessage, vbat);
        Serial.println(scaleMessage);
        delay(2000);

        scaleMessage = "Remove any weights from scale.";
        displayText(scaleMessage, vbat);
        Serial.println(scaleMessage);
        delay(2000);

        scaleTare();
        
        scaleMessage = "Tare done.\nPlace known weight.";
        displayText(scaleMessage, vbat);
        Serial.println(scaleMessage);
        delay(2000);

        result = scale.get_units(10);
        scaleMessage = "Calibration: " + String(result, 2);
        displayText(scaleMessage, vbat);
        Serial.println(scaleMessage);
        delay(10000);
    } else {
        Serial.println("HX711 not found for calibrate.");
    }
    if (scaleMutex) xSemaphoreGive(scaleMutex);
    return result;
}

// Check tare button state with debouncing and return true if pressed (called from main loop)
bool checkTareButton() {
  static unsigned long lastTarePressTime = 0;  // Track last successful tare press
  const unsigned long DEBOUNCE_DELAY = 500;     // ms debounce delay
  bool tareButtonState = digitalRead(tareButtonPin);
  unsigned long currentTime = millis();
  
  // return true if button is currently pressed (active LOW) and was not pressed in the last check (to detect new presses)
  // AND enough time has passed since the last successful tare press
  if (tareButtonState == LOW && lastTareButtonState == HIGH) {
    if (currentTime - lastTarePressTime >= DEBOUNCE_DELAY) {
      debugln("Tare button press detected!");
      lastTareButtonState = tareButtonState;
      lastTarePressTime = currentTime;  // record this successful press
      return true;
    } else {
      debugln("Tare button ignored (debounce)");
      lastTareButtonState = tareButtonState;
      return false;
    }
  } else {
    lastTareButtonState = tareButtonState; // update last state even if not pressed
    return false;
  }
}

// Tare the single scale on child node
void scaleTare() {
    Serial.println("Starting smart tare...");
    
    scaleMessage = "TARE!";
    displayText(scaleMessage, vbat);
    Serial.println(scaleMessage);
    delay(500);

    // Starting Smart Tare: wait for stable readings before performing tare

    if (scaleMutex) xSemaphoreTake(scaleMutex, portMAX_DELAY);


    // Tighter values for extra stability (outdoors, etc) could be:
    // stabilityThreshold = 0.05;
    // requiredStableReadings = 20;
    // samplesToAverage = 40;

    int scaleReadDelayMs = 12; // delay between readings during stabilization (80Hz)
    int stableCount = 0;
    float lastReading = 0;
    unsigned long startTime = millis();
    const unsigned long maxStabilizeMillis = 15000; // give up after 15s
    const float stabilityThreshold = 0.1; // how much readings can differ to be considered stable
    const int requiredStableReadings = 5; // how many stable readings in a row to confirm stability
    const int samplesToAverage = 5; // how many readings to average for each stability check   


    scaleMessage = "Stabilizing...";
    displayText(scaleMessage, vbat);
    Serial.println(scaleMessage);

    while (stableCount < requiredStableReadings)
    {

        if (!scale.wait_ready_timeout(200)) continue;

        float reading = scale.get_units(samplesToAverage);
        debugln("Tare stabilization reading: " + String(reading, 2));

        if (isnan(reading)) {
            Serial.println("Scale reading NaN during tare stabilization, skipping...");
            delay(scaleReadDelayMs);
            if (millis() - startTime > maxStabilizeMillis) break;
            continue;
        }

        if (abs(reading - lastReading) < stabilityThreshold) {
            stableCount++;
            scaleMessage = "Stabilizing... (" + String(stableCount) + "/" + String(requiredStableReadings) + ")";
            displayText(scaleMessage, vbat);
            debugln(scaleMessage);
        } else {
            stableCount = 0;  // reset if unstable
        }

        lastReading = reading;
        delay(scaleReadDelayMs); // ~80Hz pacing

        if (millis() - startTime > maxStabilizeMillis) {
            Serial.println("Tare stabilization timeout");
            break;
        }
    }
     
    // Now take high-accuracy average for tare offset
    // Compute raw ADC offset (library expects raw offset, not scaled units)
    long sumRaw = 0;
    int validSamples = 0;
    for (int i = 0; i < samplesToAverage; i++) {
        if (scale.wait_ready_timeout(200)) {
            long r = scale.get_value(samplesToAverage); // raw average reading
            sumRaw += r;
            validSamples++;
        }
        delay(scaleReadDelayMs);
        if (millis() - startTime > maxStabilizeMillis) break; // don't hang here either
    }

    if (validSamples > 0) {
        long rawOffset = sumRaw / validSamples;
        // set_offset expects raw ADC offset; add to existing raw offset
        long newOffset = scale.get_offset() + rawOffset;
        scale.set_offset(newOffset);
        debugln("Tare raw offset set to: " + String(rawOffset));
    } else {
        // Fallback: perform a simple tare to set offset if we couldn't get valid samples
        Serial.println("No valid samples for precise tare; performing simple tare as fallback");
        scale.tare();
    }

    if (scaleMutex) xSemaphoreGive(scaleMutex);

    scaleMessage = "TARE Done";
    displayText(scaleMessage, vbat);
    Serial.println(scaleMessage);    
    delay(500);
    if (scaleMutex) xSemaphoreGive(scaleMutex);
}


// Read from the scale (child nodes only)
float scaleRead() {
    float result = NAN;
    if (scaleMutex) xSemaphoreTake(scaleMutex, portMAX_DELAY);
    // Child nodes have one scale
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