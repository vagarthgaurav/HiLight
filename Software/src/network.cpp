#include "network.h"
#include "config.h"
#include "device.h"
#include "leds.h"
#include "secrets.h"

// clang-format off
// WebSocketsClient.h must be included before MQTTPubSubClient.h
#include <WebSocketsClient.h>
#include <MQTTPubSubClient.h>
// clang-format on
#include <Preferences.h>
#include <WebServer.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>

static const char *broker_host = "mqtt.vagarth.dev";
static const int broker_port = 443;
static const char *ws_path = "/mqtt";

static WiFiClientSecure secureClient;
static WebSocketsClient webSocket;
static MQTTPubSubClient mqtt;
static WebServer webServer(80);
static Preferences prefs;

static String wifiSSID;
static String wifiPassword;

static bool wifiConnecting = false;
static unsigned long wifiConnectStart = 0;
static bool firstWifiAttempt = true;
static unsigned long lastWifiAttempt = 0;

static bool apModeActive = false;
static bool shouldExitAP = false;

static const char *AP_SETUP_PAGE = R"(<!DOCTYPE html>
<html>
<head>
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>HiLight Setup</title>
<style>
body{font-family:sans-serif;max-width:380px;margin:60px auto;padding:20px}
h2{color:#ff8c00}
label{display:block;margin:12px 0 4px;font-size:14px}
input{width:100%;padding:10px;box-sizing:border-box;border:1px solid #ccc;border-radius:4px;font-size:16px}
button{margin-top:20px;width:100%;padding:12px;background:#ff8c00;color:white;border:none;border-radius:4px;font-size:16px;cursor:pointer}
button:active{background:#cc7000}
</style>
</head>
<body>
<h2>HiLight WiFi Setup</h2>
<form method="POST" action="/save">
<label>WiFi Network (SSID)</label>
<input type="text" name="ssid" placeholder="Network name" autocomplete="off">
<label>Password</label>
<input type="password" name="password" placeholder="Password">
<button type="submit">Save &amp; Connect</button>
</form>
</body>
</html>)";

static const char *AP_SAVED_PAGE = R"(<!DOCTYPE html>
<html>
<head>
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>HiLight Setup</title>
<style>body{font-family:sans-serif;max-width:380px;margin:60px auto;padding:20px}h2{color:#ff8c00}</style>
</head>
<body>
<h2>Saved!</h2>
<p>Connecting to your WiFi network. You can close this page.</p>
</body>
</html>)";

static void loadCredentials()
{
  prefs.begin("wifi", true);
  wifiSSID = prefs.getString("ssid", WIFI_SSID);
  wifiPassword = prefs.getString("pass", WIFI_PASSWORD);
  prefs.end();
}

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
  loadCredentials();
  WiFi.begin(wifiSSID.c_str(), wifiPassword.c_str());
  deviceId = "HiLight_" + WiFi.macAddress();
  Serial.print("Device ID: ");
  Serial.println(deviceId);
  wifiConnecting = true;
  wifiConnectStart = millis();
}

void startAPMode()
{
  WiFi.disconnect();
  WiFi.mode(WIFI_AP);
  String apSSID = "HiLight-Setup";
  WiFi.softAP(apSSID.c_str());
  Serial.print("AP Mode started. SSID: ");
  Serial.println(apSSID);
  Serial.print("AP IP: ");
  Serial.println(WiFi.softAPIP());

  webServer.on("/", HTTP_GET, []() { webServer.send(200, "text/html", AP_SETUP_PAGE); });

  webServer.on("/save", HTTP_POST, []()
  {
    String ssid = webServer.arg("ssid");
    String password = webServer.arg("password");
    if (ssid.length() == 0)
    {
      webServer.send(400, "text/plain", "SSID cannot be empty");
      return;
    }
    prefs.begin("wifi", false);
    prefs.putString("ssid", ssid);
    prefs.putString("pass", password);
    prefs.end();
    Serial.println("WiFi credentials saved.");
    webServer.send(200, "text/html", AP_SAVED_PAGE);
    shouldExitAP = true;
  });

  webServer.begin();
  apModeActive = true;
  wifiConnecting = false;
}

bool isAPMode()
{
  return apModeActive;
}

static void exitAPMode()
{
  webServer.stop();
  apModeActive = false;
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_STA);

  loadCredentials();
  WiFi.begin(wifiSSID.c_str(), wifiPassword.c_str());
  wifiConnecting = true;
  wifiConnectStart = millis();
  firstWifiAttempt = true;

  stopAPAnim();
}

void updateNetwork()
{
  if (apModeActive)
  {
    webServer.handleClient();
    if (shouldExitAP)
    {
      shouldExitAP = false;
      exitAPMode();
    }
    return;
  }

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
    WiFi.begin(wifiSSID.c_str(), wifiPassword.c_str());
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
