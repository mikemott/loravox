#include <Arduino.h>
#include <RadioLib.h>
#include <Wire.h>
#include <SSD1306Wire.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <PubSubClient.h>
#include <ESPmDNS.h>
#include "Packet.h"
#include "Types.h"
#include "BLEProtocol.h"

using namespace lorapnp;

#define LED_PIN   37
#define LORA_CS     7
#define LORA_DIO0   9
#define LORA_RST    8
#define LORA_DIO1   33
#define LORA_BUSY   34
#define LORA_SCK    5
#define LORA_MISO   3
#define LORA_MOSI   6

#define I2C_SDA   18
#define I2C_SCL   17

#define LORA_FREQ   915.0
#define LORA_BW     125.0
#define LORA_SF     7
#define LORA_CR     5
#define LORA_SYNC   0x12
#define LORA_PWR    14

constexpr uint8_t MAX_SENSORS_PER_NODE = 8;
constexpr uint8_t SENSOR_LABEL_LEN = 16;

#ifdef LORA_USE_SX1262
  Module radioModule(LORA_CS, LORA_DIO1, LORA_RST, LORA_BUSY, SPI);
  SX1262 radio(&radioModule);
#else
  Module radioModule(LORA_CS, LORA_DIO0, LORA_RST, LORA_DIO1, SPI);
  SX1276 radio(&radioModule);
#endif
SSD1306Wire display(0x3c, I2C_SDA, I2C_SCL);
WebServer webServer(80);
Preferences prefs;
WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);
bool mqtt_enabled = false;
char mqtt_broker[64] = "";
uint16_t mqtt_port = 1883;

// WiFi configuration
bool wifi_sta_enabled = false;
char wifi_ssid[32] = "";
char wifi_password[64] = "";
bool wifi_connected = false;

// Radio watchdog (resiliency)
uint32_t radio_consecutive_failures = 0;
uint32_t radio_last_success_ms = 0;
uint32_t radio_reset_count = 0;
constexpr uint32_t RADIO_FAILURE_THRESHOLD = 5;
constexpr uint32_t RADIO_STUCK_TIMEOUT_MS = 300000; // 5 minutes

// Stale node detection (Phase 2)
constexpr uint32_t NODE_STALE_TIMEOUT_MS = 300000; // 5 minutes
uint32_t stale_node_cleanups = 0;

struct NodeInfo {
  bool used;
  bool active;
  uint16_t node_id;
  uint8_t device_uid[6];
  uint16_t session_token;
  uint32_t last_seen;
  int16_t rssi;
  float snr;
  float battery_v;
  uint32_t uptime_s;
  uint32_t free_heap;
  uint8_t status_flags;
  uint16_t last_seq;
  uint32_t join_time;
  bool join_ack_pending;
  uint16_t join_accept_seq;
  uint32_t join_accept_sent_ms;
  uint8_t join_accept_retries;
  uint8_t sensor_count;
  uint8_t sensor_ids[MAX_SENSORS_PER_NODE];
  uint8_t sensor_types[MAX_SENSORS_PER_NODE];
  uint8_t sensor_units[MAX_SENSORS_PER_NODE];
  char sensor_labels[MAX_SENSORS_PER_NODE][SENSOR_LABEL_LEN + 1];
  bool config_ack_pending;
  uint16_t config_update_seq;
  uint32_t config_update_sent_ms;
  uint8_t config_update_retries;
  uint32_t pending_report_interval_ms;
  uint32_t pending_sleep_interval_ms;
  uint8_t pending_sf;
  uint8_t pending_tx_power;
};

NodeInfo nodes[32];
uint16_t next_node_id = 2;
uint32_t last_registry_print = 0;
uint32_t last_display_update_ms = 0;
uint32_t gateway_id = 0x0001;
uint16_t tx_seq = 0;

// BLE Remote Sensor tracking
BLERemoteSensorInfo ble_remote_sensors[MAX_BLE_SENSORS_TOTAL];
uint8_t ble_remote_sensor_count = 0;

// Custom name storage
String getNodeName(uint16_t node_id) {
  prefs.begin("lorapnp", true);
  String name = prefs.getString(("name_n" + String(node_id)).c_str(), "");
  prefs.end();
  return name.length() > 0 ? name : ("Node " + String(node_id));
}

void setNodeName(uint16_t node_id, const String& name) {
  prefs.begin("lorapnp", false);
  prefs.putString(("name_n" + String(node_id)).c_str(), name);
  prefs.end();
  Serial.printf("Node %u renamed to: %s\n", node_id, name.c_str());
}

String getSensorName(uint16_t node_id, uint8_t sensor_id, const char* default_label) {
  prefs.begin("lorapnp", true);
  String key = "name_s" + String(node_id) + "_" + String(sensor_id);
  String name = prefs.getString(key.c_str(), "");
  prefs.end();
  return name.length() > 0 ? name : String(default_label);
}

void setSensorName(uint16_t node_id, uint8_t sensor_id, const String& name) {
  prefs.begin("lorapnp", false);
  String key = "name_s" + String(node_id) + "_" + String(sensor_id);
  prefs.putString(key.c_str(), name);
  prefs.end();
  Serial.printf("Node %u sensor %u renamed to: %s\n", node_id, sensor_id, name.c_str());
}

String getBLESensorName(const uint8_t* uid) {
  char uid_str[13];
  snprintf(uid_str, sizeof(uid_str), "%02X%02X%02X%02X%02X%02X",
           uid[0], uid[1], uid[2], uid[3], uid[4], uid[5]);
  prefs.begin("lorapnp", true);
  String key = "name_b" + String(uid_str);
  String name = prefs.getString(key.c_str(), "");
  prefs.end();
  return name.length() > 0 ? name : String(uid_str).substring(0, 8);
}

void setBLESensorName(const uint8_t* uid, const String& name) {
  char uid_str[13];
  snprintf(uid_str, sizeof(uid_str), "%02X%02X%02X%02X%02X%02X",
           uid[0], uid[1], uid[2], uid[3], uid[4], uid[5]);
  prefs.begin("lorapnp", false);
  String key = "name_b" + String(uid_str);
  prefs.putString(key.c_str(), name);
  prefs.end();
  Serial.printf("BLE sensor %s renamed to: %s\n", uid_str, name.c_str());
}

// Forward declarations
int findActiveNodeById(uint16_t node_id);
bool sendPacket(const Packet& pkt);
void sendStationNameUpdate(uint16_t station_id, const char* name);
void sendSensorNameUpdate(uint16_t station_id, uint8_t sensor_id, const char* name);
void sendRemoteSensorNameUpdate(uint16_t station_id, const uint8_t* ble_uid, const char* name);
void recordRadioSuccess();
void recordRadioFailure();
bool radioNeedsReset();
void resetRadio();

String uidKey(const uint8_t* uid) {
  char key[14];
  snprintf(key, sizeof(key), "n%02X%02X%02X%02X%02X%02X",
           uid[0], uid[1], uid[2], uid[3], uid[4], uid[5]);
  return String(key);
}

uint16_t loadNodeIdForUID(const uint8_t* uid) {
  prefs.begin("lorapnp", true);
  uint16_t node_id = prefs.getUShort(uidKey(uid).c_str(), UNASSIGNED_ID);
  prefs.end();
  return node_id;
}

void saveNodeIdForUID(const uint8_t* uid, uint16_t node_id) {
  prefs.begin("lorapnp", false);
  prefs.putUShort(uidKey(uid).c_str(), node_id);
  prefs.end();
}

void loadPersistedRegistry() {
  prefs.begin("lorapnp", true);
  size_t keys_len = prefs.freeEntries();
  prefs.end();

  prefs.begin("lorapnp", true);
  uint16_t max_node_id = 1;

  for (int i = 0; i < 32; i++) {
    nodes[i].used = false;
    nodes[i].active = false;
  }

  Serial.println("Loading persisted node mappings...");
  String test_keys[] = {"n", ""};

  prefs.end();
  Serial.println("Registry will rebuild from STATUS packets");
}

void bumpNextNodeId(uint16_t node_id) {
  if (node_id >= next_node_id) {
    next_node_id = node_id + 1;
  }
}

void updateDisplay(const char* line1, const char* line2 = "", const char* line3 = "") {
  display.clear();
  display.drawString(0, 0, line1);
  display.drawString(0, 20, line2);
  display.drawString(0, 40, line3);
  display.display();
}

void updateStatusDisplay() {
  char line1[32], line2[32], line3[32];

  // Count active nodes
  uint8_t active_nodes = 0;
  for (int i = 0; i < 32; i++) {
    if (nodes[i].active) active_nodes++;
  }

  // Count total sensors (from nodes + BLE sensors)
  uint16_t total_sensors = 0;
  for (int i = 0; i < 32; i++) {
    if (nodes[i].active) total_sensors += nodes[i].sensor_count;
  }
  total_sensors += ble_remote_sensor_count;

  // Line 1: Gateway with node count
  snprintf(line1, sizeof(line1), "Gateway: %u nodes", active_nodes);

  // Line 2: Total sensors
  snprintf(line2, sizeof(line2), "Sensors: %u", total_sensors);

  // Line 3: Status
  if (active_nodes > 0) {
    snprintf(line3, sizeof(line3), "Listening...");
  } else {
    snprintf(line3, sizeof(line3), "Waiting for nodes");
  }

  updateDisplay(line1, line2, line3);
}

void handleRoot() {
  StaticJsonDocument<4096> doc;
  doc["gateway_id"] = gateway_id;
  doc["active_nodes"] = 0;
  doc["active_ble_sensors"] = ble_remote_sensor_count;
  doc["wifi_configured"] = wifi_sta_enabled;
  doc["wifi_connected"] = wifi_connected;
  if (wifi_sta_enabled) {
    doc["wifi_ssid"] = wifi_ssid;
  }
  if (wifi_connected) {
    doc["wifi_ip"] = WiFi.localIP().toString();
  }

  JsonArray arr = doc.createNestedArray("nodes");
  for (int i = 0; i < 32; i++) {
    if (nodes[i].active) {
      JsonObject n = arr.createNestedObject();
      n["id"] = nodes[i].node_id;
      n["name"] = getNodeName(nodes[i].node_id);
      n["type"] = "node";
      n["rssi"] = nodes[i].rssi;
      n["snr"] = nodes[i].snr;
      n["battery"] = nodes[i].battery_v;
      n["uptime_s"] = nodes[i].uptime_s;
      n["free_heap"] = nodes[i].free_heap;
      n["status_flags"] = nodes[i].status_flags;
      n["last_seen"] = (millis() - nodes[i].last_seen) / 1000;

      JsonArray sensors = n.createNestedArray("sensors");
      for (int s = 0; s < nodes[i].sensor_count; s++) {
        JsonObject sensor = sensors.createNestedObject();
        sensor["id"] = nodes[i].sensor_ids[s];
        sensor["type"] = nodes[i].sensor_types[s];
        sensor["unit"] = nodes[i].sensor_units[s];
        sensor["label"] = nodes[i].sensor_labels[s];
        sensor["custom_name"] = getSensorName(nodes[i].node_id, nodes[i].sensor_ids[s], nodes[i].sensor_labels[s]);
      }

      // Add BLE sensors forwarded by this node (including offline ones)
      JsonArray ble_sensors_arr = n.createNestedArray("ble_sensors");
      for (uint8_t b = 0; b < ble_remote_sensor_count; b++) {
        if (ble_remote_sensors[b].active &&
            ble_remote_sensors[b].parent_node_id == nodes[i].node_id) {
          JsonObject ble = ble_sensors_arr.createNestedObject();

          char uid_str[18];
          formatBLEUID(ble_remote_sensors[b].device_uid, uid_str, sizeof(uid_str));
          ble["uid"] = uid_str;
          ble["name"] = getBLESensorName(ble_remote_sensors[b].device_uid);
          ble["rssi"] = ble_remote_sensors[b].rssi;
          ble["battery_mv"] = ble_remote_sensors[b].battery_mv;

          uint32_t last_seen_seconds = (millis() - ble_remote_sensors[b].last_seen) / 1000;
          ble["last_seen"] = last_seen_seconds;
          ble["offline"] = (last_seen_seconds > 60); // Mark as offline if not seen for 60+ seconds

          JsonArray ble_sensor_arr = ble.createNestedArray("sensors");
          for (uint8_t s = 0; s < ble_remote_sensors[b].sensor_count; s++) {
            JsonObject sensor = ble_sensor_arr.createNestedObject();
            sensor["id"] = ble_remote_sensors[b].sensor_ids[s];
            sensor["type"] = ble_remote_sensors[b].sensor_types[s];
            sensor["unit"] = ble_remote_sensors[b].sensor_units[s];
            sensor["value"] = ble_remote_sensors[b].last_values[s];
          }
        }
      }

      doc["active_nodes"] = doc["active_nodes"].as<int>() + 1;
    }
  }

  // Add health metrics (Phase 2)
  JsonObject health = doc.createNestedObject("health");
  health["uptime_s"] = millis() / 1000;
  health["radio_resets"] = radio_reset_count;
  health["stale_cleanups"] = stale_node_cleanups;
  health["radio_failures"] = radio_consecutive_failures;

  String json;
  serializeJson(doc, json);
  webServer.send(200, "application/json", json);
}

void handleDashboard() {
  const char* html = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>LoRa PNP Gateway</title>
  <style>
    * { margin: 0; padding: 0; box-sizing: border-box; }
    body {
      font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif;
      background: #0f1419;
      color: #e1e8ed;
      padding: 16px;
      line-height: 1.5;
    }
    .header {
      background: linear-gradient(135deg, #1da1f2 0%, #0d8bd9 100%);
      padding: 20px;
      border-radius: 12px;
      margin-bottom: 20px;
      box-shadow: 0 4px 6px rgba(0,0,0,0.3);
    }
    .header h1 { font-size: 24px; font-weight: 600; }
    .header .stats {
      display: flex;
      gap: 20px;
      margin-top: 12px;
      flex-wrap: wrap;
    }
    .stat { display: flex; flex-direction: column; }
    .stat-label { font-size: 12px; opacity: 0.9; }
    .stat-value { font-size: 20px; font-weight: 700; }

    .card {
      background: #192734;
      border-radius: 12px;
      padding: 16px;
      margin-bottom: 16px;
      border: 1px solid #38444d;
    }
    .card-header {
      display: flex;
      justify-content: space-between;
      align-items: center;
      margin-bottom: 12px;
      padding-bottom: 12px;
      border-bottom: 1px solid #38444d;
    }
    .card-title {
      font-size: 16px;
      font-weight: 600;
      display: flex;
      align-items: center;
      gap: 8px;
    }
    .badge {
      background: #1da1f2;
      color: white;
      padding: 2px 8px;
      border-radius: 12px;
      font-size: 12px;
      font-weight: 600;
    }
    .badge.station { background: #17bf63; }
    .badge.remote { background: #f45d22; }

    .signal {
      display: flex;
      align-items: center;
      gap: 6px;
      font-size: 13px;
    }
    .signal-bar {
      width: 40px;
      height: 8px;
      background: #38444d;
      border-radius: 4px;
      overflow: hidden;
    }
    .signal-fill {
      height: 100%;
      transition: width 0.3s;
    }
    .signal-good { background: #17bf63; }
    .signal-fair { background: #ffad1f; }
    .signal-poor { background: #e0245e; }

    .metrics {
      display: grid;
      grid-template-columns: repeat(auto-fit, minmax(100px, 1fr));
      gap: 12px;
      margin-top: 12px;
    }
    .metric {
      display: flex;
      flex-direction: column;
    }
    .metric-label {
      font-size: 11px;
      color: #8899a6;
      text-transform: uppercase;
      letter-spacing: 0.5px;
    }
    .metric-value {
      font-size: 16px;
      font-weight: 600;
      margin-top: 4px;
    }

    .sensors {
      display: grid;
      gap: 8px;
      margin-top: 12px;
    }
    .sensor {
      background: #0f1419;
      padding: 10px;
      border-radius: 8px;
      display: flex;
      justify-content: space-between;
      align-items: center;
    }
    .sensor-name { font-size: 13px; color: #8899a6; }
    .sensor-value {
      font-size: 18px;
      font-weight: 600;
      color: #1da1f2;
    }

    .timestamp {
      font-size: 11px;
      color: #657786;
      margin-top: 8px;
    }

    .offline {
      opacity: 0.5;
    }
    .offline .badge {
      background: #657786;
    }

    .loading {
      text-align: center;
      padding: 40px;
      color: #657786;
    }

    .btn {
      background: #1da1f2;
      color: white;
      border: none;
      padding: 10px 20px;
      border-radius: 8px;
      font-size: 14px;
      font-weight: 600;
      cursor: pointer;
      transition: background 0.2s;
    }
    .btn:hover { background: #1a91da; }
    .btn-secondary {
      background: #38444d;
      color: #e1e8ed;
    }
    .btn-secondary:hover { background: #4a5661; }
    .btn-reboot {
      background: #ff6b6b;
      color: white;
      border: none;
      padding: 6px 12px;
      border-radius: 6px;
      font-size: 12px;
      font-weight: 600;
      cursor: pointer;
      transition: background 0.2s;
      margin-top: 12px;
      width: 100%;
    }
    .btn-reboot:hover { background: #ff5252; }
    .btn-reboot:active { background: #e04848; }

    .modal {
      display: none;
      position: fixed;
      top: 0;
      left: 0;
      width: 100%;
      height: 100%;
      background: rgba(0,0,0,0.8);
      z-index: 1000;
      align-items: center;
      justify-content: center;
    }
    .modal.show { display: flex; }
    .modal-content {
      background: #192734;
      border-radius: 12px;
      padding: 24px;
      max-width: 500px;
      width: 90%;
      border: 1px solid #38444d;
    }
    .modal-header {
      font-size: 20px;
      font-weight: 600;
      margin-bottom: 20px;
    }
    .form-group {
      margin-bottom: 16px;
    }
    .form-group label {
      display: block;
      font-size: 13px;
      color: #8899a6;
      margin-bottom: 6px;
    }
    .form-group input {
      width: 100%;
      padding: 10px;
      background: #0f1419;
      border: 1px solid #38444d;
      border-radius: 6px;
      color: #e1e8ed;
      font-size: 14px;
    }
    .form-group input:focus {
      outline: none;
      border-color: #1da1f2;
    }
    .form-actions {
      display: flex;
      gap: 12px;
      margin-top: 20px;
    }
    .wifi-status {
      font-size: 12px;
      color: #8899a6;
      margin-top: 8px;
    }
    .wifi-status.connected { color: #17bf63; }

    @media (max-width: 600px) {
      .header .stats { gap: 12px; }
      .metrics { grid-template-columns: repeat(2, 1fr); }
    }
  </style>
</head>
<body>
  <div class="header">
    <div style="display: flex; justify-content: space-between; align-items: center; margin-bottom: 12px;">
      <h1>🛰️ LoRa PNP Gateway</h1>
      <button class="btn" onclick="showWifiModal()">⚙️ WiFi</button>
    </div>
    <div class="stats">
      <div class="stat">
        <span class="stat-label">Stations</span>
        <span class="stat-value" id="node-count">-</span>
      </div>
      <div class="stat">
        <span class="stat-label">Remote Sensors</span>
        <span class="stat-value" id="ble-count">-</span>
      </div>
      <div class="stat">
        <span class="stat-label">Total Sensors</span>
        <span class="stat-value" id="sensor-count">-</span>
      </div>
    </div>
    <div id="wifi-status" class="wifi-status"></div>
  </div>

  <div class="header" style="margin-top: 16px;">
    <h2 style="margin: 0 0 12px 0; font-size: 18px;">📊 Health Metrics</h2>
    <div class="stats">
      <div class="stat">
        <span class="stat-label">Uptime</span>
        <span class="stat-value" id="uptime">-</span>
      </div>
      <div class="stat">
        <span class="stat-label">Radio Resets</span>
        <span class="stat-value" id="radio-resets">0</span>
      </div>
      <div class="stat">
        <span class="stat-label">Stale Cleanups</span>
        <span class="stat-value" id="stale-cleanups">0</span>
      </div>
    </div>
  </div>

  <div id="wifi-modal" class="modal">
    <div class="modal-content">
      <div class="modal-header">WiFi Settings</div>
      <form id="wifi-form" onsubmit="saveWifi(event)">
        <div class="form-group">
          <label for="ssid">Network Name (SSID)</label>
          <input type="text" id="ssid" name="ssid" placeholder="Your WiFi network name" required>
        </div>
        <div class="form-group">
          <label for="password">Password</label>
          <input type="password" id="password" name="password" placeholder="WiFi password" required>
        </div>
        <div class="wifi-status">
          Gateway will restart after saving. You can access it at <strong>http://lorapnp.local</strong> or via its new IP address on your network.
        </div>
        <div class="form-actions">
          <button type="submit" class="btn">Save & Restart</button>
          <button type="button" class="btn btn-secondary" onclick="hideWifiModal()">Cancel</button>
        </div>
      </form>
    </div>
  </div>

  <div id="nodes-container" class="loading">Loading...</div>

  <script>
    function getSignalClass(rssi) {
      if (rssi > -70) return 'signal-good';
      if (rssi > -85) return 'signal-fair';
      return 'signal-poor';
    }

    function getSignalPercent(rssi) {
      return Math.max(0, Math.min(100, (rssi + 100) * 2));
    }

    function formatUptime(seconds) {
      const h = Math.floor(seconds / 3600);
      const m = Math.floor((seconds % 3600) / 60);
      return h > 0 ? `${h}h ${m}m` : `${m}m`;
    }

    function getSensorUnit(unit) {
      const units = {
        0: '', 1: '°C', 2: '°F', 3: '%', 4: 'hPa', 5: 'lux',
        6: 'V', 7: 'mA', 8: 'ppm', 9: 'µg/m³'
      };
      return units[unit] || '';
    }

    function getSensorTypeName(type) {
      const types = {
        0: 'Unknown', 1: 'Temperature', 2: 'Humidity', 3: 'Pressure',
        4: 'Light', 5: 'Battery', 6: 'Soil Moisture', 7: 'CO2', 8: 'PM2.5'
      };
      return types[type] || `Sensor ${type}`;
    }

    function renameItem(type, params, currentName) {
      const newName = prompt(`Rename ${type}:`, currentName);
      if (!newName || newName === currentName) return;

      const payload = { type, name: newName, ...params };

      fetch('/api/rename', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(payload)
      })
      .then(r => r.json())
      .then(data => {
        if (data.success) {
          fetchData();
        } else {
          alert('Failed to rename: ' + (data.error || 'Unknown error'));
        }
      })
      .catch(e => alert('Error: ' + e.message));
    }

    function rebootStation(nodeId, nodeName) {
      if (!confirm(`Reboot ${nodeName}?\n\nStation will restart and re-detect all I2C sensors.\n\nThis is useful after plugging in new sensors.`)) {
        return;
      }

      fetch('/api/reboot', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ node_id: nodeId })
      })
      .then(r => r.json())
      .then(data => {
        if (data.success) {
          alert(`✓ Reboot command sent to ${nodeName}\n\nStation will restart in ~30 seconds and appear offline briefly.`);
        } else {
          alert('Failed to reboot: ' + (data.error || 'Unknown error'));
        }
      })
      .catch(e => alert('Error: ' + e.message));
    }

    function renderDashboard(data) {
      document.getElementById('node-count').textContent = data.active_nodes || 0;
      document.getElementById('ble-count').textContent = data.active_ble_sensors || 0;

      // Update health metrics
      if (data.health) {
        const uptime = data.health.uptime_s;
        const hours = Math.floor(uptime / 3600);
        const minutes = Math.floor((uptime % 3600) / 60);
        document.getElementById('uptime').textContent = hours > 0 ? `${hours}h ${minutes}m` : `${minutes}m`;
        document.getElementById('radio-resets').textContent = data.health.radio_resets || 0;
        document.getElementById('stale-cleanups').textContent = data.health.stale_cleanups || 0;
      }

      let totalSensors = 0;
      const container = document.getElementById('nodes-container');
      container.innerHTML = '';

      if (!data.nodes || data.nodes.length === 0) {
        container.innerHTML = '<div class="loading">No stations connected</div>';
        document.getElementById('sensor-count').textContent = '0';
        return;
      }

      data.nodes.forEach(node => {
        const isOffline = node.last_seen > 60;
        const signalClass = getSignalClass(node.rssi);
        const signalPercent = getSignalPercent(node.rssi);
        totalSensors += node.sensors.length;

        let sensorsHtml = '';
        node.sensors.forEach(s => {
          const sensorName = s.custom_name || s.label || getSensorTypeName(s.type);
          sensorsHtml += `
            <div class="sensor">
              <span class="sensor-name" style="cursor:pointer;"
                onclick="renameItem('sensor', {node_id: ${node.id}, sensor_id: ${s.id}}, '${sensorName}')"
                title="Click to rename">
                ${sensorName}
              </span>
              <span class="sensor-value">- ${getSensorUnit(s.unit)}</span>
            </div>`;
        });

        let bleSensorsHtml = '';
        if (node.ble_sensors && node.ble_sensors.length > 0) {
          node.ble_sensors.forEach(ble => {
            totalSensors += ble.sensors.length;
            const bleName = ble.name || ble.uid.substring(0,8);
            const offlineClass = ble.offline ? 'offline' : '';
            const offlineIndicator = ble.offline ? '⚠️ ' : '📡 ';
            const lastSeenText = ble.offline ? ` (offline ${ble.last_seen}s)` : '';

            ble.sensors.forEach(s => {
              bleSensorsHtml += `
                <div class="sensor ${offlineClass}">
                  <span class="sensor-name" style="cursor:pointer;"
                    onclick="renameItem('ble', {uid: '${ble.uid}'}, '${bleName}')"
                    title="Click to rename BLE sensor">
                    ${offlineIndicator}${getSensorTypeName(s.type)} (${bleName})${lastSeenText}
                  </span>
                  <span class="sensor-value">${s.value.toFixed(1)} ${getSensorUnit(s.unit)}</span>
                </div>`;
            });
          });
        }

        container.innerHTML += `
          <div class="card ${isOffline ? 'offline' : ''}">
            <div class="card-header">
              <div class="card-title">
                <span class="badge station" style="cursor:pointer;"
                  onclick="renameItem('node', {node_id: ${node.id}}, '${node.name || 'Station ' + node.id}')"
                  title="Click to rename station">
                  ${node.name || 'Station ' + node.id}
                </span>
                ${node.ble_sensors && node.ble_sensors.length > 0 ? `<span class="badge remote">${node.ble_sensors.length} Remote</span>` : ''}
              </div>
              <div class="signal">
                <span>${node.rssi} dBm</span>
                <div class="signal-bar">
                  <div class="signal-fill ${signalClass}" style="width: ${signalPercent}%"></div>
                </div>
              </div>
            </div>

            <div class="metrics">
              <div class="metric">
                <span class="metric-label">Battery</span>
                <span class="metric-value">${node.battery.toFixed(2)}V</span>
              </div>
              <div class="metric">
                <span class="metric-label">SNR</span>
                <span class="metric-value">${node.snr.toFixed(1)} dB</span>
              </div>
              <div class="metric">
                <span class="metric-label">Uptime</span>
                <span class="metric-value">${formatUptime(node.uptime_s)}</span>
              </div>
              <div class="metric">
                <span class="metric-label">Heap</span>
                <span class="metric-value">${(node.free_heap / 1024).toFixed(0)}KB</span>
              </div>
            </div>

            ${sensorsHtml || bleSensorsHtml ? '<div class="sensors">' + sensorsHtml + bleSensorsHtml + '</div>' : ''}

            <div class="timestamp">Last seen: ${node.last_seen}s ago</div>
            <button class="btn-reboot" onclick="rebootStation(${node.id}, '${node.name || 'Station ' + node.id}')" title="Reboot station to detect new sensors">
              🔄 Reboot Station
            </button>
          </div>`;
      });

      document.getElementById('sensor-count').textContent = totalSensors;
    }

    function showWifiModal() {
      document.getElementById('wifi-modal').classList.add('show');
    }

    function hideWifiModal() {
      document.getElementById('wifi-modal').classList.remove('show');
    }

    function saveWifi(event) {
      event.preventDefault();
      const ssid = document.getElementById('ssid').value;
      const password = document.getElementById('password').value;

      fetch('/api/wifi', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ ssid, password })
      })
      .then(r => r.json())
      .then(data => {
        if (data.success) {
          alert('WiFi settings saved! Gateway is restarting...\n\nReconnect to your WiFi network and access the gateway at:\nhttp://lorapnp.local');
          hideWifiModal();
        } else {
          alert('Failed to save: ' + (data.error || 'Unknown error'));
        }
      })
      .catch(e => alert('Error: ' + e.message));
    }

    function updateWifiStatus(data) {
      const statusEl = document.getElementById('wifi-status');
      if (data.wifi_connected) {
        statusEl.innerHTML = `<span class="connected">✓ Connected to ${data.wifi_ssid || 'WiFi'} (${data.wifi_ip || ''})</span>`;
        statusEl.className = 'wifi-status connected';
      } else if (data.wifi_configured) {
        statusEl.innerHTML = `<span>⚠ WiFi configured but not connected</span>`;
        statusEl.className = 'wifi-status';
      } else {
        statusEl.innerHTML = `<span>Click WiFi button to connect to your network</span>`;
        statusEl.className = 'wifi-status';
      }
    }

    function fetchData() {
      fetch('/api')
        .then(r => r.json())
        .then(data => {
          renderDashboard(data);
          updateWifiStatus(data);
        })
        .catch(e => {
          document.getElementById('nodes-container').innerHTML =
            '<div class="loading">Connection error</div>';
        });
    }

    fetchData();
    setInterval(fetchData, 3000);
  </script>
</body>
</html>
)rawliteral";

  webServer.send(200, "text/html", html);
}

void handleRename() {
  if (webServer.method() != HTTP_POST) {
    webServer.send(405, "text/plain", "Method not allowed");
    return;
  }

  StaticJsonDocument<256> doc;
  DeserializationError error = deserializeJson(doc, webServer.arg("plain"));

  if (error) {
    webServer.send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
    return;
  }

  const char* type = doc["type"];

  if (strcmp(type, "node") == 0) {
    uint16_t station_id = doc["node_id"];
    const char* name = doc["name"];

    if (name && strlen(name) > 0 && strlen(name) <= 32) {
      setNodeName(station_id, String(name));

      // Send name update to station
      int slot = findActiveNodeById(station_id);
      if (slot >= 0) {
        sendStationNameUpdate(station_id, name);
      }

      webServer.send(200, "application/json", "{\"success\":true}");
    } else {
      webServer.send(400, "application/json", "{\"error\":\"Invalid name\"}");
    }

  } else if (strcmp(type, "sensor") == 0) {
    uint16_t station_id = doc["node_id"];
    uint8_t sensor_id = doc["sensor_id"];
    const char* name = doc["name"];

    if (name && strlen(name) > 0 && strlen(name) <= 32) {
      setSensorName(station_id, sensor_id, String(name));

      // Send sensor name update to station
      int slot = findActiveNodeById(station_id);
      if (slot >= 0) {
        sendSensorNameUpdate(station_id, sensor_id, name);
      }

      webServer.send(200, "application/json", "{\"success\":true}");
    } else {
      webServer.send(400, "application/json", "{\"error\":\"Invalid name\"}");
    }

  } else if (strcmp(type, "ble") == 0) {
    const char* uid_str = doc["uid"];
    const char* name = doc["name"];

    if (uid_str && name && strlen(name) > 0 && strlen(name) <= 32) {
      uint8_t uid[6];
      sscanf(uid_str, "%2hhx:%2hhx:%2hhx:%2hhx:%2hhx:%2hhx",
             &uid[0], &uid[1], &uid[2], &uid[3], &uid[4], &uid[5]);
      setBLESensorName(uid, String(name));

      // Find which station is forwarding this BLE sensor
      uint16_t parent_station_id = 0;
      for (uint8_t i = 0; i < ble_remote_sensor_count; i++) {
        if (ble_remote_sensors[i].active &&
            memcmp(ble_remote_sensors[i].device_uid, uid, 6) == 0) {
          parent_station_id = ble_remote_sensors[i].parent_node_id;
          break;
        }
      }

      // Send remote sensor name update to parent station
      if (parent_station_id > 0) {
        int slot = findActiveNodeById(parent_station_id);
        if (slot >= 0) {
          sendRemoteSensorNameUpdate(parent_station_id, uid, name);
        }
      }

      webServer.send(200, "application/json", "{\"success\":true}");
    } else {
      webServer.send(400, "application/json", "{\"error\":\"Invalid name\"}");
    }

  } else {
    webServer.send(400, "application/json", "{\"error\":\"Unknown type\"}");
  }
}

void handleReboot() {
  if (webServer.method() != HTTP_POST) {
    webServer.send(405, "application/json", "{\"error\":\"Method not allowed\"}");
    return;
  }

  StaticJsonDocument<128> doc;
  DeserializationError error = deserializeJson(doc, webServer.arg("plain"));

  if (error || !doc.containsKey("node_id")) {
    webServer.send(400, "application/json", "{\"error\":\"Invalid request\"}");
    return;
  }

  uint16_t node_id = doc["node_id"];

  // Find station in registry
  int slot = findActiveNodeById(node_id);
  if (slot < 0) {
    webServer.send(404, "application/json", "{\"error\":\"Station not found\"}");
    return;
  }

  // Send REBOOT command
  Packet pkt;
  pkt.type = MessageType::REBOOT;
  pkt.src_id = GATEWAY_ID;
  pkt.dst_id = node_id;
  pkt.seq = tx_seq++;
  pkt.payload_len = 2;
  pkt.payload[0] = 5;    // 5 second delay (time to send ACK)
  pkt.payload[1] = 0x00; // Reserved flags

  if (sendPacket(pkt)) {
    Serial.printf("REBOOT command sent to station %u\n", node_id);
    webServer.send(200, "application/json", "{\"success\":true}");
  } else {
    Serial.printf("Failed to send REBOOT to station %u\n", node_id);
    webServer.send(500, "application/json", "{\"error\":\"Failed to send command\"}");
  }
}

void handleWifiConfig() {
  if (webServer.method() != HTTP_POST) {
    webServer.send(405, "text/plain", "Method not allowed");
    return;
  }

  StaticJsonDocument<256> doc;
  DeserializationError error = deserializeJson(doc, webServer.arg("plain"));

  if (error) {
    webServer.send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
    return;
  }

  const char* ssid = doc["ssid"];
  const char* password = doc["password"];

  if (!ssid || strlen(ssid) == 0 || strlen(ssid) > 31) {
    webServer.send(400, "application/json", "{\"error\":\"Invalid SSID\"}");
    return;
  }

  if (!password || strlen(password) > 63) {
    webServer.send(400, "application/json", "{\"error\":\"Invalid password\"}");
    return;
  }

  // Save WiFi credentials
  strncpy(wifi_ssid, ssid, sizeof(wifi_ssid));
  strncpy(wifi_password, password, sizeof(wifi_password));
  wifi_sta_enabled = true;

  prefs.begin("lorapnp", false);
  prefs.putBool("wifi_sta_en", true);
  prefs.putString("wifi_ssid", wifi_ssid);
  prefs.putString("wifi_pass", wifi_password);
  prefs.end();

  Serial.printf("WiFi configured: %s\n", wifi_ssid);

  webServer.send(200, "application/json", "{\"success\":true}");

  // Restart after a short delay to allow response to be sent
  delay(1000);
  ESP.restart();
}

int findNodeByUID(const uint8_t* uid) {
  for (int i = 0; i < 32; i++) {
    if (nodes[i].used && memcmp(nodes[i].device_uid, uid, 6) == 0) {
      return i;
    }
  }
  return -1;
}

int findEmptySlot() {
  for (int i = 0; i < 32; i++) {
    if (!nodes[i].used) {
      return i;
    }
  }
  return -1;
}

bool sendPacket(const Packet& pkt) {
  uint8_t buf[HEADER_SIZE + MAX_PAYLOAD_SIZE + 2];
  size_t len = 0;
  Status status = pkt.serialize(buf, &len);
  if (status != Status::OK) {
    Serial.printf("Packet serialize failed: %d\n", static_cast<int>(status));
    return false;
  }

  int state = radio.transmit(buf, len);
  radio.startReceive();
  if (state != RADIOLIB_ERR_NONE) {
    Serial.printf("Radio TX failed: %d\n", state);
    recordRadioFailure();
    return false;
  }

  recordRadioSuccess();
  return true;
}

void sendAck(uint16_t dst_id, uint16_t ack_seq) {
  delay(ACK_TURNAROUND_DELAY_MS);

  Packet ack;
  ack.type = MessageType::ACK;
  ack.src_id = GATEWAY_ID;
  ack.dst_id = dst_id;
  ack.seq = tx_seq++;
  ack.payload_len = 2;
  putU16(ack.payload, ack_seq);

  if (sendPacket(ack)) {
    Serial.printf("ACK sent to node %u for seq %u\n", dst_id, ack_seq);
  }
}

void sendStationNameUpdate(uint16_t station_id, const char* name) {
  delay(ACK_TURNAROUND_DELAY_MS);

  Packet pkt;
  pkt.type = MessageType::NAME_UPDATE;
  pkt.src_id = GATEWAY_ID;
  pkt.dst_id = station_id;
  pkt.seq = tx_seq++;

  size_t name_len = strlen(name);
  if (name_len > 32) name_len = 32;

  pkt.payload_len = 1 + name_len;
  pkt.payload[0] = static_cast<uint8_t>(NameType::STATION);
  memcpy(&pkt.payload[1], name, name_len);

  if (sendPacket(pkt)) {
    Serial.printf("NAME_UPDATE (station) sent to %u: %s\n", station_id, name);
  }
}

void sendSensorNameUpdate(uint16_t station_id, uint8_t sensor_id, const char* name) {
  delay(ACK_TURNAROUND_DELAY_MS);

  Packet pkt;
  pkt.type = MessageType::NAME_UPDATE;
  pkt.src_id = GATEWAY_ID;
  pkt.dst_id = station_id;
  pkt.seq = tx_seq++;

  size_t name_len = strlen(name);
  if (name_len > 32) name_len = 32;

  pkt.payload_len = 2 + name_len;
  pkt.payload[0] = static_cast<uint8_t>(NameType::I2C_SENSOR);
  pkt.payload[1] = sensor_id;
  memcpy(&pkt.payload[2], name, name_len);

  if (sendPacket(pkt)) {
    Serial.printf("NAME_UPDATE (sensor) sent to station %u sensor %u: %s\n",
                  station_id, sensor_id, name);
  }
}

void sendRemoteSensorNameUpdate(uint16_t station_id, const uint8_t* ble_uid, const char* name) {
  delay(ACK_TURNAROUND_DELAY_MS);

  Packet pkt;
  pkt.type = MessageType::NAME_UPDATE;
  pkt.src_id = GATEWAY_ID;
  pkt.dst_id = station_id;
  pkt.seq = tx_seq++;

  size_t name_len = strlen(name);
  if (name_len > 32) name_len = 32;

  pkt.payload_len = 1 + 6 + name_len;
  pkt.payload[0] = static_cast<uint8_t>(NameType::REMOTE_SENSOR);
  memcpy(&pkt.payload[1], ble_uid, 6);
  memcpy(&pkt.payload[7], name, name_len);

  if (sendPacket(pkt)) {
    char uid_str[18];
    formatBLEUID(ble_uid, uid_str, sizeof(uid_str));
    Serial.printf("NAME_UPDATE (remote) sent to station %u BLE %s: %s\n",
                  station_id, uid_str, name);
  }
}

bool sendJoinAccept(int slot) {
  uint16_t node_id = nodes[slot].node_id;

  Packet accept;
  accept.type = MessageType::JOIN_ACCEPT;
  accept.setFlag(FLAG_ACK_REQUEST);
  accept.src_id = GATEWAY_ID;
  accept.dst_id = node_id;
  accept.seq = nodes[slot].join_ack_pending ? nodes[slot].join_accept_seq : tx_seq++;
  accept.payload_len = 14;
  putU16(&accept.payload[0], 0x0001);
  putU16(&accept.payload[2], node_id);
  putU32(&accept.payload[4], DEFAULT_REPORT_INTERVAL_MS);
  putU32(&accept.payload[8], DEFAULT_SLEEP_INTERVAL_MS);
  accept.payload[12] = DEFAULT_SF;
  accept.payload[13] = DEFAULT_TX_POWER;

  if (sendPacket(accept)) {
    nodes[slot].join_ack_pending = true;
    nodes[slot].join_accept_seq = accept.seq;
    nodes[slot].join_accept_sent_ms = millis();
    Serial.printf("JOIN_ACCEPT sent to node %u seq=%u retry=%u\n",
                  node_id, accept.seq, nodes[slot].join_accept_retries);
    return true;
  }

  return false;
}

int registerNodeSlot(const uint8_t* device_uid, uint16_t known_node_id = UNASSIGNED_ID) {
  int slot = findNodeByUID(device_uid);
  if (slot >= 0) return slot;

  slot = findEmptySlot();
  if (slot < 0) {
    Serial.println("Registry full");
    return -1;
  }

  nodes[slot].used = true;
  nodes[slot].active = false;
  nodes[slot].node_id = known_node_id;
  nodes[slot].join_ack_pending = false;
  nodes[slot].join_accept_retries = 0;
  nodes[slot].config_ack_pending = false;
  nodes[slot].config_update_retries = 0;
  nodes[slot].sensor_count = 0;
  memcpy(nodes[slot].device_uid, device_uid, DEVICE_UID_LEN);
  if (known_node_id != UNASSIGNED_ID) {
    bumpNextNodeId(known_node_id);
  }
  return slot;
}

int findActiveNodeById(uint16_t node_id) {
  for (int i = 0; i < 32; i++) {
    if (nodes[i].active && nodes[i].node_id == node_id) {
      return i;
    }
  }
  return -1;
}

bool isDuplicateReliablePacket(const Packet& pkt) {
  int slot = findActiveNodeById(pkt.src_id);
  return slot >= 0 && nodes[slot].last_seq == pkt.seq;
}

void handleDiscover(const Packet& pkt, int16_t rssi, float snr) {
  if (pkt.payload_len < DEVICE_UID_LEN) return;

  uint8_t device_uid[DEVICE_UID_LEN];
  memcpy(device_uid, pkt.payload, DEVICE_UID_LEN);

  uint16_t stored_node_id = loadNodeIdForUID(device_uid);
  int slot = registerNodeSlot(device_uid, stored_node_id);
  if (slot < 0) return;

  if (nodes[slot].active) {
    Serial.println("Known node requested discovery again");
  }

  uint16_t session_token = random(1, 65535);
  nodes[slot].session_token = session_token;
  nodes[slot].rssi = rssi;
  nodes[slot].snr = snr;
  nodes[slot].last_seen = millis();

  Packet offer;
  offer.type = MessageType::OFFER;
  offer.src_id = GATEWAY_ID;
  offer.dst_id = BROADCAST_ID;
  offer.seq = tx_seq++;
  offer.payload_len = 12;
  putU16(&offer.payload[0], session_token);
  putU16(&offer.payload[2], 0x0001);
  putU16(&offer.payload[4], nodes[slot].node_id);
  memcpy(&offer.payload[6], device_uid, DEVICE_UID_LEN);

  sendPacket(offer);
  Serial.printf("OFFER sent to %02X:%02X:%02X:%02X:%02X:%02X\n",
                device_uid[0], device_uid[1], device_uid[2],
                device_uid[3], device_uid[4], device_uid[5]);
}

void handleJoinRequest(const Packet& pkt) {
  if (pkt.payload_len < 8) return;

  uint8_t device_uid[6];
  memcpy(device_uid, pkt.payload, 6);
  uint16_t session_token = getU16(&pkt.payload[6]);
  uint16_t requested_network_id = pkt.payload_len >= 12 ? getU16(&pkt.payload[8]) : 0x0001;
  uint16_t requested_node_id = pkt.payload_len >= 12 ? getU16(&pkt.payload[10]) : UNASSIGNED_ID;

  int slot = findNodeByUID(device_uid);
  if (slot < 0) {
    uint16_t stored_node_id = loadNodeIdForUID(device_uid);
    if (session_token == 0 && requested_network_id == 0x0001 &&
        (stored_node_id != UNASSIGNED_ID || requested_node_id != UNASSIGNED_ID)) {
      slot = registerNodeSlot(device_uid, stored_node_id != UNASSIGNED_ID ? stored_node_id : requested_node_id);
      if (slot < 0) return;
    } else {
      Serial.println("JOIN_REQ: unknown device");
      return;
    }
  }

  if (session_token != 0 && session_token != nodes[slot].session_token) {
    Serial.println("JOIN_REQ: invalid session token");
    return;
  }

  uint16_t node_id = nodes[slot].node_id;
  if (node_id == UNASSIGNED_ID) {
    node_id = requested_node_id != UNASSIGNED_ID ? requested_node_id : next_node_id++;
  }
  bumpNextNodeId(node_id);
  nodes[slot].node_id = node_id;
  nodes[slot].active = true;
  nodes[slot].join_time = millis();
  nodes[slot].last_seen = millis();
  nodes[slot].last_seq = 0;
  nodes[slot].join_ack_pending = false;
  nodes[slot].join_accept_retries = 0;
  saveNodeIdForUID(device_uid, node_id);

  sendJoinAccept(slot);
  Serial.printf("JOIN_ACCEPT: assigned node ID %d\n", node_id);

  char buf[32];
  snprintf(buf, sizeof(buf), "Node %d joined", node_id);
  updateDisplay("LoRa PNP Gateway", buf);
}

void handleData(const Packet& pkt, int16_t rssi, float snr) {
  uint16_t src_id = pkt.src_id;
  float battery = 0.0;
  if (pkt.payload_len >= 2) {
    battery = getU16(&pkt.payload[0]) / 1000.0;
  }

  for (int i = 0; i < 32; i++) {
    if (nodes[i].active && nodes[i].node_id == src_id) {
      nodes[i].last_seen = millis();
      nodes[i].rssi = rssi;
      nodes[i].snr = snr;
      nodes[i].battery_v = battery;
      Serial.printf("DATA from node %d: RSSI=%d SNR=%.1f Battery=%.2fV\n",
                    src_id, rssi, (double)snr, (double)battery);
      break;
    }
  }
}

void handleStatus(const Packet& pkt, int16_t rssi, float snr) {
  if (pkt.payload_len < 11) return;

  uint16_t src_id = pkt.src_id;
  float battery = getU16(&pkt.payload[0]) / 1000.0;
  uint32_t uptime_s = getU32(&pkt.payload[2]);
  uint32_t free_heap = getU32(&pkt.payload[6]);
  uint8_t status_flags = pkt.payload[10];

  int slot = -1;
  for (int i = 0; i < 32; i++) {
    if (nodes[i].active && nodes[i].node_id == src_id) {
      slot = i;
      break;
    }
  }

  if (slot < 0 && pkt.payload_len >= 17) {
    const uint8_t* device_uid = &pkt.payload[11];
    slot = registerNodeSlot(device_uid, src_id);
    if (slot >= 0) {
      nodes[slot].active = true;
      nodes[slot].join_time = millis();
      saveNodeIdForUID(device_uid, src_id);
      Serial.printf("STATUS rebuilt registry for node %u\n", src_id);
    }
  }

  if (slot >= 0) {
    nodes[slot].last_seen = millis();
    nodes[slot].rssi = rssi;
    nodes[slot].snr = snr;
    nodes[slot].battery_v = battery;
    nodes[slot].uptime_s = uptime_s;
    nodes[slot].free_heap = free_heap;
    nodes[slot].status_flags = status_flags;
    nodes[slot].last_seq = pkt.seq;
    Serial.printf("STATUS from node %d: RSSI=%d SNR=%.1f Battery=%.2fV Uptime=%lus Heap=%lu Flags=0x%02X\n",
                  src_id, rssi, (double)snr, (double)battery,
                  (unsigned long)uptime_s, (unsigned long)free_heap, status_flags);
  } else {
    Serial.printf("STATUS ignored from unknown node %u\n", src_id);
  }
}

void handleAck(const Packet& pkt) {
  if (pkt.payload_len < 2) return;

  uint16_t ack_seq = getU16(pkt.payload);
  int slot = findActiveNodeById(pkt.src_id);
  if (slot < 0) {
    Serial.printf("ACK ignored from unknown node %u seq=%u\n", pkt.src_id, ack_seq);
    return;
  }

  if (nodes[slot].join_ack_pending && nodes[slot].join_accept_seq == ack_seq) {
    nodes[slot].join_ack_pending = false;
    Serial.printf("JOIN_ACCEPT ACK received from node %u seq=%u\n", pkt.src_id, ack_seq);
  } else {
    Serial.printf("ACK received from node %u for seq=%u\n", pkt.src_id, ack_seq);
  }
}

void handleSensorDescriptor(const Packet& pkt) {
  if (pkt.payload_len < 4) return;

  int slot = findActiveNodeById(pkt.src_id);
  if (slot < 0) {
    Serial.printf("SENSOR_DESC ignored from unknown node %u\n", pkt.src_id);
    return;
  }

  uint8_t sensor_id = pkt.payload[0];
  uint8_t sensor_type = pkt.payload[1];
  uint8_t sensor_unit = pkt.payload[2];
  uint8_t flags = pkt.payload[3];
  (void)flags;

  char label[SENSOR_LABEL_LEN + 1];
  memset(label, 0, sizeof(label));
  size_t label_len = pkt.payload_len > 4 ? min((size_t)SENSOR_LABEL_LEN, (size_t)(pkt.payload_len - 4)) : 0;
  if (label_len > 0) {
    memcpy(label, &pkt.payload[4], label_len);
  }

  int sensor_slot = -1;
  for (int i = 0; i < nodes[slot].sensor_count; i++) {
    if (nodes[slot].sensor_ids[i] == sensor_id) {
      sensor_slot = i;
      break;
    }
  }

  if (sensor_slot < 0) {
    if (nodes[slot].sensor_count >= MAX_SENSORS_PER_NODE) {
      Serial.printf("SENSOR_DESC ignored for node %u: list full\n", pkt.src_id);
      return;
    }
    sensor_slot = nodes[slot].sensor_count++;
  }

  nodes[slot].sensor_ids[sensor_slot] = sensor_id;
  nodes[slot].sensor_types[sensor_slot] = sensor_type;
  nodes[slot].sensor_units[sensor_slot] = sensor_unit;
  strncpy(nodes[slot].sensor_labels[sensor_slot], label, SENSOR_LABEL_LEN);
  nodes[slot].sensor_labels[sensor_slot][SENSOR_LABEL_LEN] = '\0';

  Serial.printf("SENSOR_DESC node=%u sensor=%u type=%u unit=%u label=%s\n",
                pkt.src_id, sensor_id, sensor_type, sensor_unit,
                nodes[slot].sensor_labels[sensor_slot]);

  char sensor_key[32];
  snprintf(sensor_key, sizeof(sensor_key), "s%u_%u", pkt.src_id, sensor_id);
  prefs.begin("lorapnp", false);
  prefs.putBytes(sensor_key, &pkt.payload[0], min((size_t)pkt.payload_len, (size_t)20));
  prefs.end();
}

void handleSensorData(const Packet& pkt) {
  if (pkt.payload_len < 6) return;

  uint8_t sensor_id = pkt.payload[0];
  uint8_t data_type = pkt.payload[1];
  float float_value = 0.0f;
  uint16_t uint_value = 0;

  if (data_type == SENSOR_DATA_FLOAT32) {
    memcpy(&float_value, &pkt.payload[2], sizeof(float_value));
    Serial.printf("SENSOR_DATA node=%u sensor=%u value=%.2f\n",
                  pkt.src_id, sensor_id, (double)float_value);
  } else if (data_type == SENSOR_DATA_UINT16) {
    uint_value = getU16(&pkt.payload[2]);
    Serial.printf("SENSOR_DATA node=%u sensor=%u value=%u\n",
                  pkt.src_id, sensor_id, uint_value);
  } else {
    Serial.printf("SENSOR_DATA node=%u sensor=%u unsupported type=%u\n",
                  pkt.src_id, sensor_id, data_type);
    return;
  }

  if (mqtt_enabled && mqttClient.connected()) {
    char topic[64];
    snprintf(topic, sizeof(topic), "lorapnp/node/%u/sensor/%u", pkt.src_id, sensor_id);

    StaticJsonDocument<128> doc;
    if (data_type == SENSOR_DATA_FLOAT32) {
      doc["value"] = float_value;
    } else {
      doc["value"] = uint_value;
    }
    doc["type"] = sensor_id;

    String payload;
    serializeJson(doc, payload);
    mqttClient.publish(topic, payload.c_str());
  }
}

int findBLESensorByUID(const uint8_t* uid) {
  for (uint8_t i = 0; i < ble_remote_sensor_count; i++) {
    if (memcmp(ble_remote_sensors[i].device_uid, uid, 6) == 0) {
      return i;
    }
  }
  return -1;
}

int registerBLESensor(const uint8_t* uid, uint16_t parent_node_id) {
  if (ble_remote_sensor_count >= MAX_BLE_SENSORS_TOTAL) {
    Serial.println("BLE sensor registry full");
    return -1;
  }

  int slot = ble_remote_sensor_count++;
  ble_remote_sensors[slot].active = true;
  memcpy(ble_remote_sensors[slot].device_uid, uid, 6);
  ble_remote_sensors[slot].parent_node_id = parent_node_id;
  ble_remote_sensors[slot].sensor_count = 0;
  ble_remote_sensors[slot].rssi = 0;
  ble_remote_sensors[slot].battery_mv = 0;
  ble_remote_sensors[slot].last_seen = millis();

  char uid_str[18];
  formatBLEUID(uid, uid_str, sizeof(uid_str));
  Serial.printf("Registered BLE sensor: %s via node %u\n", uid_str, parent_node_id);

  return slot;
}

void handleBLESensorData(const Packet& pkt) {
  if (pkt.payload_len < sizeof(BLESensorDataPayload)) return;

  const BLESensorDataPayload* payload = (const BLESensorDataPayload*)pkt.payload;

  // Find or register BLE sensor
  int slot = findBLESensorByUID(payload->ble_device_uid);
  if (slot < 0) {
    slot = registerBLESensor(payload->ble_device_uid, pkt.src_id);
    if (slot < 0) return;
  }

  // Update sensor info
  ble_remote_sensors[slot].rssi = payload->rssi;
  ble_remote_sensors[slot].battery_mv = payload->battery_mv;
  ble_remote_sensors[slot].last_seen = millis();
  ble_remote_sensors[slot].parent_node_id = pkt.src_id;

  // Update sensor reading
  bool found = false;
  for (uint8_t i = 0; i < ble_remote_sensors[slot].sensor_count; i++) {
    if (ble_remote_sensors[slot].sensor_ids[i] == payload->sensor_id) {
      ble_remote_sensors[slot].sensor_types[i] = payload->sensor_type;
      ble_remote_sensors[slot].sensor_units[i] = payload->sensor_unit;
      if (payload->data_type == SENSOR_DATA_FLOAT32) {
        ble_remote_sensors[slot].last_values[i] = payload->value.float_val;
      } else {
        ble_remote_sensors[slot].last_values[i] = payload->value.uint_val;
      }
      found = true;
      break;
    }
  }

  if (!found && ble_remote_sensors[slot].sensor_count < 4) {
    uint8_t idx = ble_remote_sensors[slot].sensor_count++;
    ble_remote_sensors[slot].sensor_ids[idx] = payload->sensor_id;
    ble_remote_sensors[slot].sensor_types[idx] = payload->sensor_type;
    ble_remote_sensors[slot].sensor_units[idx] = payload->sensor_unit;
    if (payload->data_type == SENSOR_DATA_FLOAT32) {
      ble_remote_sensors[slot].last_values[idx] = payload->value.float_val;
    } else {
      ble_remote_sensors[slot].last_values[idx] = payload->value.uint_val;
    }
  }

  char uid_str[18];
  formatBLEUID(payload->ble_device_uid, uid_str, sizeof(uid_str));

  float value = (payload->data_type == SENSOR_DATA_FLOAT32) ?
                payload->value.float_val : payload->value.uint_val;

  Serial.printf("BLE_SENSOR_DATA %s via node %u: sensor=%u value=%.2f RSSI=%d battery=%u mV\n",
                uid_str, pkt.src_id, payload->sensor_id, (double)value,
                payload->rssi, payload->battery_mv);

  // MQTT publishing
  if (mqtt_enabled && mqttClient.connected()) {
    char topic[96];
    char uid_hex[13];
    snprintf(uid_hex, sizeof(uid_hex), "%02X%02X%02X%02X%02X%02X",
             payload->ble_device_uid[0], payload->ble_device_uid[1],
             payload->ble_device_uid[2], payload->ble_device_uid[3],
             payload->ble_device_uid[4], payload->ble_device_uid[5]);
    snprintf(topic, sizeof(topic), "lorapnp/node/%u/ble/%s/sensor/%u",
             pkt.src_id, uid_hex, payload->sensor_id);

    StaticJsonDocument<256> doc;
    if (payload->data_type == SENSOR_DATA_FLOAT32) {
      doc["value"] = payload->value.float_val;
    } else {
      doc["value"] = payload->value.uint_val;
    }
    doc["type"] = payload->sensor_type;
    doc["unit"] = payload->sensor_unit;
    doc["rssi"] = payload->rssi;
    doc["battery_mv"] = payload->battery_mv;
    doc["source"] = "ble";
    doc["timestamp"] = payload->timestamp_ms;

    String mqtt_payload;
    serializeJson(doc, mqtt_payload);
    mqttClient.publish(topic, mqtt_payload.c_str());
  }
}

bool sendConfigUpdate(int slot) {
  uint16_t node_id = nodes[slot].node_id;

  Packet config;
  config.type = MessageType::CONFIG_UPDATE;
  config.setFlag(FLAG_ACK_REQUEST);
  config.src_id = GATEWAY_ID;
  config.dst_id = node_id;
  config.seq = nodes[slot].config_ack_pending ? nodes[slot].config_update_seq : tx_seq++;
  config.payload_len = 10;
  putU32(&config.payload[0], nodes[slot].pending_report_interval_ms);
  putU32(&config.payload[4], nodes[slot].pending_sleep_interval_ms);
  config.payload[8] = nodes[slot].pending_sf;
  config.payload[9] = nodes[slot].pending_tx_power;

  if (sendPacket(config)) {
    nodes[slot].config_ack_pending = true;
    nodes[slot].config_update_seq = config.seq;
    nodes[slot].config_update_sent_ms = millis();
    Serial.printf("CONFIG_UPDATE sent to node %u seq=%u retry=%u (report=%lu ms)\n",
                  node_id, config.seq, nodes[slot].config_update_retries,
                  (unsigned long)nodes[slot].pending_report_interval_ms);
    return true;
  }

  return false;
}

void handleConfigAck(const Packet& pkt) {
  if (pkt.payload_len < 2) return;

  uint16_t ack_seq = getU16(pkt.payload);
  int slot = findActiveNodeById(pkt.src_id);
  if (slot < 0) {
    Serial.printf("CONFIG_ACK ignored from unknown node %u seq=%u\n", pkt.src_id, ack_seq);
    return;
  }

  if (nodes[slot].config_ack_pending && nodes[slot].config_update_seq == ack_seq) {
    nodes[slot].config_ack_pending = false;
    Serial.printf("CONFIG_ACK received from node %u seq=%u - config applied\n", pkt.src_id, ack_seq);
  } else {
    Serial.printf("CONFIG_ACK received from node %u for seq=%u (not pending)\n", pkt.src_id, ack_seq);
  }
}

void setup() {
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  Wire.begin(I2C_SDA, I2C_SCL);
  display.init();
  display.flipScreenVertically();
  display.setFont(ArialMT_Plain_10);
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  updateDisplay("T3-S3 Gateway", "Initializing...");

  Serial.begin(115200);
  delay(1000);
  Serial.println("\n=== LoRa PNP Gateway ===");

  loadPersistedRegistry();

  // Load configuration
  prefs.begin("lorapnp", true);
  mqtt_enabled = prefs.getBool("mqtt_en", false);
  prefs.getString("mqtt_host", mqtt_broker, sizeof(mqtt_broker));
  mqtt_port = prefs.getUShort("mqtt_port", 1883);
  wifi_sta_enabled = prefs.getBool("wifi_sta_en", false);
  prefs.getString("wifi_ssid", wifi_ssid, sizeof(wifi_ssid));
  prefs.getString("wifi_pass", wifi_password, sizeof(wifi_password));
  prefs.end();

  // Start WiFi AP mode (dual mode to support both AP and Station)
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP("LoRaPNP-Gateway");
  Serial.print("AP IP: ");
  Serial.println(WiFi.softAPIP());

  // Start WiFi Station mode if configured
  if (wifi_sta_enabled && strlen(wifi_ssid) > 0) {
    Serial.printf("Connecting to WiFi: %s\n", wifi_ssid);
    WiFi.begin(wifi_ssid, wifi_password);

    uint32_t wifi_start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - wifi_start < 10000) {
      delay(100);
    }

    if (WiFi.status() == WL_CONNECTED) {
      wifi_connected = true;
      Serial.print("WiFi connected! IP: ");
      Serial.println(WiFi.localIP());
    } else {
      Serial.println("WiFi connection failed, AP mode only");
    }
  }

  // Start mDNS
  if (MDNS.begin("lorapnp")) {
    Serial.println("mDNS started: http://lorapnp.local");
    MDNS.addService("http", "tcp", 80);
  }

  // Update display with access info
  if (wifi_connected) {
    char line2[32];
    snprintf(line2, sizeof(line2), "WiFi: %s", WiFi.localIP().toString().c_str());
    updateDisplay("lorapnp.local", line2, "AP: 192.168.4.1");
  } else {
    updateDisplay("Gateway Ready", "AP: LoRaPNP-Gateway", "lorapnp.local");
  }

  // Register web routes
  webServer.on("/", handleDashboard);
  webServer.on("/api", handleRoot);
  webServer.on("/api/rename", HTTP_POST, handleRename);
  webServer.on("/api/reboot", HTTP_POST, handleReboot);
  webServer.on("/api/wifi", HTTP_POST, handleWifiConfig);
  webServer.begin();
  Serial.println("Web server started");
  Serial.println("Dashboard: http://lorapnp.local or http://192.168.4.1");
  Serial.println("JSON API: http://lorapnp.local/api");

  if (mqtt_enabled && strlen(mqtt_broker) > 0) {
    mqttClient.setServer(mqtt_broker, mqtt_port);
    Serial.printf("MQTT configured: %s:%u\n", mqtt_broker, mqtt_port);
  }

  SPI.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_CS);

  int state = radio.begin(LORA_FREQ, LORA_BW, LORA_SF, LORA_CR, LORA_SYNC, LORA_PWR, 8);

  if (state == RADIOLIB_ERR_NONE) {
    Serial.println("Radio init OK");
    updateDisplay("T3-S3 Gateway", "Radio OK", "Listening...");
    for (int i = 0; i < 3; i++) {
      digitalWrite(LED_PIN, HIGH);
      delay(100);
      digitalWrite(LED_PIN, LOW);
      delay(100);
    }
  } else {
    Serial.print("Radio init failed, code: ");
    Serial.println(state);
    updateDisplay("T3-S3 Gateway", "Radio FAILED!", String(state).c_str());
    while (true) {
      digitalWrite(LED_PIN, HIGH);
      delay(1000);
      digitalWrite(LED_PIN, LOW);
      delay(1000);
    }
  }

  radio.startReceive();
  radio_last_success_ms = millis();
  delay(2000);
}

// ========== RADIO WATCHDOG (RESILIENCY) ==========
void recordRadioSuccess() {
  radio_consecutive_failures = 0;
  radio_last_success_ms = millis();
}

void recordRadioFailure() {
  radio_consecutive_failures++;
}

bool radioNeedsReset() {
  if (radio_consecutive_failures < RADIO_FAILURE_THRESHOLD) {
    return false;
  }
  uint32_t time_since_success = millis() - radio_last_success_ms;
  return time_since_success > RADIO_STUCK_TIMEOUT_MS;
}

void resetRadio() {
  Serial.printf("🔧 RADIO WATCHDOG: Resetting radio (%u failures over %lu seconds)\n",
                radio_consecutive_failures, (millis() - radio_last_success_ms) / 1000);
  radio_reset_count++;

  radio.standby();
  delay(10);

  int state = radio.begin(LORA_FREQ, LORA_BW, LORA_SF, LORA_CR, LORA_SYNC, LORA_PWR);
  if (state == RADIOLIB_ERR_NONE) {
    Serial.println("✓ Radio reset successful");
    radio.startReceive();
    recordRadioSuccess();
  } else {
    Serial.printf("✗ Radio reset FAILED (state=%d)\n", state);
  }
}
// ========== END RADIO WATCHDOG ==========

// ========== STALE NODE DETECTION (PHASE 2) ==========
void cleanupStaleNodes() {
  uint32_t now = millis();
  for (int i = 0; i < 32; i++) {
    if (nodes[i].active) {
      uint32_t time_since_seen = now - nodes[i].last_seen;
      if (time_since_seen > NODE_STALE_TIMEOUT_MS) {
        Serial.printf("🧹 STALE NODE CLEANUP: Node %u inactive for %lu seconds, marking inactive\n",
                      nodes[i].node_id, time_since_seen / 1000);
        nodes[i].active = false;
        stale_node_cleanups++;
      }
    }
  }
}
// ========== END STALE NODE DETECTION ==========

void loop() {
  webServer.handleClient();

  if (mqtt_enabled && !mqttClient.connected() && strlen(mqtt_broker) > 0) {
    if (mqttClient.connect("LoRaPNP-Gateway")) {
      Serial.printf("MQTT connected to %s:%u\n", mqtt_broker, mqtt_port);
    }
  }
  if (mqttClient.connected()) {
    mqttClient.loop();
  }

  if (digitalRead(LORA_DIO0) == HIGH) {
    uint8_t buf[HEADER_SIZE + MAX_PAYLOAD_SIZE + 2];
    size_t len = radio.getPacketLength();
    if (len > sizeof(buf)) {
      Serial.printf("RX too large: %d\n", (int)len);
      radio.startReceive();
      return;
    }

    int state = radio.readData(buf, len);

    if (state == RADIOLIB_ERR_NONE) {
      recordRadioSuccess();  // Successful radio operation
      int16_t rssi = radio.getRSSI();
      float snr = radio.getSNR();

      Packet pkt;
      Status status = pkt.deserialize(buf, len);
      if (status != Status::OK) {
        Serial.printf("RX packet rejected: %d len=%d\n", static_cast<int>(status), (int)len);
        radio.startReceive();
        return;
      }

      Serial.printf("RX: %s src=%u dst=%u seq=%u RSSI=%d SNR=%.1f\n",
                    pkt.typeStr(), pkt.src_id, pkt.dst_id, pkt.seq, rssi, (double)snr);

      digitalWrite(LED_PIN, HIGH);
      delay(50);
      digitalWrite(LED_PIN, LOW);

      if (pkt.type == MessageType::DISCOVER) {
        handleDiscover(pkt, rssi, snr);
      } else if (pkt.type == MessageType::JOIN_REQUEST) {
        handleJoinRequest(pkt);
      } else if (pkt.type == MessageType::DATA) {
        bool duplicate = isDuplicateReliablePacket(pkt);
        if (!duplicate) {
          handleData(pkt, rssi, snr);
        } else {
          Serial.printf("Duplicate DATA from node %u seq=%u suppressed\n", pkt.src_id, pkt.seq);
        }
        if (pkt.flagSet(FLAG_ACK_REQUEST)) {
          sendAck(pkt.src_id, pkt.seq);
        }
      } else if (pkt.type == MessageType::STATUS) {
        bool duplicate = isDuplicateReliablePacket(pkt);
        if (!duplicate) {
          handleStatus(pkt, rssi, snr);
        } else {
          Serial.printf("Duplicate STATUS from node %u seq=%u suppressed\n", pkt.src_id, pkt.seq);
        }
        if (pkt.flagSet(FLAG_ACK_REQUEST)) {
          sendAck(pkt.src_id, pkt.seq);
        }
      } else if (pkt.type == MessageType::ACK) {
        handleAck(pkt);
      } else if (pkt.type == MessageType::CONFIG_ACK) {
        handleConfigAck(pkt);
      } else if (pkt.type == MessageType::SENSOR_DESCRIPTOR) {
        handleSensorDescriptor(pkt);
        if (pkt.flagSet(FLAG_ACK_REQUEST)) {
          sendAck(pkt.src_id, pkt.seq);
        }
      } else if (pkt.type == MessageType::SENSOR_DATA) {
        bool duplicate = isDuplicateReliablePacket(pkt);
        if (!duplicate) {
          handleSensorData(pkt);
          int slot = findActiveNodeById(pkt.src_id);
          if (slot >= 0) nodes[slot].last_seq = pkt.seq;
        } else {
          Serial.printf("Duplicate SENSOR_DATA from node %u seq=%u suppressed\n", pkt.src_id, pkt.seq);
        }
        if (pkt.flagSet(FLAG_ACK_REQUEST)) {
          sendAck(pkt.src_id, pkt.seq);
        }
      } else if (pkt.type == MessageType::BLE_SENSOR_DATA) {
        handleBLESensorData(pkt);
        radio.startReceive();
      } else {
        radio.startReceive();
      }
    } else {
      Serial.printf("RX error: %d\n", state);
      radio.startReceive();
    }
  }

  // Radio watchdog check
  if (radioNeedsReset()) {
    resetRadio();
  }

  // Stale node cleanup (Phase 2) - check every 30 seconds
  static uint32_t last_stale_check = 0;
  if (millis() - last_stale_check >= 30000) {
    last_stale_check = millis();
    cleanupStaleNodes();
  }

  // Update status display every 2 seconds
  if (millis() - last_display_update_ms >= 2000) {
    last_display_update_ms = millis();
    updateStatusDisplay();
  }

  if (millis() - last_registry_print > 10000) {
    int active_count = 0;
    for (int i = 0; i < 32; i++) {
      if (nodes[i].active) active_count++;
    }
    Serial.printf("\n=== Node Registry [%d/32] ===\n", active_count);
    Serial.println("ID   RSSI  SNR   Batt  Uptime  Heap   Sensors  LastSeen");
    Serial.println("---  ----  ----  ----  ------  -----  -------  --------");
    for (int i = 0; i < 32; i++) {
      if (nodes[i].active) {
        uint32_t age_s = (millis() - nodes[i].last_seen) / 1000;
        Serial.printf("%-4u %-5d %-5.1f %-5.2f %-7lu %-6lu %-8u %us\n",
                      nodes[i].node_id,
                      nodes[i].rssi,
                      (double)nodes[i].snr,
                      (double)nodes[i].battery_v,
                      (unsigned long)nodes[i].uptime_s,
                      (unsigned long)nodes[i].free_heap,
                      nodes[i].sensor_count,
                      (unsigned int)age_s);
      }
    }

    // Print BLE remote sensors
    if (ble_remote_sensor_count > 0) {
      Serial.printf("\n=== BLE Remote Sensors [%d/%d] ===\n",
                    ble_remote_sensor_count, MAX_BLE_SENSORS_TOTAL);
      Serial.println("UID               Node  RSSI  Batt  Sensors  LastSeen");
      Serial.println("----------------  ----  ----  ----  -------  --------");
      for (uint8_t i = 0; i < ble_remote_sensor_count; i++) {
        if (!ble_remote_sensors[i].active) continue;

        char uid_str[18];
        formatBLEUID(ble_remote_sensors[i].device_uid, uid_str, sizeof(uid_str));
        uint32_t age_s = (millis() - ble_remote_sensors[i].last_seen) / 1000;

        Serial.printf("%-16s  %-4u  %-4d  %-4u  %-7u  %us\n",
                      uid_str,
                      ble_remote_sensors[i].parent_node_id,
                      ble_remote_sensors[i].rssi,
                      ble_remote_sensors[i].battery_mv,
                      ble_remote_sensors[i].sensor_count,
                      (unsigned int)age_s);
      }
    }

    Serial.println();
    last_registry_print = millis();
  }

  for (int i = 0; i < 32; i++) {
    if (!nodes[i].active || !nodes[i].join_ack_pending) continue;

    if (millis() - nodes[i].join_accept_sent_ms >= JOIN_ACK_TIMEOUT_MS) {
      if (nodes[i].join_accept_retries < JOIN_ACCEPT_MAX_RETRIES) {
        nodes[i].join_accept_retries++;
        Serial.printf("JOIN_ACCEPT ACK timeout node=%u seq=%u retry=%u\n",
                      nodes[i].node_id, nodes[i].join_accept_seq, nodes[i].join_accept_retries);
        sendJoinAccept(i);
      } else {
        Serial.printf("JOIN_ACCEPT ACK abandoned node=%u seq=%u\n",
                      nodes[i].node_id, nodes[i].join_accept_seq);
        nodes[i].join_ack_pending = false;
      }
    }
  }

  for (int i = 0; i < 32; i++) {
    if (!nodes[i].active || !nodes[i].config_ack_pending) continue;

    if (millis() - nodes[i].config_update_sent_ms >= CONFIG_ACK_TIMEOUT_MS) {
      if (nodes[i].config_update_retries < CONFIG_UPDATE_MAX_RETRIES) {
        nodes[i].config_update_retries++;
        Serial.printf("CONFIG_UPDATE ACK timeout node=%u seq=%u retry=%u\n",
                      nodes[i].node_id, nodes[i].config_update_seq, nodes[i].config_update_retries);
        sendConfigUpdate(i);
      } else {
        Serial.printf("CONFIG_UPDATE ACK abandoned node=%u seq=%u\n",
                      nodes[i].node_id, nodes[i].config_update_seq);
        nodes[i].config_ack_pending = false;
      }
    }
  }

  if (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();

    if (cmd.startsWith("config ")) {
      int firstSpace = cmd.indexOf(' ');
      int secondSpace = cmd.indexOf(' ', firstSpace + 1);
      if (secondSpace > 0) {
        uint16_t target_node_id = cmd.substring(firstSpace + 1, secondSpace).toInt();
        uint32_t new_interval = cmd.substring(secondSpace + 1).toInt();

        int slot = findActiveNodeById(target_node_id);
        if (slot >= 0) {
          nodes[slot].pending_report_interval_ms = new_interval;
          nodes[slot].pending_sleep_interval_ms = new_interval;
          nodes[slot].pending_sf = DEFAULT_SF;
          nodes[slot].pending_tx_power = DEFAULT_TX_POWER;
          nodes[slot].config_update_retries = 0;
          sendConfigUpdate(slot);
          Serial.printf("Sending config update to node %u: report_interval=%lu ms\n",
                        target_node_id, (unsigned long)new_interval);
        } else {
          Serial.printf("Node %u not found or not active\n", target_node_id);
        }
      } else {
        Serial.println("Usage: config <node_id> <report_interval_ms>");
      }
    } else if (cmd.startsWith("mqtt ")) {
      int firstSpace = cmd.indexOf(' ');
      int secondSpace = cmd.indexOf(' ', firstSpace + 1);
      if (secondSpace > 0) {
        String host = cmd.substring(firstSpace + 1, secondSpace);
        uint16_t port = cmd.substring(secondSpace + 1).toInt();

        host.toCharArray(mqtt_broker, sizeof(mqtt_broker));
        mqtt_port = port > 0 ? port : 1883;
        mqtt_enabled = true;

        prefs.begin("lorapnp", false);
        prefs.putBool("mqtt_en", mqtt_enabled);
        prefs.putString("mqtt_host", mqtt_broker);
        prefs.putUShort("mqtt_port", mqtt_port);
        prefs.end();

        mqttClient.setServer(mqtt_broker, mqtt_port);
        Serial.printf("MQTT configured: %s:%u\n", mqtt_broker, mqtt_port);
      } else {
        Serial.println("Usage: mqtt <broker_ip> <port>");
      }
    } else if (cmd == "mqtt off") {
      mqtt_enabled = false;
      prefs.begin("lorapnp", false);
      prefs.putBool("mqtt_en", false);
      prefs.end();
      Serial.println("MQTT disabled");
    } else if (cmd.startsWith("wifi ")) {
      int firstSpace = cmd.indexOf(' ');
      int secondSpace = cmd.indexOf(' ', firstSpace + 1);
      if (secondSpace > 0) {
        String ssid = cmd.substring(firstSpace + 1, secondSpace);
        String password = cmd.substring(secondSpace + 1);

        ssid.toCharArray(wifi_ssid, sizeof(wifi_ssid));
        password.toCharArray(wifi_password, sizeof(wifi_password));
        wifi_sta_enabled = true;

        prefs.begin("lorapnp", false);
        prefs.putBool("wifi_sta_en", true);
        prefs.putString("wifi_ssid", wifi_ssid);
        prefs.putString("wifi_pass", wifi_password);
        prefs.end();

        Serial.printf("WiFi configured: %s\n", wifi_ssid);
        Serial.println("Restart gateway to connect");
      } else {
        Serial.println("Usage: wifi <ssid> <password>");
      }
    } else if (cmd == "wifi off") {
      wifi_sta_enabled = false;
      prefs.begin("lorapnp", false);
      prefs.putBool("wifi_sta_en", false);
      prefs.end();
      Serial.println("WiFi station mode disabled. Restart to apply.");
    } else if (cmd == "wifi status") {
      Serial.println("\n=== WiFi Status ===");
      Serial.printf("AP Mode: LoRaPNP-Gateway (%s)\n", WiFi.softAPIP().toString().c_str());
      if (wifi_sta_enabled) {
        Serial.printf("Station Mode: %s\n", wifi_ssid);
        if (wifi_connected) {
          Serial.printf("  Status: Connected (%s)\n", WiFi.localIP().toString().c_str());
          Serial.printf("  RSSI: %d dBm\n", WiFi.RSSI());
        } else {
          Serial.println("  Status: Not connected");
        }
      } else {
        Serial.println("Station Mode: Disabled");
      }
      Serial.println("\nAccess dashboard at:");
      Serial.println("  http://lorapnp.local");
      Serial.println("  http://192.168.4.1");
      if (wifi_connected) {
        Serial.printf("  http://%s\n", WiFi.localIP().toString().c_str());
      }
    } else if (cmd.length() > 0) {
      Serial.println("Commands:");
      Serial.println("  config <node_id> <interval_ms>");
      Serial.println("  mqtt <broker_ip> <port>");
      Serial.println("  mqtt off");
      Serial.println("  wifi <ssid> <password>");
      Serial.println("  wifi off");
      Serial.println("  wifi status");
    }
  }
}
