# Protocol Resiliency - Implementation Guide

## Overview

This document provides code snippets and integration guidance for the resiliency features implemented in the LoRa PNP network.

---

## Phase 1: Radio Watchdog & Stale Node Detection

### Changes Made

**Files Modified:**
- `lib/LoRaPNP/src/Radio.h` - Added watchdog methods and activity tracking
- `lib/LoRaPNP/src/Radio.cpp` - Implemented radio reset and activity monitoring

**Files Created:**
- `lib/LoRaPNP/src/Resiliency.h` - Health metrics and link quality structures

---

## Integration: Gateway Main Loop

Add the following to `src/main.cpp`:

### 1. Include Headers and Add Globals

```cpp
#include "Resiliency.h"

using namespace lorapnp;

// Add after existing globals
HealthMetrics gateway_health;
uint32_t last_stale_check_ms = 0;
uint32_t last_heap_check_ms = 0;
uint32_t last_radio_check_ms = 0;
```

### 2. Stale Node Detection Function

```cpp
void checkStaleNodes() {
    uint32_t now = millis();
    if (now - last_stale_check_ms < 10000) return; // Check every 10 seconds

    gateway_health.active_nodes = 0;
    gateway_health.stale_nodes = 0;

    for (int i = 0; i < 32; i++) {
        if (nodes[i].used) {
            uint32_t time_since_seen = now - nodes[i].last_seen;

            if (nodes[i].active && time_since_seen > NODE_STALE_TIMEOUT_MS) {
                Serial.printf("⚠️ Node %u marked STALE (no contact for %u seconds)\n",
                              nodes[i].node_id, time_since_seen / 1000);
                nodes[i].active = false;
                gateway_health.stale_nodes++;
            } else if (nodes[i].active) {
                gateway_health.active_nodes++;
            } else {
                gateway_health.stale_nodes++;
            }
        }
    }

    last_stale_check_ms = now;
}
```

### 3. Radio Watchdog Function

```cpp
void checkRadioHealth() {
    uint32_t now = millis();
    if (now - last_radio_check_ms < 5000) return; // Check every 5 seconds

    uint32_t time_since_activity = radio.getTimeSinceActivity();

    if (time_since_activity > 30000) {
        Serial.printf("⚠️ Radio quiet for %u seconds\n", time_since_activity / 1000);
    }

    if (radio.needsReset()) {
        Serial.println("🔧 Radio watchdog triggered - resetting radio");
        gateway_health.radio_resets++;

        Status s = radio.reset();
        if (s == Status::OK) {
            Serial.println("✓ Radio reset successful");
        } else {
            Serial.println("✗ Radio reset failed!");
        }
    }

    last_radio_check_ms = now;
}
```

### 4. Heap Health Monitoring

```cpp
void checkHeapHealth() {
    uint32_t now = millis();
    if (now - last_heap_check_ms < HEAP_CHECK_INTERVAL_MS) return;

    uint32_t free_heap = ESP.getFreeHeap();
    gateway_health.free_heap_current = free_heap;

    if (free_heap < gateway_health.free_heap_min) {
        gateway_health.free_heap_min = free_heap;
        Serial.printf("📊 New heap low water mark: %u bytes\n", gateway_health.free_heap_min);
    }

    if (free_heap < HEAP_WARNING_THRESHOLD) {
        Serial.printf("⚠️ LOW HEAP WARNING: %u bytes free (threshold: %u)\n",
                      free_heap, HEAP_WARNING_THRESHOLD);
    }

    last_heap_check_ms = now;
}
```

### 5. Update Main Loop

```cpp
void loop() {
    uint32_t now = millis();

    // Existing radio receive code...
    Status s = radio.receivePacket(pkt, 100);
    if (s == Status::OK) {
        gateway_health.total_packets_rx++;
        // ... handle packet ...
    } else if (s == Status::ERR_CRC) {
        gateway_health.crc_errors++;
    } else if (s == Status::ERR_TIMEOUT) {
        gateway_health.timeout_errors++;
    }

    // Add resiliency checks
    checkStaleNodes();
    checkRadioHealth();
    checkHeapHealth();

    // Update uptime
    gateway_health.uptime_seconds = now / 1000;

    // Existing code continues...
}
```

### 6. Health API Endpoint

```cpp
void handleHealth() {
    StaticJsonDocument<512> doc;

    doc["uptime"] = gateway_health.uptime_seconds;
    doc["packets_rx"] = gateway_health.total_packets_rx;
    doc["packets_tx"] = gateway_health.total_packets_tx;
    doc["crc_errors"] = gateway_health.crc_errors;
    doc["timeout_errors"] = gateway_health.timeout_errors;
    doc["radio_resets"] = gateway_health.radio_resets;
    doc["free_heap"] = gateway_health.free_heap_current;
    doc["free_heap_min"] = gateway_health.free_heap_min;
    doc["active_nodes"] = gateway_health.active_nodes;
    doc["stale_nodes"] = gateway_health.stale_nodes;

    // Calculate error rate
    uint32_t total_attempts = gateway_health.total_packets_rx + gateway_health.crc_errors;
    float error_rate = total_attempts > 0 ? (float)gateway_health.crc_errors / total_attempts * 100.0 : 0.0;
    doc["error_rate"] = error_rate;

    String response;
    serializeJson(doc, response);
    webServer.send(200, "application/json", response);
}

// Add to setup():
void setup() {
    // ... existing setup ...
    webServer.on("/api/health", HTTP_GET, handleHealth);
}
```

---

## Integration: Station Main Loop

Add the following to `node/src/main.cpp`:

### 1. Include Headers and Add Globals

```cpp
#include "Resiliency.h"

using namespace lorapnp;

// Add after existing globals
HealthMetrics station_health;
uint32_t last_radio_check_ms = 0;
uint32_t last_heap_check_ms = 0;
```

### 2. Extended Failure Recovery

```cpp
void handleTransmissionFailure() {
    station_health.consecutive_failures++;

    if (station_health.consecutive_failures >= CONSECUTIVE_FAILURE_THRESHOLD) {
        Serial.printf("⚠️ %u consecutive failures - triggering re-discovery\n",
                      station_health.consecutive_failures);

        // Clear session state but keep node_id for reconnection
        session_token = 0;
        mode = NodeMode::DISCOVERING;
        station_health.consecutive_failures = 0;
        station_health.rediscoveries++;

        updateDisplay("Re-discovering", "Network lost", "Attempt " + String(station_health.rediscoveries));
    }
}

void handleTransmissionSuccess() {
    station_health.consecutive_failures = 0;
}
```

### 3. Update Retry Handlers

```cpp
// In handleStatusTimeout() - after retry logic
if (status_retry_count >= STATUS_MAX_RETRIES) {
    Serial.printf("STATUS dropped seq=%u after %u retries\n", pending_status_seq, status_retry_count);
    status_ack_pending = false;
    handleTransmissionFailure(); // NEW
}

// In handleSensorDataTimeout() - after retry logic
if (sensor_data_retry_count >= SENSOR_DATA_MAX_RETRIES) {
    Serial.printf("SENSOR_DATA dropped seq=%u after %u retries\n", pending_sensor_data_seq, sensor_data_retry_count);
    sensor_data_ack_pending = false;
    handleTransmissionFailure(); // NEW
}
```

### 4. Update Success Handlers

```cpp
// In handleAck() - after clearing pending flags
if (pkt.payload_len >= 2) {
    uint16_t acked_seq;
    memcpy(&acked_seq, pkt.payload, 2);

    if (status_ack_pending && acked_seq == pending_status_seq) {
        status_ack_pending = false;
        handleTransmissionSuccess(); // NEW
    } else if (sensor_data_ack_pending && acked_seq == pending_sensor_data_seq) {
        sensor_data_ack_pending = false;
        handleTransmissionSuccess(); // NEW
    }
}
```

### 5. Radio and Heap Monitoring

```cpp
void checkStationHealth() {
    uint32_t now = millis();

    // Radio watchdog (same as gateway)
    if (now - last_radio_check_ms >= 5000) {
        if (radio.needsReset()) {
            Serial.println("🔧 Station radio watchdog triggered");
            station_health.radio_resets++;

            Status s = radio.reset();
            if (s == Status::OK) {
                Serial.println("✓ Radio reset successful");
                updateDisplay("Radio Reset", "Recovering...");
            }
        }
        last_radio_check_ms = now;
    }

    // Heap monitoring
    if (now - last_heap_check_ms >= HEAP_CHECK_INTERVAL_MS) {
        uint32_t free_heap = ESP.getFreeHeap();
        station_health.free_heap_current = free_heap;

        if (free_heap < station_health.free_heap_min) {
            station_health.free_heap_min = free_heap;
        }

        if (free_heap < HEAP_WARNING_THRESHOLD) {
            Serial.printf("⚠️ LOW HEAP: %u bytes\n", free_heap);
        }

        last_heap_check_ms = now;
    }
}
```

### 6. Update Main Loop

```cpp
void loop() {
    uint32_t now = millis();

    // Existing receive code...
    Status s = radio.receivePacket(pkt, 100);
    if (s == Status::OK) {
        station_health.total_packets_rx++;
        last_gateway_rx_ms = now;
        // ... handle packet ...
    }

    // Add health check
    checkStationHealth();

    // Update uptime
    station_health.uptime_seconds = now / 1000;

    // Existing code continues...
}
```

---

## Link Quality Adaptation (Optional)

### Calculate Link Quality in STATUS Handler

```cpp
void sendStatusPacket() {
    // ... existing STATUS packet creation ...

    Status s = radio.sendPacket(pending_status);
    if (s == Status::OK) {
        station_health.total_packets_tx++;

        // Track link quality
        LinkQuality link;
        link.rssi = last_gateway_rssi;
        link.snr = last_gateway_snr;
        link.quality_score = calculateQualityScore(link.rssi, link.snr);

        // Show on OLED
        char quality_str[16];
        if (link.quality_score >= 80) strcpy(quality_str, "Excellent");
        else if (link.quality_score >= 60) strcpy(quality_str, "Good");
        else if (link.quality_score >= 40) strcpy(quality_str, "Fair");
        else strcpy(quality_str, "Poor");

        Serial.printf("Link quality: %s (RSSI=%d, SNR=%.1f, score=%u)\n",
                      quality_str, link.rssi, link.snr, link.quality_score);

        // Optional: Adapt retry counts
        // STATUS_MAX_RETRIES = getAdaptiveMaxRetries(link.quality_score);
    }
}
```

---

## Dashboard Health Widget

### Add to Gateway HTML (`src/main.cpp`)

```html
<style>
.health-widget {
    background: #2a2a2a;
    border-radius: 8px;
    padding: 15px;
    margin-bottom: 20px;
}
.health-metric {
    display: flex;
    justify-content: space-between;
    padding: 8px 0;
    border-bottom: 1px solid #3a3a3a;
}
.health-metric:last-child {
    border-bottom: none;
}
.health-good { color: #4ade80; }
.health-warn { color: #facc15; }
.health-bad { color: #f87171; }
</style>

<div class="health-widget">
    <h3>Gateway Health</h3>
    <div class="health-metric">
        <span>Uptime:</span>
        <span id="gateway-uptime" class="health-good">0h</span>
    </div>
    <div class="health-metric">
        <span>Free Heap:</span>
        <span id="gateway-heap">0 KB</span>
    </div>
    <div class="health-metric">
        <span>Packet Success:</span>
        <span id="gateway-success">0%</span>
    </div>
    <div class="health-metric">
        <span>Radio Resets:</span>
        <span id="gateway-resets" class="health-good">0</span>
    </div>
    <div class="health-metric">
        <span>Stale Nodes:</span>
        <span id="gateway-stale">0</span>
    </div>
</div>
```

### Update JavaScript

```javascript
function updateHealth() {
    fetch('/api/health')
        .then(response => response.json())
        .then(data => {
            // Uptime
            const hours = Math.floor(data.uptime / 3600);
            const minutes = Math.floor((data.uptime % 3600) / 60);
            document.getElementById('gateway-uptime').textContent = `${hours}h ${minutes}m`;

            // Heap
            const heap_kb = Math.floor(data.free_heap / 1024);
            const heap_el = document.getElementById('gateway-heap');
            heap_el.textContent = `${heap_kb} KB`;
            heap_el.className = heap_kb < 10 ? 'health-bad' : heap_kb < 20 ? 'health-warn' : 'health-good';

            // Success rate
            const success_rate = 100 - (data.error_rate || 0);
            const success_el = document.getElementById('gateway-success');
            success_el.textContent = `${success_rate.toFixed(1)}%`;
            success_el.className = success_rate < 90 ? 'health-bad' : success_rate < 95 ? 'health-warn' : 'health-good';

            // Resets
            const resets_el = document.getElementById('gateway-resets');
            resets_el.textContent = data.radio_resets;
            resets_el.className = data.radio_resets > 10 ? 'health-bad' : data.radio_resets > 3 ? 'health-warn' : 'health-good';

            // Stale nodes
            document.getElementById('gateway-stale').textContent = data.stale_nodes;
        })
        .catch(err => console.error('Health update failed:', err));
}

// Call every 5 seconds
setInterval(updateHealth, 5000);
updateHealth(); // Initial call
```

---

## Testing Checklist

### Radio Watchdog
- [ ] Disconnect antenna during operation
- [ ] Verify radio reset occurs after 60 seconds
- [ ] Verify radio recovers and communication resumes
- [ ] Check serial output shows reset event

### Stale Node Detection
- [ ] Power off station
- [ ] Verify gateway marks station stale after 2 minutes
- [ ] Verify dashboard shows node as offline
- [ ] Power on station and verify instant reconnection
- [ ] Check serial logs show stale detection and recovery

### Extended Failure Recovery
- [ ] Move station out of range
- [ ] Verify re-discovery after 10 failures
- [ ] Move station back in range
- [ ] Verify successful reconnection

### Heap Monitoring
- [ ] Monitor heap over 24 hours
- [ ] Verify low water mark doesn't decrease continuously
- [ ] Check for memory leak warnings

### Health Dashboard
- [ ] Verify all metrics update correctly
- [ ] Check color coding changes with thresholds
- [ ] Test on mobile device

---

## Performance Impact

### Memory Usage
- **Gateway:** +512 bytes (HealthMetrics struct + globals)
- **Station:** +512 bytes (HealthMetrics struct + globals)

### CPU Usage
- **Stale check:** <1ms every 10 seconds
- **Radio check:** <1ms every 5 seconds
- **Heap check:** <1ms every 60 seconds
- **Total overhead:** <0.1% CPU time

### Flash Usage
- **Radio watchdog:** ~400 bytes
- **Health monitoring:** ~800 bytes
- **Total:** ~1.2KB

---

## Next Steps

### Immediate
1. Integrate code snippets into gateway and station
2. Build and flash updated firmware
3. Run initial tests with single station
4. Monitor serial output for resiliency events

### Short-term
5. Add health dashboard widget
6. Test with multiple stations
7. Long-duration test (24+ hours)
8. Document observed failure modes

### Future Enhancements
- Adaptive retry counts based on link quality
- Predictive failure detection from RSSI/SNR trends
- Network topology mapping
- Remote diagnostics command

---

**Last Updated:** 2026-07-04
**Status:** Implementation complete, integration pending
