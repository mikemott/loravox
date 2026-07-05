# LoRa PNP Network - Assembly Guide

**Version**: 1.0
**Date**: 2026-07-04
**Skill Level**: Beginner to Intermediate
**Time**: 3-10 minutes per device

---

## Table of Contents

1. [Tools & Materials](#tools--materials)
2. [Gateway Assembly](#gateway-assembly)
3. [Node Assembly](#node-assembly)
4. [Remote Sensor Assembly](#remote-sensor-assembly)
5. [Quality Control](#quality-control)
6. [Troubleshooting](#troubleshooting)

---

## Tools & Materials

### Required Tools

**For No-Solder Assembly**:
- ✅ Small Phillips screwdriver (#1)
- ✅ USB-C cable (for programming)
- ✅ Computer with PlatformIO or Arduino IDE
- ✅ Multimeter (for voltage checks)

**For Soldering (One-Time Setup)**:
- 🔧 Soldering iron (temperature-controlled, 315-343°C)
- 🔧 Lead-free solder (SAC305, 0.8mm diameter)
- 🔧 Wire strippers
- 🔧 Flush cutters
- 🔧 Helping hands or PCB holder
- 🔧 Solder wick or desoldering pump
- 🔧 ESD wrist strap
- 🔧 Safety glasses

### Consumables

- Isopropyl alcohol (99%, for cleaning)
- Lint-free wipes
- Heat shrink tubing (assorted sizes)
- Zip ties (for cable management)
- Kapton tape (for insulation)

---

## Gateway Assembly

### Option A: Raspberry Pi 5 + RAK2287 HAT

**Time**: 5-10 minutes
**Difficulty**: ⭐☆☆☆☆ (Easiest)
**Soldering**: None required

#### Parts List

| Qty | Part | Notes |
|-----|------|-------|
| 1 | Raspberry Pi 5 (4GB) | |
| 1 | RAK2287 LoRa Concentrator HAT | SX1302, 915MHz |
| 1 | 32GB microSD card (Class 10, A1) | Pre-loaded with OS |
| 1 | Raspberry Pi 5 Case | With fan, HAT-compatible |
| 1 | 915MHz LoRa antenna (5dBi) | RP-SMA or N-type |
| 1 | USB-C power supply (5V 5A, 27W) | Official Pi 5 PSU |
| 4 | M2.5 standoffs (11mm) | Usually included with HAT |

#### Assembly Steps

**Step 1: Prepare the Raspberry Pi**

1. Remove Pi 5 from anti-static bag
2. Place on clean, static-free surface
3. Insert microSD card into slot on underside of Pi
   - Push gently until clicks (spring-loaded)
   - Card label faces up

![Pi 5 microSD insertion diagram]

**Step 2: Install RAK2287 HAT**

1. Locate 40-pin GPIO header on Pi 5 (near USB ports)
2. Remove RAK2287 from packaging
3. Align HAT's 40-pin female header with Pi GPIO
   - Check pin 1 alignment (square pad on both boards)
   - HAT should be parallel to Pi, not tilted
4. Press down firmly and evenly on both sides
   - You should feel resistance, then seating
   - Do not rock or twist

5. **Install standoffs** (for mechanical support):
   - Locate 4× mounting holes at corners
   - Thread M2.5 standoffs through HAT into Pi mounting holes
   - Tighten by hand (do not overtighten)

![HAT installation diagram]

**Step 3: Connect Antenna**

1. Locate RP-SMA connector on RAK2287 HAT (usually near edge)
2. Thread antenna onto connector
   - Turn clockwise until snug
   - Do not use pliers (hand-tight only)
3. Position antenna vertically if possible

⚠️ **NEVER power on without antenna connected** - can damage transmitter

**Step 4: Install in Case** (Optional but recommended)

1. Place Pi + HAT assembly into case bottom
2. Route antenna through case cutout
3. Attach case fan connector to Pi GPIO pin 4 (5V) and pin 6 (GND)
   - Or use fan header if case provides one
4. Secure case lid with screws

**Step 5: Connect Peripherals**

1. Ethernet cable → RJ45 port (for network)
2. HDMI cable → micro HDMI port (for initial setup, optional)
3. Keyboard/mouse → USB ports (for initial setup, optional)
4. **USB-C power last** (powers on automatically)

**Step 6: First Boot**

1. Apply power via USB-C
2. Green LED should blink (indicates SD card activity)
3. Red LED solid (indicates power good)
4. Wait 60-90 seconds for first boot
5. See `docs/software/GATEWAY-SETUP.md` for OS configuration

#### Verification

- [ ] Pi 5 boots (LEDs blinking)
- [ ] SSH accessible (default: `pi@raspberrypi.local`)
- [ ] `ls /dev/spidev*` shows SPI devices
- [ ] `sudo i2cdetect -y 1` shows no errors
- [ ] RAK2287 detected by gateway software

---

### Option B: LilyGO T-Internet-POE + E22 LoRa Module

**Time**: 15-30 minutes
**Difficulty**: ⭐⭐⭐☆☆ (Intermediate, requires soldering or breadboard)
**Soldering**: Optional (can use breadboard for testing)

#### Parts List

| Qty | Part | Notes |
|-----|------|-------|
| 1 | LilyGO T-Internet-POE | ESP32, W5500 Ethernet |
| 1 | Ebyte E22-900M30S | SX1262, 30dBm, DIP package |
| 1 | 915MHz LoRa antenna (3dBi) | RP-SMA |
| 1 | Breadboard (400 tie-points) | For temporary assembly |
| 10 | Female-to-male jumper wires | 15-20cm length |
| 1 | PoE injector (802.3af) | Or use USB-C power |

#### Assembly Steps (Breadboard Method)

**Step 1: Insert E22 Module into Breadboard**

1. Locate center channel of breadboard
2. Insert E22-900M30S DIP pins into breadboard
   - All 14 pins should be firmly seated
   - Module should sit perpendicular to board

**Step 2: Wire E22 to T-Internet-POE**

Refer to wiring diagram in `WIRING-DIAGRAMS.md`.

Use this mapping:

| E22 Pin | Wire Color | T-Internet-POE GPIO |
|---------|------------|---------------------|
| VCC | Red | 3.3V |
| GND | Black | GND |
| MOSI | Yellow | GPIO19 |
| MISO | Green | GPIO23 |
| SCK | Orange | GPIO18 |
| NSS | Blue | GPIO5 |
| DIO1 | Purple | GPIO14 |
| RST | White | GPIO27 |
| BUSY | Gray | GPIO26 |

**Step 3: Connect jumper wires**:
1. Female end → T-Internet-POE GPIO header
2. Male end → Breadboard row with E22 pin

**Step 4: Attach Antenna**
1. Screw RP-SMA antenna onto E22 module's antenna connector
2. Ensure hand-tight connection

**Step 5: Power Connection**
- **Option A**: Connect PoE-enabled Ethernet cable
  - T-Internet-POE extracts power from cable
- **Option B**: Connect USB-C cable (5V 2A minimum)

**Step 6: Test Assembly**
1. Connect USB-C to computer (for serial debugging)
2. Apply power
3. Blue LED on T-Internet-POE should blink (WiFi activity)
4. Flash firmware (see `docs/software/GATEWAY-FIRMWARE.md`)
5. Verify LoRa communication in serial monitor

#### Assembly Steps (Production PCB Shield)

**For permanent installation**, design a shield PCB that:
- Stacks onto T-Internet-POE headers (Arduino-style)
- Has E22 module soldered to shield
- No loose wires

See `docs/pcb/GATEWAY-SHIELD.md` for PCB design files.

---

## Node Assembly

### RAK4631 WisBlock Node

**Time**: 5-8 minutes
**Difficulty**: ⭐⭐☆☆☆ (Easy)
**Soldering**: Optional (only for battery holder JST pigtail)

#### Parts List

| Qty | Part | Supplier | Price |
|-----|------|----------|-------|
| 1 | RAK4631 Core Module | RAKwireless | $35 |
| 1 | RAK19007 Base Board | RAKwireless | $10 |
| 1 | RAK1920 QWIIC Adapter | RAKwireless | $5 |
| 1 | Samsung 18650 battery (2500mAh) | Amazon/Mouser | $8 |
| 1 | Keystone 1042 battery holder | Mouser | $2 |
| 1 | JST-PH 2.0mm pigtail (2-pin) | Adafruit | $1 |
| 1 | Adafruit SHT40 (QWIIC) | Adafruit | $5 |
| 1 | QWIIC cable (100mm) | SparkFun/Adafruit | $2 |
| 1 | 915MHz LoRa antenna (2dBi) | Amazon | $5 |
| 1 | Bud Industries NBF-32026 enclosure | Mouser | $8 |

**Total**: ~$81

#### Assembly Steps

**Step 1: Install RAK4631 Core Module**

1. Unpack RAK19007 base board
2. Locate CPU slot (large connector near center)
3. Remove RAK4631 from anti-static bag
4. Align RAK4631 with slot:
   - Notch on module aligns with notch on base
   - Gold contacts face down
   - USB-C connector on module faces same direction as base USB-C
5. Insert module at 45° angle
6. Press down firmly until **clicks** (friction connector)
7. Module should sit flush with base

![RAK4631 installation diagram]

⚠️ **Common mistake**: Inserting backwards. Check USB-C alignment!

**Step 2: Install RAK1920 QWIIC Adapter**

1. Locate sensor slot (smaller connector, usually labeled "SLOT A")
2. Align RAK1920 adapter module with slot
   - Check notch alignment
   - QWIIC connector should face outward
3. Press down firmly until clicks
4. Module sits flush with base

![RAK1920 installation diagram]

**Step 3: Prepare Battery Holder** (One-Time Soldering)

**If battery holder doesn't have pre-attached JST connector**:

1. Strip JST pigtail wires 3-4mm
2. Tin stripped wire ends with solder
3. Solder red (+) wire to battery holder positive terminal
4. Solder black (-) wire to battery holder negative terminal
5. Secure with heat shrink tubing
6. Test polarity with multimeter before connecting

**If using pre-made holder with JST**: Skip to Step 4

![Battery holder soldering diagram]

**Step 4: Insert Battery**

1. Insert 18650 battery into holder
   - Button top (+) to spring terminal
   - Flat bottom (-) to flat terminal
2. Verify polarity with multimeter:
   - JST red wire = +3.7V
   - JST black wire = 0V (GND)

**Step 5: Connect Battery to Base**

1. Locate JST-PH connector on RAK19007 (labeled "BAT" or "VBAT")
2. Plug battery JST connector into base
   - Connector is keyed (can only insert one way)
   - Should click when fully seated
3. Red LED on RAK19007 may blink briefly (charge detection)

⚠️ **Do not force** - if it doesn't fit, flip connector 180°

**Step 6: Connect Sensor via QWIIC**

1. Plug one end of QWIIC cable into RAK1920 adapter
2. Plug other end into sensor module (e.g., SHT40)
   - Cable is symmetrical, either end works
   - Should click when seated

![QWIIC connection diagram]

**Step 7: Attach LoRa Antenna**

1. Locate U.FL connector on RAK4631 (tiny gold socket)
2. Align antenna U.FL plug with socket
3. Press straight down gently until **click**
   - Do not pull on wire, only on connector body
   - Snap should be audible and tactile

⚠️ **Fragile connection** - use fingernail to pry off, not force

**Step 8: USB Connection & Programming**

1. Connect USB-C cable to RAK19007
2. Computer should recognize new serial device
   - macOS: `/dev/cu.usbmodem*`
   - Linux: `/dev/ttyACM*`
   - Windows: `COM*`
3. Flash firmware (see `docs/software/NODE-FIRMWARE.md`)

**Step 9: Install in Enclosure**

1. Route antenna cable through enclosure PG7 cable gland
2. Place assembled node in enclosure
3. Secure with mounting screws or standoffs
4. Close enclosure lid
5. Tighten cable gland for waterproof seal

#### Verification Checklist

- [ ] RAK4631 module firmly seated (no wobble)
- [ ] RAK1920 adapter firmly seated
- [ ] Battery voltage 3.6-4.2V (check with multimeter)
- [ ] QWIIC cable clicks into both connectors
- [ ] U.FL antenna connection feels secure
- [ ] USB recognized by computer
- [ ] Serial monitor shows boot messages (115200 baud)
- [ ] Sensor readings appear in serial output
- [ ] Node discovers gateway (if gateway running)

---

## Remote Sensor Assembly

### XIAO nRF52840 + Custom Carrier PCB

**Time**: 3-5 minutes
**Difficulty**: ⭐☆☆☆☆ (Easiest, if PCB pre-assembled)
**Soldering**: Optional (only if assembling carrier PCB from scratch)

#### Parts List (Per Sensor)

| Qty | Part | Supplier | Price |
|-----|------|----------|-------|
| 1 | XIAO nRF52840 Sense | Seeed Studio | $15 |
| 1 | Custom carrier PCB (assembled) | JLCPCB | $5 |
| 1 | Keystone 2462 (2×AA holder) | Mouser | $2 |
| 2 | Energizer Ultimate Lithium AA | Amazon | $4 |
| 1 | Adafruit SHT40 (QWIIC) | Adafruit | $5 |
| 1 | QWIIC cable (50mm) | Adafruit | $1.50 |
| 1 | Hammond 1551BTBU enclosure | Mouser | $5 |

**Total**: ~$37.50 per sensor

#### Assembly Steps (Using Pre-Assembled Carrier PCB)

**Step 1: Inspect Carrier PCB**

1. Remove PCB from protective packaging
2. Visual inspection:
   - [ ] No solder bridges
   - [ ] All components present
   - [ ] XIAO socket clean and undamaged
   - [ ] QWIIC connector seated properly

**Step 2: Install XIAO Module**

**If PCB has pin header socket**:
1. Remove XIAO from anti-static bag
2. Align XIAO pins with carrier PCB socket
   - USB-C connector orientation matches silkscreen
3. Press straight down until seated
4. XIAO should sit flush (no gap)

**If PCB has hot-swap socket** (recommended):
1. Insert XIAO into socket at 45° angle
2. Press down and forward to latch
3. To remove: Pull tab to release

![XIAO installation on carrier PCB]

**Step 3: Insert Batteries**

1. Note polarity markings on carrier PCB and battery holder
2. Insert 2× AA batteries:
   - (+) button end to spring
   - (-) flat end to flat contact
3. Verify voltage with multimeter:
   - Across battery holder terminals: ~3.0-3.2V
   - At XIAO VIN pin: ~3.3V (after regulator)

⚠️ **Use Energizer Ultimate Lithium** for longest life (not alkaline)

**Step 4: Connect Sensor**

1. Plug QWIIC cable (50mm) into carrier PCB QWIIC connector
2. Plug other end into sensor module
3. Both connections should click

**Step 5: Install in Enclosure**

1. Place carrier PCB in enclosure
   - Align mounting holes with standoffs or screws
2. Secure with M3 screws (if using threaded inserts)
3. Route QWIIC cable so it's not pinched
4. Close enclosure lid gently (don't crush sensor)
5. Secure lid screws (hand-tight, not stripped)

**Step 6: Programming**

1. Open enclosure
2. Connect USB-C cable to XIAO
3. Computer detects new serial device
4. Flash firmware (see `docs/software/REMOTE-SENSOR-FIRMWARE.md`)
5. Verify sensor readings in serial monitor
6. Verify BLE advertising (use phone app "nRF Connect")
7. Disconnect USB, close enclosure

#### Alternative: XIAO Expansion Board Method

**If not using custom PCB**:

**Parts List**:
- XIAO nRF52840 Sense: $15
- XIAO Expansion Board: $5
- 500mAh LiPo battery (JST): $7
- Grove SHT40 sensor or Grove-to-QWIIC adapter + QWIIC sensor: $5-7
- Enclosure: $5

**Assembly**:
1. Insert XIAO into expansion board socket (align USB-C)
2. Plug LiPo battery JST into expansion board
3. Connect Grove cable to expansion board Grove port
4. Connect other end to sensor
5. Place in enclosure

**Total**: ~$37-39 (similar cost, less compact)

---

## Quality Control

### Inspection Checklist (Before Sealing Enclosure)

**Visual Inspection**:
- [ ] No loose wires or components
- [ ] No solder bridges or cold joints (if soldered)
- [ ] Connectors fully seated (no gaps)
- [ ] Battery polarity correct
- [ ] Antenna connected

**Electrical Testing**:
- [ ] Battery voltage correct (3.0-4.2V depending on chemistry)
- [ ] No short circuit between VCC and GND (measure >1MΩ)
- [ ] I2C pull-ups present (2-10kΩ SDA/SCL to VCC)
- [ ] Serial console accessible via USB

**Functional Testing**:
- [ ] Firmware uploads successfully
- [ ] Boot messages appear in serial console
- [ ] Sensor readings valid (not 0x00 or 0xFF)
- [ ] LoRa/BLE advertising visible
- [ ] LED indicators working

**Environmental Testing** (for outdoor units):
- [ ] Enclosure IP rating suitable for deployment (IP54 minimum)
- [ ] Cable glands tightened
- [ ] Desiccant pack inside enclosure (optional, reduces condensation)
- [ ] Mounting hardware corrosion-resistant (stainless steel)

---

## Testing & Verification

### Bench Test Procedure

**Before field deployment**:

1. **Power-On Test**:
   - Connect to battery or USB power
   - Verify LED blinks within 5 seconds
   - No smoke or unusual heat

2. **Serial Console Test**:
   - Connect USB-C cable
   - Open serial monitor (115200 baud, newline)
   - Verify boot messages:
     ```
     LoRa PNP Remote Sensor v1.0
     Device UID: AA:BB:CC:DD:EE:FF
     SHT40 initialized
     BLE advertising started
     Entering deep sleep...
     ```

3. **Sensor Reading Test**:
   - Enter `status` command in serial console
   - Verify temperature and humidity readings are reasonable:
     - Temp: 15-30°C (room temperature)
     - Humidity: 20-80% RH
   - Compare with known-good thermometer

4. **BLE Advertising Test** (Remote Sensors):
   - Use phone app "nRF Connect" (iOS/Android)
   - Scan for BLE devices
   - Look for device with name "LoRaPNP-AABBCC" (last 6 hex of UID)
   - Tap device → "RAW" → Check manufacturer data:
     - Company ID: 0xFFFF
     - Data length: 29 bytes
   - Verify sensor readings in BLE advertisement match serial console

5. **LoRa Communication Test** (Nodes):
   - Power on gateway first
   - Power on node
   - Watch node serial console for:
     ```
     Sending DISCOVER...
     Received OFFER from gateway
     Sending JOIN_REQUEST...
     Received JOIN_ACCEPT, assigned ID: 2
     ```
   - Check gateway serial console shows node in registry

6. **Range Test** (Optional):
   - Place gateway in fixed location
   - Walk away with node/sensor while monitoring signal
   - Note RSSI at various distances (10m, 50m, 100m, 500m, 1km)
   - Map dead zones or obstacles

---

## Troubleshooting

### Common Issues

**Problem**: Device doesn't power on (no LED)

**Solutions**:
- [ ] Check battery voltage with multimeter (should be >2.8V)
- [ ] Verify battery polarity (red = +, black = -)
- [ ] Check JST connector fully seated
- [ ] Try USB power instead of battery
- [ ] Inspect for solder bridges on power rails

---

**Problem**: USB not recognized by computer

**Solutions**:
- [ ] Try different USB-C cable (must be data-capable, not charge-only)
- [ ] Try different USB port on computer
- [ ] Check USB-C connector solder joints (if hand-soldered)
- [ ] Verify USB drivers installed (Windows)
- [ ] Reset device (press reset button if available)

---

**Problem**: Sensor not detected (I2C error)

**Solutions**:
- [ ] Check QWIIC cable fully clicked into both connectors
- [ ] Verify sensor I2C address matches firmware (use `i2c scan` command)
- [ ] Try different QWIIC cable (may be damaged)
- [ ] Check SDA and SCL pull-up resistors present (2-10kΩ to VCC)
- [ ] Verify sensor power: Measure 3.3V on sensor VCC pin

---

**Problem**: LoRa transmission fails

**Solutions**:
- [ ] Verify antenna connected (U.FL clicked or RP-SMA tight)
- [ ] Check antenna is correct frequency (915MHz for US)
- [ ] Verify LoRa settings match gateway (frequency, SF, BW)
- [ ] Check for metal shielding blocking signal
- [ ] Increase TX power in firmware (default 14dBm, max 22dBm)
- [ ] Measure supply voltage during TX (should not drop below 3.0V)

---

**Problem**: BLE advertising not visible

**Solutions**:
- [ ] Verify advertising is enabled in firmware (`ble on` command)
- [ ] Check phone Bluetooth is on and scanning
- [ ] Verify BLE TX power set correctly (0 dBm default)
- [ ] Reduce advertising interval for testing (1 second vs 30 seconds)
- [ ] Check nRF52840 antenna connection (internal PCB antenna, no external)
- [ ] Move closer to phone (BLE range ~10m indoors initially)

---

**Problem**: Battery drains quickly (<1 week)

**Solutions**:
- [ ] Verify deep sleep is working (measure current: should be <10µA)
- [ ] Check for firmware infinite loop or stuck state
- [ ] Disable OLED display if enabled (uses 5-10mA)
- [ ] Increase sleep interval (30s → 60s)
- [ ] Verify sensors power down between readings
- [ ] Check for battery self-discharge (test with new battery)
- [ ] Measure actual current draw with multimeter in series

---

**Problem**: Sensor readings incorrect or frozen

**Solutions**:
- [ ] Verify sensor type matches firmware configuration
- [ ] Check sensor datasheet for warm-up time (some need 100ms)
- [ ] Try power-cycling device (full reset)
- [ ] Verify I2C clock speed not too fast (100 kHz or 400 kHz)
- [ ] Check for I2C bus lock (SCL/SDA stuck low)
- [ ] Replace sensor (may be damaged)

---

## Safety Warnings

### Electrical Safety

- ⚠️ Never short-circuit lithium batteries (fire/explosion risk)
- ⚠️ Don't exceed 4.2V when charging (overcharge → fire)
- ⚠️ Use protected cells with over-discharge cutoff
- ⚠️ Fuse battery circuits (2A fast-blow recommended)
- ⚠️ Work in ventilated area when soldering
- ⚠️ Wear safety glasses when cutting wires or components

### RF Safety

- ⚠️ Never transmit without antenna (can damage transmitter)
- ⚠️ Keep LoRa antennas >20cm from body when transmitting
- ⚠️ Don't exceed regional power limits (US: 30dBm / 1W EIRP)
- ⚠️ Gateway antennas should be outdoors or >2m from people

### Environmental Safety

- ⚠️ Dispose of batteries properly (lithium recycling)
- ⚠️ Use lead-free solder (SAC305)
- ⚠️ Don't expose lithium batteries to temperatures >60°C
- ⚠️ IP-rated enclosures required for outdoor deployment

---

## Packaging & Storage

### Long-Term Storage (Undeployed Units)

**Remote Sensors**:
- Remove batteries (prevents self-discharge damage)
- Store in anti-static bags
- Cool, dry environment (15-25°C, <60% RH)
- Away from direct sunlight

**Nodes**:
- Charge LiPo batteries to 50-60% (storage voltage)
- Disconnect battery
- Store in anti-static bags

**Gateway**:
- Power off
- Store indoors
- Protect from moisture and dust

### Shipping Assembled Units

**Packaging**:
- Anti-static bag for electronics
- Bubble wrap or foam padding
- Mark "FRAGILE - ELECTRONIC EQUIPMENT"
- Include "CONTAINS LITHIUM BATTERIES" label (if applicable)

**Regulations**:
- Lithium batteries >100Wh require special shipping (most are <20Wh, exempt)
- Check carrier regulations (USPS, FedEx, UPS) for battery shipping

---

## Next Steps

After assembly is complete:

1. **Firmware**: Flash latest firmware (see `docs/software/`)
2. **Calibration**: Sensor calibration procedures (see `docs/calibration/`)
3. **Deployment**: Installation guide (see `docs/deployment/`)
4. **Monitoring**: Setup dashboard and alerts (see `docs/dashboard/`)

---

**Document Version**: 1.0
**Last Updated**: 2026-07-04
**Questions?** See `docs/TROUBLESHOOTING.md` or open GitHub issue
