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

bool buttonPressed = false;
unsigned long pressStart = 0;
const unsigned long longPressTime = 2000;

bool whiteLedState = false;

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

void toggleWhiteLight()
{
  if (whiteLedState == 1)
  {

    Serial.print("White LED state: ");
    Serial.println(whiteLedState);

    analogWrite(WHITE_LED_PIN, 100);
    delay(100);
    for (int i = 0; i <= NUM_LEDS; i++)
    {
      leds[i] = CRGB::Black;
      FastLED.show();
    }
  }
  else
  {
    Serial.print("White LED state: ");
    Serial.println(whiteLedState);

    analogWrite(WHITE_LED_PIN, 0);
  }
}

void setup()
{
  Serial.begin(115200);

  FastLED.addLeds<NEOPIXEL, RGB_DATA_PIN>(leds, NUM_LEDS); // GRB ordering is assumed

  WiFi.begin(ssid, password);

  // Disable certificate validation for testing (or use setCACert for production)
  secureClient.setInsecure();

  // 1. Initialize WebSocket Connection
  // Port 443 + "true" for SSL (WSS)
  webSocket.beginSSL(broker_host, broker_port, ws_path, "", "mqtt");

  // 2. Initialize MQTT over the WebSocket client
  mqtt.begin(webSocket);

  if (mqtt.connect("esp32_client"))
  {
    Serial.println("Connected!");
    mqtt.publish("client/connected", "Hello MQTT!");
  }

  mqtt.subscribe("publish/hi", [](const String &payload, const size_t size)
  {
    Serial.print("publish/hi: ");
    Serial.println(payload);

    for (int i = 0; i <= NUM_LEDS; i++)
    {
      leds[i] = CRGB::LightBlue;
      FastLED.show();
      delay(100);
    }
  });

  pinMode(WHITE_LED_PIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT);
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
          mqtt.publish("publish/hi", "vagarth");
        }
      }
      else
      {
        Serial.println("Short press");
        whiteLedState = !whiteLedState;
      }

      buttonPressed = false;
    }
  }
  toggleWhiteLight();
  // delay(100);
}
