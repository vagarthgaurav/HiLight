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
#include <DNSServer.h>
#include <HTTPUpdate.h>
#include <Preferences.h>
#include <WebServer.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>

static const char *broker_host = "mqtt.vagarth.dev";
static const int broker_port = 443;
static const char *ws_path = "/mqtt";

static DNSServer dnsServer;
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
static unsigned long apModeStart = 0;

static bool mqttConnected = false;
static unsigned long lastMqttAttempt = 0;

static String pendingOtaUrl = "";

static String buildSetupPage(const String &scanOptions)
{
  return String(R"HTML(<!DOCTYPE html>
<html>
<head>
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>HiLight Setup</title>
<style>
body{font-family:sans-serif;max-width:380px;margin:60px auto;padding:20px}
h2{color:#ff8c00}
label{display:block;margin:12px 0 4px;font-size:14px}
select,input[type=text],input[type=password]{width:100%;padding:10px;box-sizing:border-box;border:1px solid #ccc;border-radius:4px;font-size:16px}
.radio-row{display:flex;gap:16px;margin:12px 0 8px}
.radio-row label{margin:0;display:flex;align-items:center;gap:6px;font-size:14px;cursor:pointer}
button{margin-top:20px;width:100%;padding:12px;background:#ff8c00;color:white;border:none;border-radius:4px;font-size:16px;cursor:pointer}
button:active{background:#cc7000}
</style>
</head>
<body>
<h2>HiLight WiFi Setup</h2>
<form method="POST" action="/save">
<label>WiFi Network (SSID)</label>
<div class="radio-row">
<label><input type="radio" name="ssid_mode" value="list" checked onchange="toggle(this)"> Choose from list</label>
<label><input type="radio" name="ssid_mode" value="manual" onchange="toggle(this)"> Enter manually</label>
</div>
<select id="ssid_sel" onchange="document.getElementById('ssid_in').value=this.value">
)HTML") + scanOptions +
         R"HTML(</select>
<input type="hidden" name="ssid" id="ssid_in">
<input type="text" id="ssid_manual" placeholder="Network name" autocomplete="off" style="display:none">
<label>Password</label>
<input type="password" name="password" placeholder="Password">
<button type="submit">Save &amp; Connect</button>
</form>
<script>
var sel=document.getElementById('ssid_sel');
var manual=document.getElementById('ssid_manual');
var hidden=document.getElementById('ssid_in');
sel.dispatchEvent(new Event('change'));
function toggle(r){
  if(r.value==='manual'){sel.style.display='none';manual.style.display='block';manual.name='ssid';hidden.name='';}
  else{sel.style.display='block';manual.style.display='none';manual.name='';hidden.name='ssid';sel.dispatchEvent(new Event('change'));}
}
</script>
</body>
</html>)HTML";
}

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

static void publishDiscovery()
{
  String disco = "{";
  disco += "\"name\":\"" + deviceId + "\",";
  disco += "\"unique_id\":\"" + deviceId + "\",";
  disco += "\"state_topic\":\"hilight/" + deviceId + "/power/state\",";
  disco += "\"command_topic\":\"hilight/" + deviceId + "/power\",";
  disco += "\"brightness_state_topic\":\"hilight/" + deviceId + "/brightness/state\",";
  disco += "\"brightness_command_topic\":\"hilight/" + deviceId + "/brightness\",";
  disco += "\"availability_topic\":\"hilight/" + deviceId + "/availability\",";
  disco += "\"payload_on\":\"ON\",";
  disco += "\"payload_off\":\"OFF\",";
  disco += "\"brightness_scale\":255";
  disco += "}";
  mqtt.publish("homeassistant/light/" + deviceId + "/config",
               (uint8_t *)disco.c_str(), disco.length(), true, 0);
}

static void onMqttConnect()
{
  mqtt.publish("hilight/" + deviceId + "/availability", (uint8_t *)"online", 6, true, 0);
  publishDiscovery();
  mqtt.publish("client/connected", deviceId);
  mqtt.subscribe("publish/hi", [](const String &payload, const size_t size)
  {
    if (payload == deviceId) // Ignore messages from self
      return;

    ledMode = LED_RGB_ANIM;
    applyWhiteLight();

    CRGB color = colorForId(payload);
    for (int i = 0; i < NUM_LEDS; i++)
      leds[i] = color;
    FastLED.setBrightness(0);
    FastLED.show();
    hiAnimActive = true;
    hiAnimStart = millis();
  });

  mqtt.subscribe("hilight/ota", [](const String &url, const size_t size)
  {
    if (url.length() > 0)
      pendingOtaUrl = url;
  });

  mqtt.subscribe("hilight/" + deviceId + "/power", [](const String &payload, const size_t size)
  {
    if (payload != "ON" && payload != "OFF")
      return;

    hiAnimActive = false;
    for (int i = 0; i < NUM_LEDS; i++)
      leds[i] = CRGB::Black;
    FastLED.show();

    ledMode = (payload == "ON") ? LED_WHITE : LED_IDLE;
    whiteLedChanged = true;
    publishPowerState();
  });

  mqtt.subscribe("hilight/" + deviceId + "/brightness", [](const String &payload, const size_t size)
  {
    int brightness = map(constrain(payload.toInt(), 1, 255), 1, 255, brightnessLUT[0],
                         brightnessLUT[ENCODER_MAX_POS]);
    whiteBrightness = brightness;

    // Find the nearest brightness level in the LUT and update encoderPos accordingly
    int nearest = 0;
    for (int i = 1; i <= ENCODER_MAX_POS; i++)
    {
      if (abs(brightnessLUT[i] - brightness) < abs(brightnessLUT[nearest] - brightness))
        nearest = i;
    }
    encoderPos = nearest;

    if (ledMode == LED_WHITE)
      whiteLedChanged = true;
    publishBrightnessState();
  });

  publishPowerState();
  publishBrightnessState();
}

static void setupMQTT()
{
  secureClient.setInsecure();
  webSocket.beginSSL(broker_host, broker_port, ws_path, "", "mqtt");
  mqtt.begin(webSocket);
  mqtt.setKeepAliveTimeout(15); // broker declares client dead after ~22s; triggers LWT delivery
  mqtt.setWill("hilight/" + deviceId + "/availability", "offline", true, 0);

  if (mqtt.connect(deviceId))
  {
    mqttConnected = true;
    onMqttConnect();
  }
}

void initNetwork()
{
  loadCredentials();
  WiFi.begin(wifiSSID.c_str(), wifiPassword.c_str());
  deviceId = "HiLight_" + WiFi.macAddress();
  wifiConnecting = true;
  wifiConnectStart = millis();
}

void startAPMode()
{
  WiFi.disconnect();
  WiFi.mode(WIFI_AP_STA);

  int n = WiFi.scanNetworks();
  String scanOptions = "";
  for (int i = 0; i < n; i++)
  {
    String ssid = WiFi.SSID(i);
    ssid.replace("\"", "&quot;");
    scanOptions += "<option value=\"" + ssid + "\">" + ssid + "</option>\n";
  }
  WiFi.scanDelete();

  String apSSID = "HiLight-Setup";
  WiFi.softAP(apSSID.c_str());

  dnsServer.start(53, "*", WiFi.softAPIP());

  String setupPage = buildSetupPage(scanOptions);
  webServer.on("/", HTTP_GET, [setupPage]() { webServer.send(200, "text/html", setupPage); });

  // Captive portal detection — redirect OS connectivity checks to the setup page
  auto captiveRedirect = []()
  {
    webServer.sendHeader("Location", "http://" + WiFi.softAPIP().toString(), true);
    webServer.send(302, "text/plain", "");
  };
  webServer.on("/generate_204", HTTP_GET, captiveRedirect);        // Android
  webServer.on("/hotspot-detect.html", HTTP_GET, captiveRedirect); // iOS / macOS
  webServer.on("/library/test/success.html", HTTP_GET, captiveRedirect);
  webServer.on("/connecttest.txt", HTTP_GET, captiveRedirect); // Windows
  webServer.on("/ncsi.txt", HTTP_GET, captiveRedirect);
  webServer.onNotFound([captiveRedirect]() { captiveRedirect(); }); // Catch-all

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
    webServer.send(200, "text/html", AP_SAVED_PAGE);
    shouldExitAP = true;
  });

  webServer.begin();
  apModeActive = true;
  apModeStart = millis();
  wifiConnecting = false;
}

bool isAPMode()
{
  return apModeActive;
}

static void exitAPMode()
{
  dnsServer.stop();
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
    dnsServer.processNextRequest();
    webServer.handleClient();
    if (shouldExitAP || millis() - apModeStart >= AP_MODE_TIMEOUT)
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
      setupMQTT();
      firstWifiAttempt = false;
    }
    else if (millis() - wifiConnectStart >= WIFI_CONNECT_TIMEOUT)
    {
      wifiConnecting = false;
      lastWifiAttempt = millis();
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

    if (pendingOtaUrl.length() > 0)
    {
      String url = pendingOtaUrl;
      pendingOtaUrl = "";

      // Clear the retained message on the broker before starting the download
      // so the device does not re-trigger OTA with the stale URL after reboot
      mqtt.publish("hilight/ota", (uint8_t *)"", 0, true, 0);

      startOTAAnim();
      httpUpdate.onProgress([](int current, int total) { advanceOTASpinner(); });
      httpUpdate.onEnd([]()
      {
        FastLED.setBrightness(0);
        for (int i = 0; i < NUM_LEDS; i++)
          leds[i] = CRGB::Black;
        FastLED.show();
      });

      WiFiClientSecure otaClient;
      otaClient.setInsecure();
      t_httpUpdate_return result = httpUpdate.update(otaClient, url);

      if (result == HTTP_UPDATE_FAILED)
        startErrorAnim();
      // HTTP_UPDATE_NO_UPDATES: silently ignore (not an error)
      // HTTP_UPDATE_OK: device was rebooted by httpUpdate, never reached
      return;
    }

    bool nowConnected = mqtt.isConnected();
    if (nowConnected && !mqttConnected)
    {
      mqttConnected = true;
      onMqttConnect();
    }
    else if (!nowConnected)
    {
      mqttConnected = false;
      if (millis() - lastMqttAttempt >= MQTT_RETRY_INTERVAL)
      {
        lastMqttAttempt = millis();
        if (!webSocket.isConnected())
          webSocket.beginSSL(broker_host, broker_port, ws_path, "", "mqtt");
        mqtt.connect(deviceId);
      }
    }
  }
  else if (millis() - lastWifiAttempt >= WIFI_RETRY_INTERVAL)
  {
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

void publishPowerState()
{
  if (!mqtt.isConnected())
    return;
  String stateTopic = "hilight/" + deviceId + "/power/state";
  const char *payload = (ledMode == LED_WHITE) ? "ON" : "OFF";
  mqtt.publish(stateTopic, (uint8_t *)payload, strlen(payload), true, 0);
}

void publishBrightnessState()
{
  if (!mqtt.isConnected())
    return;
  String stateTopic = "hilight/" + deviceId + "/brightness/state";
  int haValue = map(whiteBrightness, brightnessLUT[0], brightnessLUT[ENCODER_MAX_POS], 1, 255);
  String payload = String(haValue);
  mqtt.publish(stateTopic, (uint8_t *)payload.c_str(), payload.length(), true, 0);
}
