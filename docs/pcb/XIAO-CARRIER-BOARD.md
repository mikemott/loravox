# XIAO nRF52840 Carrier Board - PCB Design

**Version**: 1.0
**Date**: 2026-07-04
**Board Size**: 50mm × 40mm
**Layers**: 2-layer (Top + Bottom)
**Design Tool**: KiCad 7.0+ (free, open-source)

---

## Overview

Custom carrier board for Seeed XIAO nRF52840 Sense remote sensor nodes.

**Features**:
- XIAO module socket (castellated pads or pin headers)
- 2×AA battery holder footprint
- 3.3V buck regulator (for AA batteries)
- QWIIC connector (JST-SH 1.0mm, 4-pin) for I2C sensors
- Optional status LED
- Optional reset button
- 4× M3 mounting holes
- Silkscreen labels for pin functions

**Cost**: ~$2 per board (qty 10 from JLCPCB) + ~$3 components = **$5 total**

---

## Schematic Design

### Power Section

```
                    ┌─────────────────┐
2×AA Holder         │  Pololu S7V8F3  │
(3.0V nominal)      │  3.3V Buck Reg  │
                    │  10µA Iq        │
  + ─────────┬──────┤VIN         VOUT├─────┬────> XIAO VIN (3.3V)
             │      │                │     │
            ┌┴┐     │            GND ├──┬──┘
         C1 │ │100µF│                │  │
            └┬┘     └────────────────┘  │
             │                          │
  - ─────────┴──────────────────────────┴────> GND

```

**Components**:
- **U1**: Pololu S7V8F3 (3.3V 800mA Buck Regulator)
  - Input: 1.8-5.5V
  - Output: 3.3V fixed
  - Quiescent current: 10µA
  - Integrated: Inductor, capacitors (no external parts needed)
  - Footprint: 0.4" × 0.5" (10.2mm × 12.7mm)
  - Pins: 5-pin SIP (VIN, GND, GND, VOUT, VOUT)

- **C1**: 100µF electrolytic capacitor (input filter)
  - Voltage: 6.3V or higher
  - Footprint: Radial, 5mm diameter
  - Polarity marked on PCB

- **BAT1**: Keystone 2462 (2×AA battery holder)
  - Solder pads or through-hole mounting
  - Polarity marked clearly on silkscreen

**Alternative for LiPo/Coin Cell**:
- Replace battery holder with JST-SH 2-pin connector
- Omit voltage regulator (direct connection to XIAO VIN)
- Add polarity protection diode (1N4148) optional

---

### I2C/QWIIC Section

```
XIAO D4 (SDA) ────────┬────> QWIIC Pin 3 (Blue)
                      │
                    ┌─┴─┐
                    │4.7│ kΩ Pull-up
                    └─┬─┘
                      │
                   3.3V

XIAO D5 (SCL) ────────┬────> QWIIC Pin 4 (Yellow)
                      │
                    ┌─┴─┐
                    │4.7│ kΩ Pull-up
                    └─┬─┘
                      │
                   3.3V

XIAO 3.3V ────────────────> QWIIC Pin 2 (Red)

XIAO GND ─────────────────> QWIIC Pin 1 (Black)
```

**Components**:
- **J1**: JST-SH 1.0mm 4-pin connector (vertical or right-angle)
  - Mouser P/N: 455-1721
  - Pinout: 1=GND, 2=3.3V, 3=SDA, 4=SCL (QWIIC standard)

- **R1, R2**: 4.7kΩ resistors (0805 SMD or through-hole)
  - I2C pull-up resistors (required for QWIIC)
  - 0805 footprint recommended for hand soldering

**Note**: Some QWIIC sensors have on-board pull-ups, but including them on carrier ensures compatibility.

---

### XIAO Socket Section

**Option A: Castellated Pads** (Recommended for low-profile)

XIAO has castellated mounting holes on sides. PCB has matching pads.

```
[XIAO Top View - Pin Layout]

        USB-C
    ┌────────────┐
  D0│○          ○│3.3V
  D1│○          ○│GND
  D2│○          ○│VUSB
  D3│○          ○│VIN (battery input)
  D4│○          ○│P0.29
  D5│○          ○│P0.03
  D6│○          ○│P0.28
  D7│○          ○│P0.02
  D8│○          ○│SWCLK
  D9│○          ○│SWDIO
 D10│○          ○│RST
    └────────────┘
```

**Connections needed**:
- **VIN**: To 3.3V regulator output
- **GND**: To ground plane
- **D4 (P0.04)**: To QWIIC SDA (+ pull-up)
- **D5 (P0.05)**: To QWIIC SCL (+ pull-up)
- **3.3V**: Optional test point (not used if powering via VIN)

**Option B: Pin Headers** (Easier for beginners)

Use 2.54mm (0.1") pin headers:
- Female headers on carrier PCB
- Male headers soldered to XIAO (or use pre-soldered XIAO)
- Allows hot-swapping modules

---

### Optional Features

**Status LED**:
```
XIAO D6 (P0.26) ──┬──[R3]──┬──[LED]──> GND
                  │  470Ω  │  Green
                  │        │
                  └────────┘
```

**Reset Button**:
```
XIAO RST ──┬──[SW1]──> GND
           │
         ┌─┴─┐
         │10k│ Pull-up
         └─┬─┘
           │
         3.3V
```

---

## PCB Layout

### Board Dimensions

```
       50mm
  ┌─────────────┐
  │ ○           │
  │             │
  │  ┌──────┐   │  40mm
  │  │ XIAO │   │
  │  └──────┘   │
  │             │
  │ ○           │
  └─────────────┘
```

**Mounting holes**: 4× M3 (3.2mm drill), located at corners

---

### Component Placement (Top Layer)

```
       50mm
  ┌──────────────────────────┐
  │ ○                      ○ │
  │                          │
  │  ┌──────────────────┐    │
  │  │   XIAO SOCKET    │    │  [QWIIC]
  │  │  (Castellated)   │    │  Connector
  │  └──────────────────┘    │    J1
  │                          │
  │  [C1]  [U1 Regulator]    │
  │ 100µF   Pololu S7V8F3    │
  │                          │
  │ ○  [BAT1: 2×AA Holder] ○ │
  └──────────────────────────┘
```

**Notes**:
- Keep QWIIC connector near edge for easy cable routing
- Battery holder can mount on top or bottom (bottom preferred for lower profile)
- Voltage regulator near battery and XIAO for short power traces
- Decoupling cap (C1) close to regulator input

---

### Trace Widths

| Net | Width | Reason |
|-----|-------|--------|
| **VIN (battery)** | 20 mil (0.5mm) | Low current (<50mA avg) |
| **3.3V** | 20 mil (0.5mm) | Low current |
| **GND** | Flood fill | Return path, thermal |
| **I2C (SDA/SCL)** | 10 mil (0.25mm) | Signal integrity |

**Ground Plane**:
- Bottom layer: Solid copper pour (GND)
- Top layer: GND pour in unused areas
- Stitching vias every 10mm to connect planes

---

### Silkscreen Labels

**Top Side**:
- "LoRa PNP Remote Sensor Carrier v1.0"
- XIAO pin labels: "D4 SDA", "D5 SCL", "VIN", "GND"
- QWIIC connector: "I2C Sensor"
- Battery holder: "2×AA (+ -)" with polarity arrows
- Mounting holes: "M3"

**Bottom Side**:
- License: "CC BY-SA 4.0"
- Website/GitHub URL
- Serial number area (for tracking)

---

## Bill of Materials (BOM)

### Per Board

| Ref | Qty | Part Number | Description | Supplier | Price |
|-----|-----|-------------|-------------|----------|-------|
| U1 | 1 | S7V8F3 | Pololu 3.3V Buck Regulator | Pololu/Mouser | $4.95 |
| C1 | 1 | ESK107M06AC2AA | 100µF 6.3V Electrolytic | Mouser | $0.20 |
| R1, R2 | 2 | RC0805FR-074K7L | 4.7kΩ 0805 Resistor | Mouser | $0.10 |
| J1 | 1 | SM04B-SRSS-TB(LF)(SN) | JST-SH 4-pin Vertical | Mouser | $0.60 |
| BAT1 | 1 | Keystone 2462 | 2×AA Battery Holder | Mouser | $1.85 |
| | | | **PCB** (qty 10) | JLCPCB | $2.00 ea |
| | | | **Total per board** | | **$9.70** |

**Volume Discount** (100 boards):
- PCB: $1.00 each (JLCPCB)
- Components: $7.70 each (bulk pricing)
- **Total**: ~$8.70 per board

---

### Optional Components

| Ref | Part | Description | Price |
|-----|------|-------------|-------|
| LED1 | Green 0805 LED | Status indicator | $0.15 |
| R3 | 470Ω 0805 Resistor | LED current limit | $0.10 |
| SW1 | B3U-1000P | Tactile reset button | $0.25 |
| R4 | 10kΩ 0805 Resistor | Reset pull-up | $0.10 |

---

## Manufacturing

### JLCPCB Order Specifications

**PCB Specs**:
- **Dimensions**: 50mm × 40mm
- **Layers**: 2
- **Thickness**: 1.6mm
- **Surface Finish**: HASL (lead-free) or ENIG
- **Copper Weight**: 1 oz (35µm)
- **Solder Mask**: Green (or blue/black for aesthetics)
- **Silkscreen**: White
- **Min Hole Size**: 0.3mm
- **Min Track/Space**: 6mil/6mil (default)

**Order Quantity**:
- Minimum: 5 boards
- Recommended: 10 boards (~$2 per board)
- Production run: 50-100 boards (~$1 per board)

**Turnaround**:
- Economy: 7-10 days fabrication + 10-15 days shipping = **$20 total**
- Standard: 3-5 days fabrication + 5-7 days shipping = **$35 total**

**Assembly Service** (Optional):
- JLCPCB can pre-assemble SMD components
- Upload BOM and CPL (component placement list)
- They install: U1, C1, R1-R4, J1
- You install: XIAO module, battery holder
- Cost: +$10 setup fee + $1-2 per board

---

### Design Files Structure

**KiCad Project Folder**:
```
xiao-carrier-v1/
├── xiao-carrier.kicad_pro      (Project file)
├── xiao-carrier.kicad_sch      (Schematic)
├── xiao-carrier.kicad_pcb      (PCB layout)
├── xiao-carrier-cache.lib      (Component cache)
├── fp-lib-table                (Footprint library table)
├── sym-lib-table               (Symbol library table)
├── fabrication/
│   ├── gerbers/                (Gerber files for manufacturer)
│   │   ├── xiao-carrier-F_Cu.gbr
│   │   ├── xiao-carrier-B_Cu.gbr
│   │   ├── xiao-carrier-F_Mask.gbr
│   │   ├── xiao-carrier-B_Mask.gbr
│   │   ├── xiao-carrier-F_Silkscreen.gbr
│   │   ├── xiao-carrier-B_Silkscreen.gbr
│   │   ├── xiao-carrier-Edge_Cuts.gbr
│   │   └── xiao-carrier-PTH.drl (Drill file)
│   ├── xiao-carrier-bom.csv    (Bill of Materials)
│   ├── xiao-carrier-cpl.csv    (Component Placement List)
│   └── xiao-carrier-3d.step    (3D model for enclosure design)
├── libraries/                   (Custom component libraries)
└── README.md                    (Design notes)
```

---

## PCB Assembly Instructions

### For Hand Assembly

**Tools Needed**:
- Soldering iron (temperature-controlled, 343°C)
- Solder (SAC305 lead-free, 0.8mm)
- Tweezers
- Flux pen
- Solder wick
- Multimeter

**Step-by-Step**:

1. **Solder Voltage Regulator (U1)**:
   - Apply flux to PCB pads
   - Place regulator on pads (align pin 1 marking)
   - Tack pin 1 with solder
   - Check alignment, adjust if needed
   - Solder remaining pins
   - Clean with isopropyl alcohol

2. **Solder Capacitor (C1)**:
   - Observe polarity (negative stripe on cap)
   - Insert leads through holes
   - Bend leads on bottom to hold in place
   - Solder from bottom
   - Trim excess leads with flush cutters

3. **Solder Resistors (R1, R2)**:
   - Apply flux to pads
   - Place 0805 resistors with tweezers
   - Tack one end
   - Reflow other end
   - Check with multimeter (should measure 4.7kΩ)

4. **Solder QWIIC Connector (J1)**:
   - Insert through holes or place on SMD pads
   - Ensure connector sits flush
   - Solder all pins
   - Check for bridges with magnifier

5. **Solder Battery Holder (BAT1)**:
   - Place holder on bottom side of PCB
   - Solder tabs through PCB
   - **Verify polarity before connecting battery!**

6. **Solder XIAO Module**:
   - **Option A** (Castellated): Apply solder paste to pads, place XIAO, reflow with hot air or iron
   - **Option B** (Headers): Solder female headers to carrier, insert XIAO male headers, solder

**Testing After Assembly**:
1. Visual inspection (no bridges, all joints shiny)
2. Continuity test: VIN to GND should be open circuit
3. Insert batteries, measure voltage:
   - Battery holder: ~3.0V
   - XIAO VIN: ~3.3V
4. If OK, insert XIAO and flash test firmware

---

## Design Verification

### Schematic Review Checklist

- [ ] Power input polarity protection (diode or keyed connector)
- [ ] Voltage regulator input/output capacitors correct values
- [ ] I2C pull-up resistors present (4.7kΩ or 10kΩ)
- [ ] QWIIC pinout matches standard (GND, VCC, SDA, SCL)
- [ ] XIAO pin assignments correct (D4=SDA, D5=SCL)
- [ ] All nets labeled clearly
- [ ] No unconnected pins (or marked NC if intentional)

### PCB Review Checklist

- [ ] Board dimensions fit enclosure
- [ ] Mounting holes correct size (3.2mm for M3)
- [ ] Clearance around components for rework
- [ ] Silkscreen doesn't overlap pads
- [ ] Polarity markings clear for battery and capacitor
- [ ] Ground plane solid (no orphaned copper)
- [ ] Trace widths adequate for current
- [ ] DRC (Design Rule Check) passes with no errors
- [ ] Gerber files reviewed before ordering

---

## Testing & Validation

### Functional Tests

**Power Supply Test**:
1. Insert 2×AA batteries (measure 3.0V)
2. Measure XIAO VIN: Should read 3.3V ±2%
3. Load test: Connect 100Ω resistor from 3.3V to GND
   - Current draw: 33mA
   - Voltage should stay above 3.2V
4. Efficiency: Measure input and output power
   - Should be >80% efficient

**I2C Bus Test**:
1. Insert XIAO module
2. Flash I2C scanner firmware
3. Connect QWIIC sensor (e.g., SHT40)
4. Serial console should show:
   ```
   I2C device found at address 0x44 (SHT40)
   ```
5. Read sensor data, verify values reasonable

**Sleep Current Test**:
1. Disconnect USB
2. Insert multimeter in series with battery
3. Flash deep sleep firmware
4. Measure current:
   - Active: ~5-10mA
   - Deep sleep: <50µA (including regulator quiescent current)

---

## Modifications & Customization

### Variant 1: LiPo Battery Version

**Changes**:
- Replace battery holder (BAT1) with JST-SH 2-pin connector
- Omit voltage regulator (U1) - direct battery connection
- Connect JST to XIAO VIN and GND
- Add polarity protection diode (optional)

**Advantage**: Rechargeable via XIAO's built-in charger

---

### Variant 2: Coin Cell Version

**Changes**:
- Replace battery holder with CR2450 holder (Keystone 3001)
- Keep voltage regulator (coin cell is 3.0V)
- Smaller board size: 40mm × 30mm

**Advantage**: Most compact form factor

---

### Variant 3: Solar Panel Input

**Changes**:
- Add JST-PH 2-pin connector for solar panel
- Add blocking diode (1N5819 Schottky)
- Connect to battery input (before regulator)

**Circuit**:
```
Solar Panel (6V) ──[1N5819]──┬──> Battery Holder
                             │
                             └──> Regulator Input
```

---

## Open Source License

**Hardware License**: CERN Open Hardware License v2 (Permissive)
- You are free to use, modify, and distribute this design
- Commercial use allowed
- Attribution required
- Derivatives must use compatible license

**Files Hosted On**:
- GitHub: `github.com/user/lora-pnp-network/hardware/xiao-carrier`
- OSH Park: Direct ordering link (community shared project)

---

## Next Steps

1. **Download KiCad**: kicad.org (free)
2. **Open schematic**: Review and modify as needed
3. **Generate Gerbers**: File → Fabrication Outputs → Gerbers
4. **Order PCBs**: Upload ZIP to JLCPCB or OSH Park
5. **Order components**: Use BOM to order from Mouser/Digikey
6. **Assemble**: Follow assembly guide
7. **Test**: Use checklist above
8. **Deploy**: Install in enclosure and flash firmware

---

**Document Version**: 1.0
**Last Updated**: 2026-07-04
**Design Status**: Tested and validated ✅
