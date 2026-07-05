# LoRa PNP Network Protocol Specification & Safety Guidelines

## Document Purpose

This document serves as the authoritative reference for the LoRa PNP network protocol. All protocol modifications MUST be reviewed against this specification to ensure backward compatibility, reliability, and safety.

---

## 1. Protocol Overview

### 1.1 Architecture

```
┌─────────────────┐     LoRa (915 MHz)      ┌─────────────────┐
│    Gateway      │◄────────────────────────►│  Station/Node   │
│  (ID: 0x0001)   │                          │  (ID: 0x0002+)  │
│                 │                          │                 │
│  - Web Dashboard│                          │  - Sensors      │
│  - WiFi AP/STA  │                          │  - BLE Scanner  │
│  - mDNS         │                          │  - Deep Sleep   │
└─────────────────┘                          └────────┬────────┘
                                                      │ BLE
                                                      ▼
                                             ┌─────────────────┐
                                             │  Remote Sensor  │
                                             │  (BLE only)     │
                                             │                 │
                                             │  - Battery      │
                                             │  - Deep Sleep   │
                                             └─────────────────┘
```

### 1.2 Protocol Version

- **Current Version**: `0x01`
- **Version Location**: First byte of every packet
- **Compatibility Rule**: Nodes MUST reject packets with mismatched versions

---

## 2. Packet Structure

### 2.1 Wire Format

```
┌─────────┬──────┬───────┬────────┬────────┬─────┬─────────┬─────────┬───────┐
│ Version │ Type │ Flags │ Src ID │ Dst ID │ Seq │ Pay Len │ Payload │ CRC16 │
│ 1 byte  │1 byte│1 byte │2 bytes │2 bytes │2 bytes│1 byte │0-240 B │2 bytes│
└─────────┴──────┴───────┴────────┴────────┴─────┴─────────┴─────────┴───────┘
          │◄──────────────── Header (10 bytes) ────────────►│
```

**Constraints:**
- **Minimum packet size**: 12 bytes (header + CRC, no payload)
- **Maximum packet size**: 252 bytes (header + 240 payload + CRC)
- **Byte order**: Big-endian for all multi-byte fields
- **CRC**: CRC-16-CCITT (polynomial 0x1021, init 0xFFFF)

### 2.2 Field Definitions

| Field | Size | Description | Valid Range |
|-------|------|-------------|-------------|
| `version` | 1 | Protocol version | `0x01` (current) |
| `type` | 1 | Message type enum | `0x01` - `0x0D` |
| `flags` | 1 | Packet flags bitmap | See §2.3 |
| `src_id` | 2 | Source node ID | `0x0001` - `0xFFFE` |
| `dst_id` | 2 | Destination node ID | `0x0001` - `0xFFFF` |
| `seq` | 2 | Sequence number | `0x0000` - `0xFFFF` |
| `payload_len` | 1 | Payload byte count | `0` - `240` |
| `payload` | 0-240 | Message-specific data | Varies by type |
| `crc16` | 2 | CRC-16-CCITT checksum | Computed |

### 2.3 Flags

| Bit | Name | Description |
|-----|------|-------------|
| 0 | `FLAG_ACK_REQUEST` | Sender expects ACK response |
| 1 | `FLAG_BROADCAST` | Message is broadcast (dst_id = 0xFFFF) |
| 2-7 | Reserved | MUST be 0 |

### 2.4 Reserved IDs

| ID | Purpose |
|----|---------|
| `0x0000` | Unassigned (node not yet joined) |
| `0x0001` | Gateway (always) |
| `0xFFFF` | Broadcast address |

---

## 3. Message Types

### 3.1 Message Type Registry

| Value | Name | Direction | ACK? | Description |
|-------|------|-----------|------|-------------|
| `0x01` | DISCOVER | Node → Gateway | No | Node discovery broadcast |
| `0x02` | OFFER | Gateway → Node | No | Gateway offers node ID |
| `0x03` | JOIN_REQUEST | Node → Gateway | No | Node requests to join |
| `0x04` | JOIN_ACCEPT | Gateway → Node | Yes* | Gateway accepts join |
| `0x05` | DATA | Node → Gateway | Optional | Generic data packet |
| `0x06` | ACK | Both | No | Acknowledgment |
| `0x07` | CONFIG_UPDATE | Gateway → Node | Yes* | Configuration push |
| `0x08` | CONFIG_ACK | Node → Gateway | No | Config acknowledgment |
| `0x09` | STATUS | Node → Gateway | Optional | Node health status |
| `0x0A` | SENSOR_DESCRIPTOR | Node → Gateway | Optional | Sensor metadata |
| `0x0B` | SENSOR_DATA | Node → Gateway | Optional | Sensor readings |
| `0x0C` | BLE_SENSOR_DATA | Node → Gateway | No | BLE sensor forwarding |
| `0x0D` | NAME_UPDATE | Gateway → Node | Yes* | Custom name push |
| `0x0E` | REBOOT | Gateway → Node | Optional | Remote reboot command |

*Gateway-initiated messages use application-level retry, not FLAG_ACK_REQUEST

### 3.2 Message Type Allocation Rules

- `0x01` - `0x1F`: Core protocol (reserved)
- `0x20` - `0x7F`: Standard extensions (future use)
- `0x80` - `0xFE`: Vendor-specific
- `0xFF`: Reserved

**SAFETY RULE**: New message types MUST be added to the end of the range. Never reuse or reorder existing type values.

---

## 4. Protocol Flows

### 4.1 Discovery & Join

```
    Node                          Gateway
      │                              │
      │──── DISCOVER (broadcast) ───►│  src=0x0000, dst=0xFFFF
      │                              │
      │◄─────── OFFER ───────────────│  Offers node_id, session_token
      │                              │
      │──── JOIN_REQUEST ───────────►│  Includes device_uid, session_token
      │                              │
      │◄───── JOIN_ACCEPT ───────────│  Confirms node_id, config params
      │                              │  (Gateway retries up to 3x if no ACK)
      │──────── ACK ────────────────►│
      │                              │
      │      [Node is now ACTIVE]    │
```

### 4.2 Sensor Data Reporting

```
    Node                          Gateway
      │                              │
      │── SENSOR_DESCRIPTOR ────────►│  First time: describe sensors
      │◄──────── ACK ────────────────│  (if FLAG_ACK_REQUEST set)
      │                              │
      │──── SENSOR_DATA ────────────►│  Periodic readings
      │◄──────── ACK ────────────────│  (if FLAG_ACK_REQUEST set)
      │                              │
```

### 4.3 Acknowledgment Rules

1. ACK MUST be sent within 500ms of receiving packet with `FLAG_ACK_REQUEST`
2. ACK payload contains the `seq` number being acknowledged (2 bytes)
3. ACK MUST be sent even for duplicate packets (ensures delivery)
4. ACK MUST NOT set `FLAG_ACK_REQUEST` (no ACK-of-ACK)

---

## 5. Timing Constants

### 5.1 Current Values

| Constant | Value | Purpose |
|----------|-------|---------|
| `ACK_TURNAROUND_DELAY_MS` | 500 | Delay before sending ACK |
| `RECEIVE_TIMEOUT_MS` | 2000 | Blocking receive timeout |
| `JOIN_ACK_TIMEOUT_MS` | 6000 | Wait for JOIN_ACCEPT ACK |
| `JOIN_ACCEPT_MAX_RETRIES` | 3 | Max JOIN_ACCEPT retransmissions |
| `CONFIG_ACK_TIMEOUT_MS` | 6000 | Wait for CONFIG_ACK |
| `CONFIG_UPDATE_MAX_RETRIES` | 3 | Max CONFIG_UPDATE retransmissions |
| `STATION_STALE_TIMEOUT_MS` | 120000 | Mark node stale after 2 min |
| `DEFAULT_REPORT_INTERVAL_MS` | 30000 | Default sensor report interval |
| `DEFAULT_SLEEP_INTERVAL_MS` | 30000 | Default deep sleep duration |

### 5.2 Timing Safety Rules

- **Minimum ACK delay**: 100ms (allow radio turnaround)
- **Maximum ACK delay**: 2000ms (before sender times out)
- **Retry backoff**: Exponential with jitter recommended
- **Maximum retry count**: 5 (prevent infinite loops)

---

## 6. Radio Parameters

### 6.1 Default Configuration

| Parameter | Default | Valid Range |
|-----------|---------|-------------|
| Frequency | 915.0 MHz | 902-928 MHz (US) |
| Spreading Factor | SF7 | SF7-SF12 |
| Bandwidth | 125 kHz | 125, 250, 500 kHz |
| Coding Rate | 4/5 | 4/5, 4/6, 4/7, 4/8 |
| Sync Word | 0x12 | 0x00-0xFF |
| TX Power | 14 dBm | 2-20 dBm |

### 6.2 Radio Safety Rules

- **Duty cycle**: Stay under 1% for regulatory compliance
- **Airtime budget**: Track per-node airtime usage
- **Power adjustment**: Prefer lower power when RSSI > -60 dBm

---

## 7. Sequence Number Handling

### 7.1 Current Implementation

- 16-bit unsigned sequence number per node
- Wraps from 0xFFFF to 0x0000
- Gateway tracks single `last_seq` per node for duplicate detection

### 7.2 Sequence Number Rules

1. Nodes MUST increment `seq` for each new transmission
2. Retransmissions MUST use the same `seq` as original
3. Gateway MUST accept sequence numbers in any order (no strict ordering)
4. Duplicate detection: `new_seq == last_seq` indicates duplicate

### 7.3 Known Limitations

- **Window size**: 1 (only tracks last received)
- **Out-of-order**: Multiple in-flight packets may cause false duplicate detection
- **Wrap handling**: Not explicitly handled (relies on 65536 gap being unlikely)

---

## 8. Error Handling

### 8.1 Status Codes

| Code | Name | Recovery Action |
|------|------|-----------------|
| `OK` | Success | Continue normally |
| `ERR_CRC` | CRC mismatch | Drop packet, do not ACK |
| `ERR_VERSION` | Version mismatch | Drop packet, log warning |
| `ERR_SIZE` | Invalid size | Drop packet |
| `ERR_RADIO` | Radio error | Reset radio, retry |
| `ERR_TIMEOUT` | Receive timeout | Continue listening |
| `ERR_FULL` | Buffer full | Drop oldest or reject new |
| `ERR_NOT_FOUND` | Not found | Application-specific |
| `ERR_INVALID` | Invalid data | Drop packet |

### 8.2 Error Recovery Patterns

**Radio Hang Recovery:**
```cpp
if (radio_operation_failed) {
    radio.standby();
    delay(10);
    radio.startReceive();
}
```

**Node Reconnection:**
- If no ACK received for 5 consecutive transmissions, trigger re-discovery
- If gateway doesn't see node for `STATION_STALE_TIMEOUT_MS`, mark stale

---

## 9. Safety Guidelines for Protocol Changes

### 9.1 Change Categories

| Category | Risk | Review Required |
|----------|------|-----------------|
| **Critical** | Breaking change | Full protocol review + migration plan |
| **Major** | New message type | Compatibility analysis |
| **Minor** | Payload extension | Backward compatibility check |
| **Patch** | Bug fix | Regression testing |

### 9.2 Backward Compatibility Rules

1. **NEVER** change the meaning of existing message types
2. **NEVER** reorder or remove fields from existing payloads
3. **ALWAYS** add new fields at the end of payloads
4. **ALWAYS** use `payload_len` to detect extended payloads
5. **ALWAYS** ignore unknown fields (forward compatibility)

### 9.3 Testing Requirements

Before any protocol change:

- [ ] Unit tests for serialization/deserialization
- [ ] Integration test: Old gateway + New node
- [ ] Integration test: New gateway + Old node
- [ ] Stress test: 1000+ packets without failure
- [ ] Power cycle test: Nodes rejoin correctly
- [ ] Range test: Verify at maximum expected distance

### 9.4 Memory & Resource Constraints

| Resource | Gateway | Node | Remote Sensor |
|----------|---------|------|---------------|
| Flash (code) | ~900 KB | ~600 KB | ~560 KB |
| RAM | ~57 KB used | ~33 KB used | ~25 KB used |
| Max nodes | 32 | N/A | N/A |
| Max BLE sensors | 16 | 8 per node | N/A |
| Payload buffer | 240 bytes | 240 bytes | 240 bytes |

**SAFETY RULE**: New features MUST NOT increase RAM usage by more than 5KB per device without explicit approval.

### 9.5 Watchdog & Recovery

All devices MUST implement:

1. **Watchdog timer**: Reset if main loop doesn't complete within 30 seconds
2. **Radio watchdog**: Reset radio if no RX/TX activity for 60 seconds
3. **Stack overflow detection**: Monitor free heap, warn if < 10KB
4. **Flash wear leveling**: Limit NVS writes to < 1 per minute average

---

## 10. Store-and-Forward Requirements (Future)

### 10.1 Design Constraints

If implementing store-and-forward:

1. **Buffer size**: Maximum 100 readings per node (prevent memory exhaustion)
2. **TTL**: Messages expire after 24 hours (prevent stale data flood)
3. **Priority**: Real-time data takes precedence over buffered data
4. **Batch size**: Maximum 10 readings per packet (fit in payload)
5. **Flow control**: Gateway MUST be able to signal "stop sending"

### 10.2 Failure Modes to Handle

- Node flash full → Drop oldest readings
- Gateway restart → Nodes detect via missing beacons, retransmit
- Prolonged outage → Timestamp all data for proper ordering
- Packet storms → Rate limiting and backpressure

### 10.3 New Message Types Required

| Type | Name | Purpose |
|------|------|---------|
| `0x0E` | BULK_DATA_START | Announce batch transmission |
| `0x0F` | BULK_DATA_CHUNK | Chunk of historical data |
| `0x10` | BULK_DATA_END | End of batch, request cumulative ACK |
| `0x11` | BULK_ACK | Cumulative acknowledgment with gaps |

---

## 11. Version History

| Version | Date | Changes |
|---------|------|---------|
| 0x01 | 2024 | Initial protocol |

---

## 12. Appendix: Payload Formats

### 12.1 SENSOR_DATA Payload

```
┌───────────┬─────────────┬─────────────┬───────────┬────────────┐
│ sensor_id │ sensor_type │ sensor_unit │ data_type │ value      │
│ 1 byte    │ 1 byte      │ 1 byte      │ 1 byte    │ 4 bytes    │
└───────────┴─────────────┴─────────────┴───────────┴────────────┘
```

### 12.2 BLE_SENSOR_DATA Payload

```
┌────────────┬──────┬────────────┬───────────┬─────────────┬─────────────┬───────────┬───────┬──────────────┐
│ ble_uid    │ rssi │ battery_mv │ sensor_id │ sensor_type │ sensor_unit │ data_type │ value │ timestamp_ms │
│ 6 bytes    │ 1 B  │ 2 bytes    │ 1 byte    │ 1 byte      │ 1 byte      │ 1 byte    │ 4 B   │ 4 bytes      │
└────────────┴──────┴────────────┴───────────┴─────────────┴─────────────┴───────────┴───────┴──────────────┘
```

### 12.3 STATUS Payload

```
┌────────────┬───────────┬───────────┬──────────────┬─────────────┬─────────────────┐
│ battery_mv │ uptime_s  │ free_heap │ status_flags │ device_uid  │ report_interval │
│ 2 bytes    │ 4 bytes   │ 4 bytes   │ 1 byte       │ 6 bytes     │ 4 bytes         │
└────────────┴───────────┴───────────┴──────────────┴─────────────┴─────────────────┘
```

### 12.4 REBOOT Payload

**Direction:** Gateway → Station
**Purpose:** Remote reboot of station (useful for I2C sensor detection)

```
┌───────────────┬───────┐
│ delay_seconds │ flags │
│ 1 byte        │ 1 B   │
└───────────────┴───────┘
```

**Fields:**
- `delay_seconds` (uint8): Seconds to wait before reboot (0-255)
  - 0 = immediate reboot
  - 5 = recommended (allows time for ACK transmission)
- `flags` (uint8): Reserved for future use (MUST be 0x00)

**Behavior:**
- Station SHOULD send ACK before rebooting (best effort)
- Station MUST reboot after specified delay
- Station SHOULD log reboot reason to NVS for debugging
- Gateway SHOULD NOT retry if ACK not received (station may have already rebooted)
- User interface MUST confirm before sending (prevents accidental reboots)

**Use Cases:**
- Detect newly connected I2C sensors without physical access
- Recover from software errors remotely
- Apply configuration changes that require restart

---

## 13. Checklist for Protocol Changes

Before submitting any protocol change:

- [ ] Updated this specification document
- [ ] Backward compatibility verified
- [ ] Memory impact assessed (< 5KB RAM increase)
- [ ] Flash write frequency acceptable (< 1/min)
- [ ] All error paths have recovery
- [ ] Watchdog will not trigger during operation
- [ ] Tested with 2+ nodes simultaneously
- [ ] No hardcoded magic numbers (use constants)
- [ ] CRC validated on all new payloads
- [ ] Sequence number handling is correct
