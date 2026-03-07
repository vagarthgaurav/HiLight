#include <Arduino.h>

#include <FastLED.h>
// clang-format off
// WebSocketsClient.h must be included before MQTTPubSubClient.h
#include <WebSocketsClient.h>
#include <MQTTPubSubClient.h>
// clang-format on
#include <WiFi.h>
#include <WiFiClientSecure.h>

#define BUTTON_PIN 8
#define WHITE_LED_PIN 4

#define NUM_LEDS 20
#define RGB_DATA_PIN 10

#define CLK_PIN 7 // pin 6 connected to rotary encoder CLK (OUT A)
#define DT_PIN 6  // pin 7 connected to rotary encoder DT (OUT B)

bool buttonPressed = false;
unsigned long pressStart = 0;
const unsigned long longPressTime = 2000;
const unsigned long animStartDelay = 300;
int animLedCount = 0;

bool whiteLedState = false;
bool whiteLedChanged = false;
bool rgbLedState = false;

bool hiAnimActive = false;
unsigned long hiAnimStart = 0;
const unsigned long HI_FADE_DURATION = 800; // ms per fade in or out (4 phases = 2 full cycles)

String deviceId;

struct MacColor
{
  const char *id; // "HiLight_XX:XX:XX:XX:XX:XX"
  CRGB color;
};

const MacColor macColorTable[] = {
    {"HiLight_7C:2C:67:0B:83:48", CRGB::OrangeRed},   // Sabrina & Vagarth
    {"HiLight_7C:2C:67:0B:9F:F0", CRGB::ForestGreen}, // Jutta & Patrick
    {"HiLight_7C:2C:67:0B:92:08", CRGB::Turquoise},   // Alexandra & Gabriel
    {"HiLight_7C:2C:67:0B:93:90", CRGB::DeepPink},    // Edith & Robert
};
const int macColorTableSize = sizeof(macColorTable) / sizeof(macColorTable[0]);

CRGB colorForId(const String &id)
{
  for (int i = 0; i < macColorTableSize; i++)
  {
    if (id == macColorTable[i].id)
      return macColorTable[i].color;
  }
  return CRGB::LightBlue; // default
}

const char *ssid = "REDACTED";
const char *password = "REDACTED";
const char *broker_host = "mqtt.vagarth.dev";
const int broker_port = 443;
const char *ws_path = "/mqtt"; // Common path for MQTT over WS

WiFiClientSecure secureClient;
WebSocketsClient webSocket;
MQTTPubSubClient mqtt;

// Define the array of leds
CRGB leds[NUM_LEDS];

void applyWhiteLight()
{
  if (whiteLedState)
  {
    // White LED on: turn off RGB strip
    for (int i = 0; i < NUM_LEDS; i++)
      leds[i] = CRGB::Black;
    FastLED.show();
    analogWrite(WHITE_LED_PIN, 100);
  }
  else
  {
    // White LED off
    analogWrite(WHITE_LED_PIN, 0);
  }
  Serial.print("White LED state: ");
  Serial.println(whiteLedState);
}

void setup()
{
  Serial.begin(115200);

  FastLED.addLeds<NEOPIXEL, RGB_DATA_PIN>(leds, NUM_LEDS); // GRB ordering is assumed

  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }
  Serial.println(" connected.");

  deviceId = "HiLight_" + WiFi.macAddress();
  Serial.print("Device ID: ");
  Serial.println(deviceId);

  // Disable certificate validation for testing (or use setCACert for production)
  secureClient.setInsecure();

  // 1. Initialize WebSocket Connection
  // Port 443 + "true" for SSL (WSS)
  webSocket.beginSSL(broker_host, broker_port, ws_path, "", "mqtt");

  // 2. Initialize MQTT over the WebSocket client
  mqtt.begin(webSocket);

  if (mqtt.connect(deviceId))
  {
    Serial.println("Connected!");
    mqtt.publish("client/connected", deviceId);
  }

  mqtt.subscribe("publish/hi", [](const String &payload, const size_t size)
  {
    if (payload == deviceId) // Ignore messages from self
      return;

    Serial.print("publish/hi: ");
    Serial.println(payload);

    whiteLedState = false;
    applyWhiteLight();

    CRGB color = colorForId(payload);
    for (int i = 0; i < NUM_LEDS; i++)
      leds[i] = color;
    FastLED.setBrightness(0);
    FastLED.show();
    hiAnimActive = true;
    hiAnimStart = millis();
    rgbLedState = true;
  });

  pinMode(WHITE_LED_PIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
}

void loop()
{
  webSocket.loop();
  mqtt.update();

  if (digitalRead(BUTTON_PIN) == LOW)
  {
    if (!buttonPressed)
    {
      buttonPressed = true;
      pressStart = millis();
      animLedCount = 0;
    }

    // Animate LEDs filling up sequentially during long press (after initial delay)
    unsigned long elapsed = millis() - pressStart;
    int targetLeds =
        elapsed < animStartDelay
            ? 0
            : (int)(((elapsed - animStartDelay) * NUM_LEDS) / (longPressTime - animStartDelay));
    if (targetLeds > NUM_LEDS)
      targetLeds = NUM_LEDS;

    if (targetLeds != animLedCount)
    {
      if (animLedCount == 0)
      {
        whiteLedState = false;
        applyWhiteLight();
      }
      CRGB color = colorForId(deviceId);
      for (int i = animLedCount; i < targetLeds; i++)
        leds[i] = color;
      FastLED.show();
      rgbLedState = true;
      animLedCount = targetLeds;
    }
  }
  else
  {
    if (buttonPressed)
    {
      unsigned long pressDuration = millis() - pressStart;

      if (pressDuration >= longPressTime)
      {
        Serial.println("Long press");

        if (mqtt.isConnected())
        {
          Serial.println("Saying Hi!");
          mqtt.publish("publish/hi", deviceId);
          whiteLedState = false; // Turn off white LED on long press
          whiteLedChanged = true;
        }
      }
      else
      {
        Serial.println("Short press");
        // Clear animation LEDs
        hiAnimActive = false;
        FastLED.setBrightness(255);
        for (int i = 0; i < NUM_LEDS; i++)
          leds[i] = CRGB::Black;
        FastLED.show();
        if (rgbLedState)
        {
          rgbLedState = false;
        }
        else
        {
          whiteLedState = !whiteLedState;
          whiteLedChanged = true;
        }
      }

      buttonPressed = false;
      animLedCount = 0;
    }
  }

  if (whiteLedChanged)
  {
    applyWhiteLight();
    whiteLedChanged = false;
  }

  if (hiAnimActive)
  {
    unsigned long elapsed = millis() - hiAnimStart;
    unsigned long totalDuration = HI_FADE_DURATION * 3; // 3 phases: in, out, in, (end solid)

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
}
