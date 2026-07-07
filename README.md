# LoRaVox

A peer-to-peer LoRa mesh network implementation for ESP32 devices with BLE interface and multi-sensor support.

## Overview

LoRaVox is a robust LoRa communication system designed for building distributed sensor networks. The system supports three device types:

- **Coordinator**: Central hub that manages the network and provides BLE interface
- **Node**: Sensor-equipped device with BLE capability for local interaction
- **Remote Sensor**: Battery-powered sensor node for remote deployments

## Features

- **Mesh Networking**: Multi-hop message routing with automatic route discovery
- **Adaptive Transport**: WiFi > BLE > LoRa priority with automatic fallback
- **Secure OTA Updates**: RSA-2048 signed firmware updates with rollback protection
- **BLE Interface**: Bluetooth Low Energy protocol for mobile app integration
- **Resiliency**: Packet retransmission, acknowledgments, and error recovery
- **Flexible Sensors**: Support for BMP581, SHT40, BH1750, and other I2C sensors
- **Low Power**: Optimized for battery operation on remote sensors

## Hardware

Built for ESP32-S3 (LilyGO T3-S3) with SX1262 LoRa radio. See [hardware documentation](docs/hardware/) for full specifications and wiring diagrams.

## Documentation

### Core Specifications
- [Protocol Specification](PROTOCOL-SPEC.md)
- [BLE Protocol](BLE-PROTOCOL.md)
- [Network Selection](NETWORK-SELECTION-SPEC.md)
- [OTA Updates](OTA-UPDATE-SPEC.md)
- [Testing Strategy](THREE-TIER-TESTING.md)

### Additional Documentation
- [Changelog](CHANGELOG.md)
- [Feature Tracker](docs/FEATURE-TRACKER.md)
- [Resiliency Status](docs/RESILIENCY-STATUS.md)
- [Dashboard](docs/DASHBOARD.md)
- [Naming System](docs/NAMING-SYSTEM.md)
- [Hardware Recommendations](docs/hardware/HARDWARE-RECOMMENDATIONS.md)
- [Assembly Guide](docs/assembly/ASSEMBLY-GUIDE.md)

## Project Structure

```
├── src/                    # Coordinator firmware
├── node/                   # Node firmware with BLE
├── remote_sensor/          # Remote sensor firmware
├── lib/LoRaPNP/           # Shared protocol library
└── docs/                   # Technical documentation
```

## Getting Started

This project uses PlatformIO. To build:

```bash
# Build coordinator
pio run

# Build node
cd node && pio run

# Build remote sensor
cd remote_sensor && pio run
```

## OTA Firmware Updates

To create signed firmware updates:

```bash
# Build firmware
pio run -e lilygo-t3s3

# Sign firmware
python3 tools/sign_firmware.py .pio/build/lilygo-t3s3/firmware.bin -v

# Deploy firmware.bin.signed to OTA server
```

See [OTA-UPDATE-SPEC.md](OTA-UPDATE-SPEC.md) for complete documentation and [tools/keys/README.md](tools/keys/README.md) for key management.

## Development Status

See [CHANGELOG.md](CHANGELOG.md) for recent changes, [FEATURE-TRACKER.md](docs/FEATURE-TRACKER.md) for current implementation status, and [RESILIENCY-STATUS.md](docs/RESILIENCY-STATUS.md) for reliability improvements.
