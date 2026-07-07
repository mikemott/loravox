# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added - PR #3 (OTA Updates)
- OTA firmware update system with RSA-2048 signature verification
- Resume support after power loss (NVS checkpoints)
- Out-of-order chunk support via `esp_partition_write()`
- Firmware signing script (`tools/sign_firmware.py`)
- RSA-2048 key pair generation and management
- Comprehensive key management documentation
- Delta firmware generation tool (Phase 1)

### Added - PR #2 (Network Selection)
- Adaptive network transport layer (WiFi > BLE > LoRa priority)
- Non-blocking async WiFi connection state machine
- Daily connectivity checks (configurable hour)
- Battery-aware transport selection
- Network metrics tracking (RSSI, packet counts, failures)
- WiFi credential storage in NVS
- NetworkManager coordinator for transport selection

### Added - PR #1 (OTA Partition Table)
- Custom partition table with OTA support
- 1.3 MB app partitions (app0 and app1)
- OTA data partition for rollback protection

### Fixed - PR #3
- Signature verification memory exhaustion (streaming hash computation)
- Hash verification boundary bug (non-4096-aligned firmware)
- Resume after reboot (switched to partition API)
- Firmware size validation against partition size
- NVS session restore (firmware_size and chunk_size)
- Embedded real RSA-2048 public key (replaced placeholder)

### Fixed - PR #2
- Serial `wifi off` command now uses NetworkManager API
- WiFi.mode() conflict (preserved AP+STA mode)
- Transport switching on WiFi connection state change
- Serial WiFi config consistency with web interface

### Security
- RSA-2048 signature verification for firmware updates
- SHA256 firmware integrity checks
- ESP-IDF rollback protection with health diagnostics
- Private key excluded from git repository

## [0.1.0] - 2026-07-02

### Added
- Initial project structure
- LoRa mesh networking protocol
- BLE GATT server for mobile app integration
- Multi-sensor support (BMP581, SHT40, BH1750)
- Web dashboard for configuration
- Three device types: Coordinator, Node, Remote Sensor
- Packet routing and acknowledgment system
- Custom naming system for devices
- mDNS discovery (lorapnp.local)

### Documentation
- Protocol specification (PROTOCOL-SPEC.md)
- BLE protocol documentation (BLE-PROTOCOL.md)
- Network selection specification (NETWORK-SELECTION-SPEC.md)
- OTA update specification (OTA-UPDATE-SPEC.md)
- Three-tier testing strategy (THREE-TIER-TESTING.md)
- Hardware recommendations
- Assembly guide

[Unreleased]: https://github.com/mikemott/loravox/compare/v0.1.0...HEAD
[0.1.0]: https://github.com/mikemott/loravox/releases/tag/v0.1.0
