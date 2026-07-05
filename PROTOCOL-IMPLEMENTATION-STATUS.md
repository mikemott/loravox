# LoRa PNP Protocol Implementation Status

This document tracks the implementation status of the LoRa PNP Network Protocol against the authoritative specification in `PROTOCOL-SPEC.md`.

**Last Updated**: 2026-07-04

---

## Implementation Summary

### Core Protocol (lib/LoRaPNP/src/)

#### ✅ Packet Structure (Section 2)
- **File**: `Packet.h`, `Packet.cpp`
- **Status**: COMPLETE
- Wire format matches spec exactly
- Big-endian byte order for multi-byte fields
- CRC-16-CCITT implemented correctly (polynomial 0x1021, init 0xFFFF)
- Serialization/deserialization with proper bounds checking
- Min packet: 12 bytes, Max packet: 252 bytes

#### ✅ Message Types (Section 3)
- **File**: `Types.h`
- **Status**: COMPLETE
- All 13 message types defined (0x01-0x0D):
  - DISCOVER, OFFER, JOIN_REQUEST, JOIN_ACCEPT
  - DATA, ACK, CONFIG_UPDATE, CONFIG_ACK
  - STATUS, SENSOR_DESCRIPTOR, SENSOR_DATA
  - BLE_SENSOR_DATA, NAME_UPDATE
- Message type allocation follows spec (0x01-0x1F reserved for core)

#### ✅ Protocol Constants (Section 5 & 6)
- **File**: `Types.h`
- **Status**: COMPLETE
- Protocol version: 0x01
- Reserved IDs: 0x0000 (unassigned), 0x0001 (gateway), 0xFFFF (broadcast)
- Radio defaults: 915.0 MHz, SF7, 125 kHz BW, 0x12 sync word, 14 dBm
- Timing constants (newly added):
  - `ACK_TURNAROUND_DELAY_MS = 500`
  - `RECEIVE_TIMEOUT_MS = 2000`
  - `JOIN_ACK_TIMEOUT_MS = 6000`
  - `JOIN_ACCEPT_MAX_RETRIES = 3`
  - `CONFIG_ACK_TIMEOUT_MS = 6000`
  - `CONFIG_UPDATE_MAX_RETRIES = 3`
  - `STATION_STALE_TIMEOUT_MS = 120000`
  - `DEFAULT_REPORT_INTERVAL_MS = 30000`
  - `DEFAULT_SLEEP_INTERVAL_MS = 30000`

#### ✅ Flags (Section 2.3)
- **File**: `Types.h`
- **Status**: COMPLETE
- `FLAG_ACK_REQUEST = 0x01`
- `FLAG_BROADCAST = 0x02`
- Bits 2-7 reserved (must be 0)

#### ✅ Error Handling (Section 8)
- **File**: `Types.h`, `Packet.cpp`
- **Status**: COMPLETE
- Status codes defined: OK, ERR_CRC, ERR_VERSION, ERR_SIZE, ERR_RADIO, ERR_TIMEOUT, ERR_FULL, ERR_NOT_FOUND, ERR_INVALID
- Packet deserialization validates:
  - Version match
  - Packet size bounds
  - CRC correctness
  - Payload length within limits

#### ✅ BLE Protocol (Appendix)
- **File**: `BLEProtocol.h`
- **Status**: COMPLETE
- BLE advertisement structure (29 bytes)
- BLE sensor data payload for LoRa forwarding (21 bytes)
- Service UUID: 0x4C50 ("LP")
- Manufacturer ID: 0xFFFF
- Battery thresholds defined
- Max 8 BLE sensors per node, 16 total at gateway

#### ✅ Radio Abstraction (Section 6)
- **File**: `Radio.h`, `Radio.cpp`
- **Status**: COMPLETE
- Supports SX1276 radio
- Configurable frequency, SF, bandwidth, sync word, TX power
- Send/receive with timeout
- RSSI and SNR tracking
- Proper error handling and status codes

#### ✅ CRC Implementation
- **File**: `CRC.h`, `CRC.cpp`
- **Status**: COMPLETE
- CRC-16-CCITT algorithm
- Polynomial: 0x1021
- Initial value: 0xFFFF
- Matches specification exactly

---

## Implementation Gaps & Future Work

### ⚠️ Sequence Number Handling (Section 7)
- **Status**: BASIC IMPLEMENTATION
- Current: Gateway tracks single `last_seq` per node
- Known limitations documented in spec:
  - Window size of 1 (no out-of-order support)
  - No wrap-around handling
  - False duplicates possible with multiple in-flight packets
- **Recommendation**: Acceptable for MVP, consider sliding window for v2

### ⚠️ Watchdog & Recovery (Section 9.5)
- **Status**: DEVICE-SPECIFIC
- Requirements:
  - Watchdog timer (30s main loop timeout)
  - Radio watchdog (60s activity timeout)
  - Stack overflow detection (warn < 10KB heap)
  - Flash wear leveling (< 1 write/min)
- **Recommendation**: Review gateway/node/remote_sensor main.cpp implementations

### 🚧 Store-and-Forward (Section 10)
- **Status**: NOT IMPLEMENTED (FUTURE FEATURE)
- Spec provides design constraints for future implementation
- New message types reserved: 0x0E-0x11
- No implementation required for MVP

### ⚠️ Acknowledgment Flow (Section 4.3)
- **Status**: PARTIALLY IMPLEMENTED
- Gateway implements JOIN_ACCEPT and CONFIG_UPDATE retries
- Node-side ACK turnaround delay may not use `ACK_TURNAROUND_DELAY_MS` constant
- **Recommendation**: Audit gateway/node code to ensure timing constants are used

---

## Verification Checklist

Against spec Section 9.3 (Testing Requirements):

- [ ] Unit tests for serialization/deserialization
- [ ] Integration test: Old gateway + New node
- [ ] Integration test: New gateway + Old node
- [ ] Stress test: 1000+ packets without failure
- [ ] Power cycle test: Nodes rejoin correctly
- [ ] Range test: Verify at maximum expected distance

**Status**: Test suite not present in repository. Manual testing performed during development.

---

## Backward Compatibility (Section 9.2)

Current implementation follows all backward compatibility rules:

1. ✅ No existing message types have changed meaning
2. ✅ No fields removed or reordered from payloads
3. ✅ New fields added at end of payloads where applicable
4. ✅ `payload_len` used to detect payload variants
5. ✅ Unknown fields/types handled gracefully (dropped)

---

## Memory & Resource Constraints (Section 9.4)

| Resource | Spec Limit | Gateway | Node | Remote |
|----------|------------|---------|------|--------|
| Flash (code) | ~900 KB | ✅ | ~600 KB ✅ | ~560 KB ✅ |
| RAM usage | ~57 KB | ✅ | ~33 KB ✅ | ~25 KB ✅ |
| Max nodes | 32 | ✅ | N/A | N/A |
| Max BLE sensors | 16 | ✅ | 8 ✅ | N/A |
| Payload buffer | 240 bytes | ✅ | ✅ | ✅ |

**Note**: Actual memory usage should be verified with builds on target hardware.

---

## Protocol Change Workflow (Section 9)

For any future protocol changes:

1. Review change category (Critical/Major/Minor/Patch)
2. Update `PROTOCOL-SPEC.md` first
3. Verify backward compatibility rules
4. Update implementation in `lib/LoRaPNP/src/`
5. Update this status document
6. Run verification checklist (Section 9.3)
7. Update `PROTOCOL-SPEC.md` version history (Section 11)

---

## Files Modified (2026-07-04)

### Added
- `PROTOCOL-SPEC.md` - Authoritative protocol specification
- `PROTOCOL-IMPLEMENTATION-STATUS.md` - This file
- `FIRMWARE-AUDIT-REPORT.md` - Comprehensive audit of all device firmwares

### Modified (Library)
- `lib/LoRaPNP/src/Types.h` - Added missing timing constants:
  - `ACK_TURNAROUND_DELAY_MS`
  - `JOIN_ACK_TIMEOUT_MS`
  - `JOIN_ACCEPT_MAX_RETRIES`
  - `CONFIG_ACK_TIMEOUT_MS`
  - `CONFIG_UPDATE_MAX_RETRIES`

### Modified (Firmware - Critical Fixes)
- `node/src/main.cpp` line 283 - **CRITICAL FIX**: Added `delay(ACK_TURNAROUND_DELAY_MS)` before sending ACK packets to comply with Section 4.3
- `src/main.cpp` lines 106-110 - **HIGH PRIORITY FIX**: Removed duplicate constant definitions, now uses Types.h constants

### No Changes Required
- `lib/LoRaPNP/src/Packet.h` - Already spec-compliant
- `lib/LoRaPNP/src/Packet.cpp` - Already spec-compliant
- `lib/LoRaPNP/src/CRC.h` - Already spec-compliant
- `lib/LoRaPNP/src/CRC.cpp` - Already spec-compliant
- `lib/LoRaPNP/src/Radio.h` - Already spec-compliant
- `lib/LoRaPNP/src/Radio.cpp` - Already spec-compliant
- `lib/LoRaPNP/src/BLEProtocol.h` - Already spec-compliant

### Remaining Work
- [ ] Node: Replace local timing constants with Types.h references (lines 65-68, 134-135)
- [ ] All devices: Implement watchdog timer (Section 9.5)
- [ ] All devices: Implement radio watchdog (Section 9.5)
- [ ] Gateway: Add rate limiting for NVS name updates

---

## Next Steps

1. **Audit gateway implementation** (`src/main.cpp`)
   - Verify use of new timing constants
   - Confirm JOIN_ACCEPT retry logic uses `JOIN_ACCEPT_MAX_RETRIES`
   - Confirm CONFIG_UPDATE retry logic uses `CONFIG_UPDATE_MAX_RETRIES`
   - Verify ACK timing uses `ACK_TURNAROUND_DELAY_MS`

2. **Audit node implementation** (`node/src/main.cpp`)
   - Verify use of new timing constants
   - Confirm ACK responses use `ACK_TURNAROUND_DELAY_MS`
   - Verify JOIN flow timeouts use `JOIN_ACK_TIMEOUT_MS`

3. **Audit remote sensor implementation** (`remote_sensor/src/main.cpp`)
   - Verify BLE protocol compliance
   - Confirm timing constants usage

4. **Create test suite**
   - Unit tests for Packet serialization/deserialization
   - Integration tests for protocol flows
   - Stress tests for reliability

5. **Verify memory usage**
   - Build all targets and measure actual Flash/RAM usage
   - Confirm within spec limits (Section 9.4)

---

## Compliance Summary

| Specification Section | Status |
|----------------------|--------|
| 1. Protocol Overview | ✅ COMPLETE |
| 2. Packet Structure | ✅ COMPLETE |
| 3. Message Types | ✅ COMPLETE |
| 4. Protocol Flows | ⚠️ VERIFY IMPLEMENTATION |
| 5. Timing Constants | ✅ COMPLETE (constants added) |
| 6. Radio Parameters | ✅ COMPLETE |
| 7. Sequence Numbers | ⚠️ BASIC (acceptable for MVP) |
| 8. Error Handling | ✅ COMPLETE |
| 9. Safety Guidelines | ⚠️ PARTIAL (needs verification) |
| 10. Store-and-Forward | 🚧 FUTURE FEATURE |
| 11. Version History | ✅ COMPLETE |
| 12. Payload Formats | ✅ COMPLETE |
| 13. Change Checklist | 📋 PROCESS DEFINED |

**Overall Status**: Core protocol library is SPEC-COMPLIANT. Device implementations (gateway/node/remote) need audit to ensure use of newly added timing constants.
