# Rename Feature

## Overview

You can now rename nodes, sensors, and BLE remote sensors through the dashboard. Custom names are stored persistently on the gateway and survive reboots.

---

## How to Rename

### From Dashboard (Recommended)

1. **Open Dashboard:** http://lorapnp.local
2. **Click any item** to rename:
   - **Node badge** (green "Node 2" badge)
   - **Sensor name** (e.g., "Temperature", "Humidity")
   - **BLE sensor name** (e.g., "84D109...")
3. **Enter new name** in the prompt dialog
4. **Press OK** to save

Changes apply immediately and persist across gateway reboots.

### Examples

**Rename Node:**
- Click "Node 2" → Enter "Kitchen" → Now shows as "Kitchen"

**Rename Sensor:**
- Click "Temperature" → Enter "Fridge Temp" → Now shows as "Fridge Temp"

**Rename BLE Sensor:**
- Click "84D109..." → Enter "Outdoor Sensor" → Now shows as "Outdoor Sensor"

---

## Visual Indicators

- **Clickable items** show `cursor:pointer` (hand icon on hover)
- **Tooltip** appears: "Click to rename"
- **Badge background** remains colored even with custom names
- **Custom names** replace default names everywhere in the dashboard

---

## API Usage

### Rename Node

```bash
POST /api/rename
Content-Type: application/json

{
  "type": "node",
  "node_id": 2,
  "name": "Kitchen"
}
```

### Rename Sensor

```bash
POST /api/rename
Content-Type: application/json

{
  "type": "sensor",
  "node_id": 2,
  "sensor_id": 1,
  "name": "Fridge Temp"
}
```

### Rename BLE Sensor

```bash
POST /api/rename
Content-Type: application/json

{
  "type": "ble",
  "uid": "84:D1:09:13:CF:D0",
  "name": "Outdoor Sensor"
}
```

### Response

**Success:**
```json
{
  "success": true
}
```

**Error:**
```json
{
  "error": "Invalid name"
}
```

---

## Storage Details

### Gateway Preferences Storage

Names are stored in ESP32 NVS (Non-Volatile Storage) with these keys:

| Type | Key Format | Example |
|------|------------|---------|
| Node | `name_n{node_id}` | `name_n2` → "Kitchen" |
| Sensor | `name_s{node_id}_{sensor_id}` | `name_s2_1` → "Fridge Temp" |
| BLE | `name_b{uid_hex}` | `name_b84D10913CFD0` → "Outdoor" |

**Storage Limits:**
- Max name length: 32 characters
- Persists across gateway reboots
- Survives firmware updates (NVS partition preserved)

---

## JSON API Changes

The `/api` endpoint now includes custom names:

**Before:**
```json
{
  "nodes": [
    {
      "id": 2,
      "sensors": [
        {"id": 1, "label": "Temp", "type": 1}
      ],
      "ble_sensors": [
        {"uid": "84:D1:09:13:CF:D0"}
      ]
    }
  ]
}
```

**After:**
```json
{
  "nodes": [
    {
      "id": 2,
      "name": "Kitchen",
      "sensors": [
        {
          "id": 1,
          "label": "Temp",
          "custom_name": "Fridge Temp",
          "type": 1
        }
      ],
      "ble_sensors": [
        {
          "uid": "84:D1:09:13:CF:D0",
          "name": "Outdoor Sensor"
        }
      ]
    }
  ]
}
```

**Fields:**
- `node.name`: Custom node name (defaults to "Node {id}")
- `sensor.custom_name`: Custom sensor name (defaults to label or type name)
- `ble_sensor.name`: Custom BLE sensor name (defaults to UID prefix)

---

## Use Cases

### Home Monitoring
```
Node 2 → "Living Room"
Node 3 → "Bedroom"
Node 4 → "Garage"

Sensor 1 → "Room Temp"
Sensor 2 → "Humidity"
```

### Greenhouse
```
Node 2 → "Greenhouse A"
Node 3 → "Greenhouse B"

Sensor 1 → "Soil Moisture"
Sensor 2 → "Air Temp"
BLE 84D109... → "Outdoor Weather"
```

### Office Deployment
```
Node 2 → "Conference Room"
Node 3 → "Server Room"
Node 4 → "Reception"

Sensor 1 → "CO2 Level"
Sensor 2 → "Temperature"
```

---

## Limitations

### Current Implementation

- **No bulk rename:** Must rename items individually
- **No export/import:** Names stored locally on gateway only
- **No undo:** Changes are immediate and persistent
- **Names not pushed to nodes:** Nodes still see default names on OLED

### Future Enhancements (Optional)

**Phase 2 - Name Propagation:**
- New protocol message: `NAME_UPDATE`
- Gateway pushes custom names to nodes
- Nodes display custom names on OLED
- Example: Node shows "Kitchen" instead of "Node #2"

**Phase 3 - Bulk Management:**
- CSV export/import for names
- Bulk rename via web UI
- Name templates (e.g., "Node {id} - {location}")

---

## Troubleshooting

### Names don't persist after reboot

**Check:**
1. Gateway has enough NVS storage (should be automatic)
2. Serial console shows "Node X renamed to: Y"
3. Try factory reset if NVS is corrupted: `reset` command

### Name changes don't appear immediately

**Solution:**
- Dashboard auto-refreshes every 3 seconds
- Force refresh browser (Cmd+R or Ctrl+R)
- Check serial console for errors

### Special characters in names

**Supported:**
- Letters (A-Z, a-z)
- Numbers (0-9)
- Spaces
- Hyphens, underscores

**Not recommended:**
- Quotes (`'` or `"`) - may break JavaScript
- HTML characters (`<`, `>`, `&`) - may break rendering

---

## Serial Console Messages

When renaming, you'll see:

```
Node 2 renamed to: Kitchen
Node 2 sensor 1 renamed to: Fridge Temp
BLE sensor 84D10913CFD0 renamed to: Outdoor
```

These confirm the rename was saved to NVS successfully.

---

## Performance Impact

**Storage:**
- Each name: ~40 bytes NVS
- 100 named items: ~4KB total
- ESP32 NVS: 512KB available

**Network:**
- Rename request: ~100 bytes
- JSON API size increase: ~30 bytes per custom name
- Dashboard render: No measurable impact

---

## Example Workflow

**Setting up a new deployment:**

1. Flash gateway firmware
2. Power on nodes and let them join
3. Open dashboard: http://lorapnp.local
4. Rename gateway nodes:
   - "Node 2" → "Kitchen"
   - "Node 3" → "Living Room"
5. Rename sensors with meaningful names:
   - "Temp" → "Kitchen Temp"
   - "Humidity" → "Kitchen Humidity"
6. Rename BLE sensors:
   - "84D109..." → "Outdoor Weather Station"
7. Names persist forever - deployment complete!

**Updating names later:**
- Just click and rename - no code changes needed
- Changes sync immediately to all connected dashboards
- Perfect for reorganizing spaces or adding new sensors

---

## Summary

✅ **Click-to-rename** from dashboard
✅ **Persistent storage** in gateway NVS
✅ **Real-time updates** across all clients
✅ **API available** for automation
✅ **Works for nodes, sensors, and BLE sensors**
✅ **No code changes required** to add/update names

Ready to make your sensor network more user-friendly!
