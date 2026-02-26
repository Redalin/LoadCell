// nodeConfig.h - configuration and preferences for each node
#ifndef NODECONFIG_H
#define NODECONFIG_H

#include "config.h"
#include <Preferences.h>

// Node configuration is loaded from preferences at runtime.
// On first boot, defaults are: parent (ID 0), "LaunchScale" hostname, etc.
// To reassign a node ID, call saveNodeId(newId) or POST to /nodeid?id=X

void configMode(); // call from setup() before any ESP‑NOW initialisation
void loadNodeConfig();
void saveNodeId(uint8_t id);
void printNodeConfig();


#endif  // NODECONFIG_H