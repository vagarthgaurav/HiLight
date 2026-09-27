#pragma once
#include <Arduino.h>

void initNetwork();
void updateNetwork();
bool isMqttConnected();
void publishHi();
void publishPowerState();
void publishBrightnessState();
void publishCCTState();
void startAPMode();
bool isAPMode();

// Shared command handlers - apply an incoming power/brightness/color-temp
// command regardless of which integration (MQTT, HomeKit) it came from, and
// publish the resulting state back out to all of them.
void applyPowerCommand(bool on);
void applyBrightnessCommand(int haValue); // 1-255 scale
void applyColorTempCommand(int mireds);
