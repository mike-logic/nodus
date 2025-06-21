// ===================
// 🛰️ Nodus Node Sketch
// ===================

#include <Arduino.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <esp_now.h>
#include <Preferences.h>
#include <NimBLEDevice.h>
#include <ArduinoJson.h>
#include <vector>

Preferences prefs;
String node_id;

uint8_t broadcastPeer[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

struct Detection {
  String mac;
  String protocol;
  int rssi;
  unsigned long timestamp;
};

std::vector<Detection> recentDetections;
unsigned long lastHeartbeat = 0;
unsigned long lastScan = 0;
unsigned long lastReport = 0;

String generateRandomNodeID() {
  uint32_t r = esp_random();
  char id[16];
  snprintf(id, sizeof(id), "node_%06X", r & 0xFFFFFF);
  return String(id);
}

void loadOrGenerateNodeID() {
  prefs.begin("nodus", false);
  node_id = prefs.getString("node_id", "");
  if (node_id == "") {
    node_id = generateRandomNodeID();
    prefs.putString("node_id", node_id);
  }
  prefs.end();
}

// ==== BLE SCANNING ====

class AdvertisedDeviceCallbacks : public NimBLEAdvertisedDeviceCallbacks {
  void onResult(NimBLEAdvertisedDevice* device) override {
    Detection d;
    d.mac = device->getAddress().toString().c_str();
    d.protocol = "BLE";
    d.rssi = device->getRSSI();
    d.timestamp = millis();
    recentDetections.push_back(d);
  }
};

void startBLEScan() {
  NimBLEScan* scan = NimBLEDevice::getScan();
  scan->setAdvertisedDeviceCallbacks(new AdvertisedDeviceCallbacks(), false);
  scan->setActiveScan(false);
  scan->setInterval(45);
  scan->setWindow(15);
  scan->start(3, false);
}

// ==== WIFI SCANNING ====

void onWiFiScanDone(int n) {
  if (n <= 0) return;
  wifi_ap_record_t results[n];
  uint16_t count = n;
  if (esp_wifi_scan_get_ap_records(&count, results) == ESP_OK) {
    for (int i = 0; i < count; i++) {
      Detection d;
      char macStr[18];
      snprintf(macStr, sizeof(macStr), "%02x:%02x:%02x:%02x:%02x:%02x",
               results[i].bssid[0], results[i].bssid[1], results[i].bssid[2],
               results[i].bssid[3], results[i].bssid[4], results[i].bssid[5]);
      d.mac = String(macStr);
      d.protocol = "WiFi";
      d.rssi = results[i].rssi;
      d.timestamp = millis();
      recentDetections.push_back(d);
    }
  }
}

void startWiFiScan() {
  WiFi.scanNetworks(true, true);
}

// ==== GATEWAY DISCOVERY ====

int findBaseChannel() {
  Serial.println("📡 Scanning for gateway...");
  int networks = WiFi.scanNetworks();
  for (int i = 0; i < networks; i++) {
    String ssid = WiFi.SSID(i);
    if (ssid.startsWith("nodus-ch")) {
      int ch = ssid.substring(8).toInt();
      Serial.printf("🔗 Found channel %d\n", ch);
      return ch;
    }
  }
  Serial.println("❌ No gateway found.");
  return -1;
}

// ==== ESP-NOW ====

void onDataSent(const uint8_t *mac, esp_now_send_status_t status) {
  Serial.print("📤 ESP-NOW send status: ");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "✅ Success" : "❌ Fail");
}

void setupESPNow(int ch) {
  esp_wifi_set_channel(ch, WIFI_SECOND_CHAN_NONE);
  if (esp_now_init() != ESP_OK) {
    Serial.println("❌ ESP-NOW init failed");
    return;
  }

  esp_now_peer_info_t peer = {};
  memcpy(peer.peer_addr, broadcastPeer, 6);
  peer.channel = ch;
  peer.encrypt = false;
  esp_now_add_peer(&peer);

  esp_now_register_send_cb(onDataSent);
}

// ==== SENDING DATA ====

void sendHeartbeat() {
  StaticJsonDocument<256> doc;
  doc["type"] = "heartbeat";
  doc["node_id"] = node_id;
  doc["uptime"] = millis();

  char out[256];
  size_t len = serializeJson(doc, out);
  esp_now_send(broadcastPeer, (uint8_t*)out, len);

  Serial.println("📤 Sent heartbeat");
}

void sendDetections() {
  for (auto& d : recentDetections) {
    StaticJsonDocument<256> doc;
    doc["type"] = "detection";
    doc["mac"] = d.mac;
    doc["rssi"] = d.rssi;
    doc["protocol"] = d.protocol;
    doc["timestamp"] = d.timestamp;
    doc["node_id"] = node_id;

    char out[256];
    size_t len = serializeJson(doc, out);
    esp_now_send(broadcastPeer, (uint8_t*)out, len);
  }
  recentDetections.clear();
}

// ==== SETUP ====

void setup() {
  Serial.begin(115200);
  delay(200);

  loadOrGenerateNodeID();
  Serial.printf("📍 Node ID: %s\n", node_id.c_str());

  WiFi.mode(WIFI_STA);
  WiFi.disconnect(true);
  delay(100);

  int ch = findBaseChannel();
  if (ch < 1 || ch > 13) {
    Serial.println("❌ Channel not found — restarting...");
    delay(2000);
    ESP.restart();
  }

  setupESPNow(ch);

  NimBLEDevice::init("");
  startBLEScan();
  startWiFiScan();
}

// ==== LOOP ====

void loop() {
  if (WiFi.scanComplete() >= 0) {
    onWiFiScanDone(WiFi.scanComplete());
    WiFi.scanDelete();
  }

  if (millis() - lastHeartbeat > 10000) {
    sendHeartbeat();
    lastHeartbeat = millis();
  }

  if (millis() - lastReport > 5000) {
    sendDetections();
    lastReport = millis();
  }

  if (millis() - lastScan > 5000) {
    startBLEScan();
    startWiFiScan();
    lastScan = millis();
  }
}
