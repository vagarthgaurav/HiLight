#pragma once
#include <Arduino.h>
#include <FastLED.h>
#include "config.h"

extern CRGB leds[NUM_LEDS];

// White LED state
extern bool whiteLedState;
extern bool whiteLedChanged;
extern bool rgbLedState;

// Encoder / brightness
extern const uint8_t brightnessLUT[];
extern int encoderPos;
extern int whiteBrightness;
extern int lastClkState;

// Animation state
extern bool hiAnimActive;
extern unsigned long hiAnimStart;
extern bool errorAnimActive;
extern unsigned long errorAnimStart;

void applyWhiteLight();
void startErrorAnim();
void updateHiAnim();
void updateErrorAnim();
