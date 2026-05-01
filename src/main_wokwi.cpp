// Basic Wokwi-friendly preview of the LoadCell project.
// Mirrors parent-mode UX (OLED + tare button + 4 lane buttons + battery icon)
// but skips WiFi / ESP-NOW / HX711 / LittleFS so it runs in a simulator.
//
// Wiring (see diagram.json):
//   OLED SSD1306 128x32 on I2C  -> SDA=21, SCL=22, addr 0x3C
//   Tare button (active low)    -> GPIO14
//   Lane 1..4 buttons           -> GPIO16, 17, 18, 19
//   VBAT divider potentiometer  -> GPIO36 (VP)
//   Simulated weight pot        -> GPIO39 (VN)

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH  128
#define SCREEN_HEIGHT 32
#define OLED_RESET    -1

#define TARE_BTN_PIN     14
#define VBAT_PIN         36
#define WEIGHT_POT_PIN   39

#define VBAT_DIVIDER_R1  20000.0f
#define VBAT_DIVIDER_R2  4700.0f
#define VBAT_DIVIDER     ((VBAT_DIVIDER_R1 + VBAT_DIVIDER_R2) / VBAT_DIVIDER_R2)

static const uint8_t lanePins[4] = { 16, 17, 18, 19 };

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

static int laneCountdown[4] = { 0, 0, 0, 0 };
static float vbat = 0.0f;
static float weight = 0.0f;

static unsigned long lastLaneCheck   = 0;
static unsigned long lastCountdownAt = 0;
static unsigned long lastWeightAt    = 0;
static unsigned long lastBatteryAt   = 0;
static unsigned long lastDrawAt      = 0;
static unsigned long tempUntil       = 0;
static String        tempMessage;

static float readVBAT() {
  int raw = analogRead(VBAT_PIN);
  return (raw / 4095.0f) * 3.3f * VBAT_DIVIDER;
}

static float readWeight() {
  int raw = analogRead(WEIGHT_POT_PIN);
  return (raw / 4095.0f) * 2000.0f; // 0..2000 g
}

static void drawBatteryIcon(float voltage) {
  const int x = SCREEN_WIDTH - 16;
  const int y = 0;
  display.drawRect(x, y, 14, 7, SSD1306_WHITE);
  display.fillRect(x + 14, y + 2, 2, 3, SSD1306_WHITE);
  float pct = (voltage - 3.0f) / (4.2f - 3.0f);
  if (pct < 0) pct = 0;
  if (pct > 1) pct = 1;
  int fill = (int)(pct * 12);
  display.fillRect(x + 1, y + 1, fill, 5, SSD1306_WHITE);
}

static void drawDefault() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("LaunchScale");
  display.printf("Wgt: %6.1f g\n", weight);
  display.printf("Bat: %5.2f V\n", vbat);
  drawBatteryIcon(vbat);
  display.display();
}

static void drawTemporary(const String &msg) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println(msg);
  drawBatteryIcon(vbat);
  display.display();
}

static void showTemp(const String &msg, unsigned long ms = 1500) {
  tempMessage = msg;
  tempUntil = millis() + ms;
  drawTemporary(msg);
}

void setup() {
  Serial.begin(115200);
  delay(100);
  Serial.println("Wokwi LoadCell preview booting");

  pinMode(TARE_BTN_PIN, INPUT_PULLUP);
  for (uint8_t i = 0; i < 4; i++) pinMode(lanePins[i], INPUT_PULLUP);

  analogReadResolution(12);
  analogSetPinAttenuation(VBAT_PIN, ADC_0db);
  analogSetPinAttenuation(WEIGHT_POT_PIN, ADC_11db);

  Wire.begin(); // ESP32 default SDA=21, SCL=22
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("SSD1306 allocation failed"));
    for (;;) delay(1000);
  }
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("Booting...");
  display.display();
  delay(600);

  vbat = readVBAT();
  weight = readWeight();
  drawDefault();
}

void loop() {
  const unsigned long now = millis();

  // Tare button (active low). Simulate the parent-broadcast UX.
  static int lastTare = HIGH;
  int tare = digitalRead(TARE_BTN_PIN);
  if (tare == LOW && lastTare == HIGH) {
    Serial.println("Tare pressed");
    weight = 0;
    showTemp("Taring all nodes...");
  }
  lastTare = tare;

  // Lane switches: debounce at 50ms, start 30s countdown on press.
  if (now - lastLaneCheck > 50) {
    lastLaneCheck = now;
    for (uint8_t i = 0; i < 4; i++) {
      static int lastLane[4] = { HIGH, HIGH, HIGH, HIGH };
      int s = digitalRead(lanePins[i]);
      if (s == LOW && lastLane[i] == HIGH) {
        laneCountdown[i] = 30;
        Serial.printf("Lane %u pilot swap, countdown=%d\n", i + 1, laneCountdown[i]);
        showTemp(String("Lane ") + (i + 1) + ": Pilot Swap");
      }
      lastLane[i] = s;
    }
  }

  // Decrement countdowns each second.
  if (now - lastCountdownAt >= 1000) {
    lastCountdownAt = now;
    for (uint8_t i = 0; i < 4; i++) {
      if (laneCountdown[i] > 0) laneCountdown[i]--;
    }
  }

  // Refresh weight 5x/s.
  if (now - lastWeightAt > 200) {
    lastWeightAt = now;
    weight = readWeight();
  }

  // Battery every 2s.
  if (now - lastBatteryAt > 2000) {
    lastBatteryAt = now;
    vbat = readVBAT();
    Serial.printf("vbat=%.2fV  weight=%.1fg\n", vbat, weight);
  }

  // Redraw 5x/s, but leave temporary messages on screen until they expire.
  if (now - lastDrawAt > 200) {
    lastDrawAt = now;
    if (tempUntil && (long)(now - tempUntil) < 0) {
      drawTemporary(tempMessage);
    } else {
      tempUntil = 0;
      drawDefault();
    }
  }
}
