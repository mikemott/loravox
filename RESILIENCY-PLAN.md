# LoRa PNP Network - Resiliency & Self-Healing Implementation Plan

## Overview

This document outlines a comprehensive plan to enhance protocol resiliency and self-healing capabilities for the LoRa PNP network. The goal is to create a robust system that automatically recovers from common failure modes without manual intervention.

---

## Current State Analysis

### Implemented Features ✅
- Basic retry logic for JOIN_ACCEPT, STATUS, and SENSOR_DATA (3 retries, 6s timeout)
- Reconnection path for nodes with saved credentials
- Sequence number duplicate detection
- Last seen tracking in gateway
- Error status codes (CRC, timeout, radio errors)

### Missing Resiliency Features ⚠️
1. Radio hardware hang detection and recovery
2. Stale node detection and cleanup
3. Extended failure recovery (automatic re-discovery)
4. Gateway restart detection by stations
5. Watchdog timer implementation
6. Health monitoring and degradation detection
7. Adaptive retry strategies based on link quality
8. Radio activity monitoring
9. Memory leak detection
10. Flash wear protection

---

## Priority 1: Critical Self-Healing Features

### 1. Radio Watchdog & Recovery

**Problem:** Radio hardware can hang, requiring reset. No automatic detection or recovery.

**Solution:** Radio activity monitor with automatic reset

**Implementation:**
```cpp
// Radio.h additions
class Radio {
public:
    void updateActivity();
    bool needsReset() const;
    Status reset();
private:
    uint32_t last_tx_ms_;
    uint32_t last_rx_ms_;
    static constexpr uint32_t RADIO_ACTIVITY_TIMEOUT_MS = 60000;
};

// Radio.cpp
void Radio::updateActivity() {
    last_rx_ms_ = millis();
}

bool Radio::needsReset() const {
    uint32_t now = millis();
    uint32_t last_activity = max(last_tx_ms_, last_rx_ms_);
    return (now - last_activity) > RADIO_ACTIVITY_TIMEOUT_MS;
}

Status Radio::reset() {
    radio_->standby();
    delay(10);
    return init(current_config_);
}
```

**Integration Points:**
- Gateway main loop: Check `radio.needsReset()` and call `radio.reset()` if needed
- Station main loop: Same check with display notification
- Log radio resets for debugging

**Testing:**
- Simulate radio hang by disconnecting antenna during transmission
- Verify automatic recovery within 60 seconds
- Monitor serial output for reset events

---

### 2. Stale Node Detection

**Problem:** Gateway tracks nodes but doesn't mark them as stale/offline after timeout.

**Solution:** Automatic stale detection with configurable timeout

**Implementation:**
```cpp
// src/main.cpp additions
constexpr uint32_t NODE_STALE_TIMEOUT_MS = 120000; // 2 minutes

void checkStaleNodes() {
    uint32_t now = millis();
    for (int i = 0; i < 32; i++) {
        if (nodes[i].active) {
            uint32_t time_since_seen = now - nodes[i].last_seen;
            if (time_since_seen > NODE_STALE_TIMEOUT_MS) {
                Serial.printf("Node %u marked STALE (no contact for %u seconds)\n",
                              nodes[i].node_id, time_since_seen / 1000);
                nodes[i].active = false;
                // Keep UID mapping for fast reconnection
                // Don't recycle node_id yet
            }
        }
    }
}

// Call from main loop every 10 seconds
```

**Features:**
- Nodes marked inactive after 2 minutes of silence
- UID mapping preserved for instant reconnection
- Dashboard shows node as offline
- Serial log notifies of state changes

**Testing:**
- Power off station and verify gateway marks it stale after 2 minutes
- Power on station and verify instant reconnection using saved node_id
- Check dashboard shows correct online/offline status

---

### 3. Extended Failure Recovery (Auto Re-Discovery)

**Problem:** Stations keep retrying failed transmissions indefinitely without re-discovering network.

**Solution:** Automatic re-discovery after extended failures

**Implementation:**
```cpp
// node/src/main.cpp additions
constexpr uint8_t CONSECUTIVE_FAILURE_THRESHOLD = 10;
uint8_t consecutive_failures = 0;
uint32_t last_successful_tx_ms = 0;

void handleTransmissionFailure() {
    consecutive_failures++;

    if (consecutive_failures >= CONSECUTIVE_FAILURE_THRESHOLD) {
        Serial.printf("⚠️ %u consecutive failures - triggering re-discovery\n",
                      consecutive_failures);

        // Clear session state but keep node_id for reconnection
        session_token = 0;
        mode = NodeMode::DISCOVERING;
        consecutive_failures = 0;

        updateDisplay("Re-discovering", "Network lost");

        // Note: UID and node_id are preserved in NVS for fast rejoin
    }
}

void handleTransmissionSuccess() {
    consecutive_failures = 0;
    last_successful_tx_ms = millis();
}

// Call after each ACK timeout or successful ACK
```

**Trigger Conditions:**
- 10 consecutive transmission failures (no ACKs received)
- Automatic transition to DISCOVERING mode
- Preserves node_id for reconnection attempt first
- Falls back to new discovery if gateway doesn't recognize

**Testing:**
- Block gateway radio and verify station re-discovers after 10 failures
- Unblock gateway and verify station reconnects using saved credentials
- Test with gateway restart scenario

---

### 4. Gateway Heartbeat (Beacon)

**Problem:** Stations can't detect gateway restarts or long periods of silence.

**Solution:** Optional gateway beacon for network presence detection

**Implementation:**
```cpp
// Gateway beacon (optional, disabled by default to save airtime)
constexpr uint32_t GATEWAY_BEACON_INTERVAL_MS = 60000; // 1 minute
uint32_t last_beacon_ms = 0;
bool beacon_enabled = false;

void sendGatewayBeacon() {
    if (!beacon_enabled) return;

    uint32_t now = millis();
    if (now - last_beacon_ms < GATEWAY_BEACON_INTERVAL_MS) return;

    Packet beacon;
    beacon.version = PROTOCOL_VERSION;
    beacon.type = MessageType::BEACON; // New message type 0x0F
    beacon.flags = FLAG_BROADCAST;
    beacon.src_id = gateway_id;
    beacon.dst_id = BROADCAST_ID;
    beacon.seq = tx_seq++;
    beacon.payload_len = 4;

    // Payload: [uptime_seconds (4 bytes)]
    uint32_t uptime = millis() / 1000;
    memcpy(beacon.payload, &uptime, 4);

    sendPacket(beacon);
    last_beacon_ms = now;
}

// Serial command: "beacon on" / "beacon off"
```

**Station Beacon Monitoring:**
```cpp
// node/src/main.cpp
uint32_t last_beacon_rx_ms = 0;
constexpr uint32_t BEACON_TIMEOUT_MS = 300000; // 5 minutes

void handleBeacon(const Packet& pkt) {
    last_beacon_rx_ms = millis();
    // Gateway still alive - no action needed
}

void checkBeaconTimeout() {
    if (!beacon_enabled) return;

    uint32_t now = millis();
    if (mode == NodeMode::JOINED &&
        last_beacon_rx_ms > 0 &&
        (now - last_beacon_rx_ms) > BEACON_TIMEOUT_MS) {
        Serial.println("⚠️ Gateway beacon timeout - may have restarted");
        // Don't immediately re-discover, but increase alertness
        // Next transmission failure will trigger re-discovery faster
    }
}
```

**Trade-offs:**
- **Airtime cost:** 1 packet/minute = ~0.1% duty cycle at SF7
- **Benefits:** Faster gateway restart detection, network presence confirmation
- **Recommendation:** Off by default, enable for critical deployments

**Testing:**
- Enable beacon, restart gateway, verify stations detect absence
- Measure airtime impact with multiple stations

---

## Priority 2: Health Monitoring

### 5. System Watchdog Timer

**Problem:** No protection against infinite loops or deadlocks.

**Solution:** ESP32 hardware watchdog with periodic feeding

**Implementation:**
```cpp
// Gateway and Station both need this
#include <esp_task_wdt.h>

constexpr uint32_t WDT_TIMEOUT_SECONDS = 30;
uint32_t last_wdt_feed_ms = 0;

void setup() {
    // ... existing setup ...

    // Enable watchdog
    esp_task_wdt_init(WDT_TIMEOUT_SECONDS, true);
    esp_task_wdt_add(NULL); // Add current task
    Serial.printf("Watchdog enabled: %u second timeout\n", WDT_TIMEOUT_SECONDS);
}

void loop() {
    uint32_t now = millis();

    // Feed watchdog every loop iteration
    if (now - last_wdt_feed_ms > 1000) {
        esp_task_wdt_reset();
        last_wdt_feed_ms = now;
    }

    // ... existing loop code ...
}
```

**Features:**
- 30-second watchdog timeout
- Automatic reset if main loop hangs
- NVS logging of watchdog resets for debugging
- Serial output on boot shows last reset reason

**Testing:**
- Introduce deliberate infinite loop and verify reset occurs
- Check serial output shows watchdog reset reason
- Verify normal operation doesn't trigger false resets

---

### 6. Health Metrics Collection

**Problem:** No systematic monitoring of system health trends.

**Solution:** Health metrics structure with trend analysis

**Implementation:**
```cpp
// Gateway health metrics
struct GatewayHealth {
    uint32_t total_packets_rx;
    uint32_t total_packets_tx;
    uint32_t crc_errors;
    uint32_t timeout_errors;
    uint32_t radio_resets;
    uint32_t uptime_seconds;
    uint32_t free_heap_min;
    uint32_t active_nodes;
    uint32_t stale_nodes;
};
GatewayHealth gateway_health;

void updateHealthMetrics() {
    gateway_health.uptime_seconds = millis() / 1000;
    gateway_health.free_heap_min = min(
        gateway_health.free_heap_min,
        ESP.getFreeHeap()
    );

    // Count active/stale nodes
    gateway_health.active_nodes = 0;
    gateway_health.stale_nodes = 0;
    for (int i = 0; i < 32; i++) {
        if (nodes[i].used) {
            if (nodes[i].active) {
                gateway_health.active_nodes++;
            } else {
                gateway_health.stale_nodes++;
            }
        }
    }
}

// Station health metrics (similar structure)
struct StationHealth {
    uint32_t total_packets_tx;
    uint32_t total_packets_rx;
    uint32_t ack_timeouts;
    uint32_t consecutive_failures;
    uint32_t radio_resets;
    uint32_t rediscoveries;
    uint32_t uptime_seconds;
    uint32_t free_heap_min;
    float avg_rssi;
    float avg_snr;
};
StationHealth station_health;
```

**API Endpoint (Gateway):**
```cpp
// GET /api/health
void handleHealth() {
    StaticJsonDocument<512> doc;
    doc["uptime"] = gateway_health.uptime_seconds;
    doc["packets_rx"] = gateway_health.total_packets_rx;
    doc["packets_tx"] = gateway_health.total_packets_tx;
    doc["crc_errors"] = gateway_health.crc_errors;
    doc["radio_resets"] = gateway_health.radio_resets;
    doc["free_heap"] = ESP.getFreeHeap();
    doc["free_heap_min"] = gateway_health.free_heap_min;
    doc["active_nodes"] = gateway_health.active_nodes;
    doc["stale_nodes"] = gateway_health.stale_nodes;

    String response;
    serializeJson(doc, response);
    webServer.send(200, "application/json", response);
}
```

**Dashboard Health Widget:**
```html
<div class="health-widget">
    <h3>Gateway Health</h3>
    <div class="health-metric">
        <span>Uptime:</span>
        <span id="gateway-uptime">0h</span>
    </div>
    <div class="health-metric">
        <span>Free Heap:</span>
        <span id="gateway-heap">0 KB</span>
    </div>
    <div class="health-metric">
        <span>Error Rate:</span>
        <span id="gateway-errors">0%</span>
    </div>
</div>
```

**Benefits:**
- Early detection of memory leaks (decreasing free_heap_min)
- Network quality trends (error rates)
- System stability monitoring (reset counts)

---

### 7. Memory Leak Detection

**Problem:** Long-running devices can develop memory leaks over time.

**Solution:** Heap monitoring with automatic alerts

**Implementation:**
```cpp
constexpr uint32_t HEAP_WARNING_THRESHOLD = 10240; // 10KB
uint32_t last_heap_check_ms = 0;
uint32_t heap_low_water_mark = UINT32_MAX;

void checkHeapHealth() {
    uint32_t now = millis();
    if (now - last_heap_check_ms < 60000) return; // Check every minute

    uint32_t free_heap = ESP.getFreeHeap();

    if (free_heap < heap_low_water_mark) {
        heap_low_water_mark = free_heap;
        Serial.printf("📊 New heap low water mark: %u bytes\n", heap_low_water_mark);
    }

    if (free_heap < HEAP_WARNING_THRESHOLD) {
        Serial.printf("⚠️ LOW HEAP WARNING: %u bytes free (threshold: %u)\n",
                      free_heap, HEAP_WARNING_THRESHOLD);
        // Could trigger MQTT alert or set warning flag in status
    }

    last_heap_check_ms = now;
}
```

**Automatic Recovery:**
- If heap drops below critical level, trigger selective cleanup
- Purge oldest BLE sensor data
- Clear stale node entries
- Last resort: automatic reboot with NVS flag

---

## Priority 3: Adaptive Behavior

### 8. Link Quality Based Adaptation

**Problem:** Fixed retry counts don't adapt to varying link conditions.

**Solution:** Dynamic retry strategy based on RSSI/SNR

**Implementation:**
```cpp
struct LinkQuality {
    int16_t rssi;
    float snr;
    uint8_t quality_score; // 0-100
};

uint8_t calculateQualityScore(int16_t rssi, float snr) {
    // Excellent: RSSI > -60, SNR > 8
    // Good: RSSI > -80, SNR > 5
    // Fair: RSSI > -100, SNR > 2
    // Poor: worse than fair

    uint8_t score = 0;

    if (rssi > -60 && snr > 8) score = 90;
    else if (rssi > -80 && snr > 5) score = 70;
    else if (rssi > -100 && snr > 2) score = 50;
    else score = 30;

    return score;
}

uint8_t getAdaptiveMaxRetries(uint8_t quality_score) {
    if (quality_score >= 80) return 2;  // Excellent link
    if (quality_score >= 60) return 3;  // Good link
    if (quality_score >= 40) return 5;  // Fair link
    return 7;  // Poor link - more retries needed
}

uint32_t getAdaptiveTimeout(uint8_t quality_score) {
    if (quality_score >= 80) return 4000;  // Excellent link
    if (quality_score >= 60) return 6000;  // Good link
    if (quality_score >= 40) return 8000;  // Fair link
    return 12000; // Poor link - longer timeout
}
```

**Integration:**
- Update retry counters to use adaptive values
- Display link quality on OLED
- Log quality changes for debugging

---

### 9. Flash Wear Protection

**Problem:** Frequent NVS writes can wear out flash memory.

**Solution:** Write throttling and dirty-bit tracking

**Implementation:**
```cpp
struct NVSWriteThrottle {
    uint32_t last_write_ms;
    bool dirty;
    static constexpr uint32_t MIN_WRITE_INTERVAL_MS = 60000; // 1 minute
};

NVSWriteThrottle nvs_throttle;

void markNVSDirty() {
    nvs_throttle.dirty = true;
}

void flushNVSIfNeeded() {
    uint32_t now = millis();

    if (!nvs_throttle.dirty) return;

    if (now - nvs_throttle.last_write_ms < NVSWriteThrottle::MIN_WRITE_INTERVAL_MS) {
        return; // Too soon, skip write
    }

    // Perform batched NVS writes here
    // (e.g., save all sensor descriptors at once)

    nvs_throttle.dirty = false;
    nvs_throttle.last_write_ms = now;
    Serial.println("💾 NVS flushed");
}
```

**Benefits:**
- Extends flash lifespan
- Reduces write amplification
- Batches multiple updates into single write cycle

---

## Implementation Phases

### Phase 1: Critical Safety (Week 1)
- [ ] Radio watchdog and recovery
- [ ] Stale node detection
- [ ] System watchdog timer
- [ ] Basic health metrics collection

### Phase 2: Self-Healing (Week 2)
- [ ] Extended failure recovery
- [ ] Memory leak detection
- [ ] Flash wear protection
- [ ] Gateway beacon (optional)

### Phase 3: Adaptive Behavior (Week 3)
- [ ] Link quality based adaptation
- [ ] Health dashboard widget
- [ ] Comprehensive testing

---

## Testing Strategy

### Unit Tests
- Radio reset recovery
- Stale node timeout calculations
- Link quality score calculation
- NVS write throttling

### Integration Tests
- Station re-discovery after 10 failures
- Gateway recovery after restart
- Multi-node stress test (8+ stations)
- Long-duration test (24+ hours)

### Failure Mode Testing
1. **Radio Hang:** Disconnect antenna mid-transmission
2. **Gateway Crash:** Force gateway reboot during active session
3. **Station Power Loss:** Remove power, restore after various intervals
4. **Poor Link Quality:** Test at maximum range with obstacles
5. **Memory Pressure:** Simulate low heap conditions
6. **Flash Wear:** Rapid configuration changes

---

## Metrics for Success

### Reliability Targets
- **Packet delivery rate:** >95% in good conditions (RSSI > -80)
- **Recovery time:** <2 minutes for station after gateway restart
- **Uptime:** >30 days continuous operation without manual intervention
- **False positive rate:** <1% for stale node detection
- **Memory stability:** Heap low water mark stays above 15KB

### Monitoring Dashboard
New `/api/reliability` endpoint returning:
```json
{
  "packet_success_rate": 97.5,
  "avg_recovery_time_s": 45,
  "radio_resets_total": 2,
  "nodes_rediscovered": 5,
  "uptime_hours": 720,
  "heap_low_water_kb": 18
}
```

---

## Documentation Updates

After implementation, update:
- [ ] PROTOCOL-SPEC.md - Add BEACON message type, update error handling section
- [ ] FEATURE-TRACKER.md - Mark resiliency features complete
- [ ] DASHBOARD.md - Document health widget
- [ ] Create RESILIENCY-GUIDE.md - Troubleshooting guide for operators

---

## Open Questions

1. **Beacon trade-off:** Enable by default or opt-in? (Recommendation: opt-in)
2. **Adaptive retries:** Should we reduce retries for excellent links? (Yes for power savings)
3. **Memory threshold:** 10KB warning level appropriate for ESP32-S3? (Conservative but safe)
4. **Stale timeout:** 2 minutes too aggressive for deep-sleep nodes? (Increase to 5 minutes?)

---

## Future Enhancements (Post-MVP)

- Network topology mapping (which nodes hear each other)
- Automatic mesh routing (store-and-forward between nodes)
- Predictive failure detection (trends in RSSI/SNR degradation)
- Remote diagnostics (request detailed logs from stations)
- Firmware update over LoRa (delta updates)

---

## Commit Strategy

Break implementation into atomic commits:
1. `feat(resiliency): add radio watchdog and recovery`
2. `feat(resiliency): implement stale node detection`
3. `feat(resiliency): add system watchdog timer`
4. `feat(resiliency): implement health metrics collection`
5. `feat(resiliency): add extended failure recovery`
6. `feat(resiliency): implement memory leak detection`
7. `feat(resiliency): add flash wear protection`
8. `feat(resiliency): implement link quality adaptation`
9. `feat(resiliency): add health dashboard widget`
10. `docs(resiliency): comprehensive testing and documentation`

Each commit should be independently testable and deployable.

---

**Document Version:** 1.0
**Date:** 2026-07-04
**Author:** Mike + Claude Sonnet 4.5
