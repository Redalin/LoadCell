#ifndef SETTINGS_H
#define SETTINGS_H

#include <Arduino.h>

void settingsInit();
String settingsGetName(int which);
String settingsGetColor(int which);
void settingsSetName(int which, const String &name);
void settingsSetColor(int which, const String &color);
bool settingsSave();
String settingsAsJson();
void settingsResetDefaults();

#endif
