#include <Arduino.h>
#include <FastLED.h>

#include "config.h"
#include "device.h"
#include "homekit.h"
#include "leds.h"
#include "network.h"

static bool buttonPressed = false;
static unsigned long pressStart = 0;
static int animLedCount = 0;
static bool apModeTriggered = false;
static bool awaitingClick = false;
static unsigned long firstClickTime = 0;
static bool spinnerPreempted = false;
static LedMode preSpinnerLedMode = LED_IDLE;

void setup()
{
#if DEBUG
  Serial.begin(115200);
  while (!Serial && millis() < 3000)
    delay(10);
#endif

  FastLED.addLeds<NEOPIXEL, RGB_DATA_PIN>(leds, NUM_LEDS); // GRB ordering is assumed

  initNetwork();
  initHomeKit();

  ledcAttach(WHITE_LED_PIN, WHITE_LED_FREQ, WHITE_LED_RES_BITS);
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
    // loop() returns before the button-release code below ever runs while AP mode is
    // active, so clear press state every frame here to avoid a stale long-press firing
    // once AP mode exits.
    buttonPressed = false;
    animLedCount = -1;
    apModeTriggered = false;
    awaitingClick = false;
    return;
  }

  int clkState = digitalRead(CLK_PIN);
  if (ledMode == LED_CCT && lastClkState == HIGH && clkState == LOW)
  {
    int delta = (digitalRead(DT_PIN) == HIGH ? -1 : 1);

    if (encoderTarget == ENC_CCT)
    {
      int newCctPos = constrain(cctPos - delta, 0, ENCODER_MAX_POS);
      if (newCctPos != cctPos)
      {
        cctPos = newCctPos;
        applyCCTLight();
        publishCCTState();
      }
    }
    else
    {
      int newBrightnessPos = constrain(brightnessPos + delta, 0, ENCODER_MAX_POS);
      if (newBrightnessPos != brightnessPos)
      {
        brightnessPos = newBrightnessPos;
        whiteBrightness = brightnessLUT[brightnessPos];
        applyCCTLight();
        publishBrightnessState();
      }
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
      spinnerPreempted = false;
    }

    // If something else (e.g. an MQTT power command) changed ledMode out from under
    // us after the spinner claimed it, stop touching leds[] for the rest of this press
    // so we don't fight over the strip or clobber whatever that writer set on release.
    if (animLedCount != -1 && ledMode != LED_RGB_ANIM)
      spinnerPreempted = true;

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

    if (!spinnerPreempted && targetSpinPos != animLedCount)
    {
      CRGB bgColor = colorForId(deviceId);

      if (animLedCount == -1)
      {
        // First frame: set background and brightness
        preSpinnerLedMode = ledMode;
        ledMode = LED_RGB_ANIM;
        stopWarmLed();
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

      if (spinnerPreempted)
      {
        // Whatever preempted the spinner (e.g. MQTT) owns ledMode/leds[] now; don't
        // treat this release as a click and don't touch the strip.
      }
      else if (pressDuration >= LONG_PRESS_TIME)
      {
        if (isMqttConnected())
        {
          publishHi();
        }
        else
        {
          startErrorAnim();
        }
        awaitingClick = false;
      }
      else if (ledMode == LED_RGB_ANIM)
      {
        // Brief touch released before the long-press threshold: cancel the spinner
        // preview and restore whatever was showing before the press instead of
        // forcing the light off.
        hiAnimActive = false;
        FastLED.setBrightness(255);
        for (int i = 0; i < NUM_LEDS; i++)
          leds[i] = CRGB::Black;
        FastLED.show();
        ledMode = preSpinnerLedMode;
        if (ledMode == LED_CCT)
          applyCCTLight();
        awaitingClick = false;
      }
      else if (awaitingClick && (millis() - firstClickTime) <= DOUBLE_CLICK_WINDOW)
      {
        awaitingClick = false;
        if (ledMode == LED_CCT)
          encoderTarget = (encoderTarget == ENC_CCT) ? ENC_BRIGHTNESS : ENC_CCT;
      }
      else
      {
        awaitingClick = true;
        firstClickTime = millis();
      }

      buttonPressed = false;
      animLedCount = -1;
      apModeTriggered = false;
    }
  }

  if (awaitingClick && (millis() - firstClickTime) > DOUBLE_CLICK_WINDOW)
  {
    awaitingClick = false;
    ledMode = (ledMode == LED_CCT) ? LED_IDLE : LED_CCT;
    if (ledMode == LED_CCT)
      encoderTarget = ENC_BRIGHTNESS;
    cctChanged = true;
    publishPowerState();
  }

  if (cctChanged)
  {
    applyCCTLight();
    cctChanged = false;
  }

  updateErrorAnim();
  updateHiAnim();
}
