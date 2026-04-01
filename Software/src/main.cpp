#include <Arduino.h>
#include <FastLED.h>

#include "config.h"
#include "device.h"
#include "leds.h"
#include "network.h"

static bool buttonPressed = false;
static unsigned long pressStart = 0;
static int animLedCount = 0;

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
    }
  }
  lastClkState = clkState;

  if (digitalRead(BUTTON_PIN) == LOW)
  {
    if (!buttonPressed)
    {
      buttonPressed = true;
      pressStart = millis();
      animLedCount = 0;
    }

    // Animate LEDs filling up sequentially during long press (after initial delay)
    unsigned long elapsed = millis() - pressStart;
    int targetLeds =
        elapsed < ANIM_START_DELAY
            ? 0
            : (int)(((elapsed - ANIM_START_DELAY) * NUM_LEDS) / (LONG_PRESS_TIME - ANIM_START_DELAY));
    if (targetLeds > NUM_LEDS)
      targetLeds = NUM_LEDS;

    if (targetLeds != animLedCount)
    {
      if (animLedCount == 0)
      {
        whiteLedState = false;
        applyWhiteLight();
      }
      CRGB color = colorForId(deviceId);
      for (int i = animLedCount; i < targetLeds; i++)
        leds[i] = color;
      FastLED.show();
      rgbLedState = true;
      animLedCount = targetLeds;
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
          whiteLedChanged = true;
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
        if (rgbLedState)
        {
          rgbLedState = false;
        }
        else
        {
          whiteLedState = !whiteLedState;
          whiteLedChanged = true;
        }
      }

      buttonPressed = false;
      animLedCount = 0;
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
