#include "network.h"
#include "config.h"
#include "device.h"
#include "leds.h"
#include "ota_pubkey.h"
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

// GTS Root R4 — self-signed root for Google Trust Services, which issues the
// certs for both mqtt.vagarth.dev and hi-light-fw.vagarth.dev. Valid until
// 2036, so it doesn't need updating as the leaf/intermediate certs rotate.
static const char *GTS_ROOT_CA = R"(-----BEGIN CERTIFICATE-----
MIICCTCCAY6gAwIBAgINAgPlwGjvYxqccpBQUjAKBggqhkjOPQQDAzBHMQswCQYD
VQQGEwJVUzEiMCAGA1UEChMZR29vZ2xlIFRydXN0IFNlcnZpY2VzIExMQzEUMBIG
A1UEAxMLR1RTIFJvb3QgUjQwHhcNMTYwNjIyMDAwMDAwWhcNMzYwNjIyMDAwMDAw
WjBHMQswCQYDVQQGEwJVUzEiMCAGA1UEChMZR29vZ2xlIFRydXN0IFNlcnZpY2Vz
IExMQzEUMBIGA1UEAxMLR1RTIFJvb3QgUjQwdjAQBgcqhkjOPQIBBgUrgQQAIgNi
AATzdHOnaItgrkO4NcWBMHtLSZ37wWHO5t5GvWvVYRg1rkDdc/eJkTBa6zzuhXyi
QHY7qca4R9gq55KRanPpsXI5nymfopjTX15YhmUPoYRlBtHci8nHc8iMai/lxKvR
HYqjQjBAMA4GA1UdDwEB/wQEAwIBhjAPBgNVHRMBAf8EBTADAQH/MB0GA1UdDgQW
BBSATNbrdP9JNqPV2Py1PsVq8JQdjDAKBggqhkjOPQQDAwNpADBmAjEA6ED/g94D
9J+uHXqnLrmvT/aDHQ4thQEd0dlq7A/Cr8deVl5c1RxYIigL9zC2L7F8AjEA8GE8
p/SgguMh1YQdc4acLa/KNJvxn7kjNuK8YAOdgLOaVsjh4rsUecrNIdSUtUlD
-----END CERTIFICATE-----)";

static DNSServer dnsServer;
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
static int mqttRetryCount = 0;

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
  wifiSSID = prefs.getString("ssid", "");
  wifiPassword = prefs.getString("pass", "");
  prefs.end();
}

static void publishDiscovery()
{
  String discoveryTopic = "homeassistant/light/" + deviceId + "/config";
  if (!haDiscoveryForId(deviceId))
  {
    // Clear any retained config from earlier firmware so HA drops the entity
    mqtt.publish(discoveryTopic, (uint8_t *)"", 0, true, 0);
    return;
  }

  String ownerName = nameForId(deviceId);
  String disco;
  disco.reserve(512);
  disco += "{";
  disco += "\"name\":\"HiLight (" + ownerName + ")\",";
  disco += "\"unique_id\":\"" + deviceId + "\",";
  disco += "\"device\":{";
  disco += "\"identifiers\":[\"" + deviceId + "\"],";
  disco += "\"name\":\"HiLight (" + ownerName + ")\",";
  disco += "\"model\":\"HiLight Lamp\"";
  disco += "},";
  disco += "\"state_topic\":\"hilight/" + deviceId + "/power/state\",";
  disco += "\"command_topic\":\"hilight/" + deviceId + "/power\",";
  disco += "\"brightness_state_topic\":\"hilight/" + deviceId + "/brightness/state\",";
  disco += "\"brightness_command_topic\":\"hilight/" + deviceId + "/brightness\",";
  disco += "\"brightness_scale\":255,";
  disco += "\"color_temp_state_topic\":\"hilight/" + deviceId + "/color_temp/state\",";
  disco += "\"color_temp_command_topic\":\"hilight/" + deviceId + "/color_temp\",";
  disco += "\"min_mireds\":" + String(MIN_MIREDS) + ",";
  disco += "\"max_mireds\":" + String(MAX_MIREDS) + ",";
  disco += "\"supported_color_modes\":[\"color_temp\"],";
  disco += "\"availability_topic\":\"hilight/" + deviceId + "/availability\",";
  disco += "\"payload_on\":\"ON\",";
  disco += "\"payload_off\":\"OFF\"";
  disco += "}";
  mqtt.publish(discoveryTopic, (uint8_t *)disco.c_str(), disco.length(), true, 0);
}

static void onMqttConnect()
{
  mqtt.publish("hilight/" + deviceId + "/availability", (uint8_t *)"online", 6, true, 0);
  publishDiscovery();
  mqtt.publish("client/connected", deviceId);
  mqtt.subscribe("publish/hi", [](const String &payload, const size_t size)
  {
    ledMode = LED_RGB_ANIM;
    stopWarmLed();

    CRGB color = colorForId(payload);
    for (int i = 0; i < NUM_LEDS; i++)
      leds[i] = color;
    FastLED.setBrightness(0);
    FastLED.show();
    hiAnimActive = true;
    hiAnimStart = millis();
  });

  mqtt.subscribe("hilight/" + deviceId + "/ota", [](const String &url, const size_t size)
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

    ledMode = (payload == "ON") ? LED_CCT : LED_IDLE;
    if (ledMode == LED_CCT)
      encoderTarget = ENC_BRIGHTNESS;
    cctChanged = true;
    publishPowerState();
  });

  mqtt.subscribe("hilight/" + deviceId + "/brightness", [](const String &payload, const size_t size)
  {
    int brightness = map(constrain(payload.toInt(), 1, 255), 1, 255, brightnessLUT[0],
                         brightnessLUT[ENCODER_MAX_POS]);
    whiteBrightness = brightness;

    // Find the nearest brightness level in the LUT and update brightnessPos accordingly
    int nearest = 0;
    for (int i = 1; i <= ENCODER_MAX_POS; i++)
    {
      if (abs(brightnessLUT[i] - brightness) < abs(brightnessLUT[nearest] - brightness))
        nearest = i;
    }
    brightnessPos = nearest;

    if (ledMode == LED_CCT)
      cctChanged = true;
    publishBrightnessState();
  });

  mqtt.subscribe("hilight/" + deviceId + "/color_temp", [](const String &payload, const size_t size)
  {
    int mireds = constrain(payload.toInt(), MIN_MIREDS, MAX_MIREDS);
    cctPos = map(mireds, MAX_MIREDS, MIN_MIREDS, 0, ENCODER_MAX_POS);

    if (ledMode == LED_CCT)
      cctChanged = true;
    publishCCTState();
  });

  publishPowerState();
  publishBrightnessState();
  publishCCTState();
}

static void setupMQTT()
{
  mqttRetryCount = 0;
  DBG_PRINTF("[MQTT] Connecting to %s:%d%s\n", broker_host, broker_port, ws_path);
  // beginSSL() with no CA silently falls back to setInsecure(); always pass the root.
  webSocket.beginSslWithCA(broker_host, broker_port, ws_path, GTS_ROOT_CA, "mqtt");
  mqtt.begin(webSocket);
  mqtt.setKeepAliveTimeout(15); // broker declares client dead after ~22s; triggers LWT delivery
  mqtt.setWill("hilight/" + deviceId + "/availability", "offline", true, 0);

  if (mqtt.connect(deviceId, MQTT_USERNAME, MQTT_PASSWORD))
  {
    DBG_PRINTLN("[MQTT] Connected");
    mqttConnected = true;
    onMqttConnect();
  }
  else
  {
    DBG_PRINTLN("[MQTT] Initial connect failed");
  }
}

void initNetwork()
{
  loadCredentials();
  DBG_PRINTF("[WiFi] Connecting to SSID: %s\n", wifiSSID.c_str());
  WiFi.begin(wifiSSID.c_str(), wifiPassword.c_str());
  String mac = WiFi.macAddress();
  mac.replace(":", "");
  deviceId = "HiLight_" + mac;
  DBG_PRINTF("[Device] ID: %s\n", deviceId.c_str());
  wifiConnecting = true;
  wifiConnectStart = millis();
}

void startAPMode()
{
  WiFi.disconnect();
  WiFi.mode(WIFI_AP_STA);

  int n = WiFi.scanNetworks();
  String scanOptions;
  scanOptions.reserve(n > 0 ? n * 48 : 0);
  for (int i = 0; i < n; i++)
  {
    String ssid = WiFi.SSID(i);
    ssid.replace("&", "&amp;");
    ssid.replace("<", "&lt;");
    ssid.replace(">", "&gt;");
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
      DBG_PRINTF("[WiFi] Connected. IP: %s\n", WiFi.localIP().toString().c_str());
      wifiConnecting = false;
      lastWifiAttempt = millis();
      setupMQTT();
      firstWifiAttempt = false;
    }
    else if (millis() - wifiConnectStart >= WIFI_CONNECT_TIMEOUT)
    {
      DBG_PRINTF("[WiFi] Connection timed out (status=%d)\n", WiFi.status());
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
    unsigned long retryInterval = (mqttRetryCount < MQTT_MAX_RETRIES) ? MQTT_RETRY_INTERVAL
                                                                       : MQTT_RETRY_INTERVAL_SLOW;
    bool dueForRetry = !mqttConnected && (millis() - lastMqttAttempt >= retryInterval);

    if (mqttConnected)
    {
      webSocket.loop();
      mqtt.update();
    }

    if (pendingOtaUrl.length() > 0)
    {
      String url = pendingOtaUrl;
      pendingOtaUrl = "";

      // Clear the retained message on the broker before starting the download
      // so the device does not re-trigger OTA with the stale URL after reboot
      mqtt.publish("hilight/" + deviceId + "/ota", (uint8_t *)"", 0, true, 0);

      // Only flash images signed with our key. The signature is checked after the
      // full download and before the boot partition is switched, so a bad image
      // is discarded and the running firmware keeps booting.
      static UpdaterECDSAVerifier otaVerifier((const uint8_t *)OTA_PUBLIC_KEY,
                                              sizeof(OTA_PUBLIC_KEY)); // incl. NUL for PEM parse
      if (!Update.installSignature(&otaVerifier))
        return;

      LedMode preOtaMode = ledMode;
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
      otaClient.setCACert(GTS_ROOT_CA);
      httpUpdate.update(otaClient, url);

      // HTTP_UPDATE_OK reboots and never gets here, so reaching this point means the
      // update failed or was rejected (e.g. bad signature). Failures are deliberately
      // silent: put the lamp back the way it was instead of flashing an error at users.
      endOTAAnim(preOtaMode);
      return;
    }

    bool nowConnected = mqtt.isConnected();
    if (nowConnected && !mqttConnected)
    {
      DBG_PRINTLN("[MQTT] Reconnected");
      mqttConnected = true;
      mqttRetryCount = 0;
      onMqttConnect();
    }
    else if (!nowConnected)
    {
      if (mqttConnected)
        DBG_PRINTLN("[MQTT] Disconnected");
      mqttConnected = false;
      if (dueForRetry)
      {
        lastMqttAttempt = millis();
        if (mqttRetryCount < MQTT_MAX_RETRIES)
          mqttRetryCount++;
        DBG_PRINTF("[MQTT] Retry %d/%d\n", mqttRetryCount, MQTT_MAX_RETRIES);

        mqtt.connect(deviceId, MQTT_USERNAME, MQTT_PASSWORD);
      }
    }
  }
  else if (millis() - lastWifiAttempt >= WIFI_RETRY_INTERVAL)
  {
    DBG_PRINTF("[WiFi] Retrying SSID: %s\n", wifiSSID.c_str());
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
  const char *payload = (ledMode == LED_CCT) ? "ON" : "OFF";
  mqtt.publish(stateTopic, (uint8_t *)payload, strlen(payload), true, 0);
}

void publishBrightnessState()
{
  if (!mqtt.isConnected())
    return;
  String stateTopic = "hilight/" + deviceId + "/brightness/state";
  int haValue = map(brightnessLUT[constrain(brightnessPos, 0, ENCODER_MAX_POS)], brightnessLUT[0],
                     brightnessLUT[ENCODER_MAX_POS], 1, 255);
  String payload = String(haValue);
  mqtt.publish(stateTopic, (uint8_t *)payload.c_str(), payload.length(), true, 0);
}

void publishCCTState()
{
  if (!mqtt.isConnected())
    return;
  String stateTopic = "hilight/" + deviceId + "/color_temp/state";
  int mireds = map(cctPos, 0, ENCODER_MAX_POS, MAX_MIREDS, MIN_MIREDS);
  String payload = String(mireds);
  mqtt.publish(stateTopic, (uint8_t *)payload.c_str(), payload.length(), true, 0);
}
