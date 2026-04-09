#include "leds.h"

CRGB leds[NUM_LEDS];

LedMode ledMode = LED_IDLE;
bool whiteLedChanged = false;

// Gamma-corrected brightness values (gamma=2.2, PWM 5..255 in 17 steps).
// Equal encoder steps produce perceptually equal brightness changes.
// Flicker-free at all levels because LEDC runs at 20 kHz (see WHITE_LED_FREQ).
const uint8_t brightnessLUT[] = {  5,   9,  15,  21,  30,  39,  51,  64,  78,
                                   94, 112, 132, 153, 176, 200, 227, 255};
int encoderPos = 0;
int whiteBrightness = brightnessLUT[0];
int lastClkState = HIGH;

bool hiAnimActive = false;
unsigned long hiAnimStart = 0;
bool errorAnimActive = false;
static unsigned long errorAnimStart = 0;
bool apAnimActive = false;
static unsigned long apAnimStart = 0;

static int otaSpinPos = 0;
static int otaProgressCount = 0;

static const CRGB OTA_BG_COLOR = CRGB(255, 220, 60); // light yellow
static const CRGB OTA_SPIN_COLOR = CRGB(160, 90, 0); // darker amber-yellow

void applyWhiteLight()
{
  if (ledMode == LED_WHITE)
  {
    // White LED on: turn off RGB strip
    for (int i = 0; i < NUM_LEDS; i++)
      leds[i] = CRGB::Black;
    FastLED.show();
    ledcWrite(WHITE_LED_CHANNEL, whiteBrightness);
  }
  else
  {
    // White LED off
    ledcWrite(WHITE_LED_CHANNEL, 0);
  }

}

void startErrorAnim()
{
  ledMode = LED_RGB_ANIM;
  applyWhiteLight();
  for (int i = 0; i < NUM_LEDS; i++)
    leds[i] = CRGB::Red;
  FastLED.setBrightness(0);
  FastLED.show();
  hiAnimActive = false;
  errorAnimActive = true;
  errorAnimStart = millis();
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
    ledMode = LED_IDLE;
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

void startAPAnim()
{
  ledMode = LED_RGB_ANIM;
  applyWhiteLight();
  for (int i = 0; i < NUM_LEDS; i++)
    leds[i] = CRGB::Orange;
  FastLED.setBrightness(0);
  FastLED.show();
  hiAnimActive = false;
  errorAnimActive = false;
  apAnimActive = true;
  apAnimStart = millis();
}

void stopAPAnim()
{
  apAnimActive = false;
  FastLED.setBrightness(0);
  for (int i = 0; i < NUM_LEDS; i++)
    leds[i] = CRGB::Black;
  FastLED.show();
  ledMode = LED_IDLE;
}

void updateAPAnim()
{
  if (!apAnimActive)
    return;

  unsigned long elapsed = (millis() - apAnimStart) % (AP_FADE_DURATION * 2);
  uint8_t brightness;
  if (elapsed < AP_FADE_DURATION)
    brightness = (uint8_t)((elapsed * 255) / AP_FADE_DURATION);
  else
    brightness = (uint8_t)(255 - ((elapsed - AP_FADE_DURATION) * 255) / AP_FADE_DURATION);

  FastLED.setBrightness(brightness);
  FastLED.show();
}

void startOTAAnim()
{
  ledMode = LED_RGB_ANIM;
  applyWhiteLight();
  hiAnimActive = false;
  errorAnimActive = false;
  apAnimActive = false;
  otaSpinPos = 0;
  otaProgressCount = 0;

  for (int i = 0; i < NUM_LEDS; i++)
    leds[i] = OTA_BG_COLOR;
  // Place SPINNER_WIDTH consecutive spinner LEDs
  for (int s = 0; s < SPINNER_WIDTH; s++)
    leds[(otaSpinPos + s) % NUM_LEDS] = OTA_SPIN_COLOR;
  FastLED.setBrightness(SPINNER_BRIGHTNESS);
  FastLED.show();
}

void advanceOTASpinner()
{
  otaProgressCount++;
  if (otaProgressCount < OTA_SPIN_STEP)
    return;
  otaProgressCount = 0;

  otaSpinPos = (otaSpinPos + 1) % NUM_LEDS;
  for (int i = 0; i < NUM_LEDS; i++)
    leds[i] = OTA_BG_COLOR;
  for (int s = 0; s < SPINNER_WIDTH; s++)
    leds[(otaSpinPos + s) % NUM_LEDS] = OTA_SPIN_COLOR;
  FastLED.show();
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
