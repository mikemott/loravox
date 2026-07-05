# LoRaVox

A peer-to-peer LoRa mesh network implementation for ESP32 devices with BLE interface and multi-sensor support.

## Overview

LoRaVox is a robust LoRa communication system designed for building distributed sensor networks. The system supports three device types:

- **Coordinator**: Central hub that manages the network and provides BLE interface
- **Node**: Sensor-equipped device with BLE capability for local interaction
- **Remote Sensor**: Battery-powered sensor node for remote deployments

## Features

- **Mesh Networking**: Multi-hop message routing with automatic route discovery
- **BLE Interface**: Bluetooth Low Energy protocol for mobile app integration
- **Resiliency**: Packet retransmission, acknowledgments, and error recovery
- **Flexible Sensors**: Support for BMP581, SHT40, BH1750, and other I2C sensors
- **Low Power**: Optimized for battery operation on remote sensors

## Hardware

Built for ESP32-S3 (LilyGO T3-S3) with SX1262 LoRa radio. See [hardware documentation](docs/hardware/) for full specifications and wiring diagrams.

## Documentation

- [Protocol Specification](PROTOCOL-SPEC.md)
- [BLE Protocol](BLE-PROTOCOL.md)
- [Hardware Recommendations](docs/hardware/HARDWARE-RECOMMENDATIONS.md)
- [Assembly Guide](docs/assembly/ASSEMBLY-GUIDE.md)
- [Testing Strategy](THREE-TIER-TESTING.md)

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

## Development Status

See [FEATURE-TRACKER.md](FEATURE-TRACKER.md) for current implementation status and [RESILIENCY-STATUS.md](RESILIENCY-STATUS.md) for reliability improvements.
