# 📡 Nodus Passive Tracking System

Nodus is a modular ESP32-based passive tracking system designed to detect and localize nearby mobile devices using Wi-Fi and BLE scanning. It consists of two main components:

- **Gateway**: A central base station that connects to Wi-Fi, receives ESP-NOW data from nodes, and relays it to an MQTT server or web dashboard.
- **Node**: A low-power device that passively scans for nearby phones and reports sightings via ESP-NOW to the gateway.

---

## 🚀 Project Overview

The Nodus system is built to track presence, movement, and approximate location of nearby devices in physical spaces like buildings, venues, or events — all without requiring any app install or active pairing.

Nodes and gateways form a local mesh over ESP-NOW, making the system highly resilient and requiring minimal infrastructure. The gateway provides a web-based dashboard for viewing real-time device check-ins, and optionally relays data upstream via MQTT.

---

## ✅ Current Features

### Gateway
- SoftAP on boot for WiFiManager config (SSID, MQTT broker, etc.)
- Hosts a local dashboard showing connected devices and their last heartbeat
- Listens for ESP-NOW packets from nodes and optionally from hubs
- Displays device role (node or hub) in dashboard logs
- Supports reset and reconfiguration via web interface

### Node
- Scans for a `nodus-gateway` SoftAP to determine Wi-Fi channel
- Initializes ESP-NOW on the correct channel after scanning
- Broadcasts heartbeat packets with node ID
- Scans for BLE and Wi-Fi devices (planned)
- Sends passive detection data to the gateway via ESP-NOW (in development)

---

## 🔧 In Development

- [ ] Node-side passive device scanning (BLE, Wi-Fi)
- [ ] ESP-NOW packet structure for tracking payloads
- [ ] Dashboard real-time updates via WebSocket
- [ ] Node triangulation and heatmap support
- [ ] MQTT relaying for backend analytics
- [ ] Auto-pairing and role-based behavior (hub, node, gateway)

---

## 📂 Repository Structure

```
/gateway/       # ESP32 sketch for gateway/base device
/node/          # ESP32 sketch for passive scanning node
/dashboard/     # Embedded HTML/JS for the gateway web UI
/docs/          # Project documentation and planning notes
```

---

## 🛠️ Getting Started

Flash either sketch to an ESP32 dev board:

- Gateway: use ESP32-WROOM with WiFi + ESP-NOW support
- Node: use low-power ESP32 or S3 module

Ensure `WiFiManager` is available in Arduino/PlatformIO environment.

---

## 📅 Project Status

> Currently working on: **Node scanning and data reporting logic**

- Gateway is functional and provides a configurable dashboard
- Node can determine correct Wi-Fi channel and heartbeat to gateway
- BLE/Wi-Fi scanning and device fingerprinting in progress

---

## ✍️ License

MIT License