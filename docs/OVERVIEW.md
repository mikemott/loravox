# LoRaVox — Project Overview

> **Drop-and-go remote monitoring for places the internet doesn't reach.**

LoRaVox is a first-party, turnkey **sensor ecosystem** for remote monitoring where
there is no connectivity at the sensor. A smart gateway bridges a self-organizing
network of field devices and sensors to the internet and to Grafana-based dashboards. The
whole system is designed around one promise: a network is **easy to deploy** and
**easy to monitor**.

---

## The problem

Monitoring remote or hard-to-reach assets today forces a bad trade-off:

- **LoRaWAN** delivers range, but demands a network server, an application server, and
  per-device provisioning — an IT project before the first reading.
- **Cellular IoT sensors** are simple, but every device needs its own SIM and
  subscription, and coverage is spotty in rural areas.
- **Cloud IoT platforms** lock your data behind a vendor and assume always-on internet.
- **None** of them work well when there is simply *no connectivity at the sensor itself*
  — the common case on a large farm, a remote facility, or across a property portfolio.

LoRaVox exists to make remote monitoring **drop-and-go**: sophisticated sensing,
deployed anywhere, owned by the customer.

---

## Who it's for

SMB operators in **agriculture, manufacturing, and property maintenance** who need eyes
on remote or hard-to-reach assets.

- **Delivered turnkey** — installed by integrators, with **custom configuration** as a
  value-add service.
- *Easy to deploy* is for the **integrator** (fast, repeatable installs).
- *Easy to monitor* is for the **operator** (dashboards, history, alerts).

**Example deployments**

| Vertical | What gets monitored |
|---|---|
| **Farm** | Gate sensors, livestock tracking, tank & feed levels, GPS asset tracking |
| **Manufacturing** | Equipment health — mmWave radar for vibration, mic arrays for acoustic anomaly detection, AI cameras that infer at the edge and send only telemetry |
| **Property** | Freeze/leak detection, generator & backup-power status, access/gate, remote-building conditions across sites |

---

## Architecture

Two device types, an adaptive transport layer, and a data/dashboard layer.

### The two device types

- **Gateway — the core.** A WiFi/LTE internet bridge with on-board processing. It
  aggregates telemetry, manages the network, handles alerting, and either serves local
  dashboards (local tier) or feeds the cloud (cloud tier).
- **Field devices — everything else.** A single device type described by its
  *capabilities*, not a fixed role:
  - **Hosts sensors** — directly-attached I2C sensors and/or an edge-AI payload
    (cameras, mmWave radar, mic arrays). Field devices report **telemetry and
    inferences — never raw data.**
  - **Relays** (optional) — forwards other devices' data toward the gateway and
    extends coverage across a property. A relaying field device is what we used to
    call a *station*; the dashboard marks it with a **Relay** tag. Relay is
    independent of power — a battery device can relay.
  - **Transport** — WiFi, BLE, and/or LoRa, chosen adaptively (below).
  - **Power** — mains or battery.

  The old *station* vs *remote sensor* labels are just two capability presets over
  this one type: a station is a field device with relaying on; a remote sensor is a
  battery-first leaf that only reports its own readings.

### Adaptive transport — the plug-and-play magic

Every device connects back over the **best available link: WiFi → BLE → LoRa.**

- Near infrastructure → **WiFi**.
- Short hop to a neighbor → **BLE**.
- Long / remote link → **LoRa** (the reach-extender of last resort).

Drop a device anywhere and it finds its own path home — no network design, no manual
channel planning. This is the *network-selection layer*, and it is what makes the
ecosystem genuinely plug-and-play.

### Data & dashboards

Telemetry flows to the gateway and on to **Grafana** for historical data and analysis.

---

## Product tiers

| | **Cloud** (subscription) | **Local** (on-gateway) |
|---|---|---|
| Storage & Grafana | Hosted | On the gateway |
| Analysis / history / retention | Full | Reduced |
| Remote access & alerting | Yes | Limited |
| Data ownership | Cloud-hosted | Stays on-prem |
| Cost model | Recurring | No subscription |

The local tier must be fully functional **without** the cloud — own-your-data is a
first-class option, not a downgrade.

---

## Value proposition

- **Deploy anywhere.** Best-available transport means it works even with no connectivity
  at the sensor.
- **Truly easy.** Plug-and-play, self-describing sensors; turnkey installs for
  integrators; adding a new sensor type is easy by design.
- **Own your hardware.** First-party gear; the cloud is optional, not required.
- **Sophisticated sensing over a tiny pipe.** Edge intelligence (AI cameras, radar, mic
  arrays) sends *inferences*, so rich monitoring fits within LoRa's constraints.

**Versus the alternatives**

- **vs LoRaWAN** — no network server, no join ceremony, no provisioning project.
- **vs cellular IoT sensors** — no per-device SIM or subscription; one backhaul point per
  site; longer range.
- **vs Meshtastic** — telemetry and equipment intelligence, not messaging.

---

## Business model

First-party hardware (gateways, field devices, sensors) + a growing **catalog of sensor
types** + **custom configurations**, sold **turnkey through integrators**, with a
**cloud subscription** for recurring revenue. The local tier is the on-prem,
own-your-data option.

---

## Design principles

1. **LoRa reality first.** Tiny bandwidth and duty-cycle limits mean sensors push
   telemetry and edge inferences, never raw data.
2. **Plug-and-play.** Self-describing sensors, auto-discovery/join, zero provisioning;
   turnkey for the integrator.
3. **Own-your-data option.** The local tier works fully without the cloud.
4. **Secure by default.** Authenticated setup, signed firmware updates, and
   (on the roadmap) authenticated LoRa links.

---

## Current state vs. vision

An honest snapshot — the vision above is the destination; here is where the
implementation stands today.

**Built today** (prototype on ESP32-S3 / LilyGO T3-S3 + SX127x LoRa):

- Device protocol: auto-discovery/join, reliable messaging (ACK/retry),
  config-over-the-air, deep sleep, click-to-rename.
- Remote sensors over BLE; relaying field devices bridge BLE→LoRa in a single-hop star.
- Local web dashboard, mDNS zero-config, MQTT/HTTP output.
- Gateway WiFi self-update with signed firmware, first-boot setup wizard, and
  Digest-authenticated web API.

**In progress / planned** (the gap between today's code and the vision):

- **Network-selection layer** (WiFi→BLE→LoRa adaptive) — foundation exists, not yet
  integrated.
- **Relay / multi-hop** range extension (today is single-hop).
- **Real plug-and-play sensor auto-detect** — driver-registry I2C detection is now
  integrated on the field device (SHT40/BMP581, unknown-device reporting); pending
  on-hardware verification.
- **Grafana + time-series storage** and the cloud/local tier split.
- **Gateway→device OTA** across the network, including delta updates (delta currently
  unimplemented).
- **Authenticated LoRa link** (control packets are currently unauthenticated).
- **LTE gateway** variant and the **edge-AI sensor line** (cameras / radar / mic arrays).
