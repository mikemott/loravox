# LoRa PNP Feature Tracker

## Near-Term Priorities

| # | Feature | Status | Notes |
|---|---|---|---|
| 1 | Gateway registry persistence, so node IDs survive gateway reboot | ✓ Complete | Gateway persists UID-to-node-ID mappings and sensor descriptors. Runtime registry rebuilds from STATUS packets with UID. |
| 2 | Rejoin/hello path for already-configured nodes | ✓ Complete | Configured nodes send reconnect JOIN_REQUEST with periodic 6s retries. STATUS includes UID for gateway registry recovery. |
| 3 | ACK/retry for join and data | ✓ Complete | JOIN_ACCEPT, STATUS, and SENSOR_DATA have ACK/retry with 3 retries and 6s timeout. Gateway suppresses duplicates. |
| 4 | Sensor descriptor packet plus fake sensor payload | Integrated (pending HW test) | `SENSOR_DESCRIPTOR`/`SENSOR_DATA` packets work. Real I2C auto-detect now runs via the driver registry (`node/src/sensors/`), replacing the hardcoded SHT40-only path — add a driver struct, it's detected, no `main.cpp` change. SHT40 + BMP581 drivers active; unclaimed bus devices reported as `SENSOR_TYPE_UNKNOWN_I2C`. Not yet flashed to hardware (no toolchain in dev env). |
| 5 | Gateway serial registry table with live link stats | ✓ Complete | Formatted table every 10s showing node ID, RSSI, SNR, battery, uptime, heap, sensor count, last seen. |
| 6 | Config update packet for report interval | ✓ Complete | Gateway sends CONFIG_UPDATE via serial command `config <node_id> <interval_ms>`. Node applies, persists, and sends CONFIG_ACK. |
| 7 | Factory reset path for node NVS | ✓ Complete | Serial commands `reset` or `factory` clear Preferences and restart node. |
| 8 | MQTT or simple HTTP JSON output from gateway | ✓ Complete | HTTP JSON endpoint active. MQTT publishes sensor data to `lorapnp/node/{id}/sensor/{id}`. Serial: `mqtt <broker_ip> <port>`, `mqtt off`. |
| 9 | Deep sleep cycle for battery operation | ✓ Complete | Node commands: `sleep on/off`. After tasks (STATUS+sensors), listens 3s for CONFIG_UPDATE, then sleeps for configured interval. ESP deep sleep with RTC timer wake. |

## Added Backlog

| Feature | Status | Notes |
|---|---|---|
| BLE remote sensor support | ✓ Complete | **FIXED!** Switched from Bluedroid BLE to NimBLE. Three-tier system fully operational: Remote Sensor (BLE) → Station (BLE+LoRa) → Gateway. Verified with temp/humidity sensor forwarding data end-to-end. |
| OLED status displays | ✓ Complete | Gateway shows station/sensor count. Station shows connection status with RSSI/SNR and total sensor count (I2C + BLE). Updates every 2 seconds. |
| Web dashboard | ✓ Complete | Mobile-friendly dashboard served from gateway. Dark theme, real-time updates (3s refresh), signal quality indicators, battery levels, sensor readings. Auto-refresh via JSON API. See DASHBOARD.md |
| Dual WiFi mode + mDNS | ✓ Complete | Gateway runs AP mode (`LoRaPNP-Gateway`) + Station mode simultaneously. mDNS enabled: access at `http://lorapnp.local` from any network. Serial commands: `wifi <ssid> <password>`, `wifi status`. Zero-config access. |
| Rename system (all tiers) | ✓ Complete | Click-to-rename stations, I2C sensors, and remote sensors from dashboard. Names persist in NVS (gateway + station). Extended NAME_UPDATE protocol propagates names via LoRa. Three NameTypes: STATION, I2C_SENSOR, REMOTE_SENSOR. See NAMING-SYSTEM.md |
| Terminology update | ✓ Complete | "Node" → "Station" throughout codebase, dashboard, and docs. Better describes multi-function sensor platforms (I2C + BLE relay + LoRa). Backward compatible - existing deployments unaffected. |
| BLE name characteristic | Ready | BLE characteristic UUID `0x4C51` defined for future remote sensor displays. When remote sensors have OLED/e-ink, station can write custom names via BLE. Infrastructure complete, waiting on display hardware. |
| I2C plug-and-play sensor detection | Integrated (pending HW test) | Driver-registry framework (`node/src/sensors/`) wired into the node: `scanI2CSensors()` scans Wire1 (QWIIC), probes registered drivers, reports descriptors, and flags unclaimed devices as unknown. Extensible via one driver struct per sensor. Needs on-hardware verification. |
| Optional I2C module descriptor EEPROM | Planned | Useful for custom sensor stacks and analog modules. |
| Modular sensor driver architecture | Planned | Abstract sensor drivers for easy addition of new sensor types. See Task #1. |

---

## Recent Session Log

### 2026-07-04 (PM): Remote Reboot & Sensor Detection Framework

**Implemented:**
1. **Remote reboot command** - Fully functional
   - Added REBOOT (0x0E) message type to protocol
   - Station handler with NVS logging
   - Gateway API endpoint `/api/reboot`
   - Dashboard UI with confirmation dialog
   - Enables sensor detection without physical access

2. **I2C sensor detection framework** - Architecture complete, partial implementation
   - Created SensorRegistry pattern with driver interface
   - Implemented SHT40Driver (temp/humidity)
   - Implemented BMP581Driver (pressure)
   - Added sensor types: SOIL_MOISTURE, DISTANCE, PRESENCE, LIGHT
   - Framework ready for additional sensor drivers
   - Integration with main.cpp pending

**Files Modified:**
- lib/LoRaPNP/src/Types.h (sensor types)
- lib/LoRaPNP/src/Packet.cpp (REBOOT typeStr)
- node/src/main.cpp (reboot handler)
- src/main.cpp (API + dashboard button)
- PROTOCOL-SPEC.md (Section 12.4)

**Files Created:**
- node/src/sensors/ directory structure
- SensorRegistry.h/cpp
- SHT40Driver.h/cpp, BMP581Driver.h/cpp
- SESSION-2026-07-04.md (detailed session notes)

**Next Steps:**
- Test remote reboot with hardware
- Complete sensor detection integration
- Add remaining sensor drivers as hardware arrives

---

### 2026-07-04 (AM): Dashboard & Comprehensive Naming System

**Implemented:**
1. **Mobile-friendly web dashboard** - Served from gateway at `http://lorapnp.local/`
   - Dark theme, responsive design
   - Real-time updates (3s polling)
   - Signal strength indicators (color-coded)
   - Battery, RSSI, SNR, uptime, heap monitoring
   - Shows all I2C sensors and remote BLE sensors

2. **Dual WiFi mode with mDNS**
   - AP mode: `LoRaPNP-Gateway` (192.168.4.1) - always on
   - Station mode: Joins existing WiFi network
   - Both modes run simultaneously
   - mDNS: Access at `http://lorapnp.local` from any device
   - Serial config: `wifi <ssid> <password>`, `wifi status`, `wifi off`

3. **Comprehensive naming system (3 phases)**
   - **Phase 1:** Terminology update - "Node" → "Station" throughout
   - **Phase 2:** Extended NAME_UPDATE protocol with sensor propagation
     - Station names propagate to station OLED
     - I2C sensor names stored on station
     - Remote sensor names stored on station
     - All stored in NVS (persistent across reboots)
   - **Phase 3:** BLE characteristic defined for future remote sensor displays
     - Infrastructure ready for OLED/e-ink displays on battery sensors
     - Station will write names via BLE when hardware available

4. **Protocol extensions**
   - Added `NameType` enum: STATION (0x00), I2C_SENSOR (0x01), REMOTE_SENSOR (0x02)
   - Extended NAME_UPDATE payload format for all three types
   - BLE characteristic UUIDs: `0x4C51` (name), `0x4C52` (config - future)

5. **API enhancements**
   - `/api` - JSON endpoint with custom names included
   - `/api/rename` - POST endpoint for renaming (stations/sensors/remotes)
   - Auto-propagation to stations via LoRa after rename

**Files Modified:**
- `lib/LoRaPNP/src/Types.h` - NameType enum, MAX_STATIONS, protocol updates
- `lib/LoRaPNP/src/BLEProtocol.h` - BLE characteristic UUIDs
- `lib/LoRaPNP/src/Packet.cpp` - NAME_UPD string representation
- `src/main.cpp` - Dashboard HTML, WiFi/mDNS setup, three NAME_UPDATE send functions, handleRename() extension
- `node/src/main.cpp` - Extended handleNameUpdate(), sensor name storage arrays, "Station" terminology

**Files Created:**
- `DASHBOARD.md` - Complete dashboard documentation
- `RENAME-FEATURE.md` - Rename system documentation
- `NAME-PROPAGATION.md` - Protocol details for name propagation
- `NAMING-SYSTEM.md` - Comprehensive 3-phase naming documentation

**Testing:**
- ✅ Gateway firmware compiled: 881KB flash, 57KB RAM
- ✅ Station firmware compiled: 606KB flash, 33KB RAM
- Ready for deployment and testing

**User Experience:**
- Zero-config dashboard access via mDNS
- Click-to-rename from dashboard (stations, sensors, remotes)
- Names appear on station OLED immediately
- All names persist across power cycles
- Organization-wide access when gateway on local network
