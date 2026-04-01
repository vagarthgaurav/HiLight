#include "network.h"
#include "secrets.h"
#include "config.h"
#include "device.h"
#include "leds.h"

// clang-format off
// WebSocketsClient.h must be included before MQTTPubSubClient.h
#include <WebSocketsClient.h>
#include <MQTTPubSubClient.h>
// clang-format on
#include <WiFi.h>
#include <WiFiClientSecure.h>

static const char *broker_host = "mqtt.vagarth.dev";
static const int broker_port = 443;
static const char *ws_path = "/mqtt";

static WiFiClientSecure secureClient;
static WebSocketsClient webSocket;
static MQTTPubSubClient mqtt;

static bool wifiConnecting = false;
static unsigned long wifiConnectStart = 0;
static bool firstWifiAttempt = true;
static unsigned long lastWifiAttempt = 0;

static void setupMQTT()
{
  secureClient.setInsecure();
  webSocket.beginSSL(broker_host, broker_port, ws_path, "", "mqtt");
  mqtt.begin(webSocket);

  if (mqtt.connect(deviceId))
  {
    Serial.println("MQTT connected!");
    mqtt.publish("client/connected", deviceId);
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
  }
}

void initNetwork()
{
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  deviceId = "HiLight_" + WiFi.macAddress();
  Serial.print("Device ID: ");
  Serial.println(deviceId);
  wifiConnecting = true;
  wifiConnectStart = millis();
}

void updateNetwork()
{
  if (wifiConnecting)
  {
    if (WiFi.status() == WL_CONNECTED)
    {
      wifiConnecting = false;
      lastWifiAttempt = millis();
      Serial.println("WiFi connected.");
      setupMQTT();
      firstWifiAttempt = false;
    }
    else if (millis() - wifiConnectStart >= WIFI_CONNECT_TIMEOUT)
    {
      wifiConnecting = false;
      lastWifiAttempt = millis();
      Serial.println("WiFi connection timed out, will retry in 5 minutes.");
      if (firstWifiAttempt)
      {
        firstWifiAttempt = false;
        startErrorAnim();
      }
    }
  }
  else if (WiFi.status() == WL_CONNECTED)
  {
    webSocket.loop();
    mqtt.update();
  }
  else if (millis() - lastWifiAttempt >= WIFI_RETRY_INTERVAL)
  {
    Serial.println("Retrying WiFi connection...");
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    wifiConnecting = true;
    wifiConnectStart = millis();
  }
}

bool isMqttConnected()
{
  return mqtt.isConnected();
}

void publishHi()
{
  mqtt.publish("publish/hi", deviceId);
}
