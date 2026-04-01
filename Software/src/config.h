#pragma once

// Pin definitions
#define BUTTON_PIN 8
#define WHITE_LED_PIN 4
#define NUM_LEDS 20
#define RGB_DATA_PIN 10
#define CLK_PIN 7 // connected to rotary encoder CLK (OUT A)
#define DT_PIN 6  // connected to rotary encoder DT (OUT B)

// Button timing (ms)
#define LONG_PRESS_TIME 2000UL
#define AP_PRESS_TIME 5000UL
#define ANIM_START_DELAY 300UL

// Network timing
#define WIFI_CONNECT_TIMEOUT 5000UL
#define WIFI_RETRY_INTERVAL (5UL * 60UL * 1000UL)

// Animation timing (ms)
#define HI_FADE_DURATION 800UL    // ms per fade phase (3 phases: in, out, in)
#define ERROR_FADE_DURATION 250UL // ms per fade phase (fast)
#define ERROR_FADE_CYCLES 3       // full fade in+out cycles
#define AP_FADE_DURATION 1500UL   // ms per fade phase (slow orange pulse)

// Encoder
#define ENCODER_MAX_POS 16
