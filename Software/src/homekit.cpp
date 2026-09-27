#include "homekit.h"
#include "config.h"
#include "device.h"
#include "leds.h"
#include "network.h"
#include <HomeSpan.h>

// HomeSpan is left in "passive" mode: we never call setWifiCredentials(), so
// it never drives WiFi itself (see Span::pollTask - the auto-reconnect/AP
// logic there is gated on stored WiFi credentials being present). It just
// listens for the WiFi connect/disconnect events that network.cpp's own
// WiFi.begin() calls generate, and starts its local mDNS/HAP server once an
// IP is acquired.

struct HiLightBulb : Service::LightBulb
{
  SpanCharacteristic *power;
  SpanCharacteristic *brightness;
  SpanCharacteristic *colorTemp;

  HiLightBulb() : Service::LightBulb()
  {
    power = new Characteristic::On(ledMode == LED_CCT);

    int haBrightness = map(brightnessLUT[constrain(brightnessPos, 0, ENCODER_MAX_POS)], brightnessLUT[0],
                            brightnessLUT[ENCODER_MAX_POS], 1, 255);
    brightness = new Characteristic::Brightness(map(haBrightness, 1, 255, 1, 100));
    brightness->setRange(1, 100, 1);

    int mireds = map(cctPos, 0, ENCODER_MAX_POS, MAX_MIREDS, MIN_MIREDS);
    colorTemp = new Characteristic::ColorTemperature(mireds);
    colorTemp->setRange(MIN_MIREDS, MAX_MIREDS, 1);
  }

  boolean update() override
  {
    if (power->updated())
      applyPowerCommand(power->getNewVal<bool>());

    if (brightness->updated())
      applyBrightnessCommand(map(brightness->getNewVal(), 1, 100, 1, 255));

    if (colorTemp->updated())
      applyColorTempCommand(colorTemp->getNewVal());

    return true;
  }
};

static HiLightBulb *hiLightBulb = nullptr;

void initHomeKit()
{
#if DEBUG
  homeSpan.setLogLevel(1);
#else
  homeSpan.setLogLevel(-1);
  homeSpan.setSerialInputDisable(true);
#endif

  String displayName = "HiLight (" + String(nameForId(deviceId)) + ")";
  homeSpan.begin(Category::Lighting, displayName.c_str());

  new SpanAccessory();
  new Service::AccessoryInformation();
  new Characteristic::Identify();
  hiLightBulb = new HiLightBulb();
}

void updateHomeKit()
{
  homeSpan.poll();
}

void homeKitSyncPower(bool on)
{
  if (hiLightBulb)
    hiLightBulb->power->setVal(on);
}

void homeKitSyncBrightness(int haValue)
{
  if (hiLightBulb)
    hiLightBulb->brightness->setVal(map(constrain(haValue, 1, 255), 1, 255, 1, 100));
}

void homeKitSyncColorTemp(int mireds)
{
  if (hiLightBulb)
    hiLightBulb->colorTemp->setVal(constrain(mireds, MIN_MIREDS, MAX_MIREDS));
}
