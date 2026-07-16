# LoRaVox

A peer-to-peer LoRa mesh network implementation for ESP32 devices with BLE interface and multi-sensor support.

## Overview

LoRaVox is a robust LoRa communication system designed for building distributed sensor networks. The system has **two device types**:

- **Gateway**: Central hub that bridges the network to the internet, aggregates telemetry, and serves the dashboard.
- **Field Device**: Everything in the field, described by its capabilities rather than a fixed role — it hosts sensors, optionally **relays** other devices' data (a relaying field device is what we used to call a "station"), and connects over WiFi, BLE, and/or LoRa. Relay is independent of power; a battery device can relay.

See [docs/OVERVIEW.md](docs/OVERVIEW.md) for the full architecture.

## Features

- **Mesh Networking**: Multi-hop message routing with automatic route discovery
- **BLE Interface**: Bluetooth Low Energy protocol for mobile app integration
- **Resiliency**: Packet retransmission, acknowledgments, and error recovery
- **Flexible Sensors**: Support for BMP581, SHT40, BH1750, and other I2C sensors
- **Low Power**: Optimized for battery operation on remote sensors

## Hardware

Built for ESP32-S3 (LilyGO T3-S3) with SX1262 LoRa radio. See [hardware documentation](docs/hardware/) for full specifications and wiring diagrams.

## Documentation

- [Protocol Specification](docs/PROTOCOL-SPEC.md)
- [BLE Protocol](docs/BLE-PROTOCOL.md)
- [Hardware Recommendations](docs/hardware/HARDWARE-RECOMMENDATIONS.md)
- [Assembly Guide](docs/assembly/ASSEMBLY-GUIDE.md)
- [Testing Strategy](docs/THREE-TIER-TESTING.md)

## Project Structure

```
├── src/                    # Gateway firmware
├── node/                   # Field device (relay build: LoRa + BLE scan/forward)
├── remote_sensor/          # Field device (leaf build: battery, BLE-only)
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

See [FEATURE-TRACKER.md](FEATURE-TRACKER.md) for current implementation status.
