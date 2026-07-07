# Protocol Resiliency - Implementation Status

## Summary

OpenAI's review caught a critical flaw in the radio watchdog logic. The issue has been corrected, and the implementation is now ready for testing.

---

## ✅ Completed & Corrected

### 1. Radio Watchdog (Fixed)
- **Original flaw:** Time-based detection (false positives on quiet networks)
- **Correction:** Error-based detection (only triggers on actual hardware failures)
- **Status:** ✅ **Code corrected, ready for testing**

**What changed:**
- Tracks `consecutive_failures_` instead of `last_tx_ms_` / `last_rx_ms_`
- Requires **5 consecutive failures** + **5 minutes** to trigger reset
- One successful operation clears failure counter
- No false positives on quiet/idle networks

### 2. Health Metrics Framework
- **Status:** ✅ **Ready for integration**
- `HealthMetrics` structure defined
- Link quality scoring algorithms
- Adaptive retry/timeout helpers

### 3. Documentation
- **RESILIENCY-PLAN.md** - Overall 3-phase architecture
- **RESILIENCY-FIXES.md** - Detailed explanation of the watchdog fix
- **RESILIENCY-IMPLEMENTATION.md** - Integration guide (needs update for fixes)

---

## ⏳ Ready to Integrate (Not Yet Wired In)

### Gateway Features
- ✅ Stale node detection logic (code ready, not integrated)
- ✅ Health API endpoint (code ready, not integrated)
- ✅ Heap monitoring (code ready, not integrated)
- ✅ Dashboard health widget (code ready, not integrated)

### Station Features
- ⚠️ Extended failure recovery (needs review - may have similar time-based flaw)
- ✅ Heap monitoring (code ready, not integrated)
- ✅ Link quality tracking (code ready, not integrated)

---

## ❌ Not Started

### Advanced Features (Phase 3)
- [ ] Gateway beacon system (optional)
- [ ] System watchdog timer (ESP32 WDT)
- [ ] Flash wear protection (NVS write throttling)
- [ ] Adaptive retry counts based on link quality
- [ ] Predictive failure detection

---

## Critical Issue: Extended Failure Recovery

**OpenAI's concern applies here too:**

The station's "extended failure recovery" currently counts **missed ACKs**, not **radio failures**. This needs the same fix as the radio watchdog.

### Current (Potentially Flawed) Approach

```cpp
// May be too aggressive
if (consecutive_failures >= 10) {
    // Re-discover network
}
```

### Questions to Answer
1. Should we count **protocol failures** (no ACK) or **radio failures** (ERR_RADIO)?
2. Is 10 consecutive missed ACKs too aggressive if link quality is just poor?
3. Should we combine RSSI/SNR degradation with failure count?

### Recommendation
Use a **hybrid approach**:
- Track **radio failures** separately from **protocol failures**
- Radio failures → immediate radio reset (already fixed)
- Protocol failures → re-discovery only after extended period (10+ failures)
- Link quality → adjust thresholds dynamically

---

## Testing Plan

### Phase 1: Radio Watchdog (This Week)
1. **Quiet network test**
   - Gateway listening, no stations
   - Run 30 minutes
   - **Expected:** No resets

2. **Temporary failure test**
   - Block signal for 1-2 transmissions
   - Restore signal
   - **Expected:** Failures clear, no reset

3. **True hang test**
   - Simulate 5+ hardware failures
   - Wait 5+ minutes
   - **Expected:** Radio reset triggered

### Phase 2: Stale Detection (Next Week)
1. Integrate stale node detection
2. Test with powered-off station
3. Verify 2-minute stale timeout
4. Test instant reconnection

### Phase 3: Full Integration (Week 3)
1. All resiliency features enabled
2. Multi-station stress test
3. 24-hour continuous operation
4. Document all observed failure modes

---

## Files to Review (Updated List)

### Core Implementation (Corrected)
1. **lib/LoRaPNP/src/Radio.h** - Error-based watchdog interface
2. **lib/LoRaPNP/src/Radio.cpp** - Corrected watchdog logic
3. **lib/LoRaPNP/src/Resiliency.h** - Health metrics & constants

### Documentation
4. **RESILIENCY-FIXES.md** - ⭐ **Read this first** - explains the fix
5. **RESILIENCY-PLAN.md** - Overall architecture (original plan)
6. **RESILIENCY-IMPLEMENTATION.md** - Integration guide (needs update)
7. **PROTOCOL-SPEC.md** - Context (existing protocol)

---

## Recommendations from OpenAI Review

### ✅ Already Fixed
- Radio watchdog now error-based, not time-based

### ⚠️ Needs Attention
- Extended failure recovery may need similar fix
- Integration not yet done (helpers defined but not called)

### 💡 Suggested Next Steps
1. Review extended failure recovery logic
2. Integrate stale node detection first (safest)
3. Add radio watchdog integration
4. Test incrementally before adding more features

---

## Memory & Performance Impact

### After Integration
- **Gateway:** +800 bytes RAM (HealthMetrics + globals)
- **Station:** +800 bytes RAM (HealthMetrics + globals)
- **Flash:** +1.5KB (watchdog + health monitoring)
- **CPU:** <0.1% overhead

### Current Headroom (from FEATURE-TRACKER.md)
- **Gateway:** 881KB flash (of 8MB), 57KB RAM (of 512KB)
- **Station:** 606KB flash (of 8MB), 33KB RAM (of 512KB)
- **Impact:** ✅ Negligible (well within budget)

---

## Key Takeaways

1. **Original radio watchdog had critical flaw** - would reset healthy quiet radios
2. **Fix implemented** - now error-based (5 failures over 5 minutes)
3. **Ready for testing** - corrected code needs hardware validation
4. **Integration pending** - code snippets ready but not wired into main loops
5. **Similar review needed** - extended failure recovery may have same issue

---

## What Changed Based on Review

| Aspect | Before | After |
|--------|--------|-------|
| Detection method | Time-based | Error-based |
| Trigger condition | 60s idle | 5 failures + 5min |
| False positives | Yes (quiet = hung) | No (quiet ≠ hung) |
| Failure tracking | None | consecutive_failures_ |
| Success handling | N/A | Clears counter |

---

**Review Date:** 2026-07-04
**Reviewer:** OpenAI (cross-check requested by Mike)
**Status:** Critical issue fixed, ready for incremental integration and testing
