# LoRa Plug-and-Play Sensor Network MVP

## Goal

Develop a working LoRa protocol that allows battery-powered sensor nodes to automatically discover a gateway, join a network, receive configuration, and reliably deliver sensor data with minimal user setup.

The MVP will run entirely on LilyGO T3-S3 LoRa development boards and use the onboard LoRa radio over SPI with RadioLib.

---

# Phase 1 – Establish Basic Communications

### Objectives

* Verify two T3-S3 boards can exchange packets.
* Create a reusable packet format.
* Build a packet encoder/decoder library.

### Deliverables

* RadioLib integrated into project.
* Common packet header implemented.
* CRC verification.
* Sequence numbers.
* Logging over Serial.

### Success Criteria

```
Node -----> Gateway
        Packet received
Gateway --> ACK
```

---

# Phase 2 – Discovery

### Objectives

Implement automatic gateway discovery.

### Node Behavior

1. Boot.
2. Listen briefly.
3. Broadcast DISCOVER.
4. Wait for OFFER.
5. Retry with randomized delay if no response.

### Gateway Behavior

1. Listen continuously.
2. Respond to DISCOVER with OFFER.
3. Ignore duplicate requests.

### Deliverables

* DISCOVER packet
* OFFER packet
* Randomized retry timer
* RSSI/SNR reporting

### Success Criteria

Powering on a new node causes the gateway to recognize it automatically.

---

# Phase 3 – Network Join

### Objectives

Assign permanent network identities.

### Join Process

```
DISCOVER
↓

OFFER
↓

JOIN_REQUEST
↓

JOIN_ACCEPT
```

Gateway assigns:

* Network ID
* Node ID
* Reporting interval
* Radio profile

Node stores configuration in flash (Preferences/NVS).

### Deliverables

* Flash storage
* Automatic reconnect after reboot
* Join timeout handling

### Success Criteria

After reboot, a node reconnects without repeating discovery.

---

# Phase 4 – Reliable Messaging

### Objectives

Implement a lightweight reliability layer.

Features:

* Sequence numbers
* ACK packets
* Retries
* Duplicate packet detection

The gateway acknowledges only packets that require reliable delivery.

### Deliverables

* ACK packet
* Retry timer
* Duplicate suppression
* Lost packet statistics

### Success Criteria

Sensor packets remain reliable even with occasional packet loss.

---

# Phase 5 – Sensor Framework

### Objectives

Create a generic sensor abstraction.

Example interface:

```
Sensor
 ├── Temperature
 ├── Humidity
 ├── Soil Moisture
 ├── GPIO
 ├── Battery
 └── Custom
```

Each sensor reports:

* Sensor ID
* Data type
* Units
* Value
* Timestamp

### Deliverables

* Generic payload format
* Fake sensor for testing
* Battery reporting

### Success Criteria

Adding a new sensor requires only implementing a sensor class.

---

# Phase 6 – Gateway Management

### Objectives

Maintain a live registry of network devices.

Track:

* Node ID
* Device UID
* Last seen
* RSSI
* SNR
* Battery voltage
* Firmware version
* Sensor list
* Join time

Display the table over the serial console.

### Success Criteria

The gateway automatically tracks every active node.

---

# Phase 7 – Configuration Updates

### Objectives

Allow the gateway to change node behavior remotely.

Examples:

* Reporting interval
* Sleep interval
* Enabled sensors
* Radio profile
* Device name

Configuration changes are acknowledged and saved to flash.

### Success Criteria

A node changes its behavior without requiring a firmware update.

---

# Phase 8 – Sleep Cycle

### Objectives

Implement realistic battery-powered operation.

Node behavior:

1. Wake
2. Read sensors
3. Transmit data
4. Listen briefly for commands
5. Return to deep sleep

Initially use a 30-second interval for development, with longer intervals tested later.

### Success Criteria

The node operates unattended through repeated sleep/wake cycles while remaining responsive to configuration updates.

---

# Software Architecture

## Shared Library

Create a reusable library containing:

* Packet definitions
* Radio abstraction
* Serialization
* CRC
* Message handlers
* Discovery
* Retry logic
* Configuration storage

This library should compile for both nodes and gateways.

---

## Gateway Application

Responsibilities:

* Network discovery
* Node registry
* Packet routing
* ACK generation
* Configuration management
* Serial diagnostics

---

## Node Application

Responsibilities:

* Discovery
* Joining
* Sensor collection
* Reliable transmission
* Deep sleep
* Configuration persistence

---

# MVP Completion Criteria

The MVP is complete when the following workflow succeeds without manual configuration:

1. Flash firmware onto a new T3-S3 node.
2. Power on the node.
3. The node discovers the gateway automatically.
4. The gateway assigns a Node ID.
5. The node stores its configuration.
6. The node begins transmitting sensor data.
7. The gateway displays live status for the node.
8. The node survives a reboot and reconnects automatically without repeating the initial join process.
9. The gateway can change the reporting interval remotely, and the node applies the new configuration.

At that point, the core protocol is proven and can be ported to custom hardware with minimal changes.

