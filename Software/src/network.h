#pragma once
#include <Arduino.h>

void initNetwork();
void updateNetwork();
bool isMqttConnected();
void publishHi();
