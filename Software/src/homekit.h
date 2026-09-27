#pragma once
#include <Arduino.h>

void initHomeKit();
void updateHomeKit();

// Push a state change that originated elsewhere (MQTT, button, encoder) into
// the HomeKit characteristics so the Home app stays in sync.
void homeKitSyncPower(bool on);
void homeKitSyncBrightness(int haValue); // 1-255 scale, same as MQTT brightness
void homeKitSyncColorTemp(int mireds);
