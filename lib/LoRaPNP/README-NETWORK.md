# Network Transport Selection

## Overview

Adaptive network transport layer that automatically selects the best available connection (WiFi > BLE > LoRa) based on availability, battery status, and signal quality.

## Architecture

```
┌──────────────────────────────────────┐
│        NetworkManager                │
│  - Coordinates transport selection   │
│  - Daily connectivity checks         │
│  - Battery-aware mode                │
└────────────┬─────────────────────────┘
             │
     ┌───────┴───────┬────────────┐
     │               │            │
┌────▼────┐    ┌─────▼──┐   ┌────▼────┐
│  WiFi   │    │  BLE   │   │  LoRa   │
│Transport│    │Transport│   │Transport│
│(async)  │    │(GATT)  │   │(always) │
└─────────┘    └────────┘   └─────────┘
```

## Components

### Transport.h
Base interface for all transport types. Defines:
- `ITransport` interface
- `TransportType` enum
- `TransportMetrics` struct

### WiFiTransport
Non-blocking WiFi connection manager with:
- **Async state machine** (doesn't block main loop)
- **Exponential backoff**: 10s, 20s, 40s, 80s, 120s
- **5-minute retry window** before giving up
- **10-second timeout** per attempt
- **RSSI tracking** for signal quality

**States:**
- `DISABLED` - WiFi not enabled
- `IDLE` - Waiting to retry
- `CONNECTING` - Connection in progress
- `CONNECTED` - Active connection
- `FAILED` - Gave up after 5 minutes

### NetworkManager
Coordinates transport selection:
- Selects best available transport on boot
- Handles transport failover
- Daily connectivity check (3 AM, battery-aware)
- Saves WiFi credentials to NVS
- Battery-aware mode (prefers BLE on low battery)

## Usage

### Basic Setup

```cpp
#include "NetworkManager.h"

NetworkManager network;

void setup() {
    // Initialize network manager
    network.begin();

    // Set WiFi credentials (optional - can be set via BLE)
    network.setWiFiCredentials("MySSID", "MyPassword");
    network.enableWiFi(true);

    // Configure daily check
    NetworkConfig config;
    config.daily_check_enabled = true;
    config.daily_check_hour = 3;  // 3 AM
    config.min_battery_for_check = 3.5f;  // Skip if <3.5V
    config.battery_aware_mode = true;
    network.setConfig(config);
}

void loop() {
    // CRITICAL: Call update() every loop iteration
    network.update();

    // Update battery voltage for battery-aware mode
    float battery_v = analogRead(BATTERY_PIN) * 0.001f;
    network.setBatteryVoltage(battery_v);

    // Check current transport
    TransportType current = network.getCurrentTransport();

    // Process packets based on active transport
    ITransport* transport = network.getActiveTransport();
    if (transport && transport->isConnected()) {
        // Send via WiFi/BLE
        transport->send(&packet);
    } else {
        // Fall back to LoRa
        sendViaLoRa(&packet);
    }
}
```

### WiFi Credential Sharing (Coming Soon)

```cpp
// Via AES-GCM encrypted BLE during setup
void onBLECredentialsReceived(const uint8_t* encrypted_data, size_t len) {
    // Decrypt and validate
    WiFiCredentials creds = decryptCredentials(encrypted_data, len);

    // Apply to network manager
    network.setWiFiCredentials(creds.ssid, creds.password);
    network.enableWiFi(true);
}
```

## Transport Selection Logic

### On Boot / Connectivity Loss

1. **WiFi** (if credentials configured and not low battery)
   - Start async connection (non-blocking)
   - Continue with LoRa while connecting
   - Switch to WiFi when connected

2. **BLE** (if in range, when implemented)
   - Wait for node-initiated GATT connection
   - Use for sensor data, commands

3. **LoRa** (always available fallback)
   - Default transport on boot
   - Fallback if WiFi/BLE unavailable

### Battery-Aware Mode

If `battery_aware_mode = true` and battery < 3.3V:
- Skip WiFi (high power consumption)
- Prefer BLE (lower power)
- Fall back to LoRa

### Daily Connectivity Check

At configured hour (default 3 AM):
- Skip if battery < threshold
- Skip if already on WiFi
- Attempt WiFi connection
- Switch if successful and currently on LoRa
- Log results for debugging

## Metrics

Track per-transport metrics:

```cpp
TransportMetrics metrics = network.getTransportMetrics(TRANSPORT_WIFI);

Serial.printf("WiFi Metrics:\n");
Serial.printf("  Packets sent: %u\n", metrics.packets_sent);
Serial.printf("  Bytes sent: %u\n", metrics.bytes_sent);
Serial.printf("  Failures: %u\n", metrics.failures);
Serial.printf("  RSSI: %d dBm\n", metrics.rssi);

uint32_t switches = network.getTransportSwitchCount();
Serial.printf("Transport switches: %u\n", switches);
```

## Configuration

Stored in NVS (namespace "network"):
- `wifi_ssid` (String)
- `wifi_password` (String)
- `wifi_enabled` (bool)

## Future Enhancements

- **BLE GATT Transport** (Phase 2)
  - Node acts as BLE peripheral
  - Gateway acts as BLE central
  - 244-byte MTU for larger payloads

- **WiFi Credential Sharing** (Phase 2)
  - AES-256-GCM encryption
  - ECDH key derivation
  - Replay protection

- **Quality-Based Selection** (Phase 3)
  - RSSI scoring
  - Packet loss rate tracking
  - Latency measurement

## Testing

```cpp
// Force WiFi connection attempt
network.selectPrimaryTransport();

// Trigger daily check manually
network.performDailyCheck();

// Simulate battery state
network.setBatteryVoltage(3.2f);  // Low battery

// Check current transport
TransportType current = network.getCurrentTransport();
assert(current == TRANSPORT_LORA);  // Should prefer LoRa on low battery
```

## Implementation Status

- ✅ Transport interface
- ✅ WiFi async state machine
- ✅ NetworkManager core
- ✅ Daily connectivity check
- ✅ Battery-aware mode
- ✅ NVS credential storage
- ⏳ BLE GATT transport (Phase 2)
- ⏳ WiFi credential sharing over BLE (Phase 2)
- ⏳ Quality-based selection (Phase 3)

See NETWORK-SELECTION-SPEC.md for full specification.
