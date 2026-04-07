#include <Arduino.h>
#include <FastLED.h>

#include "config.h"
#include "device.h"
#include "leds.h"
#include "network.h"

static bool buttonPressed = false;
static unsigned long pressStart = 0;
static int animLedCount = 0;
static bool apModeTriggered = false;

void setup()
{
  Serial.begin(115200);

  FastLED.addLeds<NEOPIXEL, RGB_DATA_PIN>(leds, NUM_LEDS); // GRB ordering is assumed

  initNetwork();

  pinMode(WHITE_LED_PIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(CLK_PIN, INPUT_PULLUP);
  pinMode(DT_PIN, INPUT_PULLUP);
  lastClkState = digitalRead(CLK_PIN);
}

void loop()
{
  updateNetwork();

  if (isAPMode())
  {
    updateAPAnim();
    return;
  }

  // Rotary encoder: adjust white LED brightness (logarithmic)
  int clkState = digitalRead(CLK_PIN);
  if (lastClkState == HIGH && clkState == LOW)
  {
    int newPos = constrain(encoderPos + (digitalRead(DT_PIN) == HIGH ? -1 : 1), 0, ENCODER_MAX_POS);
    if (newPos != encoderPos)
    {
      encoderPos = newPos;
      whiteBrightness = brightnessLUT[encoderPos];
      if (whiteLedState)
        analogWrite(WHITE_LED_PIN, whiteBrightness);
      publishBrightnessState();
    }
  }
  lastClkState = clkState;

  if (digitalRead(BUTTON_PIN) == LOW)
  {
    if (!buttonPressed)
    {
      buttonPressed = true;
      pressStart = millis();
      animLedCount = -1;
    }

    // Animate spinner during long press (after initial delay): background color + darker spin
    unsigned long elapsed = millis() - pressStart;
    int targetSpinPos = elapsed < ANIM_START_DELAY
                            ? -1
                            : min((int)(((elapsed - ANIM_START_DELAY) * (NUM_LEDS - 4)) /
                                        (LONG_PRESS_TIME - ANIM_START_DELAY)),
                                  NUM_LEDS - 4);

    if (!apModeTriggered && elapsed >= AP_PRESS_TIME)
    {
      Serial.println("5s hold — entering AP mode");
      startAPMode();
      startAPAnim();
      apModeTriggered = true;
    }

    if (targetSpinPos != animLedCount)
    {
      CRGB bgColor = colorForId(deviceId);

      if (animLedCount == -1)
      {
        // First frame: set background and brightness
        whiteLedState = false;
        applyWhiteLight();
        for (int i = 0; i < NUM_LEDS; i++)
          leds[i] = bgColor;
        FastLED.setBrightness(200);
        rgbLedState = true;
      }

      if (targetSpinPos >= 0)
      {
        CRGB spColor = spinColorForId(deviceId);

        // Restore previous spinner position to background
        if (animLedCount >= 0)
          for (int s = 0; s < 4; s++)
            leds[animLedCount + s] = bgColor;
        // Draw spinner at new position
        for (int s = 0; s < 4; s++)
          leds[targetSpinPos + s] = spColor;
        FastLED.show();
      }

      animLedCount = targetSpinPos;
    }
  }
  else
  {
    if (buttonPressed)
    {
      unsigned long pressDuration = millis() - pressStart;

      if (pressDuration >= LONG_PRESS_TIME)
      {
        Serial.println("Long press");

        if (isMqttConnected())
        {
          Serial.println("Saying Hi!");
          publishHi();
          whiteLedState = false;
          CRGB bgColor = colorForId(deviceId);
          for (int i = 0; i < NUM_LEDS; i++)
            leds[i] = bgColor;
          FastLED.setBrightness(200);
          FastLED.show();
        }
        else
        {
          Serial.println("No connection — showing error animation");
          startErrorAnim();
        }
      }
      else
      {
        Serial.println("Short press");
        // Clear animation LEDs
        hiAnimActive = false;
        FastLED.setBrightness(255);
        for (int i = 0; i < NUM_LEDS; i++)
          leds[i] = CRGB::Black;
        FastLED.show();
        if (!rgbLedState)
        {
          whiteLedState = !whiteLedState;
          whiteLedChanged = true;
          publishPowerState();
        }
        rgbLedState = false;
      }

      buttonPressed = false;
      animLedCount = -1;
      apModeTriggered = false;
    }
  }

  if (whiteLedChanged)
  {
    applyWhiteLight();
    whiteLedChanged = false;
  }

  updateErrorAnim();
  updateHiAnim();
}
