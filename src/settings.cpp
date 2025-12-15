#include "settings.h"
#include "littlefs-conf.h"
#include <ArduinoJson.h>

static String name1 = "Scale 1";
static String name2 = "Scale 2";
static String color1 = "#0077cc";
static String color2 = "#cc5500";
static const char *SETTINGS_PATH = "/settings.json";

void settingsInit() {
  // ensure LittleFS mounted (littlefs.cpp should call initLittleFS earlier)
  if (!LittleFS.exists(SETTINGS_PATH)) return;
  File f = LittleFS.open(SETTINGS_PATH, "r");
  if (!f) return;
  StaticJsonDocument<256> doc;
  DeserializationError err = deserializeJson(doc, f);
  f.close();
  if (err) return;
  if (doc.containsKey("name1")) name1 = String((const char*)doc["name1"]);
  if (doc.containsKey("name2")) name2 = String((const char*)doc["name2"]);
  if (doc.containsKey("color1")) color1 = String((const char*)doc["color1"]);
  if (doc.containsKey("color2")) color2 = String((const char*)doc["color2"]);
}

String settingsGetName(int which) {
  return (which == 2) ? name2 : name1;
}

String settingsGetColor(int which) {
  return (which == 2) ? color2 : color1;
}

void settingsSetName(int which, const String &name) {
  if (which == 2) name2 = name; else name1 = name;
}

void settingsSetColor(int which, const String &c) {
  if (which == 2) color2 = c; else color1 = c;
}

bool settingsSave() {
  StaticJsonDocument<256> doc;
  doc["name1"] = name1;
  doc["name2"] = name2;
  doc["color1"] = color1;
  doc["color2"] = color2;
  File f = LittleFS.open(SETTINGS_PATH, "w");
  if (!f) return false;
  size_t n = serializeJson(doc, f);
  f.close();
  return n > 0;
}

String settingsAsJson() {
  StaticJsonDocument<256> doc;
  doc["name1"] = name1;
  doc["name2"] = name2;
  doc["color1"] = color1;
  doc["color2"] = color2;
  String out;
  serializeJson(doc, out);
  return out;
}

void settingsResetDefaults() {
  name1 = "Scale 1";
  name2 = "Scale 2";
  color1 = "#0077cc";
  color2 = "#cc5500";
  settingsSave();
}
