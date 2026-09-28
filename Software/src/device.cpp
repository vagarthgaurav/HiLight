#include "device.h"

String deviceId;

const MacColor macColorTable[] = {
    {"HiLight_7C2C670B8348", "Alexandra & Gabriel", CRGB::OrangeRed, CRGB::DarkRed, false},
    {"HiLight_7C2C670B9FF0", "Jutta & Patrick", CRGB::ForestGreen, CRGB::DarkGreen, false},
    {"HiLight_7C2C670B9208", "Sabrina & Vagarth", CRGB::Turquoise, CRGB::DarkBlue, true},
    {"HiLight_7C2C670B9390", "Vagarth Test", CRGB::DeepPink, CRGB::DarkRed, true},
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

const char *nameForId(const String &id)
{
  for (int i = 0; i < macColorTableSize; i++)
  {
    if (id == macColorTable[i].id)
      return macColorTable[i].name;
  }
  return "HiLight"; // default
}

bool haDiscoveryForId(const String &id)
{
  for (int i = 0; i < macColorTableSize; i++)
  {
    if (id == macColorTable[i].id)
      return macColorTable[i].haDiscovery;
  }
  return false; // unknown lamps are not exposed to Home Assistant
}
