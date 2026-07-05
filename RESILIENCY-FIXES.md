# Resiliency Implementation - Corrections Based on Review

## Critical Fix: Radio Watchdog Logic

**Issue Identified by OpenAI Review:**
The original `needsReset()` logic would trigger false positives by resetting a healthy but quiet radio after 60 seconds of idle time.

### Original (Flawed) Approach ❌

```cpp
// DON'T DO THIS - resets healthy quiet radios
bool Radio::needsReset() const {
    uint32_t last_activity = max(last_tx_ms_, last_rx_ms_);
    return (millis() - last_activity) > 60000;  // False positive!
}
```

**Problem:** This treats "quiet" as "hung". A gateway listening for nodes that are all asleep would reset unnecessarily.

---

### Corrected (Error-Based) Approach ✅

```cpp
// CORRECT - only resets on actual failures
bool Radio::needsReset() const {
    // Requires BOTH:
    // 1. Multiple consecutive operation failures (5+)
    // 2. Long time since last success (5+ minutes)
    if (consecutive_failures_ < RADIO_FAILURE_THRESHOLD) {
        return false;  // Not enough failures yet
    }

    uint32_t time_since_success = millis() - last_successful_op_ms_;
    return time_since_success > RADIO_STUCK_TIMEOUT_MS;  // 5 minutes
}
```

**Key Changes:**
1. **Track operation failures**, not idle time
2. **5 consecutive failures** required (not just quiet time)
3. **5-minute timeout** instead of 60 seconds (much more conservative)
4. **Success resets counter** - one good operation clears the failure count

---

## What Counts as a Failure?

### True Radio Failures (trigger watchdog) ✅

```cpp
// In sendPacket()
int16_t state = radio_->transmit(tx_buf_, len);
if (state != RADIOLIB_ERR_NONE) {
    recordOperationFailure();  // Hardware error!
    return Status::ERR_RADIO;
}
recordOperationSuccess();  // TX worked
```

### Normal Idle Time (does NOT trigger watchdog) ✅

- Gateway listening with no packets arriving → **No failure**
- Node deep sleeping → **No failure**
- Successful TX but no ACK received → **Not a radio failure** (application layer issue)
- Timeout on receivePacket() → **Not a radio failure** (just means no packet arrived)

---

## Detection Matrix

| Scenario | Consecutive Failures | Time Since Success | Reset? |
|----------|---------------------|-------------------|--------|
| Gateway listening, nodes asleep | 0 | Any | ❌ No |
| 1 transmission fails, others work | 0 (reset after success) | Any | ❌ No |
| 5 transmissions fail | 5 | <5 min | ❌ No |
| 5 transmissions fail | 5 | >5 min | ✅ **Yes** |
| Radio hardware hung | 5+ | >5 min | ✅ **Yes** |

---

## Updated Integration

### Gateway Main Loop

```cpp
void loop() {
    // Receive packets (timeout is normal, not a failure)
    Status s = radio.receivePacket(pkt, 100);
    if (s == Status::OK) {
        gateway_health.total_packets_rx++;
        // Process packet...
    }
    // Note: ERR_TIMEOUT is NOT counted as a failure

    // Check for radio hardware issues
    if (radio.needsReset()) {
        Serial.printf("🔧 Radio watchdog: %u failures over 5+ minutes\n",
                      radio.getConsecutiveFailures());
        gateway_health.radio_resets++;

        Status rs = radio.reset();
        if (rs == Status::OK) {
            Serial.println("✓ Radio reset successful");
        } else {
            Serial.println("✗ Radio reset failed - hardware issue?");
        }
    }
}
```

### Station Main Loop

```cpp
void sendStatusPacket() {
    Status s = radio.sendPacket(pending_status);
    if (s == Status::OK) {
        station_health.total_packets_tx++;
        // Wait for ACK...
    } else {
        // Radio hardware failed - failure already recorded in sendPacket()
        Serial.println("⚠️ Radio transmission failed");
    }
}

void loop() {
    // Check radio health
    if (radio.needsReset()) {
        Serial.printf("🔧 Radio watchdog: %u failures\n", radio.getConsecutiveFailures());
        station_health.radio_resets++;
        radio.reset();
        updateDisplay("Radio Reset", "Recovering...");
    }

    // Rest of loop...
}
```

---

## Why This Approach is Better

### 1. No False Positives
- Quiet networks don't trigger resets
- Deep sleep nodes don't cause gateway resets
- Only actual hardware failures trigger resets

### 2. Conservative Thresholds
- **5 failures** - temporary issues won't trigger
- **5 minutes** - gives plenty of time for recovery
- **Cleared on success** - one good TX clears the slate

### 3. Distinguishes Layers
- **Radio layer failures** = hardware problems (radio.transmit() fails)
- **Protocol layer failures** = no ACK, timeouts (application handles separately)

### 4. Monitoring Friendly
```cpp
// You can check radio health without triggering reset
uint32_t failures = radio.getConsecutiveFailures();
if (failures > 0) {
    Serial.printf("⚠️ Radio has %u consecutive failures (threshold: 5)\n", failures);
}
```

---

## Testing the Corrected Logic

### Test 1: Quiet Network (Should NOT Reset)
1. Start gateway, no stations powered on
2. Let run for 10 minutes
3. **Expected:** No radio resets (quiet ≠ hung)

### Test 2: Temporary Interference (Should NOT Reset)
1. Gateway + station communicating
2. Block signal briefly (1-2 failures)
3. Restore signal
4. **Expected:** Failures clear after successful TX, no reset

### Test 3: True Radio Hang (SHOULD Reset)
1. Simulate radio hardware failure (disconnect SPI, corrupt state)
2. Attempt 5+ transmissions over 5+ minutes
3. **Expected:** Radio reset triggered after thresholds met

### Test 4: Deep Sleep Nodes (Should NOT Reset)
1. Station goes into deep sleep (30 min interval)
2. Gateway listens during sleep period
3. **Expected:** No gateway radio reset (listening is not a failure)

---

## Summary of Changes

**Files Modified:**
- `lib/LoRaPNP/src/Radio.h` - Changed from activity tracking to failure tracking
- `lib/LoRaPNP/src/Radio.cpp` - Implemented error-based detection logic

**Key Differences:**

| Original | Corrected |
|----------|-----------|
| Time-based (60s idle) | Error-based (5 failures) |
| Tracks TX/RX activity | Tracks operation success/failure |
| Quiet = hung (false positive) | Quiet = normal (no reset) |
| 60-second timeout | 5-minute timeout |
| No failure tracking | Consecutive failures tracked |

---

## Next Steps

1. ✅ **Radio watchdog logic corrected** - no more false positives
2. ⏳ **Stale node detection** - ready to integrate (unchanged)
3. ⏳ **Health metrics** - ready to integrate (unchanged)
4. ⏳ **Extended failure recovery** - needs similar error-based approach

**Important:** The extended failure recovery for stations should also use error-based detection, not just "no ACK" counting. A missing ACK could be protocol-level, not radio-level failure.

---

**Last Updated:** 2026-07-04 (post-OpenAI review)
**Status:** Radio watchdog corrected, ready for testing
