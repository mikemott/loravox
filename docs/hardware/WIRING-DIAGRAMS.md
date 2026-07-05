# LoRa PNP Network - Wiring Diagrams

**Version**: 1.0
**Date**: 2026-07-04

---

## 1. Gateway Wiring

### Option A: Raspberry Pi 5 + RAK2287 HAT

**No wiring required** - HAT stacks directly on GPIO pins.

```
┌─────────────────────────────────┐
│   RAK2287 LoRa Concentrator HAT │
│   ┌───────────────┐             │
│   │ SX1302 Chip   │   [RP-SMA]──┼──> Antenna (915MHz)
│   └───────────────┘             │
│                                 │
│   40-pin GPIO Header (female)   │
└─────────────┬───────────────────┘
              │ (stacks on)
┌─────────────▼───────────────────┐
│      Raspberry Pi 5             │
│   ┌──────────────────┐          │
│   │  ARM Cortex-A76  │   [RJ45]─┼──> Ethernet
│   │  Quad-core 2.4GHz│          │
│   └──────────────────┘   [USB-C]┼──> Power (5V 5A)
│                                 │
│   [microSD] [HDMI] [USB×4]      │
└─────────────────────────────────┘
```

**Assembly Steps**:
1. Insert microSD card into Pi 5
2. Align RAK2287 HAT GPIO with Pi 5 GPIO header
3. Press down gently until fully seated
4. Screw in 4× M2.5 standoffs (included with HAT)
5. Attach RP-SMA antenna to HAT
6. Connect Ethernet cable
7. Connect USB-C power (must be 5V 5A for Pi 5)

**Software Setup**: See `docs/software/GATEWAY-SETUP.md`

---

### Option B: LilyGO T-Internet-POE + E22-900M30S (SX1262)

**SPI Connection Wiring**

```
T-Internet-POE                   E22-900M30S (SX1262)
┌──────────────────┐            ┌──────────────────┐
│                  │            │                  │
│  [3.3V]──────────┼────────────┼──[VCC]           │
│  [GND]───────────┼────────────┼──[GND]           │
│                  │            │                  │
│  GPIO19 (MOSI)───┼────────────┼──[MOSI]          │
│  GPIO23 (MISO)───┼────────────┼──[MISO]          │
│  GPIO18 (SCK)────┼────────────┼──[SCK]           │
│  GPIO5  (NSS)────┼────────────┼──[NSS/CS]        │
│  GPIO14 (DIO1)───┼────────────┼──[DIO1]          │
│  GPIO27 (RST)────┼────────────┼──[RST]           │
│  GPIO26 (BUSY)───┼────────────┼──[BUSY]          │
│                  │            │                  │
│                  │            │  [ANT]───────────┼──> Antenna
│                  │            │                  │
│  [RJ45 Ethernet] │            └──────────────────┘
│  [USB-C Power]   │
└──────────────────┘
```

**Pin Mapping Table**:

| Function | T-Internet-POE Pin | E22-900M30S Pin | Wire Color (suggested) |
|----------|-------------------|-----------------|------------------------|
| Power | 3.3V | VCC | Red |
| Ground | GND | GND | Black |
| SPI MOSI | GPIO19 | MOSI | Yellow |
| SPI MISO | GPIO23 | MISO | Green |
| SPI Clock | GPIO18 | SCK | Orange |
| Chip Select | GPIO5 | NSS | Blue |
| Interrupt | GPIO14 | DIO1 | Purple |
| Reset | GPIO27 | RST | White |
| Busy | GPIO26 | BUSY | Gray |

**Assembly Steps**:

1. **Option A: Breadboard (No Soldering)**
   - Insert E22-900M30S DIP module into breadboard
   - Use female-to-male jumper wires to connect GPIO pins
   - Works for testing, not suitable for production

2. **Option B: Custom Shield PCB (Recommended)**
   - Design simple PCB shield that stacks on T-Internet-POE headers
   - E22-900M30S soldered to shield
   - Shield plugs into T-Internet-POE like Arduino shield
   - See `docs/pcb/GATEWAY-SHIELD.md` for design

3. **Option C: Point-to-Point Wiring**
   - Solder E22-900M30S to perfboard
   - Solder wires from perfboard to T-Internet-POE GPIO pins
   - Use header pins or direct solder

**Power Connection**:
```
PoE Switch/Injector ──[Ethernet Cable]──> T-Internet-POE [RJ45 Port]
   (802.3af)                                     ↓
                                          Power extracted
                                                 ↓
                                          3.3V regulator
                                                 ↓
                                          E22-900M30S VCC
```

**PoE Setup**: No separate power supply needed! Ethernet cable carries both data and power.

---

## 2. Node Wiring (RAK4631 WisBlock)

### Component Assembly (No Soldering Required)

```
┌────────────────────────────────────────────────┐
│  RAK19007 Base Board                           │
│                                                │
│  ┌──────────────┐    ┌───────────────┐        │
│  │ WisBlock     │    │ WisBlock Slot │        │
│  │ CPU Slot     │    │ Sensor Slot   │        │
│  │              │    │               │        │
│  │ [RAK4631]    │    │  [RAK1920]    │        │
│  │  nRF52840    │    │  I2C Adapter  │        │
│  │  + SX1262    │    │               │        │
│  └──────────────┘    └───────┬───────┘        │
│                              │                │
│                       [QWIIC Connector]       │
│                              ↓                │
│  [Battery JST-PH]  [USB-C]  [U.FL Antenna]    │
└────────────┬──────────────────┬────────────────┘
             │                  │
             ↓                  ↓
    18650 Battery Holder    LoRa Antenna
```

**RAK4631 Module Installation**:
1. Align RAK4631 with CPU slot on RAK19007 base
2. Press down firmly until clicks (connector is friction-fit)
3. Module should sit flush with base board

**RAK1920 I2C Adapter Installation**:
1. Align RAK1920 with sensor slot on RAK19007
2. Press down firmly until clicks
3. QWIIC connector now exposed on module

**Battery Connection**:
```
18650 Holder (Keystone 1042)
    │
    ├──[Red wire (+)]────┐
    │                    │
    └──[Black wire (-)]──┤
                         │
                    [JST-PH 2.0mm]
                         ↓
              Plug into RAK19007 battery connector
              (polarity protected - can't insert backwards)
```

**Sensor Connection via QWIIC**:
```
RAK1920 QWIIC Connector
        ↓
   [JST-SH 4-pin cable]
        ↓
   Sensor QWIIC Connector
   (e.g., Adafruit SHT40)
```

**Pin Mapping** (for reference, no wiring needed):

| Function | RAK4631 Pin | RAK1920 Connection |
|----------|-------------|-------------------|
| I2C SDA | P0.13 | Exposed on QWIIC |
| I2C SCL | P0.14 | Exposed on QWIIC |
| 3.3V | VDD | Exposed on QWIIC |
| GND | GND | Exposed on QWIIC |

**Antenna Connection**:
1. LoRa antenna has U.FL connector (tiny snap-on)
2. Align with U.FL socket on RAK4631
3. Press down gently until click
4. **Do NOT pull on cable** - use fingernail under connector to remove

**Complete Node Assembly**:
```
1. RAK4631 → RAK19007 CPU slot
2. RAK1920 → RAK19007 sensor slot
3. Battery JST → RAK19007 battery port
4. QWIIC cable → RAK1920 QWIIC port
5. QWIIC cable → Sensor module
6. U.FL antenna → RAK4631 antenna port
7. USB-C cable → RAK19007 for programming
```

**Total assembly time**: ~3 minutes, zero soldering!

---

### Optional: Solar Power Connection

```
Solar Panel (6V 1W)
    ↓
Adafruit Solar LiPo Charger
    ├──[Battery Out]──> LiPo battery (2000mAh)
    │                        ↓
    │                   [JST-PH connector]
    │                        ↓
    └─────────────────> RAK19007 battery input
```

**Wiring**:
1. Solar panel connects to charger "Solar" input (JST or solder pads)
2. LiPo battery connects to charger "Battery" port (JST)
3. Charger "Load" output connects to RAK19007 battery input
4. Charger manages charging, over-discharge protection

---

## 3. Remote Sensor Wiring (XIAO nRF52840)

### Option A: Using XIAO Expansion Board (No Soldering)

```
┌──────────────────────────────────────────┐
│  XIAO Expansion Board                    │
│                                          │
│  ┌─────────────────┐                     │
│  │  XIAO Socket    │  [Grove Port 1]────┼──> SHT40 (Grove cable)
│  │  (insert XIAO)  │                     │
│  └─────────────────┘  [Grove Port 2]     │
│                                          │
│  [Battery JST]  [USB-C Passthrough]      │
└────────┬──────────────────────────────────┘
         │
         ↓
  LiPo Battery or
  2×AA Holder with JST
```

**Assembly**:
1. Insert XIAO nRF52840 into expansion board socket (align USB-C ports)
2. Plug battery JST into expansion board battery port
3. Connect Grove cable from expansion board to Grove-compatible sensor
4. (Grove uses 4-pin connector, electrically compatible with QWIIC)

**Grove to QWIIC Adapter**:
- If using QWIIC sensor with XIAO Expansion Board:
- Use "Grove to QWIIC" adapter cable (SparkFun, $2)
- Grove side → Expansion board
- QWIIC side → Sensor

---

### Option B: Custom Carrier PCB (Recommended for Production)

```
┌──────────────────────────────────────────────┐
│  Custom Carrier PCB (50×40mm)                │
│                                              │
│  ┌─────────────────────┐                     │
│  │  XIAO nRF52840      │  [QWIIC JST-SH]────┼──> Sensor
│  │  (soldered or       │                     │
│  │   hot-swap socket)  │                     │
│  └─────────────────────┘                     │
│                                              │
│  [2×AA Holder Pads]  ──┬──> [3.3V Reg] ──┬──│─> XIAO VIN
│  (soldered to PCB)     │                  │  │
│                        └──────────────────┴──│─> XIAO GND
│                                              │
│  [Mounting Holes ○ ○ ○ ○]                   │
└──────────────────────────────────────────────┘
```

**PCB Connections**:

**Battery Circuit**:
```
2×AA Holder (6V max)
    ↓
  + │ -
    ↓
Pololu S7V8F3 (3.3V Buck Regulator)
    ↓
  3.3V out ──> XIAO VIN pin
  GND      ──> XIAO GND pin
```

**I2C/QWIIC Circuit**:
```
XIAO D4 (SDA) ──> QWIIC Pin 3 (SDA)
XIAO D5 (SCL) ──> QWIIC Pin 4 (SCL)
XIAO 3.3V     ──> QWIIC Pin 2 (VCC)
XIAO GND      ──> QWIIC Pin 1 (GND)
```

**Detailed Pin Mapping**:

| XIAO nRF52840 Pin | PCB Connection | QWIIC Pin | Wire Color |
|-------------------|----------------|-----------|------------|
| D4 (P0.04) | SDA trace | Pin 3 | Blue |
| D5 (P0.05) | SCL trace | Pin 4 | Yellow |
| 3V3 | 3.3V rail | Pin 2 | Red |
| GND | Ground plane | Pin 1 | Black |

**Assembly (One-Time)**:
1. Solder XIAO nRF52840 to carrier PCB pads (or insert into hot-swap socket)
2. Solder 2×AA battery holder to PCB pads (+ and -)
3. (All other components pre-installed by PCB manufacturer)

**Assembly (End User)**:
1. Insert 2×AA batteries into holder
2. Plug QWIIC cable into PCB connector
3. Plug QWIIC cable into sensor
4. Place in enclosure
5. Flash firmware via USB-C

**Voltage Regulator Details**:
- Input: 1.8V - 5.5V (2×AA = 3.0V nominal, safe)
- Output: 3.3V ±2%
- Quiescent current: 10µA (minimal impact on battery life)
- Max current: 800mA (sensors use <50mA)

---

### Option C: Direct Wiring (Development/Prototyping)

**Using Breadboard**:

```
XIAO nRF52840 Sense
┌────────────┐
│  D0        │
│  D1        │
│  D2        │
│  D3        │
│  D4 ───────┼───> SDA ──> Sensor SDA
│  D5 ───────┼───> SCL ──> Sensor SCL
│  D6        │
│  ...       │
│  3V3 ──────┼───> VCC ──> Sensor VCC
│  GND ──────┼───> GND ──> Sensor GND
│  VIN       │
│  VUSB      │
└────────────┘
```

**Wire Connection Table**:

| XIAO Pin | Function | Sensor Pin | Wire Color |
|----------|----------|------------|------------|
| 3V3 | Power | VCC | Red |
| GND | Ground | GND | Black |
| D4 | I2C Data | SDA | Blue or Green |
| D5 | I2C Clock | SCL | Yellow or White |

**Battery Connection (Development)**:
- XIAO has built-in LiPo charger
- Connect LiPo battery JST to XIAO battery pads (on bottom of board)
- Polarity: + is closer to USB connector, - is closer to edge

---

## 4. I2C Multi-Sensor Daisy Chain

### Connecting Multiple Sensors to One Node

**QWIIC Daisy Chain** (all sensors on same I2C bus):

```
RAK1920 QWIIC Port
        ↓
   [QWIIC Cable 1]
        ↓
   Sensor 1 (SHT40)
   ┌──────────────┐
   │ [IN] [OUT]   │
   └──────┬───────┘
          ↓
   [QWIIC Cable 2]
          ↓
   Sensor 2 (BMP581)
   ┌──────────────┐
   │ [IN] [OUT]   │
   └──────┬───────┘
          ↓
   [QWIIC Cable 3]
          ↓
   Sensor 3 (VEML7700)
```

**I2C Address Management**:
- Each sensor must have unique I2C address
- Check sensor datasheets for default addresses:
  - SHT40: 0x44
  - BMP581: 0x46 or 0x47 (solder jumper selectable)
  - VEML7700: 0x10
  - SGP40: 0x59

**If Address Conflict**:
- Some sensors have address select pins/jumpers
- Use I2C multiplexer (TCA9548A) to create 8 separate I2C buses

---

## 5. Antenna Connections

### LoRa Antenna Types

**Internal Antenna** (Development):
- PCB trace antenna on module
- 0-2 dBi gain
- Range: 1-2 km line-of-sight
- No external antenna needed

**External Antenna via U.FL**:
```
RAK4631 Module
    [U.FL Socket] ──> U.FL to RP-SMA Pigtail Cable (150mm)
                           ↓
                      [RP-SMA Male]
                           ↓
                      RP-SMA Female Bulkhead
                      (mounted in enclosure)
                           ↓
                      External Antenna (RP-SMA Male)
```

**External Antenna Types**:
- **Whip (2dBi)**: Omnidirectional, compact, $5
- **Dipole (3dBi)**: Better range, omnidirectional, $8
- **Fiberglass (5dBi)**: Best range, outdoor, $12
- **Directional Yagi (8-12dBi)**: Point-to-point links, $30

**Cable Loss**:
- U.FL to RP-SMA pigtail: ~0.5 dB loss per meter
- Keep pigtails short (<300mm)

---

## 6. Power Supply Specifications

### Gateway Power Requirements

**Raspberry Pi 5 + RAK2287**:
- Idle: 3W
- Active (receiving): 5W
- Peak (transmitting LoRa): 8W
- **Recommended**: 5V 5A (25W) USB-C power supply
- Official Pi 5 power supply strongly recommended

**LilyGO T-Internet-POE + E22**:
- Idle: 2W
- Active: 5W
- Peak (E22 at 30dBm): 8W
- **PoE (802.3af)**: Provides up to 15.4W (sufficient)
- Or USB-C 5V 2A (10W)

---

### Node Power Requirements

**RAK4631 + Sensors**:
- Deep sleep: 0.01mW (2µA @ 3.7V)
- BLE scan: 30mW (8mA @ 3.7V)
- LoRa TX: 370mW (100mA @ 3.7V, peak)
- Average: 8-10mW (2.4mA @ 3.7V)

**Battery Specifications**:
- **18650**: 3.7V nominal, 2.5-3.5Ah capacity
- **LiPo**: 3.7V nominal, 0.5-2.0Ah typical
- **Charge cutoff**: 4.2V max
- **Discharge cutoff**: 3.0V min (RAK19007 has protection)

---

### Remote Sensor Power Requirements

**XIAO nRF52840 + SHT40**:
- Deep sleep: 0.015mW (5µA @ 3.0V)
- Active: 15mW (5mA @ 3.0V, advertising)
- Sensor read: 150mW (50mA @ 3.0V, <50ms)
- Average: 3mW (1mA @ 3.0V)

**Battery Specifications**:
- **2×AA Lithium**: 1.5V each = 3.0V series, 3000mAh per cell
- **CR2450**: 3.0V, 620mAh
- **LiPo 500mAh**: 3.7V, 500mAh (use with XIAO's built-in charger)

**Voltage Regulator** (for AA batteries):
- Input: 2×AA = 1.8-3.2V (fresh to depleted)
- Output: 3.3V fixed
- Pololu S7V8F3 recommended (10µA quiescent current)

---

## 7. Enclosure Cable Glands

### Weatherproofing Connections

**For Outdoor Nodes**:

```
┌─────────────────────────────────┐
│  Enclosure (IP65)               │
│                                 │
│  ┌──────────────┐               │
│  │ RAK4631 Node │               │
│  └──────────────┘               │
│                                 │
└─────────┬───────────────────────┘
          │
     [Cable Gland PG7]
          ↓
    LoRa Antenna Pigtail
    (U.FL to RP-SMA)
          ↓
    External Antenna
```

**Cable Gland Sizes**:
- **PG7**: 3-6.5mm cable diameter (antenna pigtails)
- **PG9**: 4-8mm cable diameter (solar panel cables)
- **PG11**: 5-10mm cable diameter (Ethernet, thick cables)

**Recommended Glands**:
- Heyco M1623: Waterproof strain relief, $2 each
- Install before connecting cables (can't add after RP-SMA connector)

---

## 8. Grounding and ESD Protection

### Lightning/ESD Protection for Outdoor Installations

**LoRa Antenna Grounding**:
```
External Antenna
    ↓
PolyPhaser IS-B50LN-C2 (LoRa surge protector)
    ↓
Coax to Radio
    ↓
Chassis ground
```

**Best Practices**:
- Mount gateway in grounded metal enclosure
- Use shielded Ethernet cable (CAT6 STP)
- Install surge protector on antenna feed line
- Bond all grounds together (star ground)

**For Solar Panels**:
- Use blocking diode in solar circuit
- Fuse on solar panel positive lead (2A)

---

## 9. Assembly Safety

### Battery Safety

**Lithium Battery Warnings**:
- ⚠️ Never short-circuit battery terminals
- ⚠️ Use protected cells (built-in over-discharge protection)
- ⚠️ Don't charge above 4.2V
- ⚠️ Don't discharge below 2.75V
- ⚠️ Store in cool, dry location
- ⚠️ Don't puncture or crush

**Safe Handling**:
- Use battery holders with polarity protection
- Use JST connectors (can't be inserted backwards)
- Fuse battery circuits (2A fast-blow fuse)
- Monitor battery voltage in firmware

---

### Soldering Safety

**If Soldering Required**:
- Use lead-free solder (SAC305 or similar)
- Work in ventilated area
- Use fume extractor or fan
- Wear safety glasses
- ESD wrist strap when handling modules
- Tip temperature: 315-343°C (600-650°F)

---

## 10. Testing Connections

### Continuity Testing

**Before applying power**:

1. **Power rail check**:
   - Multimeter on resistance/continuity mode
   - Check VCC to GND is open circuit (>1MΩ)
   - If short circuit (<10Ω), DO NOT power on

2. **Battery polarity check**:
   - Multimeter on DC voltage mode
   - Measure battery voltage (should be 3.0-4.2V)
   - Verify polarity matches connector

3. **I2C pull-up check**:
   - Measure SDA and SCL to VCC with multimeter
   - Should read ~2-10kΩ (pull-up resistors present)

4. **Antenna check**:
   - Visual inspection of U.FL connection (fully seated)
   - Check antenna SWR with antenna analyzer (optional)

### Power-On Checklist

**First power-on**:
1. Connect USB-C cable (not battery yet)
2. Check LED blinks (indicates MCU running)
3. Connect serial terminal (115200 baud)
4. Verify boot messages appear
5. If OK, disconnect USB and connect battery
6. Verify device boots from battery

**Troubleshooting**:
- No LED: Check power connections, USB cable
- LED on but no serial: Check USB-C cable (must be data-capable)
- Boots on USB but not battery: Check battery voltage, JST polarity
- I2C sensor not detected: Check QWIIC cable, sensor address

---

## Reference: Pin Assignment Tables

### RAK4631 GPIO Pinout (for custom wiring)

| Pin | Function | Notes |
|-----|----------|-------|
| P0.13 | I2C SDA | Exposed on RAK1920 QWIIC |
| P0.14 | I2C SCL | Exposed on RAK1920 QWIIC |
| P0.17 | SPI MISO | LoRa (internal) |
| P0.20 | SPI MOSI | LoRa (internal) |
| P0.24 | SPI SCK | LoRa (internal) |
| P0.25 | LoRa NSS | LoRa (internal) |
| P1.11 | LoRa BUSY | LoRa (internal) |
| P1.12 | LoRa DIO1 | LoRa (internal) |
| P1.13 | LoRa RST | LoRa (internal) |

### XIAO nRF52840 Pinout

| Pin | Function | Alt Function | Notes |
|-----|----------|--------------|-------|
| D0 | GPIO | UART TX | |
| D1 | GPIO | UART RX | |
| D2 | GPIO | | |
| D3 | GPIO | | |
| D4 | GPIO | I2C SDA | **Use for QWIIC** |
| D5 | GPIO | I2C SCL | **Use for QWIIC** |
| D6 | GPIO | | |
| D7 | GPIO | | |
| D8 | GPIO | | |
| D9 | GPIO | | |
| D10 | GPIO | | |
| 3V3 | Power Out | | 3.3V @ 200mA max |
| GND | Ground | | |
| VIN | Power In | | 3.3-5.5V |
| VUSB | USB Power | | 5V when USB connected |

---

## Next Steps

- **Assembly Guide**: See `docs/assembly/ASSEMBLY-GUIDE.md` for step-by-step instructions
- **PCB Design**: See `docs/pcb/` folder for custom board schematics
- **Troubleshooting**: See `docs/TROUBLESHOOTING.md` for common issues

---

**Document Version**: 1.0
**Last Updated**: 2026-07-04
**Author**: LoRa PNP Project Team
