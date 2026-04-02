#include "device.h"

String deviceId;

const MacColor macColorTable[] = {
    {"HiLight_7C:2C:67:0B:83:48", CRGB::OrangeRed, CRGB::DarkRed},     // Alexandra & Gabriel
    {"HiLight_7C:2C:67:0B:9F:F0", CRGB::ForestGreen, CRGB::DarkGreen}, // Jutta & Patrick
    {"HiLight_7C:2C:67:0B:92:08", CRGB::Turquoise, CRGB::DarkBlue},    // Sabrina & Vagarth
    {"HiLight_7C:2C:67:0B:93:90", CRGB::DeepPink, CRGB::DarkRed},      // Edith & Robert
};
const int macColorTableSize = sizeof(macColorTable) / sizeof(macColorTable[0]);

CRGB colorForId(const String &id)
{
  for (int i = 0; i < macColorTableSize; i++)
  {
    if (id == macColorTable[i].id)
      return macColorTable[i].color;
  }
  return CRGB::LightBlue; // default
}

CRGB spinColorForId(const String &id)
{
  for (int i = 0; i < macColorTableSize; i++)
  {
    if (id == macColorTable[i].id)
      return macColorTable[i].spinColor;
  }
  return CRGB::Blue; // default
}
