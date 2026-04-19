#pragma once

// Set to 1 to enable debug serial output
#define DEBUG 0

#if DEBUG
#define DBG_PRINT(...) Serial.print(__VA_ARGS__)
#define DBG_PRINTLN(...) Serial.println(__VA_ARGS__)
#define DBG_PRINTF(...) Serial.printf(__VA_ARGS__)
#else
#define DBG_PRINT(...)
#define DBG_PRINTLN(...)
#define DBG_PRINTF(...)
#endif

// Pin definitions
#define BUTTON_PIN 8
#define WHITE_LED_PIN 4
#define WHITE_LED_CHANNEL 0    // LEDC channel for white LED
#define WHITE_LED_FREQ    20000 // 20 kHz — flicker-free at any duty cycle
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
#define MQTT_RETRY_INTERVAL 5000UL
#define AP_MODE_TIMEOUT (5UL * 60UL * 1000UL) // auto-exit AP mode after 5 minutes

// Animation timing (ms)
#define HI_FADE_DURATION 800UL    // ms per fade phase (3 phases: in, out, in)
#define ERROR_FADE_DURATION 250UL // ms per fade phase (fast)
#define ERROR_FADE_CYCLES 3       // full fade in+out cycles
#define AP_FADE_DURATION 1500UL   // ms per fade phase (slow orange pulse)

// OTA animation
#define OTA_SPIN_STEP 1 // progress callbacks to skip between spinner advances

// Encoder
#define ENCODER_MAX_POS 16

// LED rendering
#define SPINNER_BRIGHTNESS 200
#define SPINNER_WIDTH 4
