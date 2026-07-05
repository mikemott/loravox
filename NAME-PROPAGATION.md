# Name Propagation (Phase 2)

## Overview

Custom names now propagate from the gateway to nodes via LoRa. When you rename a node through the dashboard, the gateway sends a `NAME_UPDATE` message to the node, which stores it persistently and displays it on its OLED.

---

## How It Works

### User Workflow

1. **User renames node** via dashboard: "Node 2" → "Kitchen"
2. **Gateway saves name** to its NVS storage
3. **Gateway sends NAME_UPDATE** message over LoRa to Node 2
4. **Node receives and saves** custom name to its NVS storage
5. **Node OLED updates** to display "Kitchen" instead of "Node #2"

### Technical Flow

```
Dashboard (HTTP POST)
    ↓
Gateway handleRename()
    ↓
Gateway NVS Storage (persistent)
    ↓
sendNameUpdate() via LoRa
    ↓
Node handleNameUpdate()
    ↓
Node NVS Storage (persistent)
    ↓
Node OLED Display Updated
```

---

## Protocol Details

### NAME_UPDATE Message

**Type:** `0x0D`

**Format:**
```
Header (10 bytes):
  - version: 0x01
  - type: 0x0D (NAME_UPDATE)
  - flags: 0x00
  - src_id: Gateway ID (0x0001)
  - dst_id: Target node ID
  - seq: Sequence number
  - payload_len: Name length (1-32)

Payload (1-32 bytes):
  - UTF-8 encoded name string (not null-terminated in packet)
```

**Example:**
```
Rename Node 2 to "Kitchen":

Header:
  01 0D 00 00 01 00 02 00 42 07

Payload:
  4B 69 74 63 68 65 6E  (ASCII: "Kitchen")
```

---

## Gateway Implementation

### sendNameUpdate()

Located in `src/main.cpp` after `sendAck()`:

```cpp
void sendNameUpdate(uint16_t node_id, const char* name) {
  delay(ACK_TURNAROUND_DELAY_MS);

  Packet pkt;
  pkt.type = MessageType::NAME_UPDATE;
  pkt.src_id = GATEWAY_ID;
  pkt.dst_id = node_id;
  pkt.seq = tx_seq++;

  size_t name_len = strlen(name);
  if (name_len > 32) name_len = 32;
  pkt.payload_len = name_len;
  memcpy(pkt.payload, name, name_len);

  if (sendPacket(pkt)) {
    Serial.printf("NAME_UPDATE sent to node %u: %s\n", node_id, name);
  }
}
```

**Called from:** `handleRename()` after saving name to gateway NVS

**Serial Output:**
```
Node 2 renamed to: Kitchen
NAME_UPDATE sent to node 2: Kitchen
```

---

## Node Implementation

### Custom Name Storage

**Global Variable:**
```cpp
char custom_name[33] = "";  // 32 chars + null terminator
```

**Load on Boot:**
```cpp
bool loadConfig() {
  prefs.begin("lorapnp", true);
  // ... other config ...
  prefs.getString("custom_name", custom_name, sizeof(custom_name));
  prefs.end();
  return network_id != 0 && node_id != UNASSIGNED_ID;
}
```

**Save when Updated:**
```cpp
void saveConfig() {
  prefs.begin("lorapnp", false);
  // ... other config ...
  prefs.putString("custom_name", custom_name);
  prefs.end();
}
```

### handleNameUpdate()

Located in `node/src/main.cpp`:

```cpp
void handleNameUpdate(const Packet& pkt) {
  if (pkt.dst_id != node_id) return;
  if (pkt.payload_len == 0 || pkt.payload_len > 32) return;

  memset(custom_name, 0, sizeof(custom_name));
  memcpy(custom_name, pkt.payload, pkt.payload_len);
  custom_name[pkt.payload_len] = '\0';

  Serial.printf("NAME_UPDATE received: %s\n", custom_name);
  saveConfig();

  updateDisplay(custom_name[0] ? custom_name : ("Node #" + String(node_id)).c_str(),
                "Name updated",
                "Saved!");
}
```

**Registered in:** `handlePacket()` for `MessageType::NAME_UPDATE`

**Serial Output:**
```
RX: NAME_UPD src=1 dst=2 seq=42
NAME_UPDATE received: Kitchen
```

### OLED Display Update

**updateStatusDisplay() - Line 1:**

```cpp
void updateStatusDisplay() {
  char line1[32], line2[32], line3[32];

  // Line 1: Custom name or Node ID
  if (node_id == UNASSIGNED_ID) {
    snprintf(line1, sizeof(line1), "Node: Searching...");
  } else if (custom_name[0] != '\0') {
    snprintf(line1, sizeof(line1), "%s", custom_name);
  } else {
    snprintf(line1, sizeof(line1), "Node #%u", node_id);
  }

  // ... rest of display code ...
}
```

**Display Priority:**
1. If not joined: "Node: Searching..."
2. If custom name exists: Show custom name
3. Fallback: "Node #X"

---

## Example Session

### Dashboard Rename Action

**User clicks:** "Node 2" badge → Enter "Kitchen"

**Gateway Console:**
```
Node 2 renamed to: Kitchen
NAME_UPDATE sent to node 2: Kitchen
```

**Node 2 Console:**
```
RX: NAME_UPD src=1 dst=2 seq=42
NAME_UPDATE received: Kitchen
```

**Node 2 OLED (before):**
```
Node #2
GW: OK -75dBm 8.5dB
Sensors: 2
```

**Node 2 OLED (after):**
```
Kitchen
Name updated
Saved!
```

**Node 2 OLED (after auto-refresh):**
```
Kitchen
GW: OK -75dBm 8.5dB
Sensors: 2
```

---

## Persistence

### Gateway Storage

**NVS Key:** `name_n{node_id}`
- Example: `name_n2` → "Kitchen"
- Survives gateway reboots
- Survives firmware updates (NVS partition preserved)

### Node Storage

**NVS Key:** `custom_name`
- Stored alongside other node config
- Survives node reboots
- Survives firmware updates (NVS partition preserved)
- Cleared by factory reset

---

## Edge Cases

### Node Offline When Renamed

**Scenario:** User renames node while it's powered off or out of range

**Behavior:**
- Gateway saves name immediately
- NAME_UPDATE message sent but not received
- Dashboard shows new name (from gateway)
- Node still shows old name on OLED (not updated)

**Resolution:**
- When node comes back online, it still has old name
- User must rename again (gateway will resend NAME_UPDATE)
- **Future Enhancement:** Queue pending name updates for offline nodes

### Name Too Long

**Max Length:** 32 characters

**Truncation:**
- Gateway: Truncates at 32 chars before sending
- Node: Validates payload_len ≤ 32, rejects if longer

**Dashboard Validation:**
- JavaScript validates max 32 chars before POST
- Backend validates max 32 chars in `handleRename()`

### Special Characters

**Supported:** UTF-8 (all characters)

**OLED Display Limits:**
- OLED driver may not render all UTF-8 chars
- Stick to ASCII for best compatibility
- Emojis may not display correctly on OLED

### Factory Reset

**Gateway:**
- Custom names stored in separate NVS keys
- Not cleared by typical operations
- Manual clear required: `prefs.clear()`

**Node:**
- Factory reset clears all NVS including `custom_name`
- Node reverts to showing "Node #X"
- Gateway still has name stored
- Rename again to push name back to node

---

## Troubleshooting

### Node doesn't update name

**Check:**
1. Node is powered and joined to network
2. Gateway serial shows "NAME_UPDATE sent"
3. Node serial shows "NAME_UPDATE received"
4. Check LoRa signal quality (RSSI/SNR)

**Debug:**
```bash
# Gateway serial:
tail -f /dev/ttyUSB0 | grep NAME_UPDATE

# Node serial:
tail -f /dev/ttyUSB1 | grep NAME_UPDATE
```

### OLED still shows "Node #X"

**Possible causes:**
1. NAME_UPDATE not received (check serial)
2. Name cleared from NVS (factory reset)
3. OLED refresh hasn't triggered yet (wait 10s)

**Fix:**
- Rename node again via dashboard
- Power cycle node to reload from NVS
- Check node serial for "NAME_UPDATE received"

### Different names on gateway vs node

**Sync Issue:**
- Gateway and node NVS are independent
- Network partition or power loss can cause desync
- Rename via dashboard forces resync

**Resolution:**
1. Check both serial consoles
2. Rename via dashboard (authoritative source is gateway)
3. Verify NAME_UPDATE sent and received

---

## Performance Impact

**Gateway:**
- Storage: +40 bytes per named node
- Network: +20 bytes LoRa packet per rename
- CPU: Negligible (one-time send)

**Node:**
- Storage: +40 bytes NVS
- Network: +20 bytes LoRa packet received
- CPU: Negligible (one-time save)
- OLED: No performance impact (same rendering)

---

## Future Enhancements

### Phase 3 Ideas

**Bidirectional Naming:**
- Node requests current name on join
- Gateway pushes name automatically after JOIN_ACCEPT

**Bulk Name Updates:**
- Send all names to node as table
- Node stores multiple names (for other nodes)
- Useful for multi-hop networks

**Name Acknowledgement:**
- Node sends NAME_ACK back to gateway
- Gateway retries if no ACK received
- Ensures delivery even in poor signal conditions

**Name Broadcast:**
- Broadcast names to all nodes
- Nodes can display neighbor names
- Better for multi-hop mesh networks

---

## Summary

✅ **Custom names propagate** from gateway to nodes via LoRa
✅ **Persistent storage** on both gateway and node (NVS)
✅ **OLED displays** custom names instead of "Node #X"
✅ **Automatic updates** when renamed via dashboard
✅ **Survives reboots** on both devices
✅ **One-way sync** from gateway to nodes (authoritative)

**Protocol:** NAME_UPDATE message (0x0D)
**Storage:** ESP32 NVS on both gateway and node
**Display:** Node OLED updates immediately on receipt
**Persistence:** Survives reboots and firmware updates

Ready to deploy! Rename nodes via the dashboard and watch the names appear on the node OLEDs in real-time.
