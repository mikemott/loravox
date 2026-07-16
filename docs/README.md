# LoRa PNP Network - Documentation Index

**Version**: 1.0
**Date**: 2026-07-04
**Project**: LoRa Plug-and-Play Sensor Network (two device types: Gateway + Field Device)

---

## Quick Start

**New to the project?** Start here:

1. **Read**: `../MVP-Spec.md` - Understand the original LoRa protocol
2. **Read**: `../BLE-PROTOCOL.md` - Learn about the gateway + field-device (relay/leaf) architecture
3. **Review**: `hardware/HARDWARE-RECOMMENDATIONS.md` - Choose hardware
4. **Order**: `hardware/SUPPLIER-REFERENCE.md` - Purchase components
5. **Build**: `assembly/ASSEMBLY-GUIDE.md` - Assemble devices
6. **Wire**: `hardware/WIRING-DIAGRAMS.md` - Connect components
7. **Program**: `software/` (coming soon) - Flash firmware
8. **Deploy**: `deployment/` (coming soon) - Install in field

---

## Documentation Structure

```
docs/
├── README.md                           (This file)
├── hardware/
│   ├── HARDWARE-RECOMMENDATIONS.md     ⭐ Start here for hardware selection
│   ├── WIRING-DIAGRAMS.md              Detailed connection diagrams
│   └── SUPPLIER-REFERENCE.md           Where to buy parts
├── assembly/
│   └── ASSEMBLY-GUIDE.md               ⭐ Step-by-step build instructions
├── pcb/
│   ├── XIAO-CARRIER-BOARD.md           ⭐ Custom PCB design for remote sensors
│   └── GATEWAY-SHIELD.md               (Coming soon)
└── software/
    ├── GATEWAY-FIRMWARE.md             (Coming soon)
    ├── NODE-FIRMWARE.md                (Coming soon)
    └── REMOTE-SENSOR-FIRMWARE.md       (Coming soon)
```

---

## Hardware Documentation

### Gateway

**Two flavors**:

1. **ESP32 + Ethernet** (Edge deployment)
   - LilyGO T-Internet-POE + E22-900M30S
   - PoE powered, outdoor-rated
   - Cost: ~$70
   - See: `hardware/HARDWARE-RECOMMENDATIONS.md`

2. **Raspberry Pi 5 + LoRa HAT** (Home automation hub)
   - RAK2287 concentrator (8-channel)
   - Full Linux, Node-RED, InfluxDB, Grafana
   - Cost: ~$184
   - See: `hardware/HARDWARE-RECOMMENDATIONS.md`

### Node

**RAK4631 WisBlock** (nRF52840 + SX1262)
- BLE scanning + LoRa forwarding
- 2.85 years battery life (18650)
- Plug-and-play QWIIC sensors
- Cost: ~$81 per node
- See: `assembly/ASSEMBLY-GUIDE.md`

### Remote Sensor

**XIAO nRF52840 + Custom Carrier PCB**
- BLE advertising only
- 6-8 years battery life (2×AA Lithium)
- Tiny form factor (50×40mm)
- Cost: ~$37.50 per sensor
- See: `pcb/XIAO-CARRIER-BOARD.md`

---

## Assembly Documentation

### Time Estimates

| Device | Difficulty | Time (no soldering) | Time (with soldering) |
|--------|------------|---------------------|----------------------|
| **Gateway (Pi)** | ⭐☆☆☆☆ | 5 minutes | N/A |
| **Gateway (ESP32)** | ⭐⭐⭐☆☆ | 10 minutes | 30 minutes |
| **Node** | ⭐⭐☆☆☆ | 5 minutes | 10 minutes (battery) |
| **Remote Sensor** | ⭐☆☆☆☆ | 3 minutes | 15 minutes (PCB assembly) |

### Skill Requirements

**No Soldering Required** (if using recommended parts):
- Raspberry Pi gateway
- RAK4631 node (with pre-made battery holder)
- XIAO remote sensor (with pre-assembled PCB)

**Basic Soldering Required**:
- Battery holder JST pigtails (one-time per holder)
- ESP32 gateway LoRa module (or use breadboard)
- Custom carrier PCB assembly (or use expansion board)

**Advanced Soldering Required**:
- Custom PCB shield for gateway
- SMD component assembly (or use JLCPCB assembly service)

---

## Cost Summary

### Prototype System (1 Gateway + 3 Nodes + 10 Sensors)

| Component | Qty | Unit Cost | Total |
|-----------|-----|-----------|-------|
| **Gateway (Pi 5 + RAK2287)** | 1 | $184 | $184 |
| **Node (RAK4631)** | 3 | $86 | $258 |
| **Remote Sensor (XIAO)** | 10 | $37.50 | $375 |
| **Total** | | | **$817** |

### Per-Unit Costs (Production Volume)

| Device | Prototype (1-10×) | Small Scale (10-50×) | Production (100+×) |
|--------|-------------------|----------------------|-------------------|
| **Gateway** | $184 | $170 | $150 |
| **Node** | $86 | $75 | $65 |
| **Remote Sensor** | $37.50 | $32 | $28 |

*Savings from bulk component purchases, volume discounts*

---

## Battery Life Summary

| Device | Battery | Consumption | Life (Typical) | Life (Best Case) |
|--------|---------|-------------|----------------|------------------|
| **Gateway** | Mains (PoE/USB) | 5-15W | Unlimited | Unlimited |
| **Node** | 18650 (2500mAh) | 2.4 mAh/day | 2.85 years | 3+ years (sleep opt) |
| **Node** | Solar + LiPo | Net positive | Unlimited | Unlimited |
| **Remote Sensor** | 2×AA Lithium | 1.0 mAh/day | 6-8 years | 10+ years |
| **Remote Sensor** | CR2450 | 1.0 mAh/day | 1.4 years | 2 years |
| **Remote Sensor** | LiPo (500mAh) | 1.0 mAh/day | 1.4 years | 2 years (recharge) |

---

## Range Summary

### LoRa (Node ↔ Gateway)

| Environment | Range (SF7) | Range (SF12) | Notes |
|-------------|-------------|--------------|-------|
| **Indoor** | 100-300m | 500m-1km | Through walls, interference |
| **Urban** | 1-3 km | 5-10 km | Buildings, obstacles |
| **Suburban** | 3-5 km | 10-15 km | Less interference |
| **Rural** | 5-10 km | 15-30 km | Line-of-sight |

*Current settings: SF7, 125kHz BW, 14dBm TX power*

### BLE (Remote Sensor ↔ Node)

| Environment | Range | Notes |
|-------------|-------|-------|
| **Indoor** | 10-30m | Through walls (BLE 5.0) |
| **Outdoor** | 50-100m | Line-of-sight, 0dBm TX |
| **Long Range** | 100-300m | BLE 5.0 coded PHY (future) |

---

## Sensor Compatibility

### Supported I2C Sensors

**Temperature & Humidity**:
- Adafruit SHT40 (±0.2°C, ±2% RH) - **$5** ⭐ Recommended
- SparkFun SHT41 (±0.15°C, ±1.5% RH) - $7
- Adafruit BME680 (temp, humidity, pressure, VOC) - $20

**Pressure & Altitude**:
- SparkFun BMP581 (±0.3 hPa) - **$20** ⭐ Best accuracy
- Adafruit BMP390 (±0.5 hPa) - $10
- Seeed Grove BMP280 (±1 hPa) - $7

**Light**:
- SparkFun VEML7700 (0.0036-120k lux) - **$5** ⭐ Recommended
- Adafruit TSL2591 (IR + visible) - $7
- Adafruit VEML6075 (UV index) - $8

**Air Quality**:
- SparkFun SGP40 (VOC) - **$15** ⭐ Good value
- Adafruit PMSA003I (PM2.5 particulate) - $45
- SparkFun SCD41 (CO₂ 400-5000ppm) - $55

**All sensors with QWIIC/Stemma QT connectors are plug-and-play!**

---

## Key Design Principles

### 1. Plug-and-Play

**No configuration needed**:
- Remote sensors auto-advertise via BLE
- Nodes auto-discover gateway via LoRa
- Gateway auto-assigns IDs
- Sensors auto-detected via I2C

**No soldering required** (with recommended parts):
- QWIIC/Stemma QT connectors
- JST battery connectors
- WisBlock snap-fit modules

### 2. Battery Efficiency

**Remote sensors**:
- 99.67% sleep duty cycle
- 5µA deep sleep current
- 6-8 year battery life on AA

**Nodes**:
- BLE scanning optimization
- 2.4 mAh/day average
- 2.85 year battery life on 18650

### 3. Scalability

**Network capacity**:
- Gateway (ESP32): 32 nodes
- Gateway (Pi + RAK2287): 500+ nodes (8-channel concentrator)
- Each node: 8 BLE sensors + unlimited local I2C sensors
- Total: 256+ sensors per network

### 4. Extensibility

**Easy to add**:
- New sensor types (just plug into QWIIC)
- New node locations (auto-discovery)
- New features (OTA firmware updates)

---

## Development Roadmap

### Phase 1: Core Protocol ✅ Complete
- LoRa discovery and joining
- Reliable messaging with ACK
- Node registry and tracking
- Basic sensor framework

### Phase 2: Three-Tier Architecture ✅ Complete
- BLE remote sensors
- BLE→LoRa bridging
- MQTT publishing
- HTTP API

### Phase 3: Production Hardware (Current)
- Custom carrier PCB for remote sensors
- Gateway shield PCB
- Enclosure design
- Assembly documentation

### Phase 4: Advanced Features (Next)
- OTA firmware updates
- Sensor descriptor protocol
- BLE pairing/encryption
- Adaptive data rates
- Mesh networking (multi-hop)

### Phase 5: Cloud Integration (Future)
- Web dashboard (Grafana)
- Mobile app (React Native)
- Cloud storage (InfluxDB Cloud)
- Alerting and notifications
- ML/AI anomaly detection

---

## Common Use Cases

### Home Automation
- Indoor/outdoor temperature monitoring
- Humidity control for HVAC
- Air quality (CO₂, VOC, PM2.5)
- Leak detection (water sensors)
- **Setup**: 1 gateway (Pi), 3-5 nodes, 10-20 sensors

### Agriculture
- Soil moisture monitoring
- Greenhouse climate control
- Weather station
- Crop health (spectral sensors)
- **Setup**: 1 gateway (solar), 5-10 nodes (solar), 20-50 sensors

### Industrial
- Equipment vibration monitoring
- Power consumption tracking
- Ambient conditions (temp, humidity, light)
- Predictive maintenance
- **Setup**: 1 gateway (PoE), 10+ nodes (USB), 50-100 sensors

### Research & Education
- Environmental monitoring
- IoT development platform
- LoRa protocol research
- BLE mesh experiments
- **Setup**: 1 gateway (Pi), 2-3 nodes, 5-10 sensors

---

## Getting Help

### Documentation

- **This folder**: `/docs/` - All technical documentation
- **Project root**: `/` - MVP spec, BLE protocol, feature tracker
- **Code**: `/src/`, `/node/`, `/remote_sensor/` - Firmware source

### Community

- **GitHub Issues**: Report bugs, request features
- **GitHub Discussions**: Ask questions, share projects
- **Discord**: Real-time chat (coming soon)

### Contributing

Contributions welcome! See `CONTRIBUTING.md` (coming soon)

**Areas needing help**:
- PCB design review
- Enclosure 3D models
- Firmware optimization
- Documentation improvements
- Testing and validation

---

## License

**Hardware**: CERN OHL v2 (Permissive)
**Firmware**: MIT License
**Documentation**: CC BY-SA 4.0

Free to use, modify, and commercialize with attribution.

---

## Acknowledgments

Built on top of:
- **RadioLib**: LoRa communication library
- **NimBLE**: Bluetooth Low Energy stack
- **WisBlock**: Modular hardware ecosystem
- **QWIIC**: Plug-and-play sensor standard

---

**Questions?** Open an issue or check existing documentation.

**Ready to build?** Start with `hardware/HARDWARE-RECOMMENDATIONS.md`!
