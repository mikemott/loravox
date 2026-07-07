# Network Selection Algorithm Specification

## 1. Overview

Adaptive network transport selection that prioritizes the best available connection based on bandwidth, latency, and power consumption.

**Priority Order:** WiFi > BLE > LoRa

## 2. Design Goals

- **Seamless fallback:** Automatically switch to next best transport if preferred option fails
- **Power awareness:** Prefer lower-power options when bandwidth requirements allow
- **Minimal complexity:** Simple state machine, no complex routing logic
- **Deterministic:** Same conditions always produce same transport choice

## 3. Transport Characteristics

| Transport | Bandwidth | Range | Power | Latency | Use Case |
|-----------|-----------|-------|-------|---------|----------|
| **WiFi** | ~50 Mbps | ~50m | High | <10ms | Bulk data, OTA updates, video |
| **BLE** | ~1 Mbps | ~30m | Low | ~20ms | Sensor data, commands, small payloads |
| **LoRa** | ~5 kbps | ~1km | Very Low | 100-500ms | Long-range fallback, remote sensors |

## 4. Selection Algorithm

### 4.1 When Selection Runs

Network selection is **NOT continuous**. It only runs:

1. **On initial power-up** - First boot, establish best available connection
2. **On connectivity loss** - When primary transport fails
3. **Daily health check** - 3 AM check for better connectivity (optional, battery-aware)

Once a transport is selected, it remains active until failure or daily check.

### 4.2 Decision Flow

```
┌─────────────────────────┐
│  Power-up / Loss Event  │
└───────────┬─────────────┘
            │
            ▼
       ┌────────────┐      WiFi         ┌──────────────┐
       │ Check WiFi ├─────Connected─────►│ Use WiFi     │
       └─────┬──────┘                    │ (Primary)    │
             │                           └──────────────┘
             │ Not Connected
             ▼
       ┌────────────┐      BLE          ┌──────────────┐
       │ Check BLE  ├─────Connected─────►│ Use BLE      │
       └─────┬──────┘                    │ (Secondary)  │
             │                           └──────────────┘
             │ Not Connected
             ▼
       ┌────────────┐                    ┌──────────────┐
       │  Use LoRa  ├───────────────────►│ Use LoRa     │
       └────────────┘                    │ (Fallback)   │
                                         └──────────────┘
```

### 4.3 Implementation

```cpp
enum TransportType {  // Fixed naming (was TransportState)
    TRANSPORT_WIFI,
    TRANSPORT_BLE,
    TRANSPORT_LORA
};

TransportType current_transport = TRANSPORT_LORA; // Default on boot
uint32_t last_connectivity_check_ms = 0;
uint32_t transport_established_ms = 0;

// Called on boot and on connectivity loss
TransportType selectPrimaryTransport() {
    // Try WiFi first (if credentials configured)
    if (wifi_sta_enabled && strlen(wifi_ssid) > 0) {
        startWiFiConnection(); // Non-blocking async state machine
        // WiFi will become primary when connection succeeds
    }

    // Try BLE next (wait for node-initiated connection)
    if (waitForBLEConnection(30000)) { // 30s timeout
        current_transport = TRANSPORT_BLE;
        transport_established_ms = millis();
        return TRANSPORT_BLE;
    }

    // Fall back to LoRa (always available, immediate)
    current_transport = TRANSPORT_LORA;
    transport_established_ms = millis();
    return TRANSPORT_LORA;
}

// Async WiFi connection state machine (non-blocking)
enum WiFiConnectionState {
    WIFI_IDLE,
    WIFI_CONNECTING,
    WIFI_CONNECTED,
    WIFI_FAILED
};

struct WiFiRetryState {
    WiFiConnectionState state;
    uint32_t attempt_start_ms;
    uint32_t next_retry_ms;
    uint32_t retry_window_start_ms;
    uint8_t attempt_count;
    uint32_t retry_delay_ms;
};

WiFiRetryState wifi_retry = {WIFI_IDLE, 0, 0, 0, 0, 10000};

// Non-blocking WiFi connection attempt
void updateWiFiConnection() {
    uint32_t now = millis();

    switch (wifi_retry.state) {
        case WIFI_IDLE:
            // Not attempting connection
            break;

        case WIFI_CONNECTING:
            if (WiFi.status() == WL_CONNECTED) {
                wifi_retry.state = WIFI_CONNECTED;
                current_transport = TRANSPORT_WIFI;
                ESP_LOGI("WIFI", "Connected after %u attempts", wifi_retry.attempt_count);
            } else if (now - wifi_retry.attempt_start_ms > 10000) {
                // 10-second timeout per attempt (not 30s blocking)
                WiFi.disconnect();

                // Check if we've exceeded 5-minute retry window
                if (now - wifi_retry.retry_window_start_ms > 300000) {
                    wifi_retry.state = WIFI_FAILED;
                    ESP_LOGW("WIFI", "Failed after 5 minutes, falling back to LoRa");
                    break;
                }

                // Schedule next retry with exponential backoff
                wifi_retry.retry_delay_ms = min(120000U, wifi_retry.retry_delay_ms * 2);
                wifi_retry.next_retry_ms = now + wifi_retry.retry_delay_ms;
                wifi_retry.state = WIFI_IDLE;
                ESP_LOGD("WIFI", "Retry %u in %u ms", wifi_retry.attempt_count,
                         wifi_retry.retry_delay_ms);
            }
            break;

        case WIFI_CONNECTED:
            // Monitor for disconnection
            if (WiFi.status() != WL_CONNECTED) {
                ESP_LOGW("WIFI", "Connection lost, falling back");
                current_transport = TRANSPORT_LORA; // Immediate fallback
                wifi_retry.state = WIFI_IDLE;
                wifi_retry.retry_window_start_ms = now;
                wifi_retry.next_retry_ms = now + 30000; // Retry in 30s
            }
            break;

        case WIFI_FAILED:
            // Gave up, using LoRa
            break;
    }

    // Trigger retry if scheduled
    if (wifi_retry.state == WIFI_IDLE && now >= wifi_retry.next_retry_ms &&
        wifi_sta_enabled && strlen(wifi_ssid) > 0) {

        WiFi.begin(wifi_ssid, wifi_password);
        wifi_retry.state = WIFI_CONNECTING;
        wifi_retry.attempt_start_ms = now;
        wifi_retry.attempt_count++;
        ESP_LOGI("WIFI", "Connection attempt %u", wifi_retry.attempt_count);
    }
}

// Call from main loop (non-blocking)
void loop() {
    updateWiFiConnection();

    // Continue processing LoRa packets while WiFi connects
    processLoRaPackets();

    // ... rest of loop
}

// Initiate WiFi connection on boot or connectivity loss
void startWiFiConnection() {
    wifi_retry.state = WIFI_IDLE;
    wifi_retry.retry_window_start_ms = millis();
    wifi_retry.next_retry_ms = millis(); // Start immediately
    wifi_retry.attempt_count = 0;
    wifi_retry.retry_delay_ms = 10000; // Start with 10s backoff
}
```

## 5. Transport States

### 5.1 WiFi Connection States

| State | Description | Transition Condition |
|-------|-------------|---------------------|
| DISCONNECTED | No WiFi credentials or AP unavailable | STA mode disabled or scan failed |
| CONNECTING | Attempting to connect to AP | WiFi.begin() called |
| CONNECTED | Active WiFi connection | IP address assigned |
| FAILED | Connection failed after retries | Timeout or auth failure |

**WiFi Quality Thresholds:**
- `WIFI_MIN_RSSI = -75 dBm` (below this, prefer BLE/LoRa)
- `WIFI_EXCELLENT_RSSI = -50 dBm`

### 5.2 BLE Connection States

| State | Description | Transition Condition |
|-------|-------------|---------------------|
| IDLE | BLE radio off or not advertising | Node disabled BLE |
| ADVERTISING | Broadcasting availability | No peer connected |
| CONNECTED | Active peer connection | Central/peripheral paired |
| DISCONNECTED | Peer out of range or timeout | Connection lost |

**BLE Quality Thresholds:**
- `BLE_MIN_RSSI = -85 dBm`
- `BLE_EXCELLENT_RSSI = -50 dBm`

### 5.3 LoRa State

LoRa is **always available** as the fallback transport. No connection state needed.

## 6. Implementation Strategy

### 6.1 Node Types

Different node types have different transport capabilities:

| Node Type | WiFi | BLE | LoRa |
|-----------|------|-----|------|
| **Coordinator** | ✅ AP+STA | ✅ Central | ✅ Always-on |
| **Node/Station** | ✅ STA mode (optional) | ✅ Peripheral | ✅ Always-on |
| **Remote Sensor** | ❌ No WiFi | ✅ Peripheral | ✅ Sleep/wake |

**WiFi Credential Sharing (Secure Implementation):**

Gateway shares WiFi credentials with stations during initial setup via authenticated encryption (AES-GCM) over BLE:

```cpp
#include <mbedtls/gcm.h>
#include <mbedtls/md.h>

struct WiFiCredentials {
    uint8_t ssid_len;          // Explicit length (not null-terminated)
    uint8_t password_len;      // Explicit length
    char ssid[32];
    char password[64];
    uint32_t timestamp;        // Replay protection
    uint8_t reserved[8];
} __attribute__((packed));

// Derive encryption key from BLE pairing (ECDH shared secret)
void deriveSessionKey(const uint8_t* ble_shared_secret, uint8_t* session_key) {
    // Use HKDF to derive session key from ECDH output
    mbedtls_md_context_t md_ctx;
    mbedtls_md_init(&md_ctx);
    mbedtls_md_setup(&md_ctx, mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), 1);

    const uint8_t salt[16] = {0}; // Or use random salt shared during pairing
    const uint8_t info[] = "LoRaVox-WiFi-Creds-v1";

    mbedtls_md_hmac_starts(&md_ctx, salt, sizeof(salt));
    mbedtls_md_hmac_update(&md_ctx, ble_shared_secret, 32);
    mbedtls_md_hmac_update(&md_ctx, info, sizeof(info));
    mbedtls_md_hmac_finish(&md_ctx, session_key);

    mbedtls_md_free(&md_ctx);
}

// On Gateway (during station setup)
bool shareWiFiCredentials(uint16_t node_id, const uint8_t* session_key) {
    WiFiCredentials creds = {0};
    creds.ssid_len = strlen(wifi_ssid);
    creds.password_len = strlen(wifi_password);

    // CRITICAL: Use explicit lengths, not strncpy (avoids non-termination)
    memcpy(creds.ssid, wifi_ssid, min(creds.ssid_len, sizeof(creds.ssid)));
    memcpy(creds.password, wifi_password, min(creds.password_len, sizeof(creds.password)));

    creds.timestamp = millis(); // Replay protection

    // Encrypt with AES-256-GCM (authenticated encryption)
    uint8_t iv[12];  // 96-bit nonce
    esp_fill_random(iv, sizeof(iv));

    uint8_t ciphertext[sizeof(WiFiCredentials)];
    uint8_t tag[16]; // Authentication tag

    mbedtls_gcm_context gcm;
    mbedtls_gcm_init(&gcm);
    mbedtls_gcm_setkey(&gcm, MBEDTLS_CIPHER_ID_AES, session_key, 256);

    int ret = mbedtls_gcm_crypt_and_tag(&gcm, MBEDTLS_GCM_ENCRYPT,
        sizeof(WiFiCredentials),
        iv, sizeof(iv),
        NULL, 0,  // No additional authenticated data
        (uint8_t*)&creds, ciphertext,
        16, tag);

    mbedtls_gcm_free(&gcm);

    if (ret != 0) {
        ESP_LOGE("WIFI", "Encryption failed: %d", ret);
        return false;
    }

    // Send: IV (12) + Ciphertext (112) + Tag (16) = 140 bytes
    uint8_t encrypted[140];
    memcpy(encrypted, iv, 12);
    memcpy(encrypted + 12, ciphertext, sizeof(WiFiCredentials));
    memcpy(encrypted + 12 + sizeof(WiFiCredentials), tag, 16);

    ble_send_encrypted(node_id, encrypted, sizeof(encrypted));
    return true;
}

// On Station (receiving credentials)
bool receiveWiFiCredentials(const uint8_t* encrypted_data, size_t len,
                            const uint8_t* session_key) {
    if (len != 140) {
        ESP_LOGE("WIFI", "Invalid encrypted data length: %u", len);
        return false;
    }

    uint8_t iv[12];
    uint8_t ciphertext[sizeof(WiFiCredentials)];
    uint8_t tag[16];

    memcpy(iv, encrypted_data, 12);
    memcpy(ciphertext, encrypted_data + 12, sizeof(WiFiCredentials));
    memcpy(tag, encrypted_data + 12 + sizeof(WiFiCredentials), 16);

    // Decrypt and verify
    WiFiCredentials creds;
    mbedtls_gcm_context gcm;
    mbedtls_gcm_init(&gcm);
    mbedtls_gcm_setkey(&gcm, MBEDTLS_CIPHER_ID_AES, session_key, 256);

    int ret = mbedtls_gcm_auth_decrypt(&gcm,
        sizeof(WiFiCredentials),
        iv, sizeof(iv),
        NULL, 0,
        tag, 16,
        ciphertext, (uint8_t*)&creds);

    mbedtls_gcm_free(&gcm);

    if (ret != 0) {
        ESP_LOGE("WIFI", "Decryption or authentication failed: %d", ret);
        return false;
    }

    // Replay protection: Check timestamp is recent
    uint32_t now = millis();
    if (abs((int32_t)(now - creds.timestamp)) > 60000) {
        ESP_LOGE("WIFI", "Credentials expired or replayed");
        return false;
    }

    // Validate lengths
    if (creds.ssid_len > 32 || creds.password_len > 64) {
        ESP_LOGE("WIFI", "Invalid credential lengths");
        return false;
    }

    // Store in NVS with explicit null termination
    char ssid_buf[33] = {0};
    char pass_buf[65] = {0};
    memcpy(ssid_buf, creds.ssid, creds.ssid_len);
    memcpy(pass_buf, creds.password, creds.password_len);

    nvs_set_str("wifi_ssid", ssid_buf);
    nvs_set_str("wifi_password", pass_buf);
    wifi_sta_enabled = true;

    ESP_LOGI("WIFI", "Credentials received and stored");

    // Trigger network selection
    startWiFiConnection();
    return true;
}
```

**Key features:**
- AES-256-GCM provides both encryption AND authentication
- 96-bit random IV (nonce) for each transmission
- 128-bit authentication tag prevents tampering
- Explicit length fields prevent buffer issues
- Timestamp-based replay protection
- Session key derived from BLE ECDH pairing (not hardcoded)

### 6.2 BLE Transport Clarification

**CRITICAL:** Distinguish between BLE roles in this system:

1. **BLE for Remote Sensors (Current):**
   - Remote sensors advertise BLE beacons with sensor data
   - Nodes/Gateway scan and receive advertisements (one-way)
   - Uses BLE advertising packets (31-byte payloads)
   - NOT a bidirectional connection

2. **BLE for Node-Gateway Transport (NEW):**
   - Node acts as BLE peripheral (GATT server)
   - Gateway acts as BLE central (GATT client)
   - Node initiates connection (peripheral advertises, central connects)
   - Uses GATT characteristics for bidirectional packet exchange
   - Supports up to 244-byte payloads (MTU negotiation)

**BLE transport architecture:**

```cpp
// On Node (BLE Peripheral for gateway communication)
class NodeBLETransport {
    NimBLEServer* server;
    NimBLECharacteristic* tx_char;  // Gateway reads from this
    NimBLECharacteristic* rx_char;  // Gateway writes to this

    void init() {
        server = NimBLEDevice::createServer();

        NimBLEService* svc = server->createService("LoRaVox-Transport");

        // TX characteristic (node -> gateway)
        tx_char = svc->createCharacteristic(UUID_TX,
            NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);

        // RX characteristic (gateway -> node)
        rx_char = svc->createCharacteristic(UUID_RX,
            NIMBLE_PROPERTY::WRITE);
        rx_char->setCallbacks(new RxCallbacks());

        svc->start();
        server->getAdvertising()->start();
    }

    bool sendPacket(const Packet* pkt) {
        tx_char->setValue((uint8_t*)pkt, sizeof(Packet));
        tx_char->notify();
        return true;
    }
};

// On Gateway (BLE Central for node communication)
class GatewayBLETransport {
    NimBLEClient* client;

    bool connectToNode(NimBLEAddress address) {
        client = NimBLEDevice::createClient();
        if (!client->connect(address)) {
            return false;
        }

        NimBLERemoteService* svc = client->getService("LoRaVox-Transport");
        if (!svc) return false;

        // Subscribe to node's TX characteristic
        NimBLERemoteCharacteristic* tx = svc->getCharacteristic(UUID_TX);
        tx->subscribe(true, [](NimBLERemoteCharacteristic* chr,
                               uint8_t* data, size_t len, bool isNotify) {
            handlePacketFromNode(data, len);
        });

        return true;
    }
};
```

**When to use BLE transport:**
- Node is within 30m of gateway
- Faster than LoRa (lower latency, higher throughput)
- Lower power than WiFi for continuous operation
- Already have BLE pairing for credential sharing

**BLE transport vs BLE sensor scanning:**
- These are SEPARATE BLE roles running concurrently
- Transport uses GATT connections
- Sensor scanning uses passive advertisement listening
- Both can operate simultaneously on ESP32

### 6.3 Data Flow Example

**Scenario:** Node sending sensor data to Coordinator

```
Node                    Coordinator
  │                          │
  ├─ Check WiFi ──────────────X  (No WiFi on node)
  │
  ├─ Check BLE ───────────────┐
  │                           │
  │  BLE Connected?  <────────┤
  │       Yes                 │
  │                           │
  ├─ Send via BLE ───────────►│
  │                           │
  │◄───── ACK ────────────────┤
  │                           │
```

**Scenario:** Node out of BLE range

```
Node                    Coordinator
  │                          │
  ├─ Check WiFi ──────────────X
  │
  ├─ Check BLE ───────────────X  (Out of range)
  │
  ├─ Send via LoRa ──────────►│
  │                           │
  │◄───── ACK ────────────────┤
```

## 7. Edge Cases & Failure Handling

### 7.1 Transport Failure Scenarios

| Scenario | Detection | Recovery |
|----------|-----------|----------|
| WiFi drops mid-transfer | Timeout after 5s | Retry via BLE or LoRa |
| BLE disconnects | Connection lost event | Fall back to LoRa |
| LoRa packet loss | No ACK after 3 retries | Log error, continue |
| All transports fail | Consecutive failures > 5 | Store-and-forward (future) |

### 7.2 Daily Connectivity Check

Optional feature to check for better connectivity without disrupting current operation:

```cpp
constexpr uint32_t DAILY_CHECK_HOUR = 3; // 3 AM
constexpr float MIN_BATTERY_FOR_CHECK = 3.5; // Don't check if battery low

void performDailyConnectivityCheck() {
    // Only check once per day at 3 AM
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) return;
    if (timeinfo.tm_hour != DAILY_CHECK_HOUR) return;

    uint32_t now = millis();
    if (now - last_connectivity_check_ms < 86400000) return; // Already checked today
    last_connectivity_check_ms = now;

    // Skip if battery powered and low
    if (battery_v > 0 && battery_v < MIN_BATTERY_FOR_CHECK) {
        log("Skipping connectivity check - low battery");
        return;
    }

    // Only check if currently on BLE or LoRa
    if (current_transport == TRANSPORT_WIFI) {
        log("Already on WiFi, skipping check");
        return;
    }

    // Try to establish WiFi without disrupting current connection
    WiFi.begin(wifi_ssid, wifi_password);
    if (WiFi.waitForConnectResult(30000) == WL_CONNECTED) {
        log("Better connectivity available: WiFi");

        // Only switch if significantly better
        if (current_transport == TRANSPORT_LORA) {
            log("Switching from LoRa to WiFi");
            current_transport = TRANSPORT_WIFI;
            transport_established_ms = millis();
        }
    } else {
        WiFi.disconnect(); // Clean up failed attempt
    }
}

// Call from main loop
void loop() {
    performDailyConnectivityCheck();
    // ... rest of loop
}
```

**Benefits:**
- Automatic recovery from temporary WiFi outages (router reboot, etc.)
- No disruption to working LoRa connectivity
- Battery-aware (skips check if power constrained)
- Runs during low-traffic hours

**Disable if not needed:**
```cpp
#define ENABLE_DAILY_CONNECTIVITY_CHECK false
```

### 7.3 Payload Size Handling

**CRITICAL:** Network selection does NOT handle fragmentation. Payloads must fit in LoRa's 240-byte limit since LoRa is the common fallback.

| Transport | Max Payload | Behavior if Exceeded |
|-----------|-------------|----------------------|
| WiFi | 240 bytes (application limit) | Reject - use LoRa-compatible sizes |
| BLE | 240 bytes (application limit) | Reject - use LoRa-compatible sizes |
| LoRa | 240 bytes (protocol limit) | Reject if exceeded |

**Application-layer fragmentation:**

If application needs to send >240 bytes (e.g., OTA chunks), it must implement its own fragmentation:

```cpp
// Application splits large payloads before calling transport layer
bool sendLargePayload(const uint8_t* data, size_t len) {
    constexpr size_t CHUNK_SIZE = 200; // Leave room for headers

    for (size_t offset = 0; offset < len; offset += CHUNK_SIZE) {
        size_t chunk_len = min(CHUNK_SIZE, len - offset);

        Packet chunk_pkt;
        chunk_pkt.payload_len = chunk_len;
        memcpy(chunk_pkt.payload, data + offset, chunk_len);

        // Send via current transport (WiFi/BLE/LoRa - all support 240 bytes)
        if (!sendPacket(&chunk_pkt)) {
            return false;
        }
    }

    return true;
}
```

**Rationale:** Network selection chooses transport based on availability, NOT payload size. All transports use the same 240-byte limit for consistency.

## 8. Power Considerations

### 8.1 Power Consumption Profile

Approximate current draw (ESP32-S3):

| Transport | TX Current | RX Current | Idle Current |
|-----------|-----------|-----------|--------------|
| WiFi | ~180 mA | ~100 mA | ~20 mA |
| BLE | ~30 mA | ~20 mA | ~5 mA |
| LoRa | ~120 mA | ~15 mA | ~1 mA |

### 8.2 Battery-Aware Selection

For battery-powered nodes, prioritize BLE over WiFi even if WiFi is available:

```cpp
TransportType selectTransportBatteryAware(DataPacket packet, float battery_v) {
    bool low_battery = (battery_v < 3.3); // Below 3.3V

    if (low_battery) {
        // Prefer BLE (lowest power for continuous operation)
        if (ble.isConnected()) {
            return TRANSPORT_BLE;
        }
        // LoRa for periodic transmissions (deep sleep between)
        return TRANSPORT_LORA;
    }

    // Normal selection if battery OK
    return selectTransport(packet);
}
```

## 9. Metrics & Monitoring

Track transport usage and performance:

```cpp
struct TransportMetrics {
    uint32_t wifi_packets_sent;
    uint32_t wifi_bytes_sent;
    uint32_t wifi_failures;

    uint32_t ble_packets_sent;
    uint32_t ble_bytes_sent;
    uint32_t ble_failures;

    uint32_t lora_packets_sent;
    uint32_t lora_bytes_sent;
    uint32_t lora_failures;

    uint32_t transport_switches;
    uint32_t fallback_events;
};
```

Expose via web dashboard and STATUS packets.

## 10. Future Enhancements

### 10.1 Quality-Based Selection

Use link quality metrics (RSSI, SNR, packet loss rate) to make more intelligent decisions:

```cpp
float calculateLinkScore(TransportType transport) {
    float rssi_score = normalize(getRSSI(transport), -100, -30);
    float loss_rate = getPacketLossRate(transport);
    float latency_score = normalize(getLatency(transport), 0, 500);

    return 0.5 * rssi_score + 0.3 * (1 - loss_rate) + 0.2 * latency_score;
}
```

### 10.2 Mesh Routing via WiFi

Coordinator could relay data between nodes over WiFi mesh (ESP-NOW or similar).

### 10.3 Multi-Transport Redundancy

Send critical packets over multiple transports simultaneously for reliability.

## 11. Testing Strategy

### 11.1 Test Cases

- [ ] WiFi connected, BLE disconnected → Uses WiFi
- [ ] WiFi disconnected, BLE connected → Uses BLE
- [ ] WiFi + BLE disconnected → Uses LoRa
- [ ] WiFi drops mid-session → Switches to BLE within 5s
- [ ] BLE out of range → Switches to LoRa
- [ ] Large payload (>244 bytes) with BLE connected → Uses WiFi or fragments
- [ ] Battery low (<3.3V) with WiFi available → Prefers BLE
- [ ] Rapid WiFi on/off toggling → Hysteresis prevents flapping

### 11.2 Integration Testing

- Run node for 24 hours with WiFi AP going up/down every 5 minutes
- Verify no packet loss, successful failover, and consistent metrics

## 12. Configuration

Expose transport preferences via web UI:

```cpp
struct TransportConfig {
    bool wifi_enabled;           // Enable WiFi STA mode (default: false for nodes)
    bool ble_enabled;            // Enable BLE (default: true)
    bool lora_enabled;           // LoRa always enabled (cannot disable)

    int8_t wifi_min_rssi;        // Minimum WiFi RSSI (default: -75)
    int8_t ble_min_rssi;         // Minimum BLE RSSI (default: -85)

    bool battery_aware;          // Prefer low-power transports on battery (default: true)
    uint32_t switch_cooldown_ms; // Transport switch hysteresis (default: 5000)
};
```

Store in NVS, apply on boot.

## 13. Protocol Changes Required

### 13.1 New Message Type

Add transport metadata to STATUS packets:

```cpp
struct StatusPayload {
    // ... existing fields ...
    uint8_t active_transport;    // 0=LoRa, 1=BLE, 2=WiFi
    uint8_t available_transports; // Bitmask: bit0=LoRa, bit1=BLE, bit2=WiFi
};
```

### 13.2 Message Routing

Coordinator must track which transport each node last used to route responses correctly.

---

## 14. Questions & Decisions Needed

## 15. Decisions Made

Based on project requirements:

1. **WiFi on Stations:** Supported via AES-encrypted credential sharing from Gateway during setup
2. **Payload Handling:** Keep minimal for LoRa compatibility - no fragmentation
3. **Network Selection Timing:** Only on power-up, connectivity loss, and optional daily 3 AM check
4. **BLE Role:** Gateway waits for node-initiated BLE connections
5. **Failover Strategy:** 5-minute retry window (30s, 1m, 2m, 5m backoff), then LoRa fallback
6. **Daily Check:** Enabled by default, battery-aware (skips if <3.5V), runs at 3 AM
