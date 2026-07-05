# LoRa PNP Remote Sensor

BLE-only remote sensor firmware for battery-powered operation with multi-year battery life.

## Hardware Setup

**Board:** LilyGO T3-S3 (ESP32-S3)

**Connections:**
- SHT40 sensor → QWIIC connector (pins 43/44)
- Battery → Battery connector (monitored via GPIO1 ADC)
- USB → For programming and testing only

**Note:** This firmware does NOT use the LoRa radio to save power.

## Firmware Features

- **BLE Advertising**: Broadcasts sensor data every 30 seconds
- **Deep Sleep**: 99%+ duty cycle for maximum battery life
- **SHT40 Sensor**: Temperature and humidity
- **Battery Monitoring**: Tracks battery voltage
- **OLED Display**: Shows readings during testing (disable for production)

## Power Consumption

**With display enabled (testing):**
- ~1.3 mAh/day
- 167 days on CR2032 (220 mAh)
- 5+ years on 2×AA (2500 mAh)

**With display disabled (production):**
- ~1.2 mAh/day
- 182 days on CR2032
- 5.6 years on 2×AA

## Building and Flashing

```bash
cd remote_sensor
pio run --target upload
pio device monitor
```

## Serial Commands

| Command | Description |
|---------|-------------|
| `status` | Show device info, readings, battery |
| `read` | Read sensor now |
| `advertise` | Trigger immediate BLE advertisement |
| `display on/off` | Enable/disable OLED (saves power) |
| `sleep on/off` | Enable/disable deep sleep |
| `factory` or `reset` | Reset boot count and restart |

## Testing Procedure

### Phase 1: Standalone Testing

1. Flash firmware to T3-S3
2. Connect SHT40 sensor to QWIIC port
3. Power via USB
4. Monitor serial output

**Expected output:**
```
=== LoRa PNP Remote Sensor ===
Cold boot
Device UID: AA:BB:CC:DD:EE:FF
SHT40 initialized
Battery: 3700 mV

--- Sensor Cycle Start ---
Sensor: 23.45°C, 45.2% RH
BLE advertising: 23.45°C, 45.2% RH, 3700 mV
BLE advertising complete
Entering deep sleep for 30 seconds (boot #1)
```

2. Use phone app (nRF Connect) to verify BLE advertisements:
   - Service UUID: `0x4C50`
   - Manufacturer data: 29 bytes
   - Should appear every 30 seconds

### Phase 2: Power Measurement

1. Disable display: `display off`
2. Enable sleep: `sleep on`
3. Disconnect USB, connect battery
4. Measure current with multimeter:
   - Deep sleep: ~5 µA
   - Wake + sensor + BLE: ~50 mA for 150ms
   - Average: ~1.2 mAh/day

### Phase 3: Range Testing

1. Place node in scanning mode
2. Move remote sensor away from node
3. Check RSSI at different distances:
   - 10m: -50 to -60 dBm
   - 50m: -65 to -75 dBm
   - 100m: -75 to -85 dBm

## Troubleshooting

**SHT40 not found:**
- Check QWIIC cable connection
- Verify sensor has power (3.3V)
- Try `i2c scan` to check address (0x44)

**Battery reads high (>4500mV):**
- Expected when powered via USB
- Voltage divider calibration may need adjustment
- Check BATTERY_ADC_PIN definition

**BLE not advertising:**
- Check boot count increments (RTC memory working)
- Verify no error messages in serial
- Try manual `advertise` command

**Short battery life:**
- Ensure display is disabled: `display off`
- Check sleep is enabled: `sleep on`
- Measure actual current draw
- Look for sensor initialization errors

## Next Steps

Once standalone testing is complete:
1. Test with Node firmware (BLE scanning)
2. Verify node receives and logs advertisements
3. Test end-to-end: Remote Sensor → Node → Gateway
