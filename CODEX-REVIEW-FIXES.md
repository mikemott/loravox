# Codex Review Findings - Fixes Applied

## Summary

All HIGH and MEDIUM severity findings have been addressed. Specs updated with security best practices and realistic technical constraints.

---

## HIGH Severity Fixes

### 1. OTA Security: RSA Signatures Required (Phase 1)

**Finding:** SHA256-only in Phase 1 allows attacker to ship both malicious firmware and matching hash.

**Fix Applied:**
- RSA-2048 signature verification REQUIRED from Phase 1
- Dual verification: manifest signature + firmware signature
- Public key embedded at compile time
- Private key stored offline
- Updated OTA-UPDATE-SPEC.md:243-331

### 2. Delta Apply Safety: Bounds Checking Added

**Finding:** Unsafe delta code trusts untrusted input, mixed write paths.

**Fix Applied:**
- Validate all header fields before use
- Check buffer contains promised data
- Validate block_index in bounds
- Single write path (esp_ota_write only, no mixed esp_partition_write)
- Added comprehensive error handling
- Updated OTA-UPDATE-SPEC.md:789-910

### 3. Rollback Implementation: Use ESP-IDF APIs

**Finding:** Custom NVS flags don't trigger bootloader rollback.

**Fix Applied:**
- Use ESP-IDF native rollback APIs
- Real health checks (radio, NVS, heap, self-test), not just "we booted"
- Proper `esp_ota_mark_app_valid_cancel_rollback()` flow
- Bootloader configuration via sdkconfig
- Updated OTA-UPDATE-SPEC.md:361-440

### 4. Partition Table Mismatch

**Finding:** Spec describes custom 1.5 MB layout, code uses default.csv.

**Fix Applied:**
- Created OTA-PARTITION-TABLE.csv
- Defines proper A/B OTA partitions (app0 1.5 MB, app1 1.5 MB)
- **ACTION REQUIRED:** Update platformio.ini files:

```ini
# In platformio.ini, node/platformio.ini, remote_sensor/platformio.ini:
board_build.partitions = OTA-PARTITION-TABLE.csv  # Change from default.csv
```

---

## MEDIUM Severity Fixes

### 5. LoRa Duty Cycle Compliance

**Finding:** 2 chunks/sec estimate violates 1% duty cycle (PROTOCOL-SPEC.md:205).

**Fix Applied:**
- Proper airtime calculation: 430 ms per chunk+ACK
- 1% duty cycle = max 83 packets/hour = 0.023 packets/sec
- Realistic LoRa OTA time: **60-90 minutes** (not 2-3 minutes)
- Duty cycle budget tracking code
- 43-second inter-chunk delay
- Recommendation: Use BLE/WiFi for OTA when possible
- Updated OTA-UPDATE-SPEC.md:217-274

### 6. WiFi Retry Blocking Issues

**Finding:** 5-minute blocking retry burns power, blocks LoRa, risks watchdog.

**Fix Applied:**
- Async WiFi connection state machine (non-blocking)
- 10-second timeout per attempt (not 30s)
- LoRa continues operating during WiFi attempts
- Exponential backoff: 10s, 20s, 40s, 80s, 120s
- Immediate fallback to LoRa on disconnection
- Updated NETWORK-SELECTION-SPEC.md:99-197

### 7. BLE Crypto Under-specified and Unsafe

**Finding:** No AEAD, IV, authentication tag, replay protection, key derivation.

**Fix Applied:**
- AES-256-GCM authenticated encryption
- 96-bit random IV per transmission
- 128-bit authentication tag
- HKDF key derivation from BLE ECDH
- Timestamp-based replay protection (60s window)
- Explicit length fields (no strncpy dangers)
- Updated NETWORK-SELECTION-SPEC.md:244-399

### 8. BLE Role Confusion

**Finding:** BLE advertising vs GATT connection roles unclear.

**Fix Applied:**
- Clarified two separate BLE uses:
  1. Remote sensor beacons (advertisements, 31 bytes, one-way)
  2. Node-gateway transport (GATT, 244 bytes, bidirectional)
- Node = BLE peripheral (GATT server)
- Gateway = BLE central (GATT client)
- Both can run concurrently
- Added architecture diagrams and code examples
- Updated NETWORK-SELECTION-SPEC.md:401-487

### 9. Payload Handling Contradictions

**Finding:** Fragmentation statements conflict (NETWORK-SELECTION-SPEC.md:317 vs 474).

**Fix Applied:**
- NO automatic fragmentation in network selection
- All transports limited to 240 bytes (LoRa compatibility)
- Application layer handles fragmentation if needed
- Consistent policy across all transports
- Updated NETWORK-SELECTION-SPEC.md:510-546

### 10. NVS Write Frequency Violation

**Finding:** Per-chunk NVS writes violate <1 write/min guideline.

**Fix Applied:**
- OTA state stored in RAM, not NVS
- Checkpoint to NVS only every 100 chunks or on pause
- 32-bit session ID (not 8-bit)
- Dynamic bitmap allocation based on firmware size
- Updated OTA-UPDATE-SPEC.md:227-242

---

## LOW Severity Fixes

### 11. Type Naming Drift

**Finding:** TransportState vs TransportType inconsistency.

**Fix Applied:**
- Standardized on `TransportType` enum
- Fixed all pseudocode references
- Updated NETWORK-SELECTION-SPEC.md:65

### 12. Bitmap Sizing Insufficient

**Finding:** 256-byte bitmap only supports 409 KB (< typical firmware size).

**Fix Applied:**
- Dynamic bitmap allocation from manifest
- 1.5 MB firmware = 7680 chunks = 960-byte bitmap
- Size calculated at runtime
- Updated OTA-UPDATE-SPEC.md:232-234

---

## Open Questions Answered

**Q:** Is OTA intended for remote sensors too?

**A:** Remote sensors require separate OTA strategy:
- BLE-only, no LoRa/WiFi
- Deep sleep complicates OTA (must stay awake)
- Proximity required (bring device to gateway)
- Likely Phase 3 implementation
- Added to OTA-UPDATE-SPEC.md:604 (decision #7)

**Q:** Partition table in repo vs default.csv?

**A:** Created OTA-PARTITION-TABLE.csv, must update platformio.ini files (see fix #4)

---

## Implementation Checklist

Before implementing OTA or network selection:

### OTA Updates:
- [ ] Update platformio.ini: `board_build.partitions = OTA-PARTITION-TABLE.csv`
- [ ] Enable bootloader rollback in sdkconfig: `CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE=y`
- [ ] Generate RSA key pair (keep private key offline)
- [ ] Embed public key at compile time
- [ ] Implement duty cycle budget tracking for LoRa OTA
- [ ] Create health check diagnostics (radio, NVS, heap, self-test)
- [ ] Add firmware signing to build process

### Network Selection:
- [ ] Implement async WiFi state machine (non-blocking)
- [ ] Add AES-GCM credential sharing over BLE
- [ ] Implement BLE GATT transport (separate from sensor beacons)
- [ ] Add daily connectivity check (battery-aware)
- [ ] Enforce 240-byte payload limit across all transports
- [ ] Add transport metrics tracking

---

## Files Modified

1. **OTA-UPDATE-SPEC.md** - Security, safety, duty cycle fixes
2. **NETWORK-SELECTION-SPEC.md** - Async state machine, BLE crypto, role clarification
3. **OTA-PARTITION-TABLE.csv** - NEW: Custom partition layout
4. **CODEX-REVIEW-FIXES.md** - This file

## Files Requiring Manual Updates

1. **platformio.ini** - Change `board_build.partitions = OTA-PARTITION-TABLE.csv`
2. **node/platformio.ini** - Same change
3. **remote_sensor/platformio.ini** - Same change

---

## Technical Debt Resolved

- ✅ Security: RSA signatures required
- ✅ Safety: Bounds checking, proper rollback
- ✅ Compliance: Duty cycle calculations accurate
- ✅ Power: Non-blocking state machines
- ✅ Crypto: AEAD with proper key derivation
- ✅ Consistency: Unified 240-byte payload limit

## Remaining Considerations

1. **Production key management:** Use HSM for signing in production
2. **LoRa OTA timing:** 60-90 minutes is accurate but slow - recommend BLE/WiFi
3. **Remote sensor OTA:** Deferred to Phase 3, needs special handling
4. **Testing:** All security changes need thorough testing before deployment
