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
#if DEBUG
  Serial.begin(115200);
  while (!Serial && millis() < 3000)
    delay(10);
#endif

  FastLED.addLeds<NEOPIXEL, RGB_DATA_PIN>(leds, NUM_LEDS); // GRB ordering is assumed

  initNetwork();

  ledcSetup(WHITE_LED_CHANNEL, WHITE_LED_FREQ, 8);
  ledcAttachPin(WHITE_LED_PIN, WHITE_LED_CHANNEL);
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
      if (ledMode == LED_WHITE)
        ledcWrite(WHITE_LED_CHANNEL, whiteBrightness);
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
                            : min((int)(((elapsed - ANIM_START_DELAY) * (NUM_LEDS - SPINNER_WIDTH)) /
                                        (LONG_PRESS_TIME - ANIM_START_DELAY)),
                                  NUM_LEDS - SPINNER_WIDTH);

    if (!apModeTriggered && elapsed >= AP_PRESS_TIME)
    {
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
        ledMode = LED_RGB_ANIM;
        applyWhiteLight();
        for (int i = 0; i < NUM_LEDS; i++)
          leds[i] = bgColor;
        FastLED.setBrightness(SPINNER_BRIGHTNESS);
      }

      if (targetSpinPos >= 0)
      {
        CRGB spColor = spinColorForId(deviceId);

        // Restore previous spinner position to background
        if (animLedCount >= 0)
          for (int s = 0; s < SPINNER_WIDTH; s++)
            leds[animLedCount + s] = bgColor;
        // Draw spinner at new position
        for (int s = 0; s < SPINNER_WIDTH; s++)
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
        if (isMqttConnected())
        {
          publishHi();
        }
        else
        {
          startErrorAnim();
        }
      }
      else
      {
        // Clear animation LEDs
        hiAnimActive = false;
        FastLED.setBrightness(255);
        for (int i = 0; i < NUM_LEDS; i++)
          leds[i] = CRGB::Black;
        FastLED.show();
        if (ledMode == LED_RGB_ANIM)
        {
          ledMode = LED_IDLE;
        }
        else
        {
          ledMode = (ledMode == LED_WHITE) ? LED_IDLE : LED_WHITE;
          whiteLedChanged = true;
          publishPowerState();
        }
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
