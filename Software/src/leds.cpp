#include "leds.h"
#include <math.h>

CRGB leds[NUM_LEDS];

LedMode ledMode = LED_IDLE;
bool cctChanged = false;

// Gamma-corrected brightness values (gamma=2.2, PWM 5..255 in 17 steps).
// Equal encoder steps produce perceptually equal brightness changes.
// Flicker-free at all levels because LEDC runs at 20 kHz (see WHITE_LED_FREQ).
const uint8_t brightnessLUT[] = {  5,   9,  15,  21,  30,  39,  51,  64,  78,
                                   94, 112, 132, 153, 176, 200, 227, 255};
int brightnessPos = 0;
int cctPos = 0;
int whiteBrightness = brightnessLUT[0];
int lastClkState = HIGH;
EncoderTarget encoderTarget = ENC_BRIGHTNESS;

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

// Phase-based fade-in/fade-out brightness envelope shared by the hi/error/AP
// animations: within each phaseDuration-long phase, brightness ramps 0->255
// (even phase, fade in) or 255->0 (odd phase, fade out).
static uint8_t fadeEnvelope(unsigned long elapsed, unsigned long phaseDuration)
{
  unsigned long phase = elapsed / phaseDuration;
  unsigned long phaseElapsed = elapsed % phaseDuration;

  if (phase % 2 == 0)
    return (uint8_t)((phaseElapsed * 255) / phaseDuration); // fade in
  else
    return (uint8_t)(255 - (phaseElapsed * 255) / phaseDuration); // fade out
}

void stopWarmLed()
{
  ledcWrite(WHITE_LED_PIN, 0);
}

void applyCCTLight()
{
  if (ledMode == LED_CCT)
  {
    // Equal-power crossfade (cos/sin) so neither end looks dimmer than the other.
    uint8_t envelope = brightnessLUT[constrain(brightnessPos, 0, ENCODER_MAX_POS)];
    int warmVal, coldVal;

    if (cctPos <= 0)
    {
      warmVal = envelope;
      coldVal = 0;
    }
    else if (cctPos >= ENCODER_MAX_POS)
    {
      warmVal = 0;
      coldVal = envelope;
    }
    else
    {
      float theta = (cctPos / (float)ENCODER_MAX_POS) * (PI / 2.0f);
      warmVal = (int)roundf(cosf(theta) * envelope);
      coldVal = (int)roundf(sinf(theta) * envelope);
    }

    ledcWrite(WHITE_LED_PIN, constrain(warmVal, 0, 255));

    for (int i = 0; i < NUM_LEDS; i++)
      leds[i] = CRGB::White;
    FastLED.setBrightness(constrain(coldVal, 0, 255));
    FastLED.show();
  }
  else
  {
    ledcWrite(WHITE_LED_PIN, 0);
    for (int i = 0; i < NUM_LEDS; i++)
      leds[i] = CRGB::Black;
    FastLED.setBrightness(255);
    FastLED.show();
  }
}

void startErrorAnim()
{
  ledMode = LED_RGB_ANIM;
  stopWarmLed();
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
    FastLED.setBrightness(fadeEnvelope(elapsed, ERROR_FADE_DURATION));
    FastLED.show();
  }
}

void startAPAnim()
{
  ledMode = LED_RGB_ANIM;
  stopWarmLed();
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
  FastLED.setBrightness(fadeEnvelope(elapsed, AP_FADE_DURATION));
  FastLED.show();
}

void startOTAAnim()
{
  ledMode = LED_RGB_ANIM;
  stopWarmLed();
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
    FastLED.setBrightness(fadeEnvelope(elapsed, HI_FADE_DURATION));
    FastLED.show();
  }
}
