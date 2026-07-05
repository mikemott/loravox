# LoRa PNP Network - Hardware Recommendations

**Version**: 1.0
**Date**: 2026-07-04
**Status**: Production Ready

---

## System Architecture

Three-tier plug-and-play sensor network:

```
Remote Sensors (BLE, Battery)
    ↓ 100-300m BLE range
Nodes (BLE + LoRa, Battery/Solar/USB)
    ↓ 2-10km LoRa range
Gateway (LoRa + WiFi/Ethernet, Mains)
    ↓ Internet
Cloud/Dashboard
```

---

## 1. Gateway Hardware (Two Flavors)

### Gateway Type A: ESP32 + Ethernet (Edge Gateway)

**Primary Use**: Remote installations, outdoor deployments, warehouses, solar farms

**Recommended Hardware: LilyGO T-Internet-POE + External LoRa**

| Component | Model | Specs | Cost |
|-----------|-------|-------|------|
| **Base Board** | LilyGO T-Internet-POE | ESP32 240MHz, W5500 Ethernet, PoE 802.3af | $38 |
| **LoRa Module** | Ebyte E22-900M30S | SX1262, 30dBm (1W), 915MHz, DIP | $12 |
| **Antenna** | 915MHz 3dBi Omni | RP-SMA, outdoor rated | $5 |
| **Enclosure** | Polycase WC-42 | 120×80×55mm, IP65, DIN rail | $15 |
| **OLED** (optional) | SSD1306 0.96" I2C | Status display | $5 |
| **Total** | | | **$70-75** |

**Key Features**:
- PoE powered (single cable for power + network)
- No soldering if using DIP module + breadboard
- 30dBm LoRa = better receive sensitivity and range
- Suitable for outdoor installation

**Connections**:
```
T-Internet-POE GPIO → E22-900M30S
  3.3V → VCC
  GND → GND
  GPIO19 → MOSI
  GPIO23 → MISO
  GPIO18 → SCK
  GPIO5 → NSS
  GPIO14 → DIO1
  GPIO27 → RST
  GPIO26 → BUSY
```

---

### Gateway Type B: Mini PC (Advanced Automation Hub)

**Primary Use**: Home automation, data logging, AI/ML, complex rule engines

**Recommended Hardware: Raspberry Pi 5 + LoRa Concentrator**

| Component | Model | Specs | Cost |
|-----------|-------|-------|------|
| **Compute** | Raspberry Pi 5 (4GB) | Quad-core ARM, 4GB RAM, Gigabit Ethernet | $60 |
| **LoRa HAT** | RAK2287 Pi HAT | SX1302 8-channel concentrator, 868/915MHz | $80 |
| **Storage** | SanDisk 32GB U3 microSD | Class 10, A1 rated | $8 |
| **Case** | Official Pi 5 Case + Fan | Active cooling, HAT compatible | $12 |
| **Antenna** | 915MHz 5dBi Outdoor | N-type, weatherproof | $12 |
| **Power** | Pi 5 27W USB-C PSU | Official power supply | $12 |
| **Total** | | | **$184** |

**Key Features**:
- **8-channel LoRa concentrator** (receives on multiple spreading factors simultaneously)
- Can handle 500+ nodes easily
- Runs full Linux stack
- Perfect for Home Assistant, Node-RED, InfluxDB, Grafana
- Can add LTE/cellular modem for remote deployments

**Software Stack**:
```
Hardware: RAK2287 HAT
    ↓
Driver: SX1302 HAL
    ↓
Gateway Software: Custom daemon (or ChirpStack)
    ↓
Message Broker: MQTT (Mosquitto)
    ↓
Automation: Node-RED / Home Assistant
    ↓
Storage: InfluxDB
    ↓
Visualization: Grafana
```

**Installation**:
```bash
# Stack HAT on Pi GPIO (no soldering)
# Flash Raspberry Pi OS Lite 64-bit
# Install gateway software
sudo apt update
sudo apt install mosquitto influxdb grafana
# Custom daemon listens to SX1302, publishes to MQTT
```

---

### Gateway Comparison Matrix

| Feature | ESP32 Edge | Raspberry Pi Hub | Mini PC (Future) |
|---------|------------|------------------|------------------|
| **Cost** | $70 | $184 | $250+ |
| **Max Nodes** | 32 | 500+ | 1000+ |
| **LoRa Channels** | 1 | 8 | 8-16 |
| **Power** | PoE (5W) | USB-C (15W) | AC (30W+) |
| **Automation** | Basic | Advanced | Enterprise |
| **Storage** | SD card | SD card / USB SSD | NVMe SSD |
| **Remote Access** | Basic HTTP | Full Linux | Full OS + containers |
| **Use Case** | Remote site | Home hub | Industrial |

**Recommendation**:
- **Development**: Raspberry Pi 5 (easier debugging, full Linux tools)
- **Production (Remote)**: LilyGO T-Internet-POE (PoE, low power, reliable)
- **Production (Home)**: Raspberry Pi 5 (automation + sensors + gateway in one)

---

## 2. Node Hardware (RAK4631)

### What Nodes Need to Accomplish

- Continuous or periodic BLE scanning (50mA × 5s every 30s)
- Forward BLE sensor data via LoRa to gateway
- Optional local I2C sensors
- Battery life: 1.8-2.3 years on 18650
- Can run on USB/solar for always-on operation

### Recommended Hardware: RAKwireless WisBlock RAK4631

**Components:**

| Component | Model | Specs | Cost |
|-----------|-------|-------|------|
| **Core Module** | RAK4631 | nRF52840 + SX1262, BLE 5.0 + LoRa | $35 |
| **Base Board** | RAK19007 | 2× WisBlock slots, battery connector, USB-C | $10 |
| **Battery** | Samsung INR18650-25R | 2500mAh, protected | $8 |
| **Battery Holder** | Keystone 1042 + JST pigtail | Single 18650 holder with leads | $3 |
| **I2C Adapter** | RAK1920 WisBlock module | QWIIC-compatible connector | $5 |
| **Sensor** | Adafruit SHT40 QWIIC | Temp/Humidity, I2C, plug-and-play | $5 |
| **QWIIC Cable** | 100mm JST-SH 4-pin | I2C connection | $2 |
| **Antenna** | 915MHz 2dBi Stubby | RP-SMA, compact | $5 |
| **Enclosure** | Bud Industries NBF-32026 | 70×50×30mm, IP65 | $8 |
| **Total** | | | **$81** |

**Why RAK4631?**
- **Best BLE power efficiency**: nRF52840 consumes ~8mA scanning vs ESP32's 50mA
- **Native BLE 5.0**: Superior range and reliability
- **Excellent LoRa**: SX1262 with -148dBm sensitivity
- **WisBlock ecosystem**: Snap-fit modules, no soldering
- **Low sleep current**: 2µA deep sleep
- **Proven platform**: Used in commercial deployments

**Battery Life Analysis**:

Power budget (30-second cycle with BLE scanning):
```
Deep sleep:        2µA × 25s  = 0.014 mAh/cycle
BLE scan:          8mA × 5s   = 0.011 mAh/cycle
LoRa TX (avg):   100mA × 0.1s = 0.003 mAh/cycle
────────────────────────────────────────────
Total:                          0.028 mAh/cycle
                              = 2.4 mAh/day
```

**Battery life**: 2500mAh ÷ 2.4mAh/day = **1042 days (2.85 years)**

Compare to ESP32-S3: 3.0 mAh/day = 1.8 years (RAK4631 is 58% better)

---

### Power Options for Nodes

**Option A: 18650 Li-ion (Recommended for 2+ year deployments)**

- **Battery**: Samsung INR18650-25R (2500mAh, protected)
- **Holder**: Keystone 1042 with pre-soldered JST-PH pigtail
- **Life**: 2.85 years
- **Cost**: $11 total
- **Pros**: Standardized, replaceable, high capacity
- **Cons**: Bulkier than LiPo

**Option B: LiPo Pouch (Compact form factor)**

- **Battery**: Adafruit 2000mAh 3.7V LiPo with JST-PH connector
- **Life**: 2.3 years
- **Cost**: $13
- **Pros**: No soldering, flat form factor, plug-and-play
- **Cons**: Slightly lower capacity

**Option C: Solar + LiPo (Perpetual operation)**

- **Battery**: 2000mAh LiPo with JST
- **Solar Panel**: 6V 1W (110×60mm)
- **Charge Controller**: Adafruit USB/Solar LiPo charger
- **Total Cost**: $39
- **Daily consumption**: 72mAh
- **Solar generation** (4 hours sun): 800mAh
- **Net**: +728mAh/day → **Battery never depletes**

**Recommendation**:
- Battery-only: 18650 for longest life
- Outdoor/permanent: Solar + LiPo for set-and-forget

---

### Sensor Connectivity (Zero Soldering)

**Use QWIIC/Stemma QT Ecosystem**

WisBlock assembly:
```
RAK4631 Core
    ↓ (snaps into)
RAK19007 Base Board
    ↓ (WisBlock slot)
RAK1920 I2C Adapter Module
    ↓ (QWIIC connector)
QWIIC Cable (JST-SH 4-pin)
    ↓
Sensor Module (Adafruit, SparkFun, etc.)
```

**No soldering required** - everything snaps or plugs together!

**Compatible Sensors** (all plug-and-play):
- Adafruit SHT40: Temp/Humidity ($5)
- SparkFun BMP581: Pressure/Altitude ($20)
- SparkFun VEML7700: Ambient Light ($5)
- SparkFun SGP40: VOC Air Quality ($15)
- Adafruit PMSA003I: PM2.5 Particulate ($45)
- Any QWIIC/Stemma QT sensor

---

## 3. Remote Sensor Hardware (XIAO nRF52840)

### What Remote Sensors Need to Accomplish

- Read I2C sensors (SHT40, BMP581, etc.)
- BLE advertise every 30 seconds for 100ms
- Deep sleep 99.67% duty cycle
- Target: **6+ months on CR2032, 6+ years on 2×AA**
- Minimal cost (deployed in large quantities)
- Small form factor

### Recommended Hardware: Seeed XIAO nRF52840 Sense

**Base Configuration:**

| Component | Model | Specs | Cost |
|-----------|-------|-------|------|
| **MCU** | XIAO nRF52840 Sense | nRF52840, BLE 5.0, USB-C, LiPo charger | $15 |
| **Custom PCB** | Carrier board (see PCB section) | XIAO socket, battery, QWIIC | $5 |
| **Battery Holder** | Keystone 2462 (2×AA) | Pre-soldered leads | $2 |
| **Batteries** | Energizer Ultimate Lithium AA | 3000mAh, low self-discharge | $4 (2×) |
| **Sensor** | Adafruit SHT40 QWIIC | Temp/Humidity | $5 |
| **QWIIC Cable** | 50mm JST-SH 4-pin | Sensor connection | $1.50 |
| **Enclosure** | Hammond 1551BTBU | 50×35×20mm, IP54 | $5 |
| **Total** | | | **$37.50** |

**Why XIAO nRF52840?**
- **Ultra-low power**: 5µA deep sleep
- **Efficient BLE**: 5mA advertising (vs ESP32's 20mA)
- **Tiny size**: 21×17.5mm
- **Built-in LiPo charger**: Can use rechargeable batteries
- **USB-C**: Easy firmware updates
- **Low cost**: $12-15
- **QWIIC-compatible I2C**: Standard sensor ecosystem

**Battery Life Analysis**:

Power budget (30-second cycle):
```
Deep sleep:       5µA × 29.9s = 0.041 mAh/cycle
I2C read:        50mA × 0.05s = 0.0007 mAh/cycle
BLE advertise:    5mA × 0.1s  = 0.00014 mAh/cycle
────────────────────────────────────────────
Total:                          0.042 mAh/cycle
                              = 1.2 mAh/day
```

**Battery Life**:
- **CR2450** (620mAh): 517 days = **1.4 years**
- **2×AA Alkaline** (2500mAh each = 5000mAh): 4166 days = **11.4 years**
- **2×AA Lithium** (3000mAh each = 6000mAh): 5000 days = **13.7 years**

*Note: Actual life reduced by battery self-discharge. Realistic: 6-8 years on Lithium AA.*

---

### Battery Options for Remote Sensors

**Option A: 2×AA Lithium (Recommended for long deployments)** ⭐

- **Batteries**: Energizer Ultimate Lithium AA (3000mAh per cell)
- **Holder**: Keystone 2462 with pre-soldered leads
- **Voltage**: 3.0V (2×1.5V in series)
- **Life**: 6-8 years (accounting for self-discharge)
- **Cost**: $6 total
- **Pros**: Longest life, widely available, extreme temperature tolerance
- **Cons**: Slightly larger enclosure needed

**Option B: CR2450 Coin Cell (Compact)**

- **Battery**: CR2450 (620mAh)
- **Holder**: Keystone 3001 (through-hole or SMD)
- **Voltage**: 3.0V
- **Life**: 1.4 years
- **Cost**: $2 total
- **Pros**: Very compact, low cost
- **Cons**: Shorter life than AA

**Option C: Small LiPo (Rechargeable)**

- **Battery**: 500-1000mAh 3.7V LiPo with JST-SH connector
- **Life**: 1.4-2.3 years between charges
- **Cost**: $7-10
- **Pros**: Rechargeable via USB, compact
- **Cons**: Requires periodic charging

**Recommendation**:
- **Long deployment**: 2×AA Lithium (6-8 years, install and forget)
- **Accessible locations**: LiPo (recharge annually via USB)
- **Compact projects**: CR2450 (yearly battery swap)

---

### Custom Carrier PCB (Recommended for 10+ Units)

See `docs/pcb/XIAO-CARRIER-BOARD.md` for full design.

**Features**:
- XIAO nRF52840 socket (castellated pads or pin headers)
- 2×AA battery holder footprint (or JST for coin cell/LiPo)
- QWIIC connector (JST-SH 4-pin) for sensors
- 3.3V voltage regulation (for AA batteries)
- Mounting holes (M3)
- Optional: LED, reset button

**Board Size**: 50×40mm

**Cost** (10 units from JLCPCB):
- PCB fabrication: $20 (10 boards)
- Components per board: $3 (connectors, regulator, passives)
- **Total per board**: $5

**Assembly**:
1. Manufacturer pre-installs all SMD components
2. You solder XIAO module (or use hot-swap socket)
3. End user: Plug battery, plug sensor, flash firmware
   - **No end-user soldering required!**

---

## 4. Connector Standards (System-Wide)

### Power Connectors

| Device | Connector | Pinout | Notes |
|--------|-----------|--------|-------|
| **Node Battery** | JST-PH 2.0mm | 2-pin (V+, GND) | Standard LiPo connector |
| **Remote Sensor** | JST-SH 1.0mm | 2-pin (V+, GND) | Smaller for compact devices |
| **AA Battery Holder** | Pre-soldered leads | Connect to JST pigtail | One-time soldering |

### I2C/Sensor Connectors

| Standard | Connector | Pinout | Compatible With |
|----------|-----------|--------|-----------------|
| **QWIIC** (SparkFun) | JST-SH 1.0mm 4-pin | GND, 3.3V, SDA, SCL | All QWIIC sensors |
| **Stemma QT** (Adafruit) | JST-SH 1.0mm 4-pin | GND, 3.3V, SDA, SCL | Identical to QWIIC |
| **Grove** (Seeed) | 4-pin 2.0mm | GND, VCC, SDA, SCL | Adapter cable available |

**Standardize on QWIIC/Stemma QT** for maximum ecosystem compatibility.

### Antenna Connectors

| Device | Connector | Cable Type | Notes |
|--------|-----------|------------|-------|
| **RAK4631** | U.FL (IPEX) | Micro coax | Fragile, use pigtail to RP-SMA |
| **Gateway LoRa** | RP-SMA Female | Standard LoRa antenna | Robust, field-swappable |
| **External Antenna** | RP-SMA Male | N/A | Screw-on connection |

---

## 5. Recommended Sensors (QWIIC-Compatible)

### Basic Environmental

| Sensor | Measurements | Interface | Cost | Notes |
|--------|--------------|-----------|------|-------|
| **Adafruit SHT40** | Temp, Humidity | QWIIC | $5 | High accuracy, low power |
| **SparkFun SHT41** | Temp, Humidity | QWIIC | $7 | Improved accuracy over SHT40 |
| **SparkFun BMP581** | Pressure, Temp, Altitude | QWIIC | $20 | ±0.3 hPa, excellent accuracy |
| **Adafruit BME680** | Temp, Humidity, Pressure, VOC | QWIIC | $20 | All-in-one environmental |

### Advanced Environmental

| Sensor | Measurements | Interface | Cost | Notes |
|--------|--------------|-----------|------|-------|
| **SparkFun VEML7700** | Ambient Light (lux) | QWIIC | $5 | 0-120k lux, auto-range |
| **Adafruit VEML6075** | UV Index (UVA/UVB) | QWIIC | $8 | Outdoor sun exposure |
| **SparkFun SGP40** | VOC (Air Quality) | QWIIC | $15 | Alcohol, formaldehyde detection |
| **Adafruit PMSA003I** | PM1.0, PM2.5, PM10 | QWIIC | $45 | Particulate matter, laser sensor |
| **SparkFun SCD41** | CO₂ (400-5000 ppm) | QWIIC | $55 | NDIR sensor, high accuracy |

### Motion & Presence

| Sensor | Measurements | Interface | Cost | Notes |
|--------|--------------|-----------|------|-------|
| **Adafruit LSM6DS33** | Accel + Gyro (6-axis) | QWIIC | $10 | Motion, vibration, orientation |
| **SparkFun VL53L1X** | Distance (ToF, 4m) | QWIIC | $15 | Laser ranging, no false triggers |
| **DFRobot mmWave Radar** | Presence (5m) | UART/I2C | $25 | Human detection through walls |

### Specialized

| Sensor | Measurements | Interface | Cost | Notes |
|--------|--------------|-----------|------|-------|
| **Adafruit INA219** | Current, Voltage, Power | QWIIC | $10 | DC power monitoring, 0-26V |
| **SparkFun AS7341** | 11-channel Spectral | QWIIC | $20 | Color analysis, plant growth |
| **Adafruit MLX90640** | Thermal Camera (32×24) | QWIIC | $60 | IR imaging, hot spot detection |
| **DFRobot Gravity pH** | pH (0-14) | Analog | $30 | Aquaponics, hydroponics |
| **Atlas Scientific EC** | Conductivity (water) | I2C | $60 | Water quality monitoring |

---

## 6. Bill of Materials (Complete System)

### Typical Deployment: 1 Gateway + 3 Nodes + 10 Remote Sensors

**Gateway Option: Raspberry Pi 5 Hub**

| Item | Qty | Unit | Total |
|------|-----|------|-------|
| Raspberry Pi 5 (4GB) | 1 | $60 | $60 |
| RAK2287 LoRa Concentrator HAT | 1 | $80 | $80 |
| 32GB microSD (U3, A1) | 1 | $8 | $8 |
| Pi 5 Case + Fan | 1 | $12 | $12 |
| 915MHz 5dBi Antenna | 1 | $12 | $12 |
| Pi 5 27W USB-C PSU | 1 | $12 | $12 |
| **Gateway Subtotal** | | | **$184** |

**Nodes (×3)**

| Item | Qty | Unit | Total |
|------|-----|------|-------|
| RAK4631 WisBlock Starter Kit | 3 | $50 | $150 |
| Samsung 18650 (2500mAh, protected) | 3 | $8 | $24 |
| Keystone 1042 holder + JST pigtail | 3 | $3 | $9 |
| RAK1920 QWIIC adapter module | 3 | $5 | $15 |
| Adafruit SHT40 (QWIIC) | 3 | $5 | $15 |
| QWIIC cable (100mm) | 3 | $2 | $6 |
| 915MHz 2dBi antenna | 3 | $5 | $15 |
| Bud Industries NBF-32026 enclosure | 3 | $8 | $24 |
| **Node Subtotal** | | | **$258** |

**Remote Sensors (×10)**

| Item | Qty | Unit | Total |
|------|-----|------|-------|
| XIAO nRF52840 Sense | 10 | $15 | $150 |
| Custom carrier PCB | 10 | $5 | $50 |
| Keystone 2462 (2×AA holder) | 10 | $2 | $20 |
| Energizer Ultimate Lithium AA | 20 | $2 | $40 |
| Adafruit SHT40 (QWIIC) | 10 | $5 | $50 |
| QWIIC cable (50mm) | 10 | $1.50 | $15 |
| Hammond 1551BTBU enclosure | 10 | $5 | $50 |
| **Remote Sensor Subtotal** | | | **$375** |

### **Total System Cost: $817**

**Cost per sensor node deployed**: $37.50 (amortized)

**Scalability**:
- Adding 10 more remote sensors: +$375
- Adding 1 more node: +$86
- Gateway handles 100+ devices with no upgrade needed

---

## 7. Suppliers & Part Numbers

### Primary Suppliers

**RAKwireless** (LoRa modules, WisBlock)
- Website: store.rakwireless.com
- Shipping: Worldwide, 5-10 days
- RAK4631: SKU RAK4631
- RAK19007: SKU RAK19007
- RAK2287: SKU RAK2287

**Seeed Studio** (XIAO modules, Grove sensors)
- Website: seeedstudio.com
- Shipping: Worldwide, 7-14 days
- XIAO nRF52840 Sense: SKU 102010448

**Adafruit** (Sensors, cables, accessories)
- Website: adafruit.com
- Shipping: USA 3-5 days, International 7-14 days
- SHT40: Product ID 4885
- QWIIC cables: Product ID 4399, 4401

**SparkFun** (Sensors, QWIIC ecosystem)
- Website: sparkfun.com
- Shipping: USA 3-5 days, International 7-14 days
- BMP581: SKU SEN-20170
- VEML7700: SKU SEN-18981

**Mouser / Digikey** (Components, connectors, batteries)
- Website: mouser.com / digikey.com
- Shipping: Worldwide, 1-3 days (expensive)
- Keystone battery holders
- JST connectors
- Samsung 18650 cells

**Amazon** (Batteries, enclosures, antennas)
- Energizer Ultimate Lithium AA: 8-pack
- Generic 915MHz LoRa antennas
- IP65 enclosures

**JLCPCB** (Custom PCB fabrication)
- Website: jlcpcb.com
- Minimum order: 5 boards
- Typical cost: $2 for 5 boards + $15 shipping
- Turnaround: 2-3 days fabrication + 7-14 days shipping

---

## 8. Development vs Production Hardware

### Development Phase (Current)

**Continue using LilyGO T3-S3 for all roles**:
- Gateway: 1× T3-S3 ($25)
- Node: 1× T3-S3 ($25)
- Remote Sensor: 1× T3-S3 ($25)
- **Total**: $75

**Advantages**:
- Single platform = easier debugging
- All have OLED displays
- USB-C programming on all devices
- WiFi on all devices (useful for debugging)

**When to transition**: After protocol is stable and tested

---

### Production Deployment

**Optimize for cost, power, and reliability**:
- Gateway: Raspberry Pi 5 + RAK2287 ($184)
- Nodes: RAK4631 ($81 each)
- Remote Sensors: XIAO nRF52840 ($37.50 each)

**Cost comparison (1 gateway + 3 nodes + 10 sensors)**:
- **All T3-S3**: 14 boards × $25 = **$350** (plus batteries, enclosures)
- **Production mix**: $184 + $243 + $375 = **$802** (fully equipped)

**Production advantages**:
- 2× better battery life on nodes (RAK4631 vs ESP32)
- 8-channel LoRa concentrator (Pi + RAK2287) handles 100× more nodes
- Remote sensors are 1/3 the size
- Better long-term reliability

---

## 9. Recommended Purchase Order

### Initial Development Order (Getting Started)

**Seeed Studio**:
- 2× XIAO nRF52840 Sense ($30)
- 1× Grove - SHT40 Temp/Humidity ($5)
- 1× Grove Starter Kit (cables, breadboard) ($20)

**RAKwireless**:
- 1× RAK4631 WisBlock Starter Kit ($50)
- 1× RAK1920 QWIIC Adapter ($5)

**Adafruit**:
- 2× Adafruit SHT40 QWIIC ($10)
- 5× QWIIC cables 50mm ($7.50)
- 2× 500mAh LiPo with JST ($14)

**Amazon**:
- 4× 18650 battery ($32)
- 2× 18650 single holder ($4)
- 8× Energizer Ultimate Lithium AA ($16)

**Total**: ~$193.50

---

### Production Order (10 Remote Sensors + 3 Nodes)

**Seeed Studio**:
- 10× XIAO nRF52840 Sense ($150)

**RAKwireless**:
- 3× RAK4631 WisBlock Starter Kit ($150)
- 3× RAK1920 QWIIC Adapter ($15)

**Adafruit**:
- 13× SHT40 QWIIC ($65)
- 20× QWIIC cables mixed lengths ($30)

**JLCPCB**:
- 10× Custom carrier PCB ($50 including assembly)

**Amazon/Mouser**:
- 10× 2×AA battery holders ($20)
- 40× Energizer Ultimate Lithium AA ($80)
- 3× Samsung 18650 protected ($24)
- 13× Enclosures mixed sizes ($90)

**Total**: ~$674

---

## 10. Next Steps

1. **Read**: `docs/assembly/ASSEMBLY-GUIDE.md` for step-by-step build instructions
2. **Review**: `docs/pcb/XIAO-CARRIER-BOARD.md` for custom PCB design
3. **Check**: `docs/hardware/WIRING-DIAGRAMS.md` for connection details
4. **Order**: Start with XIAO nRF52840 + RAK4631 to validate design

**Questions?** See project documentation in `/docs/` folder.
