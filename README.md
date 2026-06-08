<p align="center">
  <img src="https://files.seeedstudio.com/wiki/SenseCAP/SenseCAP_Indicator/SenseCAP_Indicator_1.png" width="320" alt="SenseCAP Indicator" />
</p>

<h1 align="center">SenseCAP Indicator Starter</h1>

<p align="center">
  <strong>A clean, minimal, AI-friendly ESP-IDF starter for the <a href="https://www.seeedstudio.com/SenseCAP-Indicator-D1-p-5643.html">Seeed SenseCAP Indicator</a></strong>
</p>

<p align="center">
  <a href="https://github.com/Love4yzp/sensecap-indicator-starter/generate">
    <img src="https://img.shields.io/badge/-Use%20this%20template-brightgreen?style=for-the-badge" alt="Use this template" />
  </a>
</p>

<p align="center">
  <img src="https://img.shields.io/badge/ESP--IDF-v5.4+-blue?logo=espressif" alt="ESP-IDF" />
  <img src="https://img.shields.io/badge/Target-ESP32--S3-blue?logo=espressif" alt="Target" />
  <img src="https://img.shields.io/badge/LVGL-v9-e63c80" alt="LVGL" />
  <img src="https://img.shields.io/badge/License-MIT-green" alt="License" />
  <img src="https://img.shields.io/github/stars/Love4yzp/sensecap-indicator-starter?style=social" alt="Stars" />
</p>

---

## What Is This?

A **ready-to-build** starting point for developing on the SenseCAP Indicator — a 4-inch 480x480 touchscreen IoT device powered by **ESP32-S3 + RP2040** dual processors.

Instead of a heavy framework, this starter uses a thin **convention-over-framework** approach: an `app_manager` with `pages/` and `services/`, giving you full control with minimal boilerplate.

### Who Is This For?

- Embedded developers building custom firmware for the SenseCAP Indicator
- Makers who want a clean starting point instead of reverse-engineering the stock firmware
- AI-assisted development — the project ships with structured `CLAUDE.md` and `AGENTS.md` knowledge bases, so Claude Code / Cursor / Copilot can understand the codebase instantly

## Features

- **480x480 RGB LCD** — ST7701S display with LVGL 9 integration via `esp_lvgl_port`
- **Capacitive touch** — FT6336U over I2C, plug-and-play
- **RP2040 coprocessor communication** — UART + COBS framing for sensor data and commands
- **WiFi + BLE provisioning** — one-tap BLE pairing with QR code display
- **Sensor dashboard** — live CO2, temperature, humidity, tVOC from RP2040
- **LoRaWAN (D1L/D1Pro)** — OTAA join, Class A/C, periodic sensor uplink via RadioLib + SX1262
- **LoRa status page** — join state, uplink counter, downlink display, Auto/Manual uplink with interval presets
- **Alert system** — configurable CO2 threshold with buzzer notification
- **NVS settings storage** — persistent configuration across reboots
- **Swipeable page UI** — tileview-based navigation, easy to extend
- **AI-ready** — `CLAUDE.md` + `AGENTS.md` at every layer for instant AI agent onboarding

## Hardware

| Component | Specification |
|-----------|--------------|
| **Board** | [SenseCAP Indicator D1](https://www.seeedstudio.com/SenseCAP-Indicator-D1-p-5643.html) |
| **Main MCU** | ESP32-S3 (Xtensa LX7 dual-core, 240 MHz) |
| **Co-processor** | RP2040 (Arm Cortex-M0+, handles sensors & buzzer) |
| **Display** | 4" IPS 480x480, ST7701S, 16-bit RGB parallel |
| **Touch** | FT6336U capacitive, I2C |
| **IO Expander** | TCA9535 (I2C) |
| **Flash / PSRAM** | 8 MB / 8 MB (Octal) |
| **Connectivity** | Wi-Fi 802.11 b/g/n, Bluetooth 5.0 LE |
| **LoRa (D1L/D1Pro)** | SX1262, TCXO 2.4V, multi-band (EU868/US915/CN470/AS923/AU915) |
| **Sensors (D1Pro)** | SCD41 (CO2), AHT20 (temp/humidity), SGP40 (tVOC) |
| **Others** | Buzzer, button, microSD slot, 2x Grove |

## Quick Start

### Prerequisites

- [ESP-IDF](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/get-started/) **v5.4 or later**
- A SenseCAP Indicator board + USB-C cable

### Build & Flash

```bash
# 1. Clone from template
git clone https://github.com/Love4yzp/sensecap-indicator-starter.git
cd sensecap-indicator-starter

# 2. Set target and build
idf.py set-target esp32s3
idf.py build

# 3. Flash and monitor
idf.py flash monitor
```

> **Full clean rebuild** if you hit issues: `idf.py fullclean && idf.py build`

## Project Structure

```
.
├── CMakeLists.txt
├── sdkconfig.defaults          # PSRAM, BLE, LVGL, partition config
├── partitions.csv              # 8 MB flash layout
├── idf_component.yml           # Top-level dependencies (LVGL 9, esp_lvgl_port)
│
├── components/
│   ├── bsp/                    # Board Support Package (hardware only)
│   │   ├── include/
│   │   │   ├── bsp_indicator.h     # Pin definitions & init API
│   │   │   ├── bsp_display.h       # LCD backlight control
│   │   │   └── bsp_touch.h         # Touch panel API
│   │   └── src/
│   │       ├── bsp_init.c          # I2C + IO expander init
│   │       ├── bsp_display.c       # RGB LCD panel setup
│   │       ├── bsp_touch.c         # FT6336U touch driver
│   │       ├── bsp_io_expander.c   # TCA9535 expander
│   │       └── bsp_button.c        # Physical button
│   └── cobs/                   # COBS framing library (standalone)
│       ├── include/cobs.h
│       └── src/cobs.c
│
├── main/
│   ├── main.c                  # Boot sequence & wiring
│   ├── lv_port.c/h             # LVGL 9 display + touch integration
│   └── app/
│       ├── app_manager.c/h     # Page container + sensor data routing
│       ├── pages/
│       │   ├── page_hello.c/h      # Welcome / getting started page
│       │   ├── page_sensor.c/h     # Live sensor dashboard + alert banner
│       │   ├── page_settings.c/h   # Brightness, provisioning, system info
│       │   └── page_lora.c/h       # LoRa status + Auto/Manual uplink (D1L/D1Pro)
│       └── services/
│           ├── svc_rp2040.c/h      # RP2040 UART + COBS protocol
│           ├── svc_wifi.c/h        # WiFi + BLE provisioning
│           ├── svc_storage.c/h     # NVS settings persistence
│           ├── svc_alerts.c/h      # CO2 threshold alert + buzzer
│           ├── svc_lorawan.cpp/h   # LoRaWAN OTAA via RadioLib (Kconfig-gated)
│           └── indicator_hal.h     # SX1262 HAL for IO Expander + bit-bang SPI
│
├── CLAUDE.md                   # AI agent instructions (root)
└── AGENTS.md                   # AI agent knowledge base (root)
```

## Architecture

```
┌─────────────────────────────────────────────────────┐
│                      main.c                         │
│         nvs → bsp → lvgl → services → app           │
└────────────┬────────────────────────┬───────────────┘
             │                        │
     ┌───────▼───────┐      ┌────────▼────────┐
     │  app_manager  │      │    services      │
     │  ┌──────────┐ │      │  svc_rp2040      │
     │  │  pages   │ │◄─────│  svc_wifi        │
     │  │ tileview │ │      │  svc_lorawan     │
     │  └──────────┘ │      │  svc_storage     │
     │               │      │  svc_alerts      │
     └───────────────┘      └─────────────────┘
             │                        │
     ┌───────▼───────┐      ┌────────▼────────┐
     │   LVGL 9      │      │      BSP        │
     │ (pure UI)     │      │  (hardware)     │
     └───────────────┘      └─────────────────┘
```

**Data flow:**
```
RP2040 sensors → UART → svc_rp2040  → app_manager → page_sensor.update()
WiFi events    → svc_wifi            → app_manager → settings UI timer
LoRa events    → svc_lorawan → main  → app_manager → page_lora (via LVGL timer)
User settings  → page_settings       → callbacks   → svc_storage / svc_rp2040
```

## Extending the Starter

### Add a New Page

1. Create `main/app/pages/page_mypage.c` and `.h`:

```c
// page_mypage.h
#pragma once
#include "lvgl.h"
void page_mypage_create(lv_obj_t *parent);
void page_mypage_update(const sensor_data_t *data);  // optional
```

2. Register it in `app_manager.c`:

```c
static const page_entry_t s_pages[] = {
    // ... existing pages ...
    { "My Page", page_mypage_create, page_mypage_update },
};
```

That's it. The tileview and data routing are handled automatically.

### Add a New Service

1. Create `main/app/services/svc_myservice.c` and `.h`
2. Call `svc_myservice_init()` in `main.c`
3. Push data to UI via `app_manager_update_sensor()` or custom callbacks

### Conventions

| Rule | Rationale |
|------|-----------|
| Pages **never** include BSP headers | Keep UI portable and testable |
| Services can access BSP | They bridge hardware and app logic |
| All LVGL calls wrapped in `lv_port_lock()`/`unlock()` | Thread safety |
| COBS is a standalone component | Reusable, not coupled to BSP |

## RP2040 Communication Protocol

The ESP32-S3 communicates with the RP2040 coprocessor over UART (115200 baud) using COBS-framed packets:

| Packet Type | Code | Direction |
|-------------|------|-----------|
| `ACK` | `0x00` | Both |
| `CMD_COLLECT_INTERVAL` | `0xA0` | ESP32 → RP2040 |
| `CMD_BEEP_ON/OFF` | `0xA1/A2` | ESP32 → RP2040 |
| `CMD_SHUTDOWN/POWER_ON` | `0xA3/A4` | ESP32 → RP2040 |
| `SENSOR_SCD41_*` | `0xB0-B2` | RP2040 → ESP32 |
| `SENSOR_AHT20_*` | `0xB3-B4` | RP2040 → ESP32 |
| `SENSOR_TVOC_INDEX` | `0xB5` | RP2040 → ESP32 |

## AI-Assisted Development

This project is designed to work seamlessly with AI coding agents. Every layer includes structured knowledge files:

| File | Scope | Used By |
|------|-------|---------|
| `CLAUDE.md` | Project-level conventions & constraints | Claude Code |
| `AGENTS.md` | Detailed knowledge base per module | Cursor, Copilot, Claude Code |
| `components/bsp/AGENTS.md` | BSP hardware details | AI agents |
| `main/app/AGENTS.md` | App layer patterns & anti-patterns | AI agents |

**With Claude Code**, just open the project and start asking — the agent already understands the architecture, conventions, and where to find things.

## Resources

- [SenseCAP Indicator Wiki](https://wiki.seeedstudio.com/Sensor/SenseCAP/SenseCAP_Indicator/) — official documentation
- [ESP-IDF Programming Guide](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/) — framework docs
- [LVGL 9 Documentation](https://docs.lvgl.io/9/) — UI library
- [Seeed SenseCAP Indicator ESP32 SDK](https://github.com/Seeed-Solution/SenseCAP_Indicator_ESP32) — reference firmware

## Roadmap

- [ ] **LoRa downlink reception** — uplink works, but RX window intermittently fails to receive downlinks ([details](docs/lora-downlink-issue.md))
- [ ] **I2C Fast Mode (400 kHz)** — reduce IO Expander latency for LoRa SPI timing margins
- [ ] **LoRa P2P mode** — point-to-point communication without LoRaWAN infrastructure
- [ ] **OTA firmware update** — over-the-air update via WiFi
- [ ] **SD card data logging** — sensor history to microSD
- [ ] **Home Assistant integration** — MQTT discovery for HA dashboard

## Contributing

1. **Fork** this repository
2. Create a feature branch: `git checkout -b feat/my-feature`
3. Follow the conventions documented in `CLAUDE.md`
4. Commit with clear messages: `git commit -m "feat: add my feature"`
5. Open a **Pull Request**

Please keep pages BSP-independent and services self-contained. When in doubt, read the `AGENTS.md` for the relevant module.

## License

[MIT](LICENSE) — use it, fork it, ship it.
