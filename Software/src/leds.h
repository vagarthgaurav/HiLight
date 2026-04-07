#pragma once
#include "config.h"
#include <Arduino.h>
#include <FastLED.h>

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
extern bool apAnimActive;

void applyWhiteLight();
void startErrorAnim();
void updateHiAnim();
void updateErrorAnim();
void startAPAnim();
void stopAPAnim();
void updateAPAnim();
void startOTAAnim();
void advanceOTASpinner();
