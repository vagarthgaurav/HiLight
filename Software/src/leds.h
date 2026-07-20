#pragma once
#include "config.h"
#include <Arduino.h>
#include <FastLED.h>

extern CRGB leds[NUM_LEDS];

// LED mode
enum LedMode { LED_IDLE, LED_CCT, LED_RGB_ANIM };
extern LedMode ledMode;
extern bool cctChanged;

// Encoder / brightness
extern const uint8_t brightnessLUT[];
extern int brightnessPos;
extern int cctPos;
extern int whiteBrightness;
extern int lastClkState;

enum EncoderTarget { ENC_BRIGHTNESS, ENC_CCT };
extern EncoderTarget encoderTarget;

// Animation state
extern bool hiAnimActive;
extern unsigned long hiAnimStart;
extern bool errorAnimActive;
extern bool apAnimActive;

void stopWarmLed();
void applyCCTLight();
void startErrorAnim();
void updateHiAnim();
void updateErrorAnim();
void startAPAnim();
void stopAPAnim();
void updateAPAnim();
void startOTAAnim();
void advanceOTASpinner();
