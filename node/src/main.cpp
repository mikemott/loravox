#include <Arduino.h>
#include <Wire.h>
#include <SSD1306Wire.h>
#include <Preferences.h>
#include <Adafruit_SHT4x.h>
#include <Adafruit_BMP5xx.h>
#include <NimBLEDevice.h>
#include <RadioLib.h>
#include "BLEProtocol.h"
#include "Packet.h"
#include "Types.h"
#include "DeviceIdentity.h"          // shared MAC->UID derivation
#include "sensors/SensorRegistry.h"  // I2CSensor + driver-registry auto-detection

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

#define OLED_SDA  18
#define OLED_SCL  17
#define QWIIC_SDA 43
#define QWIIC_SCL 44

#define LORA_FREQ   915.0
#define LORA_BW     125.0
#define LORA_SF     7
#define LORA_CR     5
#define LORA_SYNC   0x12
#define LORA_PWR    14

enum class NodeMode : uint8_t {
  DISCOVERING,
  WAIT_OFFER,
  WAIT_JOIN_ACCEPT,
  JOINED,
};

#ifdef LORA_USE_SX1262
  Module radioModule(LORA_CS, LORA_DIO1, LORA_RST, LORA_BUSY, SPI);
  SX1262 radio(&radioModule);
#else
  Module radioModule(LORA_CS, LORA_DIO0, LORA_RST, LORA_DIO1, SPI);
  SX1276 radio(&radioModule);
#endif
SSD1306Wire display(0x3c, OLED_SDA, OLED_SCL);
Preferences prefs;

uint8_t device_uid[DEVICE_UID_LEN];
NodeMode mode = NodeMode::DISCOVERING;
uint16_t network_id = 0;
uint16_t node_id = UNASSIGNED_ID;
uint16_t session_token = 0;
uint16_t tx_seq = 0;
uint32_t report_interval_ms = DEFAULT_REPORT_INTERVAL_MS;
uint32_t sleep_interval_ms = DEFAULT_SLEEP_INTERVAL_MS;
uint32_t last_action_ms = 0;
uint32_t next_retry_ms = 0;
uint32_t last_status_ms = 0;
constexpr uint32_t OFFER_WAIT_MS = 6000;
constexpr uint32_t JOIN_ACCEPT_WAIT_MS = 6000;
constexpr uint32_t STATUS_ACK_TIMEOUT_MS = 6000;
constexpr uint8_t STATUS_MAX_RETRIES = 3;
constexpr uint32_t LISTEN_WINDOW_MS = 3000;
bool sleep_enabled = false;
uint32_t awake_start_ms = 0;
bool tasks_complete = false;
bool i2c_sensors_enabled = false;
int16_t last_gateway_rssi = 0;
float last_gateway_snr = 0.0f;
uint32_t last_gateway_rx_ms = 0;
uint32_t last_display_update_ms = 0;
char custom_name[33] = "";  // Custom station name from gateway

// Radio watchdog (resiliency)
uint32_t radio_consecutive_failures = 0;
uint32_t radio_last_success_ms = 0;
uint32_t radio_reset_count = 0;
constexpr uint32_t RADIO_FAILURE_THRESHOLD = 5;
constexpr uint32_t RADIO_STUCK_TIMEOUT_MS = 300000; // 5 minutes

// Proactive rejoin (Phase 2)
uint32_t last_gateway_contact_ms = 0;
constexpr uint32_t GATEWAY_SILENCE_TIMEOUT_MS = 120000; // 2 minutes
uint32_t rejoin_attempts = 0;

// Sensor name storage
constexpr uint8_t MAX_SENSOR_NAMES = 8;
struct SensorName {
  uint8_t sensor_id;
  char name[33];
  bool valid;
};
SensorName sensor_names[MAX_SENSOR_NAMES];

// Remote sensor name storage
struct RemoteSensorName {
  uint8_t device_uid[6];
  char name[33];
  bool valid;
};
RemoteSensorName remote_sensor_names[MAX_BLE_SENSORS_PER_NODE];

Packet pending_status;
bool status_ack_pending = false;
uint16_t pending_status_seq = 0;
uint32_t status_sent_ms = 0;
uint8_t status_retry_count = 0;
bool sensor_descriptor_sent = false;
uint32_t last_sensor_ms = 0;
constexpr uint32_t SENSOR_REPORT_INTERVAL_MS = 30000;
constexpr uint8_t FAKE_SENSOR_ID = 1;

// ========== I2C SENSOR DETECTION ==========
// I2CSensor and the driver registry live in sensors/SensorRegistry.h.
// sht4/bmp5 are defined by their drivers (sensors/drivers/*.cpp); we read
// from them here in the sensor-report path, so declare them extern.
constexpr uint8_t MAX_I2C_SENSORS = 8;
I2CSensor i2c_sensors[MAX_I2C_SENSORS];
uint8_t i2c_sensor_count = 0;
extern Adafruit_SHT4x sht4;
extern Adafruit_BMP5xx bmp5;
// ========== END I2C SENSOR DETECTION ==========

// ========== BLE REMOTE SENSOR SCANNING ==========
BLERemoteSensor ble_sensors[MAX_BLE_SENSORS_PER_NODE];
uint8_t ble_sensor_count = 0;
uint32_t last_ble_scan_ms = 0;
bool ble_scan_enabled = true;
NimBLEScan* ble_scanner = nullptr;
// ========== END BLE REMOTE SENSOR SCANNING ==========
Packet pending_sensor_data;
bool sensor_data_ack_pending = false;
uint16_t pending_sensor_data_seq = 0;
uint32_t sensor_data_sent_ms = 0;
uint8_t sensor_data_retry_count = 0;
constexpr uint32_t SENSOR_DATA_ACK_TIMEOUT_MS = 6000;
constexpr uint8_t SENSOR_DATA_MAX_RETRIES = 3;

// Forward declarations for radio watchdog
void recordRadioSuccess();
void recordRadioFailure();
bool radioNeedsReset();
void resetRadio();

void updateDisplay(const String& line1, const String& line2 = "", const String& line3 = "") {
  display.clear();
  display.drawString(0, 0, line1);
  display.drawString(0, 20, line2);
  display.drawString(0, 40, line3);
  display.display();
}

void updateStatusDisplay() {
  char line1[32], line2[32], line3[32];

  // Line 1: Custom name or Station ID
  if (node_id == UNASSIGNED_ID) {
    snprintf(line1, sizeof(line1), "Station: Searching");
  } else if (custom_name[0] != '\0') {
    snprintf(line1, sizeof(line1), "%s", custom_name);
  } else {
    snprintf(line1, sizeof(line1), "Station #%u", node_id);
  }

  // Line 2: Gateway connection
  if (mode == NodeMode::JOINED) {
    uint32_t age_s = (millis() - last_gateway_rx_ms) / 1000;
    if (age_s < 60) {
      snprintf(line2, sizeof(line2), "GW: OK %ddBm %.1fdB", last_gateway_rssi, last_gateway_snr);
    } else {
      snprintf(line2, sizeof(line2), "GW: Lost %us ago", age_s);
    }
  } else if (mode == NodeMode::DISCOVERING) {
    snprintf(line2, sizeof(line2), "GW: Searching");
  } else {
    snprintf(line2, sizeof(line2), "GW: Connecting");
  }

  // Line 3: Sensor count
  uint8_t total_sensors = i2c_sensor_count + ble_sensor_count;
  if (total_sensors > 0) {
    snprintf(line3, sizeof(line3), "Sensors: %u", total_sensors);
  } else {
    snprintf(line3, sizeof(line3), "Sensors: None");
  }

  updateDisplay(line1, line2, line3);
}

void loadDeviceUid() {
  lorapnp::loadDeviceUid(device_uid);  // shared MAC->UID derivation (lib/DeviceIdentity.h)
}

void printUid() {
  Serial.printf("UID: %02X:%02X:%02X:%02X:%02X:%02X\n",
                device_uid[0], device_uid[1], device_uid[2],
                device_uid[3], device_uid[4], device_uid[5]);
}

bool loadConfig() {
  prefs.begin("lorapnp", true);
  network_id = prefs.getUShort("net", 0);
  node_id = prefs.getUShort("node", UNASSIGNED_ID);
  report_interval_ms = prefs.getUInt("report", DEFAULT_REPORT_INTERVAL_MS);
  sleep_interval_ms = prefs.getUInt("sleep", DEFAULT_SLEEP_INTERVAL_MS);
  sleep_enabled = prefs.getBool("sleep_en", false);
  i2c_sensors_enabled = prefs.getBool("i2c_en", true);
  prefs.getString("custom_name", custom_name, sizeof(custom_name));
  prefs.end();
  return network_id != 0 && node_id != UNASSIGNED_ID;
}

void saveConfig() {
  prefs.begin("lorapnp", false);
  prefs.putUShort("net", network_id);
  prefs.putUShort("node", node_id);
  prefs.putUInt("report", report_interval_ms);
  prefs.putUInt("sleep", sleep_interval_ms);
  prefs.putBool("sleep_en", sleep_enabled);
  prefs.putBool("i2c_en", i2c_sensors_enabled);
  prefs.putString("custom_name", custom_name);
  prefs.end();
}

void saveSensorName(uint8_t sensor_id, const char* name) {
  prefs.begin("lorapnp", false);
  char key[16];
  snprintf(key, sizeof(key), "sen_%u", sensor_id);
  prefs.putString(key, name);
  prefs.end();
}

void saveRemoteSensorName(const uint8_t* uid, const char* name) {
  prefs.begin("lorapnp", false);
  char key[20];
  snprintf(key, sizeof(key), "ble_%02X%02X%02X%02X%02X%02X",
           uid[0], uid[1], uid[2], uid[3], uid[4], uid[5]);
  prefs.putString(key, name);
  prefs.end();
}

void factoryReset() {
  prefs.begin("lorapnp", false);
  prefs.clear();
  prefs.end();
  Serial.println("Factory reset complete. Restarting...");
  updateDisplay("T3-S3 Node", "Factory reset", "Restarting...");
  delay(2000);
  ESP.restart();
}

bool sendPacket(Packet& pkt) {
  uint8_t buf[HEADER_SIZE + MAX_PAYLOAD_SIZE + 2];
  size_t len = 0;
  Status status = pkt.serialize(buf, &len);
  if (status != Status::OK) {
    Serial.printf("Packet serialize failed: %d\n", static_cast<int>(status));
    return false;
  }

  digitalWrite(LED_PIN, HIGH);
  int state = radio.transmit(buf, len);
  digitalWrite(LED_PIN, LOW);
  radio.startReceive();

  if (state != RADIOLIB_ERR_NONE) {
    Serial.printf("TX failed: %d\n", state);
    recordRadioFailure();
    return false;
  }

  recordRadioSuccess();
  return true;
}

bool sendPendingStatus() {
  if (sendPacket(pending_status)) {
    status_ack_pending = true;
    pending_status_seq = pending_status.seq;
    status_sent_ms = millis();
    Serial.printf("STATUS sent seq=%u retry=%u\n", pending_status.seq, status_retry_count);
    updateDisplay("T3-S3 Node", "STATUS sent", "Node " + String(node_id));
    return true;
  }

  return false;
}

void sendAck(uint16_t dst_id, uint16_t ack_seq) {
  delay(ACK_TURNAROUND_DELAY_MS);

  Packet ack;
  ack.type = MessageType::ACK;
  ack.src_id = node_id;
  ack.dst_id = dst_id;
  ack.seq = tx_seq++;
  ack.payload_len = 2;
  putU16(ack.payload, ack_seq);

  if (sendPacket(ack)) {
    Serial.printf("ACK sent to %u for seq %u\n", dst_id, ack_seq);
  }
}

void sendDiscover() {
  Packet pkt;
  pkt.type = MessageType::DISCOVER;
  pkt.flags = FLAG_BROADCAST;
  pkt.src_id = UNASSIGNED_ID;
  pkt.dst_id = BROADCAST_ID;
  pkt.seq = tx_seq++;
  pkt.payload_len = DEVICE_UID_LEN;
  memcpy(pkt.payload, device_uid, DEVICE_UID_LEN);

  if (sendPacket(pkt)) {
    Serial.println("DISCOVER sent");
    updateDisplay("T3-S3 Node", "DISCOVER sent");
    mode = NodeMode::WAIT_OFFER;
    last_action_ms = millis();
  }
}

void sendJoinRequest() {
  bool reconnecting = session_token == 0 && network_id != 0 && node_id != UNASSIGNED_ID;

  Packet pkt;
  pkt.type = MessageType::JOIN_REQUEST;
  pkt.src_id = reconnecting ? node_id : UNASSIGNED_ID;
  pkt.dst_id = GATEWAY_ID;
  pkt.seq = tx_seq++;
  pkt.payload_len = reconnecting ? DEVICE_UID_LEN + 6 : DEVICE_UID_LEN + 2;
  memcpy(pkt.payload, device_uid, DEVICE_UID_LEN);
  putU16(&pkt.payload[DEVICE_UID_LEN], session_token);
  if (reconnecting) {
    putU16(&pkt.payload[DEVICE_UID_LEN + 2], network_id);
    putU16(&pkt.payload[DEVICE_UID_LEN + 4], node_id);
  }

  if (sendPacket(pkt)) {
    Serial.println(reconnecting ? "REJOIN_REQUEST sent" : "JOIN_REQUEST sent");
    updateDisplay("T3-S3 Node", reconnecting ? "REJOIN_REQUEST" : "JOIN_REQUEST");
    mode = NodeMode::WAIT_JOIN_ACCEPT;
    last_action_ms = millis();
  }
}

void sendStatus() {
  const uint16_t battery_mv = 3700;
  uint8_t status_flags = 0x00;
#ifdef ENABLE_BLE_SCAN
  // This field device runs the BLE-scan-and-forward path, so it can relay
  // remote-sensor data — report that capability to the gateway.
  status_flags |= STATUS_FLAG_CAN_RELAY;
#endif

  pending_status = Packet();
  pending_status.type = MessageType::STATUS;
  pending_status.setFlag(FLAG_ACK_REQUEST);
  pending_status.src_id = node_id;
  pending_status.dst_id = GATEWAY_ID;
  pending_status.seq = tx_seq++;
  pending_status.payload_len = 17;
  putU16(&pending_status.payload[0], battery_mv);
  putU32(&pending_status.payload[2], millis() / 1000);
  putU32(&pending_status.payload[6], ESP.getFreeHeap());
  pending_status.payload[10] = status_flags;
  memcpy(&pending_status.payload[11], device_uid, DEVICE_UID_LEN);

  status_retry_count = 0;
  sendPendingStatus();
}

void sendSensorDescriptor() {
  const char label[] = "Fake Temp";

  Packet pkt;
  pkt.type = MessageType::SENSOR_DESCRIPTOR;
  pkt.src_id = node_id;
  pkt.dst_id = GATEWAY_ID;
  pkt.seq = tx_seq++;
  pkt.payload_len = 4 + strlen(label);
  pkt.payload[0] = FAKE_SENSOR_ID;
  pkt.payload[1] = SENSOR_TYPE_FAKE;
  pkt.payload[2] = SENSOR_UNIT_CELSIUS;
  pkt.payload[3] = 0x00;
  memcpy(&pkt.payload[4], label, strlen(label));

  if (sendPacket(pkt)) {
    sensor_descriptor_sent = true;
    Serial.printf("SENSOR_DESC sent sensor=%u label=%s\n", FAKE_SENSOR_ID, label);
  }
}

bool sendPendingSensorData();

// ========== I2C SENSOR DETECTION ==========
// Report bus devices that ACK but match no driver, so an installer can see a
// miswired or unsupported sensor instead of it silently vanishing. Each such
// address is appended to i2c_sensors[] as a SENSOR_TYPE_UNKNOWN_I2C entry
// (descriptor sent, but no data — the read path skips unknown types).
// `known` holds the addresses already claimed by drivers this scan; matches
// are skipped. Returns the number of unknown devices appended.
uint8_t reportUnknownI2CDevices(const uint8_t* known, uint8_t known_count) {
  uint8_t added = 0;
  for (uint8_t addr = 0x03; addr < 0x78; addr++) {
    if (i2c_sensor_count + added >= MAX_I2C_SENSORS) break;

    Wire1.beginTransmission(addr);
    if (Wire1.endTransmission() != 0) continue;  // nothing at this address

    bool claimed = false;
    for (uint8_t i = 0; i < known_count; i++) {
      if (known[i] == addr) { claimed = true; break; }
    }
    if (claimed) continue;  // a driver already owns this device

    // IDs 100+ keep unknowns clear of the driver-assigned range (2..~10) and
    // are stable across rescans since the bus enumerates in address order.
    I2CSensor& s = i2c_sensors[i2c_sensor_count + added];
    s.sensor_id = 100 + added;
    s.i2c_addr = addr;
    s.sensor_type = SENSOR_TYPE_UNKNOWN_I2C;
    snprintf(s.label, sizeof(s.label), "Unknown 0x%02X", addr);
    s.active = true;
    Serial.printf("? Unclaimed I2C device at 0x%02X (reported as unknown)\n", addr);
    added++;
  }
  return added;
}

void scanI2CSensors() {
  Serial.println("Scanning I2C bus...");

  // List every device on the QWIIC bus (raw addresses + best-guess names).
  scanI2CBus(&Wire1);

  // Auto-detect known sensors via the driver registry (sensors/SensorRegistry).
  // Sensor IDs start at 2 — id 1 is reserved for the legacy fake-sensor path.
  i2c_sensor_count = detectAllI2CSensors(&Wire1, i2c_sensors, MAX_I2C_SENSORS, 2);

  // Note any bus devices no driver claimed.
  uint8_t known[MAX_I2C_SENSORS];
  for (uint8_t i = 0; i < i2c_sensor_count; i++) known[i] = i2c_sensors[i].i2c_addr;
  i2c_sensor_count += reportUnknownI2CDevices(known, i2c_sensor_count);

  Serial.printf("Detected %u I2C sensor(s)\n", i2c_sensor_count);
}

void sendI2CSensorDescriptors() {
  for (uint8_t i = 0; i < i2c_sensor_count; i++) {
    Packet pkt;
    pkt.type = MessageType::SENSOR_DESCRIPTOR;
    pkt.src_id = node_id;
    pkt.dst_id = GATEWAY_ID;
    pkt.seq = tx_seq++;
    pkt.payload_len = 4 + strlen(i2c_sensors[i].label);
    pkt.payload[0] = i2c_sensors[i].sensor_id;
    pkt.payload[1] = i2c_sensors[i].sensor_type;

    if (i2c_sensors[i].sensor_type == lorapnp::SENSOR_TYPE_TEMPERATURE) {
      pkt.payload[2] = SENSOR_UNIT_CELSIUS;
    } else if (i2c_sensors[i].sensor_type == lorapnp::SENSOR_TYPE_HUMIDITY) {
      pkt.payload[2] = SENSOR_UNIT_PERCENT;
    } else if (i2c_sensors[i].sensor_type == lorapnp::SENSOR_TYPE_PRESSURE) {
      pkt.payload[2] = SENSOR_UNIT_HPA;
    } else {
      pkt.payload[2] = SENSOR_UNIT_RAW;
    }

    pkt.payload[3] = 0x00;
    memcpy(&pkt.payload[4], i2c_sensors[i].label, strlen(i2c_sensors[i].label));

    if (sendPacket(pkt)) {
      Serial.printf("SENSOR_DESC sent: %s (id=%u)\n", i2c_sensors[i].label, i2c_sensors[i].sensor_id);
    }
    delay(100);
  }
}

void sendI2CSensorData() {
  // Read the SHT40 once up front if any temp/humidity sensor is registered.
  sensors_event_t humidity, temp;
  bool have_sht = false;
  for (uint8_t i = 0; i < i2c_sensor_count; i++) {
    if (i2c_sensors[i].sensor_type == lorapnp::SENSOR_TYPE_TEMPERATURE ||
        i2c_sensors[i].sensor_type == lorapnp::SENSOR_TYPE_HUMIDITY) {
      have_sht = sht4.getEvent(&humidity, &temp);
      break;
    }
  }

  for (uint8_t i = 0; i < i2c_sensor_count; i++) {
    if (!i2c_sensors[i].active) continue;

    // Resolve this sensor's current value by type; skip types we can't read.
    float value = 0.0f;
    const char* unit = "";
    switch (i2c_sensors[i].sensor_type) {
      case lorapnp::SENSOR_TYPE_TEMPERATURE:
        if (!have_sht) continue;
        value = temp.temperature; unit = "C"; break;
      case lorapnp::SENSOR_TYPE_HUMIDITY:
        if (!have_sht) continue;
        value = humidity.relative_humidity; unit = "%"; break;
      case lorapnp::SENSOR_TYPE_PRESSURE:
        value = bmp5.readPressure() / 100.0f; unit = "hPa"; break;
      default:
        continue;  // unknown/unsupported type — nothing to sample
    }

    Packet pkt;
    pkt.type = MessageType::SENSOR_DATA;
    pkt.setFlag(FLAG_ACK_REQUEST);
    pkt.src_id = node_id;
    pkt.dst_id = GATEWAY_ID;
    pkt.seq = tx_seq++;
    pkt.payload_len = 6;
    pkt.payload[0] = i2c_sensors[i].sensor_id;
    pkt.payload[1] = SENSOR_DATA_FLOAT32;
    memcpy(&pkt.payload[2], &value, sizeof(value));

    if (sendPacket(pkt)) {
      Serial.printf("%s: %.2f %s\n", i2c_sensors[i].label, (double)value, unit);
    }
    delay(100);
  }
}
// ========== END I2C SENSOR DETECTION ==========

// ========== BLE REMOTE SENSOR FUNCTIONS ==========
#ifdef ENABLE_BLE_SCAN

int findBLESensorByUID(const uint8_t* uid) {
  for (uint8_t i = 0; i < ble_sensor_count; i++) {
    if (memcmp(ble_sensors[i].device_uid, uid, 6) == 0) {
      return i;
    }
  }
  return -1;
}

int registerBLESensor(const uint8_t* uid) {
  if (ble_sensor_count >= MAX_BLE_SENSORS_PER_NODE) {
    Serial.println("BLE sensor registry full");
    return -1;
  }

  int slot = ble_sensor_count++;
  ble_sensors[slot].active = true;
  memcpy(ble_sensors[slot].device_uid, uid, 6);
  ble_sensors[slot].sensor_count = 0;
  ble_sensors[slot].rssi = 0;
  ble_sensors[slot].battery_mv = 0;
  ble_sensors[slot].last_seen = millis();
  ble_sensors[slot].flags = 0;

  char uid_str[18];
  formatBLEUID(uid, uid_str, sizeof(uid_str));
  Serial.printf("Registered BLE sensor: %s (slot %d)\n", uid_str, slot);

  return slot;
}

void forwardBLESensorData(const BLESensorAdvertisement* adv, int rssi) {
  if (mode != NodeMode::JOINED) {
    Serial.println("Not joined, cannot forward BLE data");
    return;
  }

  // Find or register BLE sensor
  int slot = findBLESensorByUID(adv->device_uid);
  if (slot < 0) {
    slot = registerBLESensor(adv->device_uid);
    if (slot < 0) return;
  }

  // Update sensor info
  ble_sensors[slot].rssi = rssi;
  ble_sensors[slot].battery_mv = adv->battery_mv;
  ble_sensors[slot].last_seen = millis();
  ble_sensors[slot].flags = adv->flags;
  ble_sensors[slot].sensor_count = adv->sensor_count;

  char uid_str[18];
  formatBLEUID(adv->device_uid, uid_str, sizeof(uid_str));

  // Forward each sensor reading to gateway via LoRa
  for (uint8_t i = 0; i < adv->sensor_count && i < 2; i++) {
    const BLESensorReading& reading = adv->readings[i];

    // Build BLE_SENSOR_DATA packet
    Packet pkt;
    pkt.type = MessageType::BLE_SENSOR_DATA;
    pkt.src_id = node_id;
    pkt.dst_id = GATEWAY_ID;
    pkt.seq = tx_seq++;
    pkt.payload_len = sizeof(BLESensorDataPayload);

    BLESensorDataPayload* payload = (BLESensorDataPayload*)pkt.payload;
    createBLESensorDataPayload(*payload, adv->device_uid, rssi,
                               adv->battery_mv, reading, millis());

    if (sendPacket(pkt)) {
      float value = (reading.data_type == SENSOR_DATA_FLOAT32) ?
                    reading.value.float_val : reading.value.uint_val;
      Serial.printf("Forwarded BLE %s sensor %u: %.2f RSSI=%d\n",
                    uid_str, reading.sensor_id, value, rssi);
    }

    delay(100);  // Small delay between packets
  }
}

class BLEAdvertisedDeviceCallback : public NimBLEAdvertisedDeviceCallbacks {
  void onResult(NimBLEAdvertisedDevice* advertisedDevice) {
    std::string mfg_data = advertisedDevice->getManufacturerData();
    if (mfg_data.length() != sizeof(BLESensorAdvertisement)) return;

    const BLESensorAdvertisement* adv =
        (const BLESensorAdvertisement*)mfg_data.data();

    if (!validateBLEAdvertisement(adv)) return;

    // Found LoRa PNP BLE sensor advertisement
    int rssi = advertisedDevice->getRSSI();

    char uid_str[18];
    formatBLEUID(adv->device_uid, uid_str, sizeof(uid_str));

    Serial.printf("BLE sensor found: %s RSSI=%d sensors=%u battery=%u mV\n",
                  uid_str, rssi, adv->sensor_count, adv->battery_mv);

    // Forward to gateway
    forwardBLESensorData(adv, rssi);
  }
};

void initBLEScanning() {
  Serial.println("Initializing BLE scanner...");

  NimBLEDevice::init("LoRaPNP-Node");
  ble_scanner = NimBLEDevice::getScan();
  ble_scanner->setAdvertisedDeviceCallbacks(new BLEAdvertisedDeviceCallback());
  ble_scanner->setActiveScan(BLE_SCAN_ACTIVE);
  ble_scanner->setInterval(100);
  ble_scanner->setWindow(99);

  Serial.println("BLE scanner initialized");
}

void performBLEScan() {
  if (!ble_scan_enabled) return;
  if (mode != NodeMode::JOINED) return;

  uint32_t now = millis();
  if (now - last_ble_scan_ms < BLE_SCAN_INTERVAL_MS) return;

  last_ble_scan_ms = now;

  Serial.printf("Starting BLE scan for %d seconds...\n", BLE_SCAN_DURATION_S);
  updateDisplay("T3-S3 Node", "BLE scanning...", "Node " + String(node_id));

  NimBLEScanResults results = ble_scanner->start(BLE_SCAN_DURATION_S, false);

  Serial.printf("BLE scan complete: %d devices found\n", results.getCount());
  updateDisplay("T3-S3 Node",
                String("BLE: ") + String(ble_sensor_count),
                "Node " + String(node_id));
}

void printBLESensorStats() {
  Serial.printf("\n=== BLE Remote Sensors [%d/%d] ===\n",
                ble_sensor_count, MAX_BLE_SENSORS_PER_NODE);
  if (ble_sensor_count == 0) {
    Serial.println("No BLE sensors registered");
    return;
  }

  Serial.println("UID               RSSI  Batt  Sensors  LastSeen  Flags");
  Serial.println("----------------  ----  ----  -------  --------  -----");

  for (uint8_t i = 0; i < ble_sensor_count; i++) {
    if (!ble_sensors[i].active) continue;

    char uid_str[18];
    formatBLEUID(ble_sensors[i].device_uid, uid_str, sizeof(uid_str));

    uint32_t age_s = (millis() - ble_sensors[i].last_seen) / 1000;

    Serial.printf("%-16s  %-4d  %-4u  %-7u  %-8us  0x%02X\n",
                  uid_str,
                  ble_sensors[i].rssi,
                  ble_sensors[i].battery_mv,
                  ble_sensors[i].sensor_count,
                  age_s,
                  ble_sensors[i].flags);
  }
  Serial.println();
}

#endif // ENABLE_BLE_SCAN
// ========== END BLE REMOTE SENSOR FUNCTIONS ==========

void sendFakeSensorData() {
  float value = 20.0f + ((millis() / 1000) % 100) / 10.0f;

  pending_sensor_data = Packet();
  pending_sensor_data.type = MessageType::SENSOR_DATA;
  pending_sensor_data.setFlag(FLAG_ACK_REQUEST);
  pending_sensor_data.src_id = node_id;
  pending_sensor_data.dst_id = GATEWAY_ID;
  pending_sensor_data.seq = tx_seq++;
  pending_sensor_data.payload_len = 6;
  pending_sensor_data.payload[0] = FAKE_SENSOR_ID;
  pending_sensor_data.payload[1] = SENSOR_DATA_FLOAT32;
  memcpy(&pending_sensor_data.payload[2], &value, sizeof(value));

  sensor_data_retry_count = 0;
  sendPendingSensorData();
}

bool sendPendingSensorData() {
  if (sendPacket(pending_sensor_data)) {
    sensor_data_ack_pending = true;
    pending_sensor_data_seq = pending_sensor_data.seq;
    sensor_data_sent_ms = millis();
    Serial.printf("SENSOR_DATA sent seq=%u retry=%u\n", pending_sensor_data.seq, sensor_data_retry_count);
    return true;
  }
  return false;
}

bool offerMatchesThisNode(const Packet& pkt) {
  return pkt.payload_len >= 12 && memcmp(&pkt.payload[6], device_uid, DEVICE_UID_LEN) == 0;
}

void handleOffer(const Packet& pkt) {
  if (!offerMatchesThisNode(pkt)) {
    Serial.println("OFFER ignored: UID mismatch");
    return;
  }

  session_token = getU16(&pkt.payload[0]);
  network_id = getU16(&pkt.payload[2]);
  Serial.printf("OFFER accepted: token=%u network=%u\n", session_token, network_id);
  sendJoinRequest();
}

void handleJoinAccept(const Packet& pkt) {
  if (pkt.payload_len < 14) return;

  last_gateway_contact_ms = millis();  // Track gateway communication
  network_id = getU16(&pkt.payload[0]);
  node_id = getU16(&pkt.payload[2]);
  report_interval_ms = getU32(&pkt.payload[4]);
  saveConfig();

  mode = NodeMode::JOINED;
  last_status_ms = millis() - report_interval_ms;
  last_sensor_ms = millis() - SENSOR_REPORT_INTERVAL_MS;
  sensor_descriptor_sent = false;
  Serial.printf("JOIN_ACCEPT: network=%u node=%u report=%lu ms\n",
                network_id, node_id, (unsigned long)report_interval_ms);
  updateDisplay("T3-S3 Node", "Joined", "Node " + String(node_id));

  if (i2c_sensors_enabled) {
    scanI2CSensors();
  }

  if (pkt.flagSet(FLAG_ACK_REQUEST)) {
    sendAck(pkt.src_id, pkt.seq);
  }
}

void handleAck(const Packet& pkt) {
  if (pkt.payload_len < 2) return;
  if (pkt.dst_id != node_id) return;

  last_gateway_contact_ms = millis();  // Track gateway communication
  uint16_t ack_seq = getU16(pkt.payload);
  if (status_ack_pending && ack_seq == pending_status_seq) {
    status_ack_pending = false;
    Serial.printf("STATUS ACK received seq=%u\n", ack_seq);
  } else if (sensor_data_ack_pending && ack_seq == pending_sensor_data_seq) {
    sensor_data_ack_pending = false;
    Serial.printf("SENSOR_DATA ACK received seq=%u\n", ack_seq);
  } else {
    Serial.printf("ACK ignored seq=%u\n", ack_seq);
  }
}

void handleConfigUpdate(const Packet& pkt) {
  if (pkt.payload_len < 10) return;
  if (pkt.dst_id != node_id) return;

  last_gateway_contact_ms = millis();  // Track gateway communication
  uint32_t new_report_interval_ms = getU32(&pkt.payload[0]);
  uint32_t new_sleep_interval_ms = getU32(&pkt.payload[4]);
  uint8_t new_sf = pkt.payload[8];
  uint8_t new_tx_power = pkt.payload[9];

  Serial.printf("CONFIG_UPDATE received: report=%lu ms sleep=%lu ms sf=%u tx_pwr=%u\n",
                (unsigned long)new_report_interval_ms, (unsigned long)new_sleep_interval_ms,
                new_sf, new_tx_power);

  report_interval_ms = new_report_interval_ms;
  sleep_interval_ms = new_sleep_interval_ms;
  saveConfig();

  Serial.printf("Config applied and saved: report=%lu ms sleep=%lu ms\n",
                (unsigned long)report_interval_ms, (unsigned long)sleep_interval_ms);
  updateDisplay("T3-S3 Node", "Config updated", "Interval: " + String(report_interval_ms / 1000) + "s");

  if (pkt.flagSet(FLAG_ACK_REQUEST)) {
    sendAck(pkt.src_id, pkt.seq);
  }

  Packet ack;
  ack.type = MessageType::CONFIG_ACK;
  ack.src_id = node_id;
  ack.dst_id = pkt.src_id;
  ack.seq = tx_seq++;
  ack.payload_len = 2;
  putU16(ack.payload, pkt.seq);

  if (sendPacket(ack)) {
    Serial.printf("CONFIG_ACK sent seq=%u\n", pkt.seq);
  }
}

void handleNameUpdate(const Packet& pkt) {
  if (pkt.dst_id != node_id) return;
  if (pkt.payload_len < 2) return;

  last_gateway_contact_ms = millis();  // Track gateway communication
  NameType name_type = static_cast<NameType>(pkt.payload[0]);

  if (name_type == NameType::STATION) {
    // Station name
    size_t name_len = pkt.payload_len - 1;
    if (name_len > 32) name_len = 32;

    memset(custom_name, 0, sizeof(custom_name));
    memcpy(custom_name, &pkt.payload[1], name_len);
    custom_name[name_len] = '\0';

    Serial.printf("NAME_UPDATE (station) received: %s\n", custom_name);
    saveConfig();

    updateDisplay(custom_name[0] ? custom_name : ("Station #" + String(node_id)).c_str(),
                  "Name updated",
                  "Saved!");

  } else if (name_type == NameType::I2C_SENSOR) {
    // I2C Sensor name
    if (pkt.payload_len < 3) return;

    uint8_t sensor_id = pkt.payload[1];
    size_t name_len = pkt.payload_len - 2;
    if (name_len > 32) name_len = 32;

    char sensor_name[33];
    memset(sensor_name, 0, sizeof(sensor_name));
    memcpy(sensor_name, &pkt.payload[2], name_len);
    sensor_name[name_len] = '\0';

    Serial.printf("NAME_UPDATE (sensor) received: ID=%u Name=%s\n", sensor_id, sensor_name);
    saveSensorName(sensor_id, sensor_name);

    // Store in runtime array
    for (uint8_t i = 0; i < MAX_SENSOR_NAMES; i++) {
      if (!sensor_names[i].valid || sensor_names[i].sensor_id == sensor_id) {
        sensor_names[i].sensor_id = sensor_id;
        strncpy(sensor_names[i].name, sensor_name, 32);
        sensor_names[i].name[32] = '\0';
        sensor_names[i].valid = true;
        break;
      }
    }

  } else if (name_type == NameType::REMOTE_SENSOR) {
    // Remote BLE sensor name
    if (pkt.payload_len < 8) return;

    uint8_t ble_uid[6];
    memcpy(ble_uid, &pkt.payload[1], 6);

    size_t name_len = pkt.payload_len - 7;
    if (name_len > 32) name_len = 32;

    char remote_name[33];
    memset(remote_name, 0, sizeof(remote_name));
    memcpy(remote_name, &pkt.payload[7], name_len);
    remote_name[name_len] = '\0';

    char uid_str[18];
    snprintf(uid_str, sizeof(uid_str), "%02X:%02X:%02X:%02X:%02X:%02X",
             ble_uid[0], ble_uid[1], ble_uid[2], ble_uid[3], ble_uid[4], ble_uid[5]);

    Serial.printf("NAME_UPDATE (remote) received: UID=%s Name=%s\n", uid_str, remote_name);
    saveRemoteSensorName(ble_uid, remote_name);

    // Store in runtime array
    for (uint8_t i = 0; i < MAX_BLE_SENSORS_PER_NODE; i++) {
      if (!remote_sensor_names[i].valid ||
          memcmp(remote_sensor_names[i].device_uid, ble_uid, 6) == 0) {
        memcpy(remote_sensor_names[i].device_uid, ble_uid, 6);
        strncpy(remote_sensor_names[i].name, remote_name, 32);
        remote_sensor_names[i].name[32] = '\0';
        remote_sensor_names[i].valid = true;
        break;
      }
    }
  }
}

void handleRebootCommand(const Packet& pkt) {
  if (pkt.dst_id != node_id) return;
  if (pkt.payload_len < 2) {
    Serial.println("Invalid REBOOT payload");
    return;
  }

  uint8_t delay_seconds = pkt.payload[0];

  Serial.println("========================================");
  Serial.println("⚠️  REBOOT command received from gateway");
  Serial.printf("    Delay: %u seconds\n", delay_seconds);
  Serial.println("========================================");

  // Send ACK before rebooting (best effort)
  sendAck(pkt.src_id, pkt.seq);

  // Give time for ACK to transmit
  delay(100);

  // Save reboot reason to NVS for debugging
  prefs.begin("lorapnp", false);
  prefs.putUChar("last_reboot_reason", 0xFF);  // 0xFF = remote reboot
  prefs.putULong("last_reboot_time", millis());
  prefs.end();

  Serial.println("Rebooting now...");
  Serial.flush();

  // Wait the requested delay
  if (delay_seconds > 0) {
    delay(delay_seconds * 1000);
  }

  // Reboot
  ESP.restart();
  // Does not return
}

void handlePacket(const Packet& pkt) {
  Serial.printf("RX: %s src=%u dst=%u seq=%u\n", pkt.typeStr(), pkt.src_id, pkt.dst_id, pkt.seq);

  if (mode != NodeMode::JOINED && pkt.type == MessageType::OFFER) {
    handleOffer(pkt);
  } else if (pkt.type == MessageType::JOIN_ACCEPT) {
    handleJoinAccept(pkt);
  } else if (pkt.type == MessageType::ACK) {
    handleAck(pkt);
  } else if (pkt.type == MessageType::CONFIG_UPDATE && mode == NodeMode::JOINED) {
    handleConfigUpdate(pkt);
  } else if (pkt.type == MessageType::NAME_UPDATE && mode == NodeMode::JOINED) {
    handleNameUpdate(pkt);
  } else if (pkt.type == MessageType::REBOOT && mode == NodeMode::JOINED) {
    handleRebootCommand(pkt);
  }
}

void pollRadio() {
  if (digitalRead(LORA_DIO0) != HIGH) return;

  uint8_t buf[HEADER_SIZE + MAX_PAYLOAD_SIZE + 2];
  size_t len = radio.getPacketLength();
  if (len > sizeof(buf)) {
    Serial.printf("RX too large: %d\n", (int)len);
    radio.startReceive();
    return;
  }

  int state = radio.readData(buf, len);

  // Capture RSSI and SNR immediately after receive
  last_gateway_rssi = radio.getRSSI();
  last_gateway_snr = radio.getSNR();
  last_gateway_rx_ms = millis();

  radio.startReceive();
  if (state != RADIOLIB_ERR_NONE) {
    Serial.printf("RX error: %d\n", state);
    return;
  }

  recordRadioSuccess();  // Successful radio RX operation

  Packet pkt;
  Status status = pkt.deserialize(buf, len);
  if (status != Status::OK) {
    Serial.printf("RX packet rejected: %d len=%d\n", static_cast<int>(status), (int)len);
    return;
  }

  handlePacket(pkt);
}

void setup() {
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  Wire.begin(OLED_SDA, OLED_SCL);
  Wire1.begin(QWIIC_SDA, QWIIC_SCL);
  display.init();
  display.flipScreenVertically();
  display.setFont(ArialMT_Plain_10);
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  updateDisplay("T3-S3 Node", "Initializing...");

  Serial.begin(115200);
  delay(1000);
  Serial.println("\n=== LoRa PNP Node ===");

  esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();
  if (wakeup_reason == ESP_SLEEP_WAKEUP_TIMER) {
    Serial.println("Woke from deep sleep");
  } else {
    Serial.println("Cold boot");
  }

  loadDeviceUid();
  printUid();

  SPI.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_CS);
  int state = radio.begin(LORA_FREQ, LORA_BW, LORA_SF, LORA_CR, LORA_SYNC, LORA_PWR, 8);
  if (state != RADIOLIB_ERR_NONE) {
    Serial.printf("Radio init failed: %d\n", state);
    updateDisplay("T3-S3 Node", "Radio FAILED", String(state));
    while (true) {
      digitalWrite(LED_PIN, HIGH);
      delay(1000);
      digitalWrite(LED_PIN, LOW);
      delay(1000);
    }
  }

  Serial.println("Radio init OK");
  if (loadConfig()) {
    mode = NodeMode::WAIT_JOIN_ACCEPT;
    session_token = 0;
    Serial.printf("Loaded config: network=%u node=%u report=%lu ms sleep=%lu ms sleep_en=%d\n",
                  network_id, node_id, (unsigned long)report_interval_ms,
                  (unsigned long)sleep_interval_ms, sleep_enabled);
    updateDisplay("T3-S3 Node", "Reconnecting", "Node " + String(node_id));
  } else {
    randomSeed(esp_random());
    next_retry_ms = millis() + random(250, 1500);
    updateDisplay("T3-S3 Node", "Searching...");
  }

  radio.startReceive();
  radio_last_success_ms = millis();  // Initialize radio watchdog timestamp
  last_gateway_contact_ms = millis();  // Initialize gateway contact tracking
  if (mode == NodeMode::WAIT_JOIN_ACCEPT && session_token == 0) {
    sendJoinRequest();
  }

#ifdef ENABLE_BLE_SCAN
  if (ble_scan_enabled) {
    initBLEScanning();
  }
#endif

  awake_start_ms = millis();
}

void enterDeepSleep() {
  Serial.printf("Entering deep sleep for %lu ms\n", (unsigned long)sleep_interval_ms);
  updateDisplay("T3-S3 Node", "Sleeping...", String(sleep_interval_ms / 1000) + "s");
  delay(100);

  radio.sleep();
  esp_sleep_enable_timer_wakeup(sleep_interval_ms * 1000ULL);
  esp_deep_sleep_start();
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
  updateDisplay("Radio Reset", "Recovering...", String(radio_consecutive_failures) + " failures");

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

void loop() {
  if (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();

    if (cmd == "reset" || cmd == "factory") {
      Serial.println("Factory reset requested");
      factoryReset();
    } else if (cmd == "status") {
      Serial.printf("Mode: %s Network: %u Node: %u Interval: %lu ms Sleep: %d\n",
                    mode == NodeMode::DISCOVERING ? "DISCOVERING" :
                    mode == NodeMode::WAIT_OFFER ? "WAIT_OFFER" :
                    mode == NodeMode::WAIT_JOIN_ACCEPT ? "WAIT_JOIN_ACCEPT" : "JOINED",
                    network_id, node_id, (unsigned long)report_interval_ms, sleep_enabled);
    } else if (cmd == "sleep on") {
      sleep_enabled = true;
      saveConfig();
      Serial.println("Deep sleep enabled");
    } else if (cmd == "sleep off") {
      sleep_enabled = false;
      saveConfig();
      Serial.println("Deep sleep disabled");
    } else if (cmd == "i2c on") {
      i2c_sensors_enabled = true;
      saveConfig();
      Serial.println("I2C sensors enabled - restart to scan");
    } else if (cmd == "i2c off") {
      i2c_sensors_enabled = false;
      saveConfig();
      Serial.println("I2C sensors disabled");
    } else if (cmd == "i2c scan") {
      if (mode == NodeMode::JOINED && i2c_sensors_enabled) {
        sensor_descriptor_sent = false;
        scanI2CSensors();
        Serial.println("I2C rescan complete");
      } else {
        Serial.println("Must be joined with i2c enabled");
      }
    }
#ifdef ENABLE_BLE_SCAN
    else if (cmd == "ble scan") {
      if (mode == NodeMode::JOINED && ble_scanner) {
        Serial.println("Forcing BLE scan...");
        last_ble_scan_ms = 0;  // Reset timer to force immediate scan
        performBLEScan();
      } else {
        Serial.println("Must be joined with BLE enabled");
      }
    } else if (cmd == "ble stats") {
      printBLESensorStats();
    } else if (cmd == "ble on") {
      ble_scan_enabled = true;
      if (!ble_scanner && mode == NodeMode::JOINED) {
        initBLEScanning();
      }
      Serial.println("BLE scanning enabled");
    } else if (cmd == "ble off") {
      ble_scan_enabled = false;
      Serial.println("BLE scanning disabled");
    }
#endif
    else if (cmd.length() > 0) {
      Serial.println("Commands: reset, factory, status, sleep on/off, i2c on/off/scan, ble scan/stats/on/off");
    }
  }

  pollRadio();

  // Radio watchdog check
  if (radioNeedsReset()) {
    resetRadio();
  }

  // Proactive rejoin (Phase 2) - if joined but no gateway contact for 2+ minutes
  if (mode == NodeMode::JOINED && last_gateway_contact_ms > 0) {
    uint32_t silence_time = millis() - last_gateway_contact_ms;
    if (silence_time > GATEWAY_SILENCE_TIMEOUT_MS) {
      Serial.printf("⚠️ PROACTIVE REJOIN: No gateway contact for %lu seconds, attempting rejoin\n",
                    silence_time / 1000);
      rejoin_attempts++;
      mode = NodeMode::WAIT_JOIN_ACCEPT;
      session_token = 0;
      sendJoinRequest();
      last_gateway_contact_ms = millis();  // Reset to avoid rapid retries
    }
  }

#ifdef ENABLE_BLE_SCAN
  // Perform periodic BLE scanning
  if (ble_scanner && ble_scan_enabled) {
    performBLEScan();
  }
#endif

  uint32_t now = millis();

  // Update status display every 2 seconds
  if (now - last_display_update_ms >= 2000) {
    last_display_update_ms = now;
    updateStatusDisplay();
  }
  if (mode == NodeMode::DISCOVERING && now >= next_retry_ms) {
    sendDiscover();
  }

  if (mode == NodeMode::WAIT_OFFER && now - last_action_ms > OFFER_WAIT_MS) {
    mode = NodeMode::DISCOVERING;
    next_retry_ms = now + random(1000, 5000);
    Serial.println("OFFER timeout, retry scheduled");
    updateDisplay("T3-S3 Node", "Retry discovery");
  }

  if (mode == NodeMode::WAIT_JOIN_ACCEPT && now - last_action_ms > JOIN_ACCEPT_WAIT_MS) {
    if (session_token == 0 && network_id != 0 && node_id != UNASSIGNED_ID) {
      Serial.printf("Reconnect timeout, retrying JOIN_REQUEST (node %u)\n", node_id);
      sendJoinRequest();
    } else {
      mode = NodeMode::DISCOVERING;
      next_retry_ms = now + random(1000, 5000);
      Serial.println("JOIN_ACCEPT timeout, restarting discovery");
      updateDisplay("T3-S3 Node", "Join timeout");
    }
  }

  if (mode == NodeMode::JOINED && now - last_status_ms >= report_interval_ms) {
    if (!status_ack_pending) {
      last_status_ms = now;
      sendStatus();
    }
  }

  if (mode == NodeMode::JOINED && status_ack_pending &&
      now - status_sent_ms >= STATUS_ACK_TIMEOUT_MS) {
    if (status_retry_count < STATUS_MAX_RETRIES) {
      status_retry_count++;
      Serial.printf("STATUS ACK timeout seq=%u retry=%u\n", pending_status_seq, status_retry_count);
      sendPendingStatus();
    } else {
      Serial.printf("STATUS dropped seq=%u after %u retries\n", pending_status_seq, status_retry_count);
      status_ack_pending = false;
      last_status_ms = now;
    }
  }

  if (mode == NodeMode::JOINED && !status_ack_pending && !sensor_descriptor_sent) {
    if (i2c_sensors_enabled) {
      sendI2CSensorDescriptors();
    } else {
      sendSensorDescriptor();
    }
    sensor_descriptor_sent = true;
  }

  if (mode == NodeMode::JOINED && sensor_data_ack_pending &&
      now - sensor_data_sent_ms >= SENSOR_DATA_ACK_TIMEOUT_MS) {
    if (sensor_data_retry_count < SENSOR_DATA_MAX_RETRIES) {
      sensor_data_retry_count++;
      Serial.printf("SENSOR_DATA ACK timeout seq=%u retry=%u\n", pending_sensor_data_seq, sensor_data_retry_count);
      sendPendingSensorData();
    } else {
      Serial.printf("SENSOR_DATA dropped seq=%u after %u retries\n", pending_sensor_data_seq, sensor_data_retry_count);
      sensor_data_ack_pending = false;
      last_sensor_ms = now;
    }
  }

  if (mode == NodeMode::JOINED && !status_ack_pending && !sensor_data_ack_pending &&
      sensor_descriptor_sent && now - last_sensor_ms >= SENSOR_REPORT_INTERVAL_MS) {
    last_sensor_ms = now;
    if (i2c_sensors_enabled) {
      sendI2CSensorData();
    } else {
      sendFakeSensorData();
    }
  }

  if (sleep_enabled && mode == NodeMode::JOINED && !status_ack_pending &&
      !sensor_data_ack_pending && sensor_descriptor_sent) {
    if (!tasks_complete) {
      tasks_complete = true;
      awake_start_ms = millis();
      Serial.printf("Tasks complete, listening for %lu ms before sleep\n",
                    (unsigned long)LISTEN_WINDOW_MS);
    } else if (now - awake_start_ms >= LISTEN_WINDOW_MS) {
      enterDeepSleep();
    }
  }
}
