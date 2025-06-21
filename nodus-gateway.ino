#include <WiFi.h>
#include <WiFiManager.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#include <Preferences.h>

WebServer server(80);
Preferences prefs;

String mqtt_host = "";
int mqtt_port = 1883;
String mqtt_topic = "";

uint8_t broadcastPeer[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

// === Load & Save MQTT Config ===

void loadMQTTConfig() {
  prefs.begin("mqtt", true);
  mqtt_host = prefs.getString("host", "broker.local");
  mqtt_port = prefs.getInt("port", 1883);
  mqtt_topic = prefs.getString("topic", "nodus/data");
  prefs.end();
}

void saveMQTTConfig(const String& host, int port, const String& topic) {
  prefs.begin("mqtt", false);
  prefs.putString("host", host);
  prefs.putInt("port", port);
  prefs.putString("topic", topic);
  prefs.end();
}

// === ESP-NOW Receiver ===

void onDataRecv(const uint8_t *mac, const uint8_t *incomingData, int len) {
  Serial.print("📥 ESP-NOW data from ");
  char macStr[18];
  snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X",
           mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  Serial.println(macStr);

  StaticJsonDocument<256> doc;
  DeserializationError err = deserializeJson(doc, incomingData, len);
  if (err) {
    Serial.println("❌ Failed to parse JSON");
    return;
  }

  serializeJsonPretty(doc, Serial);
  Serial.println();
}

// === HTTP Dashboard ===

void handleRoot() {
  String html = R"HTML(
    <html><head><title>Nodus Gateway</title></head><body>
    <h1>📡 Nodus Gateway Dashboard</h1>
    <p>Connected to: )HTML";

  html += WiFi.SSID();
  html += " (channel " + String(WiFi.channel()) + ")</p>";

  html += R"HTML(
    <form action="/mqtt" method="POST">
      <label>MQTT Host: <input name="host" value=")HTML" + mqtt_host + R"HTML("></label><br>
      <label>MQTT Port: <input name="port" type="number" value=")HTML" + String(mqtt_port) + R"HTML("></label><br>
      <label>MQTT Topic: <input name="topic" value=")HTML" + mqtt_topic + R"HTML("></label><br>
      <input type="submit" value="💾 Save MQTT Settings" style="margin-top:10px;">
    </form>

    <form action="/reset" method="POST" style="margin-top:20px;">
      <input type="submit" value="🔄 Reset WiFi Config & Reboot" style="font-size:18px;">
    </form>
    </body></html>
  )HTML";

  server.send(200, "text/html", html);
}

void handleMQTTSave() {
  if (server.hasArg("host") && server.hasArg("port") && server.hasArg("topic")) {
    mqtt_host = server.arg("host");
    mqtt_port = server.arg("port").toInt();
    mqtt_topic = server.arg("topic");

    saveMQTTConfig(mqtt_host, mqtt_port, mqtt_topic);
    Serial.println("✅ MQTT settings updated:");
    Serial.println("Host: " + mqtt_host);
    Serial.println("Port: " + String(mqtt_port));
    Serial.println("Topic: " + mqtt_topic);

    server.sendHeader("Location", "/", true);
    server.send(302, "text/plain", "");
  } else {
    server.send(400, "text/plain", "Missing parameters");
  }
}

void handleReset() {
  server.send(200, "text/html", "<p>🔄 Resetting config... Rebooting.</p>");
  delay(1000);
  WiFi.disconnect(true, true); // erase credentials
  ESP.restart();
}

void setupWebServer() {
  server.on("/", HTTP_GET, handleRoot);
  server.on("/mqtt", HTTP_POST, handleMQTTSave);
  server.on("/reset", HTTP_POST, handleReset);
  server.begin();
  Serial.println("🌐 Web server started");
}

// === SoftAP Setup ===

void setupSoftAPWithChannel(uint8_t channel) {
  String ssid = "nodus-ch" + String(channel);
  const char* password = "nodus123";
  WiFi.softAP(ssid.c_str(), password, channel);
  Serial.printf("📡 SoftAP started: %s (channel %d)\n", ssid.c_str(), channel);
}

// === ESP-NOW Setup ===

void setupESPNOW(uint8_t channel) {
  esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE);

  if (esp_now_init() != ESP_OK) {
    Serial.println("❌ ESP-NOW init failed");
    return;
  }

  esp_now_peer_info_t peer = {};
  memcpy(peer.peer_addr, broadcastPeer, 6);
  peer.channel = channel;
  peer.encrypt = false;
  esp_now_add_peer(&peer);

  esp_now_register_recv_cb(onDataRecv);

  Serial.println("✅ ESP-NOW initialized");
}

// === MAIN ===

void setup() {
  Serial.begin(115200);
  delay(500);

  WiFi.mode(WIFI_AP_STA);
  WiFiManager wm;
  wm.setConfigPortalTimeout(120);

  if (!wm.autoConnect("nodus-gateway")) {
    Serial.println("❌ WiFiManager failed — restarting");
    ESP.restart();
  }

  Serial.println("✅ Connected to Wi-Fi");

  uint8_t channel;
  wifi_second_chan_t dummy;
  esp_wifi_get_channel(&channel, &dummy);

  Serial.printf("📶 WiFi STA channel: %d\n", channel);

  setupSoftAPWithChannel(channel);
  setupESPNOW(channel);
  loadMQTTConfig();
  setupWebServer();
}

void loop() {
  server.handleClient();
}
