#include "leds.h"

CRGB leds[NUM_LEDS];

bool whiteLedState = false;
bool whiteLedChanged = false;
bool rgbLedState = false;

// Logarithmically spaced brightness values (80..250 in 17 steps)
const uint8_t brightnessLUT[] = {80,  86,  93,  100, 107, 115, 124, 133, 143,
                                  154, 165, 178, 191, 205, 221, 237, 250};
int encoderPos = 0;
int whiteBrightness = brightnessLUT[0];
int lastClkState = HIGH;

bool hiAnimActive = false;
unsigned long hiAnimStart = 0;
bool errorAnimActive = false;
unsigned long errorAnimStart = 0;

void applyWhiteLight()
{
  if (whiteLedState)
  {
    // White LED on: turn off RGB strip
    for (int i = 0; i < NUM_LEDS; i++)
      leds[i] = CRGB::Black;
    FastLED.show();
    analogWrite(WHITE_LED_PIN, whiteBrightness);
  }
  else
  {
    // White LED off
    analogWrite(WHITE_LED_PIN, 0);
  }
  Serial.print("White LED state: ");
  Serial.println(whiteLedState);
}

void startErrorAnim()
{
  whiteLedState = false;
  applyWhiteLight();
  for (int i = 0; i < NUM_LEDS; i++)
    leds[i] = CRGB::Red;
  FastLED.setBrightness(0);
  FastLED.show();
  hiAnimActive = false;
  errorAnimActive = true;
  errorAnimStart = millis();
  rgbLedState = true;
}

void updateErrorAnim()
{
  if (!errorAnimActive)
    return;

  unsigned long elapsed = millis() - errorAnimStart;
  unsigned long totalDuration = ERROR_FADE_DURATION * ERROR_FADE_CYCLES * 2;

  if (elapsed >= totalDuration)
  {
    FastLED.setBrightness(0);
    FastLED.show();
    for (int i = 0; i < NUM_LEDS; i++)
      leds[i] = CRGB::Black;
    rgbLedState = false;
    errorAnimActive = false;
  }
  else
  {
    int phase = elapsed / ERROR_FADE_DURATION;
    unsigned long phaseElapsed = elapsed % ERROR_FADE_DURATION;
    uint8_t brightness;

    if (phase % 2 == 0)
      brightness = (uint8_t)((phaseElapsed * 255) / ERROR_FADE_DURATION); // fade in
    else
      brightness = (uint8_t)(255 - (phaseElapsed * 255) / ERROR_FADE_DURATION); // fade out

    FastLED.setBrightness(brightness);
    FastLED.show();
  }
}

void updateHiAnim()
{
  if (!hiAnimActive)
    return;

  unsigned long elapsed = millis() - hiAnimStart;
  unsigned long totalDuration = HI_FADE_DURATION * 3; // 3 phases: in, out, in (end solid)

  if (elapsed >= totalDuration)
  {
    // Animation done — stay solid
    FastLED.setBrightness(255);
    FastLED.show();
    hiAnimActive = false;
  }
  else
  {
    int phase = elapsed / HI_FADE_DURATION;
    unsigned long phaseElapsed = elapsed % HI_FADE_DURATION;
    uint8_t brightness;

    if (phase % 2 == 0)
      brightness = (uint8_t)((phaseElapsed * 255) / HI_FADE_DURATION); // fade in
    else
      brightness = (uint8_t)(255 - (phaseElapsed * 255) / HI_FADE_DURATION); // fade out

    FastLED.setBrightness(brightness);
    FastLED.show();
  }
}
