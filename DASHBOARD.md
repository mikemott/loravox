# LoRa PNP Web Dashboard

## Features Implemented

✅ **Mobile-Friendly Web Dashboard**
- Dark theme optimized for mobile and desktop
- Real-time sensor data display
- Signal strength indicators (RSSI/SNR)
- Battery level monitoring
- Auto-refresh every 3 seconds
- Shows both I2C sensors and BLE remote sensors

✅ **Dual WiFi Mode**
- Access Point mode (always on): `LoRaPNP-Gateway`
- Station mode (optional): Joins your existing WiFi network
- Both modes run simultaneously for maximum flexibility

✅ **mDNS Discovery**
- Access dashboard at `http://lorapnp.local` from any device on the network
- No IP addresses to remember
- Works on iOS, Android, macOS, Windows, Linux

---

## Quick Start

### 1. First Time Setup (AP Mode)

1. Flash gateway firmware
2. Power on gateway
3. Connect to WiFi network: **LoRaPNP-Gateway** (no password)
4. Open browser to: **http://192.168.4.1** or **http://lorapnp.local**
5. See dashboard with all active nodes and sensors

### 2. Connect to Your Local Network (Station Mode)

**Via Serial Console:**
```bash
wifi YourNetworkName YourPassword
```

Restart gateway. It will now:
- Join your WiFi network
- Still broadcast `LoRaPNP-Gateway` AP as backup
- Be accessible at `http://lorapnp.local` from any device on your network

**Check WiFi Status:**
```bash
wifi status
```

**Disable Station Mode:**
```bash
wifi off
```

---

## Access Methods

| Method | URL | Notes |
|--------|-----|-------|
| mDNS (recommended) | http://lorapnp.local | Works on any network |
| AP Mode | http://192.168.4.1 | Direct connection to gateway |
| Station Mode | http://[gateway-ip] | Get IP from `wifi status` |

**Pro Tip:** Use `http://lorapnp.local` - it works everywhere and you never need to know the IP!

---

## Dashboard Features

### Header Stats
- **Nodes**: Active LoRa nodes connected
- **BLE Sensors**: Remote BLE sensors discovered
- **Total Sensors**: All sensors in the network

### Node Cards
Each node shows:
- **Node ID**: Unique identifier
- **Signal Quality**: Visual RSSI bar (green/yellow/red)
- **Battery**: Voltage level
- **SNR**: Signal-to-noise ratio
- **Uptime**: How long the node has been running
- **Free Heap**: Available memory
- **Sensors**: All attached sensors with live readings
- **BLE Sensors**: Remote sensors forwarded by this node (with 📡 icon)
- **Last Seen**: Seconds since last packet

### Color Coding
- **Green**: Good signal (> -70 dBm)
- **Yellow**: Fair signal (-70 to -85 dBm)
- **Red**: Poor signal (< -85 dBm)
- **Gray badge**: Node offline (> 60s since last packet)

---

## Serial Commands

### WiFi Configuration
```bash
wifi <ssid> <password>      # Connect to WiFi network
wifi off                    # Disable station mode
wifi status                 # Show connection status
```

### MQTT Configuration
```bash
mqtt <broker_ip> <port>     # Configure MQTT broker
mqtt off                    # Disable MQTT
```

### Node Management
```bash
config <node_id> <interval> # Update node reporting interval (ms)
```

---

## JSON API

For programmatic access or custom dashboards:

**Endpoint:** `http://lorapnp.local/api`

**Response:**
```json
{
  "gateway_id": 1,
  "active_nodes": 2,
  "active_ble_sensors": 1,
  "nodes": [
    {
      "id": 2,
      "type": "node",
      "rssi": -75,
      "snr": 8.5,
      "battery": 4.15,
      "uptime_s": 3600,
      "free_heap": 245000,
      "status_flags": 0,
      "last_seen": 5,
      "sensors": [
        {
          "id": 1,
          "type": 1,
          "unit": 1,
          "label": "Temp"
        }
      ],
      "ble_sensors": [
        {
          "uid": "84:D1:09:13:CF:D0",
          "rssi": -62,
          "battery_mv": 4009,
          "last_seen": 15,
          "sensors": [
            {
              "id": 1,
              "type": 1,
              "unit": 1,
              "value": 23.91
            }
          ]
        }
      ]
    }
  ]
}
```

---

## Network Topologies

### Standalone (AP Mode Only)
```
Phone/Laptop
    ↓ WiFi
Gateway (192.168.4.1)
    ↓ LoRa
Nodes + BLE Sensors
```

### Organization Network (Dual Mode)
```
Office WiFi Network (192.168.1.x)
    ↓
Gateway (lorapnp.local)
    ↓ LoRa
Nodes + BLE Sensors

Anyone on office WiFi → http://lorapnp.local
```

### Hybrid (Best of Both)
```
Office WiFi ← Gateway → "LoRaPNP-Gateway" WiFi
              (lorapnp.local)
                  ↓ LoRa
              Nodes + BLE Sensors

Access from:
- Office computers: http://lorapnp.local
- Direct connection: Connect to "LoRaPNP-Gateway" → http://192.168.4.1
- Backup if network down: AP mode still works
```

---

## Troubleshooting

### Can't access dashboard

1. **Check connection:**
   - AP Mode: Connected to `LoRaPNP-Gateway`?
   - Station Mode: On same network as gateway?

2. **Try different URLs:**
   - http://lorapnp.local
   - http://192.168.4.1 (AP mode)
   - Check serial console for gateway IP

3. **Restart gateway:**
   - Power cycle to re-establish connections

### mDNS not working

**iOS/macOS:** Should work automatically
**Android:** Install "BonjourBrowser" app
**Windows:** Install "Bonjour Print Services" from Apple
**Linux:** Install `avahi-daemon`

Fallback: Use IP address directly

### WiFi won't connect

1. Check credentials: `wifi status`
2. Verify SSID has no special characters
3. Check serial output during boot
4. AP mode still works as fallback

### Dashboard shows "No nodes connected"

1. Verify nodes are powered and joined gateway
2. Check serial console on gateway for node registry
3. Wait 10-15 seconds after node boot for discovery

---

## Next Steps

### Phase 2 (Future)
- Web-based WiFi setup page at `/setup`
- Captive portal (auto-opens dashboard when connecting to AP)
- Scan for available WiFi networks from web UI
- Save multiple WiFi networks with priority

### Phase 3 (Future)
- Historical data graphs (Grafana integration)
- Alerts and notifications
- Multi-gateway support with auto-discovery
- Cloud relay for remote access

---

## Performance Notes

- **Dashboard**: Refreshes every 3 seconds
- **Gateway CPU**: ~5% overhead for web server
- **Memory**: ~50KB for web server + mDNS
- **Network**: Minimal bandwidth (JSON ~1-2KB per refresh)

Dashboard works well on:
- ✅ Modern smartphones (iOS, Android)
- ✅ Tablets
- ✅ Desktop browsers
- ✅ Embedded displays with webkit browsers

---

## Example Use Cases

**Home Automation:**
```
Leave gateway on home WiFi → http://lorapnp.local
Access from phone, laptop, home assistant
```

**Greenhouse Monitoring:**
```
Gateway in AP mode (no WiFi needed)
Walk to greenhouse with phone
Connect to LoRaPNP-Gateway → View sensors
```

**Office Deployment:**
```
Gateway on office network
All employees access http://lorapnp.local
IT can monitor from any desk
```

**Field Research:**
```
Gateway on battery + portable router
Researchers access via http://192.168.4.1
No internet connection required
```

---

## Security Notes

**Current Implementation:**
- AP mode has no password (easy first-time setup)
- Station mode uses your network's security
- No authentication on web interface
- Local network only (not exposed to internet)

**For Production:**
- Add AP password via `WiFi.softAP("SSID", "password")`
- Enable web authentication for sensitive deployments
- Use HTTPS if handling sensitive sensor data
- Consider VPN for remote access instead of port forwarding

---

## Summary

✅ Zero-configuration dashboard works out of box
✅ mDNS makes it accessible at memorable URL
✅ Dual WiFi mode supports both standalone and networked use
✅ Mobile-first design works on any device
✅ Real-time updates show live sensor data
✅ Professional look suitable for demos and production

**Access dashboard:** http://lorapnp.local
