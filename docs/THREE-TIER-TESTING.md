# Gateway + Field-Device Testing Guide

> **Note:** The old "three tiers" were relabeled to **two device types** — a **Gateway**
> and **field devices**. The three builds this guide exercises (the gateway, the relaying
> `node` field device, and the leaf `remote_sensor` field device) are the gateway plus two
> field-device capability presets, not three separate device types. The filename is kept
> so other docs' links still resolve.

Complete end-to-end testing for the gateway and its field devices: a leaf remote sensor
forwarding through a relaying field device (`node`) to the Gateway.

## Hardware Setup

**You'll need:**
- 3× LilyGO T3-S3 boards
- 1× SHT40 sensor module (QWIIC)
- 3× USB cables (can only connect 2 at a time)
- Optional: Multimeter for power measurement

## Firmware Variants

| Profile | Directory | USB Port | Role |
|---------|-----------|----------|------|
| **Gateway** | `/` (root) | `/dev/cu.usbmodem21201` | LoRa + WiFi hub |
| **Node** | `node/` | `/dev/cu.usbmodem21101` | LoRa + BLE bridge |
| **Remote Sensor** | `remote_sensor/` | TBD | BLE advertiser |

## Build Commands

```bash
# Gateway (from root)
pio run --target upload
pio device monitor

# Node (from root)
cd node
pio run --target upload
pio device monitor

# Remote Sensor (from root)
cd remote_sensor
pio run --target upload
pio device monitor
```

## Testing Sequence

### Phase 1: Flash and Verify Individual Components

**Step 1.1: Flash Gateway**
```bash
# Connect Gateway T3-S3 to USB
pio run --target upload
pio device monitor
```

**Expected output:**
```
=== LoRa PNP Gateway ===
Loading persisted node mappings...
AP IP: 192.168.4.1
Web server started
Radio init OK
```

Leave gateway running and connected to power.

---

**Step 1.2: Flash Node**
```bash
# Disconnect gateway, connect Node T3-S3
cd node
pio run --target upload
pio device monitor
```

**Expected output:**
```
=== LoRa PNP Node ===
Cold boot
Device UID: XX:XX:XX:XX:XX:XX
Radio init OK
Initializing BLE scanner...
BLE scanner initialized
```

Type `status` to verify:
```
Mode: DISCOVERING Network: 0 Node: 0 Interval: 30000 ms Sleep: 0
```

Leave node running.

---

**Step 1.3: Flash Remote Sensor**
```bash
# Disconnect node, connect Remote Sensor T3-S3
# Attach SHT40 sensor to QWIIC port
cd remote_sensor
pio run --target upload
pio device monitor
```

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

**Test with phone:**
- Install "nRF Connect" app (iOS/Android)
- Scan for BLE devices
- Look for "LoRaPNP" with manufacturer data (29 bytes)
- Verify appears every 30 seconds

**Commands to try:**
```
status           # Show all info
display off      # Save power
sleep off        # Stay awake for testing
advertise        # Manual BLE advertisement
```

### Phase 2: Node Discovery and Join

**Setup:**
- Gateway: Powered and running
- Node: Powered and running
- Remote Sensor: Disconnected for now

**Step 2.1: Node Discovers Gateway**

Watch Node serial output:
```
DISCOVER sent
...
RX: OFFER src=1 dst=65535 seq=X
OFFER accepted: token=XXXX network=1
JOIN_REQUEST sent
...
RX: JOIN_ACCEPT src=1 dst=2 seq=X
JOIN_ACCEPT: network=1 node=2 report=30000 ms
```

Watch Gateway serial output:
```
RX: DISCOVER src=0 dst=65535 seq=X RSSI=-45 SNR=8.2
OFFER sent to XX:XX:XX:XX:XX:XX
RX: JOIN_REQUEST src=0 dst=1 seq=X
JOIN_ACCEPT sent to node 2 seq=X retry=0
ACK received from node 2 for seq=X
```

**Verification:**
- Node display shows: "Node 2 | Joined"
- Gateway display updates every 10s
- Both boards have solid communication

**Gateway registry (every 10s):**
```
=== Node Registry [1/32] ===
ID   RSSI  SNR   Batt  Uptime  Heap   Sensors  LastSeen
---  ----  ----  ----  ------  -----  -------  --------
2    -45   8.2   3.70  120s    250000  0       3s
```

### Phase 3: BLE Remote Sensor → Node → Gateway

**Setup:**
- Gateway: Running
- Node: Joined and scanning
- Remote Sensor: Connect and power on

**Step 3.1: Remote Sensor Advertises**

Remote Sensor serial:
```
--- Sensor Cycle Start ---
Sensor: 23.45°C, 45.2% RH
BLE advertising: 23.45°C, 45.2% RH, 3700 mV
BLE advertising complete
Entering deep sleep for 30 seconds (boot #2)
```

**Step 3.2: Node Receives and Forwards**

Node serial (after ~30s BLE scan):
```
Starting BLE scan for 5 seconds...
BLE sensor found: AA:BB:CC:DD:EE:FF RSSI=-65 sensors=2 battery=3700 mV
Registered BLE sensor: AA:BB:CC:DD:EE:FF (slot 0)
Forwarded BLE AA:BB:CC:DD:EE:FF sensor 1: 23.45 RSSI=-65
Forwarded BLE AA:BB:CC:DD:EE:FF sensor 2: 45.20 RSSI=-65
BLE scan complete: 1 devices found
```

Node commands:
```
ble stats        # Show BLE sensor registry
ble scan         # Force immediate scan
```

**Step 3.3: Gateway Receives BLE Data**

Gateway serial:
```
RX: BLE_SENSOR src=2 dst=1 seq=X RSSI=-45 SNR=8.1
Registered BLE sensor: AA:BB:CC:DD:EE:FF via node 2
BLE_SENSOR_DATA AA:BB:CC:DD:EE:FF via node 2: sensor=1 value=23.45 RSSI=-65 battery=3700 mV
BLE_SENSOR_DATA AA:BB:CC:DD:EE:FF via node 2: sensor=2 value=45.20 RSSI=-65 battery=3700 mV
```

**Gateway registry (10s refresh):**
```
=== Node Registry [1/32] ===
ID   RSSI  SNR   Batt  Uptime  Heap   Sensors  LastSeen
---  ----  ----  ----  ------  -----  -------  --------
2    -45   8.2   3.70  300s    248000  0       2s

=== BLE Remote Sensors [1/16] ===
UID               Node  RSSI  Batt  Sensors  LastSeen
----------------  ----  ----  ----  -------  --------
AA:BB:CC:DD:EE:FF 2     -65   3700  2        8s
```

### Phase 4: MQTT Verification

**Step 4.1: Configure MQTT**

Gateway serial:
```
mqtt 192.168.1.100 1883
```

Or use a public MQTT broker for testing:
```
mqtt test.mosquitto.org 1883
```

**Expected:**
```
MQTT configured: test.mosquitto.org:1883
MQTT connected to test.mosquitto.org:1883
```

**Step 4.2: Subscribe to Topics**

On another computer:
```bash
# Subscribe to all topics
mosquitto_sub -h test.mosquitto.org -t "lorapnp/#" -v

# Or specific BLE sensor
mosquitto_sub -h test.mosquitto.org -t "lorapnp/node/2/ble/+/sensor/+" -v
```

**Expected messages:**
```
lorapnp/node/2/ble/AABBCCDDEEFF/sensor/1 {"value":23.45,"type":2,"unit":1,"rssi":-65,"battery_mv":3700,"source":"ble","timestamp":12345}
lorapnp/node/2/ble/AABBCCDDEEFF/sensor/2 {"value":45.2,"type":3,"unit":2,"rssi":-65,"battery_mv":3700,"source":"ble","timestamp":12346}
```

### Phase 5: HTTP JSON API

**Connect to Gateway WiFi:**
- SSID: `LoRaPNP-Gateway`
- IP: `192.168.4.1`

**Fetch status:**
```bash
curl http://192.168.4.1/

# Or in browser:
# http://192.168.4.1/
```

**Expected JSON:**
```json
{
  "gateway_id": 1,
  "active_nodes": 1,
  "active_ble_sensors": 1,
  "nodes": [
    {
      "id": 2,
      "type": "node",
      "rssi": -45,
      "snr": 8.2,
      "battery": 3.7,
      "sensors": [],
      "ble_sensors": [
        {
          "uid": "AA:BB:CC:DD:EE:FF",
          "rssi": -65,
          "battery_mv": 3700,
          "sensors": [
            {"id": 1, "type": 2, "unit": 1, "value": 23.45},
            {"id": 2, "type": 3, "unit": 2, "value": 45.2}
          ]
        }
      ]
    }
  ]
}
```

### Phase 6: Range Testing

**Test BLE Range (Remote Sensor ↔ Node):**
1. Keep node stationary
2. Move remote sensor away
3. Watch node serial for RSSI values
4. Expected range:
   - 10m: -50 to -60 dBm (excellent)
   - 50m: -65 to -75 dBm (good)
   - 100m: -75 to -85 dBm (marginal)
   - >100m: Connection lost

**Test LoRa Range (Node ↔ Gateway):**
1. Keep gateway stationary
2. Move node away
3. Watch gateway serial for RSSI/SNR
4. Expected range:
   - 100m: -50 to -60 dBm, SNR 8+ (excellent)
   - 500m: -70 to -80 dBm, SNR 5+ (good)
   - 1km+: -90 to -100 dBm, SNR 0-3 (marginal)

### Phase 7: Power Measurement

**Remote Sensor:**
1. Disconnect USB
2. Connect battery (CR2032 or 2xAA)
3. Measure current:
   - Deep sleep: ~5 µA
   - Active: ~50 mA for 150ms
   - Average: ~1.2 mAh/day

**Commands:**
```
display off      # Disable OLED (saves ~0.1 mAh/day)
sleep on         # Enable deep sleep
```

**Battery life calculation:**
- CR2032 (220 mAh): 183 days (6 months)
- 2×AA (2500 mAh): 2083 days (5.7 years)

**Node (optional):**
- Battery powered: ~3 mAh/day with BLE scanning
- 2000 mAh battery: ~656 days (1.8 years)
- USB powered: Unlimited (recommended)

## Troubleshooting

### Remote Sensor Issues

**SHT40 not found:**
```
ERROR: SHT40 not found!
Check: 1) Sensor connected? 2) QWIIC cable? 3) Power?
```
- Verify QWIIC cable connection
- Try different cable
- Check sensor has correct I2C address (0x44)

**BLE not visible:**
- Use nRF Connect app to verify
- Check remote sensor is awake (not in deep sleep)
- Try `sleep off` to keep it awake
- Move closer to phone/node

### Node Issues

**BLE scanner not initializing:**
```
Must be joined with BLE enabled
```
- Wait for node to join gateway first
- Check `status` shows `Mode: JOINED`
- Try `ble on` to re-enable

**Not forwarding BLE data:**
- Check node has joined: `status`
- Force BLE scan: `ble scan`
- Check BLE sensors: `ble stats`
- Verify remote sensor is advertising

### Gateway Issues

**Not receiving BLE_SENSOR_DATA:**
- Check node is joined (registry shows it)
- Check MQTT not filling memory
- Restart gateway if needed

**MQTT not publishing:**
```
mqtt test.mosquitto.org 1883
```
- Check broker address is correct
- Try public broker first
- Verify network connectivity

## Serial Commands Reference

### Gateway
```
config <node> <interval>  - Update node report interval
mqtt <host> <port>        - Configure MQTT broker
mqtt off                  - Disable MQTT
```

### Node
```
status            - Show mode, network, BLE sensor count
ble scan          - Force immediate BLE scan
ble stats         - Show BLE sensor registry
ble on/off        - Enable/disable BLE scanning
i2c scan          - Scan for local I2C sensors
sleep on/off      - Enable/disable deep sleep
factory           - Factory reset
```

### Remote Sensor
```
status            - Show device info and readings
read              - Read sensor now
advertise         - Trigger BLE advertisement
display on/off    - Enable/disable OLED
sleep on/off      - Enable/disable deep sleep
factory           - Reset boot count and restart
```

## Success Criteria

✅ **Phase 1:** All three boards flash and initialize successfully
✅ **Phase 2:** Node discovers and joins gateway automatically
✅ **Phase 3:** Remote sensor data appears on gateway via node
✅ **Phase 4:** MQTT publishes BLE sensor data
✅ **Phase 5:** HTTP JSON API shows complete hierarchy
✅ **Phase 6:** Range meets expectations (100m BLE, 1km+ LoRa)
✅ **Phase 7:** Power consumption validates 6+ month battery life

## Next Steps After Testing

1. **Document results** - Take photos, record RSSI values
2. **Test failure scenarios** - Power cycle, out of range, etc.
3. **Optimize timing** - Adjust scan intervals for power vs latency
4. **Add more sensors** - Test with multiple remote sensors per node
5. **Implement modular sensor drivers** - Task #1 from backlog
6. **Design enclosures** - 3D printable cases for each profile
