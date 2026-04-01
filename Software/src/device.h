#pragma once
#include <Arduino.h>
#include <FastLED.h>

struct MacColor
{
  const char *id; // "HiLight_XX:XX:XX:XX:XX:XX"
  CRGB color;
  CRGB spinColor;
};

extern const MacColor macColorTable[];
extern const int macColorTableSize;
extern String deviceId;

CRGB colorForId(const String &id);
CRGB spinColorForId(const String &id);
