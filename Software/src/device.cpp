#include "device.h"

String deviceId;

const MacColor macColorTable[] = {
    {"HiLight_7C2C670B8348", "Alexandra & Gabriel", CRGB::OrangeRed, CRGB::DarkRed},
    {"HiLight_7C2C670B9FF0", "Jutta & Patrick", CRGB::ForestGreen, CRGB::DarkGreen},
    {"HiLight_7C2C670B9208", "Sabrina & Vagarth", CRGB::Turquoise, CRGB::DarkBlue},
    {"HiLight_7C2C670B9390", "Vagarth Test", CRGB::DeepPink, CRGB::DarkRed},
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
