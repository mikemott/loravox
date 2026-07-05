# Seeed Studio Order Recommendations

**Date**: 2026-07-04
**For**: LoRa PNP Network Development
**Budget**: Flexible (sensors beyond basics)

---

## Core Items (Required)

### XIAO Modules

| SKU | Product | Qty | Price | Notes |
|-----|---------|-----|-------|-------|
| 102010448 | **XIAO nRF52840 Sense** | 2 | $31.80 | BLE remote sensors, IMU + mic built-in |

**Why 2?** One for testing, one for deployment. Order more later.

---

## Interesting Sensors (Beyond Basic Temp/Humidity/Light)

You mentioned having: *temp, humidity, light, soil, air/water temp already*

Here are **advanced sensors** from Seeed worth experimenting with:

---

### 1. Air Quality & Gas Detection

#### **Grove - Multichannel Gas Sensor v2 (GM-102B)**
- **SKU**: 103020177
- **Price**: $27.50
- **Detects**: NO₂, C₂H₅OH (alcohol), VOC, CO
- **Interface**: I2C (Grove, compatible with QWIIC via adapter)
- **Use Cases**:
  - Indoor air quality monitoring
  - Kitchen/garage gas leak detection
  - Industrial safety
  - Breath alcohol detection (fun project!)
- **Why it's cool**: 4 sensors in one module, very sensitive
- **Power**: 3.3V, ~150mA active (not suitable for ultra-low-power nodes)

#### **Grove - CO₂ & Temperature & Humidity Sensor (SCD41)**
- **SKU**: 101020952
- **Price**: $39.90
- **Measures**: CO₂ (400-5000ppm), Temp, Humidity
- **Why**: Gold standard for CO₂ monitoring, excellent for indoor air quality
- **Use**: HVAC automation, grow rooms, classrooms, bedrooms

---

### 2. Motion & Presence Detection

#### **Grove - mmWave Human Detection Sensor Kit** ⭐ Highly Recommended
- **SKU**: 114020171
- **Price**: $24.90
- **Technology**: 60GHz mmWave radar
- **Features**:
  - Detects human presence through walls/barriers
  - No false triggers from pets or objects
  - Range: 0-5 meters
  - Motion + stationary detection
  - Configurable zones
- **Interface**: UART or I2C
- **Use Cases**:
  - Occupancy detection for lighting/HVAC
  - Security (detects people behind doors)
  - Elder care (fall detection)
  - Energy saving (turn off devices when room empty)
- **Why it's awesome**: Unlike PIR sensors, detects even stationary humans. Can see through thin walls. No camera (privacy-friendly).

#### **Grove - Gesture Sensor (PAJ7620)**
- **SKU**: 101020083
- **Price**: $11.90
- **Detects**: 9 gestures (up, down, left, right, forward, backward, clockwise, counterclockwise, wave)
- **Range**: 5-10cm
- **Use**: Touchless control interfaces, interactive art installations
- **Interface**: I2C

---

### 3. Distance & Ranging

#### **Grove - Time of Flight Distance Sensor (VL53L0X)**
- **SKU**: 101020532
- **Price**: $9.90
- **Range**: 30-2000mm
- **Accuracy**: ±3%
- **Technology**: Laser ranging
- **Use Cases**:
  - Tank level monitoring (water, grain, fuel)
  - Parking sensors
  - People counting (doorway)
  - Robotic obstacle avoidance
- **Why**: Very accurate, works in any light, small form factor

---

### 4. Vibration & Shock Detection

#### **Grove - 3-Axis Digital Accelerometer (LIS3DHTR)**
- **SKU**: 101020088
- **Price**: $9.90
- **Range**: ±2g/±4g/±8g/±16g (selectable)
- **Features**: Interrupt on motion, free-fall detection, tap detection
- **Interface**: I2C
- **Use Cases**:
  - Equipment vibration monitoring (predictive maintenance)
  - Earthquake/seismic detection
  - Tilt sensing
  - Washing machine cycle detection
  - Door/window tamper detection
- **Why**: Industrial-grade sensor, interrupts save power

---

### 5. Particulate Matter (Air Quality)

#### **Grove - Laser PM2.5 Sensor (HM3301)**
- **SKU**: 101020714
- **Price**: $18.90
- **Measures**: PM1.0, PM2.5, PM10
- **Technology**: Laser scattering
- **Interface**: I2C
- **Use Cases**:
  - Outdoor air quality monitoring
  - HVAC filter effectiveness
  - Wildfire smoke detection
  - Clean room monitoring
- **Why**: Much cheaper than Adafruit PMSA003I ($45), nearly same performance
- **Power**: ~100mA active (not for battery nodes, use on gateway or USB-powered node)

---

### 6. Color & Spectral Sensing

#### **Grove - I2C Color Sensor v2 (TCS3472)**
- **SKU**: 101020341
- **Price**: $8.90
- **Measures**: RGB + Clear (4 channels)
- **Use Cases**:
  - Color sorting/matching
  - Plant health (leaf color analysis)
  - Light color temperature detection
  - Art installations
- **Interface**: I2C

---

### 7. Liquid Level & Flow

#### **Grove - Water Sensor**
- **SKU**: 101020018
- **Price**: $2.90
- **Type**: Resistive moisture detection
- **Use**: Leak detection (under sinks, water heaters, basements)
- **Interface**: Analog (requires ADC, XIAO has built-in ADC)

#### **Grove - Water Flow Sensor (G1/2")**
- **SKU**: SEN92420P
- **Price**: $7.90
- **Measures**: Flow rate (1-30 L/min)
- **Output**: Pulse frequency
- **Use**: Water consumption monitoring, irrigation control
- **Interface**: Digital GPIO (pulse counting)

---

### 8. Magnetic & Hall Effect

#### **Grove - I2C High Accuracy Temperature Sensor (MCP9808)**
- **SKU**: 101020556
- **Price**: $5.90
- **Accuracy**: ±0.25°C (best in class)
- **Range**: -40 to +125°C
- **Use**: Precision temperature applications (cold chain, lab, incubators)
- **Why**: If you need medical-grade accuracy, this is it

---

### 9. Current & Power Monitoring

#### **Grove - I2C High Accuracy Mini Watt Meter (INA219)**
- **SKU**: Not listed, use Adafruit 3686 instead
- Alternative: **Grove - Voltage Divider**
- **Price**: $3.90
- **Use**: Monitor battery voltage, solar panel output

*Note: For current sensing, Adafruit INA219 ($9.95) is better option*

---

### 10. Atmospheric & Weather

#### **Grove - Sunlight Sensor (Si1145)**
- **SKU**: 101020089
- **Price**: $14.90
- **Measures**: UV Index, Visible Light, IR
- **Use Cases**:
  - UV exposure monitoring
  - Outdoor automation (awnings, blinds)
  - Solar panel optimization
  - Plant light tracking
- **Interface**: I2C

---

## Recommended Seeed Studio Order

### Conservative Order ($150 budget)

| Qty | Product | Price | Subtotal |
|-----|---------|-------|----------|
| 2 | XIAO nRF52840 Sense | $15.90 | $31.80 |
| 1 | mmWave Human Detection Sensor Kit | $24.90 | $24.90 |
| 1 | Multichannel Gas Sensor v2 | $27.50 | $27.50 |
| 1 | Laser PM2.5 Sensor (HM3301) | $18.90 | $18.90 |
| 1 | 3-Axis Accelerometer (LIS3DHTR) | $9.90 | $9.90 |
| 1 | ToF Distance Sensor (VL53L0X) | $9.90 | $9.90 |
| 1 | Sunlight Sensor (Si1145) | $14.90 | $14.90 |
| 5 | Grove Universal Cables (20cm) | $0.90 | $4.50 |
| 1 | Grove to QWIIC Adapter Cable | $1.95 | $1.95 |
| | **Subtotal** | | **$144.25** |
| | **Shipping (est)** | | **$8.00** |
| | **Total** | | **$152.25** |

**What you get**:
- ✅ BLE modules for remote sensors
- ✅ mmWave radar for occupancy detection (unique!)
- ✅ Multi-gas sensor for air quality
- ✅ Particulate sensor for smoke/dust
- ✅ Vibration sensor for predictive maintenance
- ✅ Distance sensor for level monitoring
- ✅ UV sensor for outdoor applications

---

### Expanded Order ($250 budget)

Add to above:

| Qty | Product | Price | Subtotal |
|-----|---------|-------|----------|
| 1 | CO₂ Sensor (SCD41) | $39.90 | $39.90 |
| 1 | Gesture Sensor (PAJ7620) | $11.90 | $11.90 |
| 1 | I2C Color Sensor v2 | $8.90 | $8.90 |
| 1 | Water Flow Sensor | $7.90 | $7.90 |
| 2 | Water Leak Sensors | $2.90 | $5.80 |
| 3 | XIAO nRF52840 Sense (additional) | $15.90 | $47.70 |
| | **Additional** | | **$122.10** |
| | **New Total** | | **$274.35** |

**Why the additions**:
- CO₂ sensor: Indoor air quality is huge use case
- Gesture sensor: Cool demo for touchless control
- Water sensors: Leak detection saves $$$
- Extra XIAOs: You'll want more once you start building!

---

## Why These Sensors?

### They Fill Capability Gaps

You already have: ✅ Temp, Humidity, Light, Soil, Water Temp

These add:
- **Air Quality**: Gas, VOC, PM2.5, CO₂
- **Motion**: mmWave radar, vibration, gesture
- **Distance**: Laser ranging for tanks/levels
- **UV**: Outdoor sun exposure
- **Color**: Spectral analysis
- **Flow**: Water consumption

### They're Unique to Seeed

- mmWave radar: Not common elsewhere
- Multi-gas sensor: Unique 4-in-1 design
- Competitive pricing vs Adafruit/SparkFun

### They Enable Cool Projects

1. **Smart HVAC**: CO₂ + PM2.5 + mmWave occupancy → auto-adjust ventilation
2. **Equipment Monitoring**: Vibration sensor detects motor problems before failure
3. **Air Quality Station**: Multi-gas + PM2.5 + UV → comprehensive outdoor monitoring
4. **Touchless Control**: Gesture sensor + mmWave → contactless device control
5. **Water Management**: Flow + leak sensors → detect leaks, track usage

---

## QWIIC Compatibility Note

**All Grove sensors work with QWIIC via adapter cable**:
- Grove connector: 4-pin, 2.0mm pitch
- QWIIC connector: 4-pin, 1.0mm pitch (JST-SH)
- Pinout: Same (GND, VCC, SDA, SCL)
- Solution: Use "Grove to QWIIC" adapter cable ($1.95)

OR, cut Grove cable and solder JST-SH connector for permanent conversion.

---

## Alternative: Skip Seeed, Use Adafruit/SparkFun

**If you prefer QWIIC-native sensors**:

- SparkFun SGP40 (VOC): $14.95 vs Seeed Multi-gas: $27.50
- Adafruit PMSA003I (PM2.5): $44.95 vs Seeed HM3301: $18.90
- SparkFun VL53L1X (distance): $14.95 vs Seeed VL53L0X: $9.90

**Seeed is cheaper, Adafruit has better documentation.**

---

## Final Recommendation

**Order from Seeed**:
- 2× XIAO nRF52840 Sense (required for project)
- 1× mmWave Human Detection Sensor (unique, very cool)
- 1× Multichannel Gas Sensor v2 (air quality)
- 1× Laser PM2.5 Sensor (air quality, half price of Adafruit)
- 1× 3-Axis Accelerometer (vibration monitoring)
- 1× ToF Distance Sensor (level monitoring)
- 5× Grove cables + 1× Grove-to-QWIIC adapter

**Total**: ~$150

**Then order from Adafruit/SparkFun**:
- CO₂ sensor (if needed, better docs from Adafruit)
- QWIIC cables (higher quality than Grove)
- Other QWIIC sensors as needed

---

## Questions?

**Need help choosing?** Consider your use case:
- **Home automation**: mmWave + CO₂ + PM2.5
- **Industrial**: Vibration + gas + distance
- **Environmental**: PM2.5 + UV + multi-gas
- **Prototyping**: One of each! 😄

---

**Ready to order?** Head to seeedstudio.com and add to cart!

**Document Version**: 1.0
**Last Updated**: 2026-07-04
