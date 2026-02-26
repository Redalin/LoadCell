#include "nodeconfig.h"
#include "display-oled.h"

// Define the global variables (declared extern in config.h)
uint8_t deviceId = 0;           // parent by default
bool espnowIsParent = true;
const char* hostName = "LaunchScale";
float calibrationFactor = 0;
uint8_t tareButtonPin = 14;     // parent tare pin

// tables for runtime configuration based on numeric id
static const char* nameTable[] = { "LaunchScale", "Yellow", "Grey", "Purple", "Black" };
static const float calTable[] = { 0, 2128.66, -2063.8, 2000, 1054.42 };

// Node configuration is loaded from preferences at runtime.
// On first boot, defaults are: parent (ID 0), "LaunchScale" hostname, etc.
// To reassign a node ID, call saveNodeId(newId) or POST to /nodeid?id=X

// call from setup() before any ESP‑NOW initialisation
void configMode() {
    pinMode(tareButtonPin, INPUT_PULLUP);       // button to ground
    unsigned long start = millis();
    if (digitalRead(tareButtonPin) == LOW) {    // held on power‑up?
        Serial.println("entering nodeID config mode");
        uint8_t id = 1;
        // flash display/LED with current id...
        while (millis() - start < 10000) {       // 10 s to choose
            if (digitalRead(tareButtonPin) == LOW) {
                // bump id every press (debounce as needed)
                delay(200);
                id = (id % 4) + 1;
                String oledmessage = "Config: ID " + String(id);
                displayText(oledmessage, vbat);
                Serial.println(oledmessage);
                // update oled/display if you have one
            }
        }
        saveNodeId(id);                          // store it permanently
        Serial.print("node id saved "); Serial.println(id);
        delay(500);
        esp_restart();                          // reboot with new id
    } else {
        String oledmessage = "Config: ID " + String(deviceId);
        displayText(oledmessage, vbat);
        Serial.println(oledmessage);
        delay(500);
    }
}

void loadNodeConfig() {
    Preferences prefs;
    prefs.begin("nodecfg", true);
    uint8_t stored = prefs.getUChar("id", deviceId);
    prefs.end();

    // apply the stored id (or default if none)
    if (stored <= 4) {
        deviceId = stored;
    } else {
        Serial.print("Invalid stored device ID: ");
        Serial.println(stored);
        Serial.println("Using default ID 0 (parent)");
        deviceId = 0;
    }

    espnowIsParent = (deviceId == 0);
    hostName = nameTable[deviceId];
    calibrationFactor = calTable[deviceId];
    tareButtonPin = espnowIsParent ? 14 : 15;

    printNodeConfig();
}

void saveNodeId(uint8_t id) {
    if (id > 4) return; // ignore invalid values
    Preferences prefs;
    prefs.begin("nodecfg", false);
    prefs.putUChar("id", id);
    prefs.end();
    // reload configuration so globals reflect new ID immediately
    loadNodeConfig();
}

void printNodeConfig() {
    Serial.println("Current Node Configuration:");
    Serial.print("Device ID: "); Serial.println(deviceId);
    Serial.print("Is Parent: "); Serial.println(espnowIsParent ? "Yes" : "No");
    Serial.print("Host Name: "); Serial.println(hostName);
    Serial.print("Calibration Factor: "); Serial.println(calibrationFactor);
    Serial.print("Tare Button Pin: "); Serial.println(tareButtonPin);
}
