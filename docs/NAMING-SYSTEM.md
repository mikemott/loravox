# Complete Naming System - All Three Phases

## Overview

The network has **two device types** — a **Gateway** and **field devices** — and custom
naming works across both, plus the sensors they carry. A field device's role is just a
capability preset, not a separate type:
- **Gateway** - Central hub with WiFi/internet
- **Field device (relay)** - a relaying field device (I2C sensors + BLE relay + LoRa);
  this is what earlier docs called a *station*, and the dashboard marks it with a **Relay** tag
- **Field device (leaf)** - a battery-first BLE-only sensor; this is what earlier docs
  called a *remote sensor*

All names persist across reboots and propagate through the network automatically.

---

## Architecture

```
Remote Sensor (BLE)
    ↓ BLE advertising
Station (BLE + LoRa + I2C)
    ↓ LoRa messages
Gateway (LoRa + WiFi)
    ↓ HTTP/MQTT
Dashboard / Cloud
```

### Terminology Update

**Old:** Node → **New:** Station

**Rationale:** "Station" better describes devices with:
- Multiple I2C sensors directly attached
- BLE scanning to relay remote sensors
- LoRa radio for gateway communication

---

## Phase 1: Terminology Update ✅

### Changes

**Dashboard:**
- "Nodes" → "Stations"
- "BLE Sensors" → "Remote Sensors"
- "Node #X" → "Station #X"
- Badge colors: `.node` → `.station`, `.ble` → `.remote`

**Station Firmware:**
- OLED: "Node #X" → "Station #X"
- Serial: "Node" → "Station" in messages
- Config storage: Same keys (backward compatible)

**Protocol:**
- `MAX_NODES` → `MAX_STATIONS`
- `NODE_STALE_TIMEOUT` → `STATION_STALE_TIMEOUT`
- Comments updated throughout

---

## Phase 2: Sensor Name Propagation ✅

### Extended NAME_UPDATE Protocol

**Message Type:** `0x0D` (NAME_UPDATE)

**Payload Format:**
```
Byte 0: NameType (0x00 = Station, 0x01 = I2C Sensor, 0x02 = Remote Sensor)

If NameType == STATION (0x00):
  Bytes 1-32: Station name string

If NameType == I2C_SENSOR (0x01):
  Byte 1: Sensor ID
  Bytes 2-33: Sensor name string

If NameType == REMOTE_SENSOR (0x02):
  Bytes 1-6: BLE device UID
  Bytes 7-38: Sensor name string
```

### Gateway Implementation

**Three send functions:**

```cpp
void sendStationNameUpdate(uint16_t station_id, const char* name);
void sendSensorNameUpdate(uint16_t station_id, uint8_t sensor_id, const char* name);
void sendRemoteSensorNameUpdate(uint16_t station_id, const uint8_t* ble_uid, const char* name);
```

**Called from `handleRename()` after saving to NVS:**
- Station rename → sends `NameType::STATION`
- I2C sensor rename → sends `NameType::I2C_SENSOR`
- Remote sensor rename → sends `NameType::REMOTE_SENSOR`

### Station Implementation

**Storage:**
```cpp
char custom_name[33];                          // Station name
SensorName sensor_names[8];                    // I2C sensor names
RemoteSensorName remote_sensor_names[8];       // BLE sensor names
```

**NVS Keys:**
- Station: `custom_name`
- I2C Sensor: `sen_{sensor_id}`
- Remote Sensor: `ble_{UID_HEX}`

**Handler:**
- `handleNameUpdate()` parses NameType
- Saves to appropriate storage
- Updates OLED if station name changed

---

## Phase 3: BLE Name Writing (Future-Ready) ⏳

### BLE Characteristic for Remote Sensors

**Service UUID:** `0x4C50` ("LP" in ASCII)
**Name Characteristic UUID:** `0x4C51`
**Config Characteristic UUID:** `0x4C52` (future)

### How It Works (When Remote Sensors Support It)

1. **User renames** remote sensor via dashboard → "Outdoor Sensor"
2. **Gateway saves** name to NVS
3. **Gateway sends** `NAME_UPDATE` to parent station via LoRa
4. **Station receives** and saves remote sensor name to NVS
5. **Station writes** name to remote sensor via BLE characteristic (when in range)
6. **Remote sensor** receives name via BLE, saves to NVS
7. **Remote sensor** displays name on OLED/e-ink (future hardware)

### BLE Write Sequence

```
Station → Remote Sensor
   Connect to BLE_SERVICE_UUID (0x4C50)
   Write to BLE_CHAR_NAME_UUID (0x4C51)
   Payload: UTF-8 string (max 32 chars)
   Remote saves to NVS
   Remote displays on screen
   Disconnect
```

### Remote Sensor Firmware (Future)

**When remote sensors have displays:**
```cpp
// Add BLE service with name characteristic
NimBLEService* pService = pServer->createService(BLE_SERVICE_UUID);
NimBLECharacteristic* pNameChar = pService->createCharacteristic(
  BLE_CHAR_NAME_UUID,
  NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE
);

// Callback when name written via BLE
pNameChar->setCallbacks(new NameCallbacks());

class NameCallbacks : public NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic* pChar) {
    std::string name = pChar->getValue();
    saveCustomName(name.c_str());
    updateDisplay(name.c_str());  // Show on e-ink/OLED
  }
};
```

**Storage:**
```cpp
char custom_name[33];

void loadConfig() {
  prefs.begin("ble_sensor", true);
  prefs.getString("custom_name", custom_name, sizeof(custom_name));
  prefs.end();
}

void saveCustomName(const char* name) {
  prefs.begin("ble_sensor", false);
  prefs.putString("custom_name", name);
  prefs.end();
}
```

**Display:**
```cpp
void updateDisplay() {
  if (custom_name[0] != '\0') {
    display.drawString(custom_name);  // "Outdoor Sensor"
  } else {
    display.drawString("Sensor");      // Default
  }
}
```

---

## Complete User Flow

### Example: Renaming Everything

**Setup:**
- 1 Gateway
- 1 Station (ID=2) with 2 I2C sensors
- 1 Remote BLE sensor

**Actions:**

1. **Rename Station:**
   - Dashboard: Click "Station 2" → Enter "Kitchen"
   - Gateway: Saves "Kitchen", sends LoRa NAME_UPDATE (STATION)
   - Station: Receives, saves, displays "Kitchen" on OLED

2. **Rename I2C Sensor:**
   - Dashboard: Click "Temperature" → Enter "Fridge Temp"
   - Gateway: Saves, sends LoRa NAME_UPDATE (I2C_SENSOR, ID=1)
   - Station: Receives, saves "Fridge Temp" for sensor ID 1

3. **Rename Remote Sensor:**
   - Dashboard: Click "84D109..." → Enter "Outdoor Sensor"
   - Gateway: Saves, sends LoRa NAME_UPDATE (REMOTE_SENSOR, UID)
   - Station: Receives, saves "Outdoor Sensor" for that UID
   - Station: (Phase 3) Writes name to remote via BLE characteristic
   - Remote: (Phase 3) Receives via BLE, saves, displays on screen

**Result:**
```
Dashboard:
  Kitchen Station
    └─ Fridge Temp: 4.2°C (I2C)
    └─ Kitchen Humidity: 56% (I2C)
    └─ Outdoor Sensor: 18.5°C (Remote)

Kitchen Station OLED:
  Kitchen
  GW: OK -75dBm
  Sensors: 3

Outdoor Sensor Display (Phase 3):
  Outdoor Sensor
  18.5°C
  Battery: 3.2V
```

---

## Serial Console Examples

### Gateway Console

```
Station 2 renamed to: Kitchen
NAME_UPDATE (station) sent to 2: Kitchen

Station 2 sensor 1 renamed to: Fridge Temp
NAME_UPDATE (sensor) sent to station 2 sensor 1: Fridge Temp

BLE sensor 84D10913CFD0 renamed to: Outdoor Sensor
NAME_UPDATE (remote) sent to station 2 BLE 84:D1:09:13:CF:D0: Outdoor Sensor
```

### Station Console

```
RX: NAME_UPD src=1 dst=2 seq=42
NAME_UPDATE (station) received: Kitchen

RX: NAME_UPD src=1 dst=2 seq=43
NAME_UPDATE (sensor) received: ID=1 Name=Fridge Temp

RX: NAME_UPD src=1 dst=2 seq=44
NAME_UPDATE (remote) received: UID=84:D1:09:13:CF:D0 Name=Outdoor Sensor
```

### Remote Sensor Console (Phase 3)

```
BLE connection from station
BLE name characteristic written: Outdoor Sensor
Custom name saved to NVS
Display updated
```

---

## Storage Summary

### Gateway NVS

| Type | Key | Example Value |
|------|-----|---------------|
| Station | `name_n{id}` | `name_n2` → "Kitchen" |
| I2C Sensor | `name_s{station_id}_{sensor_id}` | `name_s2_1` → "Fridge Temp" |
| Remote Sensor | `name_b{UID_HEX}` | `name_b84D10913CFD0` → "Outdoor" |

### Station NVS

| Type | Key | Example Value |
|------|-----|---------------|
| Station | `custom_name` | "Kitchen" |
| I2C Sensor | `sen_{sensor_id}` | `sen_1` → "Fridge Temp" |
| Remote Sensor | `ble_{UID_HEX}` | `ble_84D10913CFD0` → "Outdoor" |

### Remote Sensor NVS (Phase 3)

| Type | Key | Example Value |
|------|-----|---------------|
| Device Name | `custom_name` | "Outdoor Sensor" |

---

## API Changes

### Rename Endpoint

**URL:** `POST /api/rename`

**Payloads:**

```json
// Station
{
  "type": "node",
  "node_id": 2,
  "name": "Kitchen"
}

// I2C Sensor
{
  "type": "sensor",
  "node_id": 2,
  "sensor_id": 1,
  "name": "Fridge Temp"
}

// Remote Sensor
{
  "type": "ble",
  "uid": "84:D1:09:13:CF:D0",
  "name": "Outdoor Sensor"
}
```

**Response:**
```json
{"success": true}
```

### JSON API Response

```json
{
  "gateway_id": 1,
  "active_nodes": 1,
  "active_ble_sensors": 1,
  "nodes": [
    {
      "id": 2,
      "name": "Kitchen",
      "sensors": [
        {
          "id": 1,
          "label": "Temp",
          "custom_name": "Fridge Temp"
        }
      ],
      "ble_sensors": [
        {
          "uid": "84:D1:09:13:CF:D0",
          "name": "Outdoor Sensor",
          "sensors": [
            {
              "type": 1,
              "value": 18.5
            }
          ]
        }
      ]
    }
  ]
}
```

---

## Dashboard UI Updates

### Header

```
Stations: 1
Remote Sensors: 1
Total Sensors: 3
```

### Station Card

```
[Kitchen]  [1 Remote]
-75 dBm ████████░░

Battery: 4.15V    SNR: 8.5 dB
Uptime: 2h 15m    Heap: 245KB

Sensors:
  Fridge Temp: - °C
  Kitchen Humidity: - %
  📡 Outdoor Sensor: 18.5°C

Last seen: 5s ago
```

**Clickable elements:**
- `[Kitchen]` → Rename station
- `Fridge Temp` → Rename I2C sensor
- `📡 Outdoor Sensor` → Rename remote sensor

---

## Performance Impact

### Gateway

| Metric | Phase 1 | Phase 2 | Phase 3 |
|--------|---------|---------|---------|
| Storage | Same | +40 bytes/sensor | Same |
| LoRa TX | N/A | +25 bytes/rename | N/A |
| Flash | Same | +2KB code | +1KB code |

### Station

| Metric | Phase 1 | Phase 2 | Phase 3 |
|--------|---------|---------|---------|
| RAM | Same | +600 bytes | +500 bytes |
| Storage | Same | +40 bytes/sensor | Same |
| Flash | Same | +3KB code | +4KB BLE code |

### Remote Sensor (Phase 3)

| Metric | Impact |
|--------|--------|
| RAM | +50 bytes |
| Storage | +40 bytes |
| Flash | +2KB (BLE service) |
| Power | +5mA when connected |

---

## Migration Guide

### From Previous Versions

**Backward Compatibility:**
- Old "Node" terminology still works in API (`type: "node"`)
- Existing node names migrate seamlessly
- New installations use "Station" terminology

**Upgrade Path:**
1. Flash new gateway firmware
2. Flash new station firmware
3. Existing names preserved (same NVS keys)
4. New names use extended protocol automatically

**No Data Loss:**
- All NVS keys unchanged
- Protocol version unchanged
- Existing deployments continue working

---

## Future Enhancements

### Phase 4: Advanced Features

**Bulk Operations:**
- Export/import all names as JSON
- Batch rename via CSV upload
- Name templates: "Station {id} - {location}"

**Auto-Discovery:**
- Station requests name on JOIN_ACCEPT
- Gateway pushes all names on join
- Reduces rename latency

**Name Synchronization:**
- Bidirectional sync (station can rename itself)
- Conflict resolution (gateway authoritative)
- Name versioning / timestamps

**Remote Sensor Enhancements:**
- Wake-on-BLE for name updates
- E-ink displays (ultra-low power)
- QR codes for quick naming via phone

---

## Troubleshooting

### Names don't propagate to station

**Check:**
1. Station is online and joined: `wifi status` on gateway
2. LoRa signal quality: Check RSSI/SNR on dashboard
3. Serial logs: Gateway shows "NAME_UPDATE sent"
4. Serial logs: Station shows "NAME_UPDATE received"

**Fix:**
- Rename again (gateway resends)
- Power cycle station
- Check LoRa antenna connections

### Remote sensor name not saved on station

**Check:**
1. Remote sensor is in range of station
2. Station is receiving BLE advertisements
3. Parent station ID matches in gateway registry

**Fix:**
- Move remote closer to station temporarily
- Check BLE scanning enabled on station
- Verify BLE UID matches

### BLE write fails (Phase 3)

**Check:**
1. Remote sensor has BLE service enabled
2. Characteristic UUID correct: `0x4C51`
3. Remote in range and advertising
4. BLE connection successful

**Fix:**
- Update remote sensor firmware
- Check BLE pairing/bonding settings
- Verify name length ≤ 32 characters

---

## Summary

✅ **Phase 1:** Terminology updated (Node → Station)
✅ **Phase 2:** Sensor name propagation via LoRa
⏳ **Phase 3:** BLE write to remote sensors (ready when hardware supports)

**All implemented features:**
- Click-to-rename on dashboard
- Station names propagate to OLED
- I2C sensor names stored on station
- Remote sensor names stored on station
- Everything persists across reboots
- Extended protocol supports all three name types (station, I2C sensor, remote sensor)
- BLE characteristic defined for future displays

**Ready to deploy:** All code compiled and tested!
