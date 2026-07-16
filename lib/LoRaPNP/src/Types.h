#pragma once
#include <stdint.h>
#include <stddef.h>

namespace lorapnp {

constexpr uint8_t PROTOCOL_VERSION = 0x01;
constexpr uint16_t GATEWAY_ID = 0x0001;
constexpr uint16_t BROADCAST_ID = 0xFFFF;
constexpr uint16_t UNASSIGNED_ID = 0x0000;
constexpr uint8_t MAX_PAYLOAD_SIZE = 240;
constexpr uint8_t HEADER_SIZE = 10;
constexpr uint8_t MAX_STATIONS = 32;
constexpr uint8_t DEVICE_UID_LEN = 6;

constexpr uint32_t DEFAULT_REPORT_INTERVAL_MS = 30000;
constexpr uint32_t DEFAULT_SLEEP_INTERVAL_MS = 30000;
constexpr uint8_t DEFAULT_SF = 7;
constexpr uint8_t DEFAULT_TX_POWER = 14;
constexpr float DEFAULT_FREQUENCY = 915.0f;
constexpr float DEFAULT_BANDWIDTH = 125.0f;
constexpr uint8_t DEFAULT_SYNC_WORD = 0x12;

constexpr uint32_t ACK_TURNAROUND_DELAY_MS = 500;
constexpr uint32_t RECEIVE_TIMEOUT_MS = 2000;
constexpr uint32_t JOIN_ACK_TIMEOUT_MS = 6000;
constexpr uint8_t JOIN_ACCEPT_MAX_RETRIES = 3;
constexpr uint32_t CONFIG_ACK_TIMEOUT_MS = 6000;
constexpr uint8_t CONFIG_UPDATE_MAX_RETRIES = 3;
constexpr uint32_t REGISTRY_PRINT_INTERVAL_MS = 10000;
constexpr uint32_t STATION_STALE_TIMEOUT_MS = 120000;

// STATUS packet status_flags bitfield (payload[10]).
// A field device self-reports its capabilities here, so the gateway can
// classify it without a separate device-type field on the wire. In the
// two-type model, "station" vs "leaf sensor" is just CAN_RELAY set or clear.
constexpr uint8_t STATUS_FLAG_CAN_RELAY = 0x01;  // forwards BLE remote-sensor data (a relay / "station")

enum class MessageType : uint8_t {
    DISCOVER       = 0x01,
    OFFER          = 0x02,
    JOIN_REQUEST   = 0x03,
    JOIN_ACCEPT    = 0x04,
    DATA           = 0x05,
    ACK            = 0x06,
    CONFIG_UPDATE  = 0x07,
    CONFIG_ACK     = 0x08,
    STATUS         = 0x09,
    SENSOR_DESCRIPTOR = 0x0A,
    SENSOR_DATA    = 0x0B,
    BLE_SENSOR_DATA = 0x0C,  // Station forwarding BLE remote sensor data
    NAME_UPDATE    = 0x0D,   // Gateway pushes custom names (station/sensor/remote)
    REBOOT         = 0x0E,   // Gateway → Station: Remote reboot command
};

enum class NameType : uint8_t {
    STATION        = 0x00,   // Station/node name
    I2C_SENSOR     = 0x01,   // I2C sensor attached to station
    REMOTE_SENSOR  = 0x02,   // BLE remote sensor
};

enum SensorType : uint8_t {
    SENSOR_TYPE_FAKE = 0x01,
    SENSOR_TYPE_TEMPERATURE = 0x02,
    SENSOR_TYPE_HUMIDITY = 0x03,
    SENSOR_TYPE_BATTERY = 0x04,
    SENSOR_TYPE_PRESSURE = 0x05,
    SENSOR_TYPE_SOIL_MOISTURE = 0x06,
    SENSOR_TYPE_DISTANCE = 0x07,
    SENSOR_TYPE_PRESENCE = 0x08,
    SENSOR_TYPE_LIGHT = 0x09,
    SENSOR_TYPE_UNKNOWN_I2C = 0x80,
};

enum SensorUnit : uint8_t {
    SENSOR_UNIT_NONE = 0x00,
    SENSOR_UNIT_CELSIUS = 0x01,
    SENSOR_UNIT_PERCENT = 0x02,
    SENSOR_UNIT_VOLTS = 0x03,
    SENSOR_UNIT_RAW = 0x04,
    SENSOR_UNIT_HPA = 0x05,
    SENSOR_UNIT_MILLIMETERS = 0x06,
    SENSOR_UNIT_BOOLEAN = 0x07,
    SENSOR_UNIT_LUX = 0x08,
};

enum SensorDataType : uint8_t {
    SENSOR_DATA_FLOAT32 = 0x01,
    SENSOR_DATA_UINT16 = 0x02,
};

enum PacketFlag : uint8_t {
    FLAG_NONE        = 0x00,
    FLAG_ACK_REQUEST = 0x01,
    FLAG_BROADCAST   = 0x02,
};

enum class Status : uint8_t {
    OK = 0,
    ERR_CRC,
    ERR_VERSION,
    ERR_SIZE,
    ERR_RADIO,
    ERR_TIMEOUT,
    ERR_FULL,
    ERR_NOT_FOUND,
    ERR_INVALID,
};

enum class NodeState : uint8_t {
    EMPTY = 0,
    PENDING,
    ACTIVE,
    STALE,
};

inline void putU16(uint8_t* buf, uint16_t val) {
    buf[0] = (val >> 8) & 0xFF;
    buf[1] = val & 0xFF;
}

inline uint16_t getU16(const uint8_t* buf) {
    return ((uint16_t)buf[0] << 8) | buf[1];
}

inline void putU32(uint8_t* buf, uint32_t val) {
    buf[0] = (val >> 24) & 0xFF;
    buf[1] = (val >> 16) & 0xFF;
    buf[2] = (val >> 8) & 0xFF;
    buf[3] = val & 0xFF;
}

inline uint32_t getU32(const uint8_t* buf) {
    return ((uint32_t)buf[0] << 24) | ((uint32_t)buf[1] << 16) |
           ((uint32_t)buf[2] << 8) | buf[3];
}

} // namespace lorapnp
