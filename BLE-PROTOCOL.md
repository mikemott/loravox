# LoRa PNP - BLE Remote Sensor Protocol

## Overview

Three-tier architecture for plug-and-play sensor network with extended range via BLE remote sensors.

```
Remote Sensor (BLE only)
    ↓ BLE 5.0 advertising (100-300m)
Node (BLE + LoRa)
    ↓ LoRa (2-10km)
Gateway (LoRa + WiFi)
    ↓ MQTT/HTTP
Dashboard
```

## Device Profiles

### Profile 1: Gateway
- **Hardware**: T3-S3 with LoRa + WiFi + Display
- **Role**: Network coordinator, data sink
- **Firmware**: `src/main.cpp`
- **Power**: Mains powered (USB or 5V)
- **Capabilities**:
  - Receives LoRa packets from nodes
  - Tracks node and BLE sensor registry
  - WiFi AP + web server (JSON status)
  - MQTT publishing
  - Serial commands for configuration
  - OLED displays network status

### Profile 2: Node
- **Hardware**: T3-S3 with LoRa + BLE + Display + I2C
- **Role**: Bridge between BLE sensors and gateway
- **Firmware**: `node/src/main.cpp`
- **Power**: Battery or mains (USB/solar)
- **Capabilities**:
  - Scans for BLE sensor advertisements
  - Forwards BLE data to gateway via LoRa
  - Optional local I2C sensors
  - Receives config updates from gateway
  - Deep sleep capable (when battery powered)
  - OLED displays node status + BLE sensor count

### Profile 3: Remote Sensor
- **Hardware**: T3-S3 with BLE + Display + I2C
- **Role**: Battery-powered remote sensor
- **Firmware**: `remote_sensor/src/main.cpp` (NEW)
- **Power**: Battery only (CR2032 or AA)
- **Capabilities**:
  - BLE advertising only (no LoRa, no scanning)
  - Reads I2C sensor (SHT40, BMP5xx, etc.)
  - Deep sleep 99% duty cycle
  - OLED displays sensor reading + battery (testing only)
  - 12-24 month battery life

## BLE Advertising Protocol

### Service UUID
- **Primary Service**: `0x4C50` ("LP" in ASCII for LoRa PNP)
- **Manufacturer ID**: `0xFFFF` (test/development use)

### Advertisement Packet Format

```cpp
// Manufacturer Specific Data (max 31 bytes in advertising packet)
struct BLESensorAdvertisement {
    uint16_t company_id;         // 0xFFFF
    uint8_t  protocol_version;   // 0x01
    uint8_t  device_uid[6];      // Unique sensor ID (MAC address)
    uint8_t  sensor_count;       // Number of sensor readings (1-2)

    struct SensorReading {
        uint8_t sensor_id;       // Local sensor ID (1-255)
        uint8_t sensor_type;     // lorapnp::SensorType
        uint8_t sensor_unit;     // lorapnp::SensorUnit
        uint8_t data_type;       // lorapnp::SensorDataType
        union {
            float    float_val;  // 4 bytes
            uint16_t uint_val;   // 2 bytes (padded to 4)
        } value;
    } readings[2];               // Up to 2 readings per advertisement

    uint16_t battery_mv;         // Battery voltage in millivolts
    uint8_t  flags;              // Status flags
} __attribute__((packed));
// Total size: 2+1+6+1+(2×9)+2+1 = 31 bytes (fits perfectly)
```

### Status Flags
```cpp
#define BLE_FLAG_LOW_BATTERY     0x01  // Battery below threshold
#define BLE_FLAG_PAIRING_REQUEST 0x02  // Requesting pairing (future)
#define BLE_FLAG_ERROR           0x04  // Sensor read error
#define BLE_FLAG_FIRST_BOOT      0x08  // First advertisement after boot
```

### Timing Parameters
```cpp
// Remote sensor advertising schedule
constexpr uint32_t BLE_ADV_INTERVAL_MS = 30000;  // 30 seconds between advertisements
constexpr uint32_t BLE_ADV_DURATION_MS = 100;    // Advertise for 100ms
constexpr int8_t   BLE_TX_POWER_DBM = 0;         // 0 dBm (balance range/power)

// Node scanning schedule
constexpr uint32_t BLE_SCAN_INTERVAL_MS = 30000; // Scan every 30 seconds
constexpr uint32_t BLE_SCAN_DURATION_S = 5;      // Scan for 5 seconds
constexpr bool     BLE_SCAN_ACTIVE = false;      // Passive scan (lower power)
```

**Duty Cycles:**
- Remote sensor: 100ms / 30s = **0.33% active**
- Node scanner: 5s / 30s = **16.7% active** (only when battery powered + scanning)

## LoRa Protocol Extensions

### New Message Type

```cpp
// lib/LoRaPNP/src/Types.h
enum class MessageType : uint8_t {
    DISCOVER          = 0x01,
    OFFER             = 0x02,
    JOIN_REQUEST      = 0x03,
    JOIN_ACCEPT       = 0x04,
    DATA              = 0x05,
    ACK               = 0x06,
    CONFIG_UPDATE     = 0x07,
    CONFIG_ACK        = 0x08,
    STATUS            = 0x09,
    SENSOR_DESCRIPTOR = 0x0A,
    SENSOR_DATA       = 0x0B,
    BLE_SENSOR_DATA   = 0x0C,  // NEW - Node forwarding BLE sensor data
};
```

### BLE_SENSOR_DATA Packet

**Sent by**: Node (forwarding data from BLE remote sensor)
**Received by**: Gateway

**Payload Format:**
```cpp
// Payload structure (variable length)
struct BLESensorDataPayload {
    uint8_t  ble_device_uid[6];  // BLE sensor MAC address
    int8_t   rssi;               // BLE RSSI at node
    uint16_t battery_mv;         // BLE sensor battery voltage
    uint8_t  sensor_id;          // Sensor ID from BLE advertisement
    uint8_t  sensor_type;        // lorapnp::SensorType
    uint8_t  sensor_unit;        // lorapnp::SensorUnit
    uint8_t  data_type;          // lorapnp::SensorDataType
    union {
        float    float_val;      // 4 bytes
        uint16_t uint_val;       // 2 bytes
    } value;
    uint32_t timestamp_ms;       // Node's millis() when received
} __attribute__((packed));
// Total: 6+1+2+1+1+1+1+4+4 = 21 bytes per sensor reading
```

**Packet Flags:**
- `FLAG_ACK_REQUEST`: Set if node wants confirmation
- Node can send multiple `BLE_SENSOR_DATA` packets if BLE sensor has multiple readings

## Gateway Registry Extensions

### BLE Sensor Tracking

```cpp
// Gateway (src/main.cpp)
struct BLERemoteSensorInfo {
    uint8_t  device_uid[6];      // BLE sensor unique ID
    uint16_t parent_node_id;     // Which node is bridging this sensor
    uint8_t  sensor_count;       // Number of sensors on this device
    uint8_t  sensor_ids[4];
    uint8_t  sensor_types[4];
    uint8_t  sensor_units[4];
    float    last_values[4];

    int8_t   rssi;               // BLE RSSI at node
    uint16_t battery_mv;
    uint32_t last_seen;          // Gateway's millis()
    bool     active;
};

BLERemoteSensorInfo ble_remote_sensors[16];  // Max 16 BLE sensors across all nodes
```

### Registry Display Format

```
=== Node Registry [2/32] ===
ID   Type    RSSI  SNR   Batt  Sensors  LastSeen
---  ------  ----  ----  ----  -------  --------
2    Node    -45   8.2   3.7V  2        5s
3    Node    -52   7.1   3.6V  1        3s

=== BLE Remote Sensors [2/16] ===
UID           Node  RSSI  Batt  Sensors  LastSeen
------------  ----  ----  ----  -------  --------
AA:BB:CC:...  2     -65   3.0V  2        8s
DD:EE:FF:...  2     -72   2.9V  1        12s
```

## MQTT Publishing

### Topic Structure

**Node with local I2C sensors:**
```
lorapnp/node/2/sensor/1        # Node 2, local sensor ID 1
```

**BLE remote sensor via node:**
```
lorapnp/node/2/ble/AABBCCDDEE/sensor/1    # Node 2 forwarding BLE sensor
```

### Payload Format (JSON)

```json
{
  "value": 23.5,
  "type": 2,
  "unit": 1,
  "rssi": -65,
  "battery_mv": 3000,
  "source": "ble",
  "timestamp": 1234567890
}
```

## Web API Extensions

### GET / (Gateway Status)

```json
{
  "gateway_id": 1,
  "active_nodes": 2,
  "active_ble_sensors": 2,
  "nodes": [
    {
      "id": 2,
      "type": "node",
      "rssi": -45,
      "snr": 8.2,
      "battery": 3.7,
      "sensors": [
        {"id": 1, "type": 2, "unit": 1, "label": "SHT40 Temp"}
      ],
      "ble_sensors": [
        {
          "uid": "AA:BB:CC:DD:EE:FF",
          "rssi": -65,
          "battery_mv": 3000,
          "sensors": [
            {"id": 1, "type": 2, "unit": 1, "label": "Remote SHT40 Temp"},
            {"id": 2, "type": 3, "unit": 2, "label": "Remote SHT40 Humidity"}
          ]
        }
      ]
    }
  ]
}
```

## Firmware Build Configurations

### PlatformIO Environments

```ini
# platformio.ini (root - gateway)
[env:gateway]
platform = espressif32
board = esp32-s3-devkitc-1
build_flags =
    -D PROFILE_GATEWAY=1
upload_port = /dev/cu.usbmodem21201

# node/platformio.ini
[env:node]
platform = espressif32
board = esp32-s3-devkitc-1
build_flags =
    -D PROFILE_NODE=1
    -D ENABLE_BLE_SCAN=1
upload_port = /dev/cu.usbmodem21101

# remote_sensor/platformio.ini (NEW)
[env:remote-sensor]
platform = espressif32
board = esp32-s3-devkitc-1
build_flags =
    -D PROFILE_REMOTE_SENSOR=1
    -D ENABLE_DISPLAY=1        # For testing only
upload_port = /dev/cu.usbmodem22201  # Will need to check actual port
```

## Power Budget Analysis

### Remote Sensor (CR2032 - 220 mAh)

**30-second cycle:**
- Deep sleep: 5 µA × 29.9s = **0.041 mAh/cycle**
- Wake + I2C read: 50 mA × 0.05s = **0.0007 mAh/cycle**
- BLE advertise: 20 mA × 0.1s = **0.00055 mAh/cycle**
- **Total: 0.042 mAh/cycle = 1.21 mAh/day**

**Battery life:** 220 mAh / 1.21 mAh/day = **182 days (6 months)**

**With 2×AA (2500 mAh):** 2500 / 1.21 = **2066 days (5.6 years)**

**Testing mode (display enabled):**
- Display: +30 mA × 0.5s = **0.004 mAh/cycle**
- **Total: 0.046 mAh/cycle = 1.32 mAh/day = 167 days**

### Node (Battery powered, scanning for BLE)

**30-second cycle with BLE scan:**
- Deep sleep: 5 µA × 25s = **0.034 mAh/cycle**
- BLE scan: 50 mA × 5s = **0.069 mAh/cycle**
- LoRa TX (if forwarding): 100 mA × 0.1s = **0.003 mAh/cycle**
- **Total: 0.106 mAh/cycle = 3.05 mAh/day**

**Battery life (2000 mAh):** 2000 / 3.05 = **656 days (1.8 years)**

**Always-on mode (USB powered):**
- No deep sleep, constant scanning
- Recommended for nodes - power consumption not critical

## Testing Phases

### Phase 1: Remote Sensor Standalone
**Hardware:** 1× T3-S3 + SHT40
**Goal:** Validate BLE advertising

**Tests:**
1. Flash remote sensor firmware
2. Use phone app (nRF Connect) to view advertisements
3. Verify manufacturer data format
4. Measure current draw with multimeter
5. Validate 30s cycle timing

**Success:** Advertisement visible, data correct, power under 2 mAh/day

### Phase 2: Node BLE Scanning
**Hardware:** 1× T3-S3 (node) + 1× T3-S3 (remote sensor)
**Goal:** Node receives and logs BLE data

**Tests:**
1. Flash node firmware with BLE scanning
2. Remote sensor advertises nearby
3. Node logs received advertisements to serial
4. Verify RSSI reporting
5. Test range (10m, 50m, 100m)

**Success:** Node consistently receives advertisements, RSSI accurate

### Phase 3: Node→Gateway Bridging
**Hardware:** 2× T3-S3 (gateway + node) + 1× remote sensor
**Goal:** End-to-end data flow

**Tests:**
1. Gateway running, node running, remote sensor advertising
2. Node forwards BLE data to gateway via LoRa
3. Gateway displays BLE sensor in registry
4. Verify MQTT publishing
5. Check web API JSON output

**Success:** BLE sensor data appears on gateway, MQTT, and web UI

### Phase 4: Multi-Sensor Testing
**Hardware:** All 3 boards
**Goal:** Stress test with multiple sensors

**Tests:**
1. Node has local I2C sensor + receives BLE sensor
2. Gateway tracks both sensor sources
3. 30-minute soak test
4. Verify no packet loss
5. Check registry updates

**Success:** All sensors reporting reliably, no crashes

## Serial Commands

### Remote Sensor Commands
```
status         - Show sensor readings and battery
sleep on/off   - Enable/disable deep sleep
display on/off - Enable/disable OLED (power test)
factory        - Factory reset and restart
```

### Node Commands (existing + new)
```
status         - Show mode, network, BLE sensor count
ble scan       - Force immediate BLE scan
ble stats      - Show BLE sensor list with RSSI
sleep on/off   - Enable/disable deep sleep
i2c scan       - Scan for local I2C sensors
factory        - Factory reset
```

### Gateway Commands (existing)
```
config <node> <interval>  - Update node report interval
mqtt <host> <port>        - Configure MQTT broker
mqtt off                  - Disable MQTT
```

## Future Enhancements (Not in Initial Implementation)

1. **BLE Connection Mode**: Remote sensor accepts connections for configuration
2. **OTA Updates**: Gateway pushes firmware to nodes/sensors
3. **Pairing/Security**: Encrypted BLE advertisements
4. **Multi-Hop**: Nodes relay for other nodes (mesh)
5. **Battery Optimization**: Adaptive advertising based on motion/events
6. **Sensor Descriptors via BLE**: Remote sensor advertises capabilities
7. **Time Sync**: Gateway syncs time to nodes for accurate timestamps

## Implementation Checklist

- [x] Create `remote_sensor/` directory structure
- [x] Define `BLESensorAdvertisement` struct in shared library
- [x] Implement remote sensor firmware with BLE advertising
- [x] Add BLE scanning to node firmware
- [x] Implement BLE→LoRa bridging in node
- [x] Add `BLE_SENSOR_DATA` message handling to gateway
- [x] Update gateway registry for BLE sensors
- [x] Extend MQTT publishing for BLE sensors
- [x] Update web API JSON output
- [ ] Test Phase 1: Remote sensor standalone
- [ ] Test Phase 2: Node scanning
- [ ] Test Phase 3: End-to-end
- [ ] Measure power consumption
- [ ] Document testing results

**Status:** Implementation complete, ready for hardware testing. See `THREE-TIER-TESTING.md` for testing procedures.

## File Structure

```
/LoRa-PNP-Network/
├── BLE-PROTOCOL.md           # This document
├── MVP-Spec.md               # Original LoRa protocol spec
├── FEATURE-TRACKER.md        # Feature status
├── platformio.ini            # Gateway build config
├── src/
│   └── main.cpp              # Gateway firmware (update for BLE)
├── node/
│   ├── platformio.ini        # Node build config
│   └── src/
│       └── main.cpp          # Node firmware (add BLE scanning)
├── remote_sensor/            # NEW
│   ├── platformio.ini        # Remote sensor build config
│   └── src/
│       └── main.cpp          # Remote sensor firmware (NEW)
└── lib/
    └── LoRaPNP/
        └── src/
            ├── Types.h       # Update: add BLE_SENSOR_DATA
            ├── Packet.h/cpp  # Existing
            ├── Radio.h/cpp   # Existing
            └── BLEProtocol.h # NEW - shared BLE definitions
```
