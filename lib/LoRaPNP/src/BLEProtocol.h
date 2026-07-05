#pragma once
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include "Types.h"

namespace lorapnp {

// ========== BLE Remote Sensor Protocol ==========
// Three-tier architecture: Remote Sensor (BLE) → Node (BLE+LoRa) → Gateway (LoRa+WiFi)

// BLE Service UUID for LoRa PNP ecosystem
constexpr uint16_t BLE_SERVICE_UUID = 0x4C50;  // "LP" in ASCII

// BLE Characteristic UUIDs (Phase 3: Remote sensor configuration)
constexpr uint16_t BLE_CHAR_NAME_UUID = 0x4C51;  // Custom name characteristic (R/W)
constexpr uint16_t BLE_CHAR_CONFIG_UUID = 0x4C52;  // Configuration characteristic (future)

// Manufacturer ID for advertising data
constexpr uint16_t BLE_MANUFACTURER_ID = 0xFFFF;  // Test/development ID

// Protocol version
constexpr uint8_t BLE_PROTOCOL_VERSION = 0x01;

// Status flags for BLE sensors
constexpr uint8_t BLE_FLAG_NONE           = 0x00;
constexpr uint8_t BLE_FLAG_LOW_BATTERY    = 0x01;  // Battery below threshold
constexpr uint8_t BLE_FLAG_PAIRING_REQ    = 0x02;  // Requesting pairing (future)
constexpr uint8_t BLE_FLAG_ERROR          = 0x04;  // Sensor read error
constexpr uint8_t BLE_FLAG_FIRST_BOOT     = 0x08;  // First advertisement after boot

// Timing parameters for remote sensors
constexpr uint32_t BLE_ADV_INTERVAL_MS    = 30000; // 30 seconds between advertisements
constexpr uint32_t BLE_ADV_DURATION_MS    = 100;   // Advertise for 100ms
constexpr int8_t   BLE_TX_POWER_DBM       = 0;     // 0 dBm (balance range/power)

// Timing parameters for node scanning
constexpr uint32_t BLE_SCAN_INTERVAL_MS   = 30000; // Scan every 30 seconds
constexpr uint32_t BLE_SCAN_DURATION_S    = 5;     // Scan for 5 seconds
constexpr bool     BLE_SCAN_ACTIVE        = false; // Passive scan (lower power)

// Battery thresholds
constexpr uint16_t BLE_BATTERY_LOW_MV     = 2400;  // 2.4V - low battery warning
constexpr uint16_t BLE_BATTERY_CRITICAL_MV = 2200; // 2.2V - critical battery

// Maximum BLE sensors per node
constexpr uint8_t MAX_BLE_SENSORS_PER_NODE = 8;

// Maximum BLE sensors tracked by gateway
constexpr uint8_t MAX_BLE_SENSORS_TOTAL = 16;

/**
 * Single sensor reading within BLE advertisement
 * 8 bytes per reading
 */
struct BLESensorReading {
    uint8_t sensor_id;      // Local sensor ID (1-255)
    uint8_t sensor_type;    // lorapnp::SensorType
    uint8_t sensor_unit;    // lorapnp::SensorUnit
    uint8_t data_type;      // lorapnp::SensorDataType
    union {
        float    float_val; // 4 bytes for SENSOR_DATA_FLOAT32
        uint16_t uint_val;  // 2 bytes for SENSOR_DATA_UINT16 (padded to 4)
        uint8_t  raw[4];
    } value;
} __attribute__((packed));

/**
 * BLE Advertisement payload for remote sensors
 * Total: 29 bytes (fits comfortably in single BLE advertising packet)
 *
 * This structure is transmitted as Manufacturer Specific Data
 * in BLE advertising packets. Maximum size for manufacturer data
 * is 31 bytes, leaving 2 bytes headroom.
 */
struct BLESensorAdvertisement {
    uint16_t company_id;         // 0xFFFF (BLE_MANUFACTURER_ID)
    uint8_t  protocol_version;   // 0x01 (BLE_PROTOCOL_VERSION)
    uint8_t  device_uid[6];      // Unique sensor ID (MAC address)
    uint8_t  sensor_count;       // Number of sensor readings in this packet (1-2)

    BLESensorReading readings[2]; // Up to 2 readings per advertisement (16 bytes)

    uint16_t battery_mv;         // Battery voltage in millivolts
    uint8_t  flags;              // BLE_FLAG_* status flags
} __attribute__((packed));
// Size: 2 + 1 + 6 + 1 + (2×8) + 2 + 1 = 29 bytes

static_assert(sizeof(BLESensorAdvertisement) == 29,
              "BLESensorAdvertisement must be exactly 29 bytes");

/**
 * BLE Sensor Data forwarded from Node to Gateway via LoRa
 * This is the payload for MessageType::BLE_SENSOR_DATA packets
 *
 * Contains one sensor reading plus metadata about the BLE source
 */
struct BLESensorDataPayload {
    uint8_t  ble_device_uid[6];  // BLE sensor MAC address
    int8_t   rssi;               // BLE RSSI at node (-100 to 0 dBm)
    uint16_t battery_mv;         // BLE sensor battery voltage
    uint8_t  sensor_id;          // Sensor ID from BLE advertisement
    uint8_t  sensor_type;        // lorapnp::SensorType
    uint8_t  sensor_unit;        // lorapnp::SensorUnit
    uint8_t  data_type;          // lorapnp::SensorDataType
    union {
        float    float_val;      // 4 bytes
        uint16_t uint_val;       // 2 bytes
        uint8_t  raw[4];
    } value;
    uint32_t timestamp_ms;       // Node's millis() when received (for latency tracking)
} __attribute__((packed));
// Size: 6 + 1 + 2 + 1 + 1 + 1 + 1 + 4 + 4 = 21 bytes

/**
 * BLE Remote Sensor tracking (used by Node)
 * Node maintains this registry of BLE sensors it has heard from
 */
struct BLERemoteSensor {
    bool     active;             // Is this slot in use?
    uint8_t  device_uid[6];      // BLE sensor unique ID
    uint8_t  sensor_count;       // Number of sensors on this device
    int8_t   rssi;               // Last RSSI
    uint16_t battery_mv;         // Last battery reading
    uint32_t last_seen;          // millis() when last advertisement received
    uint8_t  flags;              // Last status flags

    // Sensor metadata (from advertisements)
    uint8_t  sensor_ids[4];
    uint8_t  sensor_types[4];
    uint8_t  sensor_units[4];
    float    last_values[4];
};

/**
 * BLE Remote Sensor tracking (used by Gateway)
 * Gateway tracks which node is forwarding each BLE sensor
 */
struct BLERemoteSensorInfo {
    bool     active;             // Is this slot in use?
    uint8_t  device_uid[6];      // BLE sensor unique ID
    uint16_t parent_node_id;     // Which node is bridging this sensor
    uint8_t  sensor_count;       // Number of sensors on this device

    // Sensor metadata
    uint8_t  sensor_ids[4];
    uint8_t  sensor_types[4];
    uint8_t  sensor_units[4];
    float    last_values[4];

    int8_t   rssi;               // BLE RSSI at node
    uint16_t battery_mv;         // Battery voltage
    uint32_t last_seen;          // Gateway's millis()
};

/**
 * Helper: Format BLE device UID as string
 * Returns: "AA:BB:CC:DD:EE:FF"
 */
inline void formatBLEUID(const uint8_t* uid, char* buf, size_t buf_size) {
    if (buf_size < 18) return;  // Need 17 chars + null
    ::snprintf(buf, buf_size, "%02X:%02X:%02X:%02X:%02X:%02X",
               uid[0], uid[1], uid[2], uid[3], uid[4], uid[5]);
}

/**
 * Helper: Check if battery is low
 */
inline bool isBatteryLow(uint16_t battery_mv) {
    return battery_mv < BLE_BATTERY_LOW_MV;
}

/**
 * Helper: Check if battery is critical
 */
inline bool isBatteryCritical(uint16_t battery_mv) {
    return battery_mv < BLE_BATTERY_CRITICAL_MV;
}

/**
 * Helper: Initialize BLESensorAdvertisement with defaults
 */
inline void initBLEAdvertisement(BLESensorAdvertisement& adv, const uint8_t* device_uid) {
    adv.company_id = BLE_MANUFACTURER_ID;
    adv.protocol_version = BLE_PROTOCOL_VERSION;
    memcpy(adv.device_uid, device_uid, 6);
    adv.sensor_count = 0;
    adv.battery_mv = 0;
    adv.flags = BLE_FLAG_NONE;
    memset(adv.readings, 0, sizeof(adv.readings));
}

/**
 * Helper: Add sensor reading to advertisement
 * Returns: true if added, false if advertisement full
 */
inline bool addBLEReading(BLESensorAdvertisement& adv,
                          uint8_t sensor_id,
                          uint8_t sensor_type,
                          uint8_t sensor_unit,
                          uint8_t data_type,
                          float value) {
    if (adv.sensor_count >= 2) return false;  // Already have 2 readings

    BLESensorReading& reading = adv.readings[adv.sensor_count];
    reading.sensor_id = sensor_id;
    reading.sensor_type = sensor_type;
    reading.sensor_unit = sensor_unit;
    reading.data_type = data_type;

    if (data_type == SENSOR_DATA_FLOAT32) {
        reading.value.float_val = value;
    } else if (data_type == SENSOR_DATA_UINT16) {
        reading.value.uint_val = (uint16_t)value;
    }

    adv.sensor_count++;
    return true;
}

/**
 * Helper: Validate BLE advertisement packet
 * Returns: true if valid LoRa PNP advertisement
 */
inline bool validateBLEAdvertisement(const BLESensorAdvertisement* adv) {
    if (!adv) return false;
    if (adv->company_id != BLE_MANUFACTURER_ID) return false;
    if (adv->protocol_version != BLE_PROTOCOL_VERSION) return false;
    if (adv->sensor_count > 2) return false;  // Max 2 readings per packet
    return true;
}

/**
 * Helper: Create BLE sensor data payload for LoRa transmission
 */
inline void createBLESensorDataPayload(BLESensorDataPayload& payload,
                                       const uint8_t* ble_device_uid,
                                       int8_t rssi,
                                       uint16_t battery_mv,
                                       const BLESensorReading& reading,
                                       uint32_t timestamp_ms) {
    memcpy(payload.ble_device_uid, ble_device_uid, 6);
    payload.rssi = rssi;
    payload.battery_mv = battery_mv;
    payload.sensor_id = reading.sensor_id;
    payload.sensor_type = reading.sensor_type;
    payload.sensor_unit = reading.sensor_unit;
    payload.data_type = reading.data_type;
    memcpy(payload.value.raw, reading.value.raw, 4);
    payload.timestamp_ms = timestamp_ms;
}

} // namespace lorapnp
