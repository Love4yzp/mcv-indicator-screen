# Architecture

## Boot Sequence

`main/main.c` — `app_main()`:

```c
nvs_flash_init()            → NVS partition
bsp_indicator_init()        → I2C, IO expander, display, touch
lv_port_init()              → LVGL + esp_lvgl_port
svc_rp2040_init()           → UART to RP2040 + sensor callback
svc_storage_init()          → Settings persistence (NVS)
svc_alerts_init()           → CO2 threshold alerts
page_settings_set_sys_info()→ MAC + IDF version string
app_manager_init(&cb)       → Create tileview, register pages (inside lv_port_lock)
svc_wifi_init()             → WiFi/BLE provisioning
bsp_button_init()           → GPIO button for alert dismiss
// if CONFIG_INDICATOR_LORAWAN_ENABLED:
svc_lorawan_init()          → RadioLib SX1262 (D1L/D1Pro only)
svc_lorawan_join()          → OTAA join + Class C switch
xTimerCreate("lora_ul")    → 60s periodic uplink timer
```

## Page Convention

Pages live in `main/app/pages/`. Each page exposes:

```c
void page_xxx_create(lv_obj_t *parent);                   // Build UI once
void page_xxx_update(const sensor_data_t *data);           // Optional: receive updates
```

Rules:
- **Pure LVGL** — no BSP includes, only `lvgl.h` + `app_manager.h`
- Register in `app_manager.c` → `s_pages[]`
- Receive data via `update()` callback, not direct service calls
- Store widget refs as `static` locals

Example — see `page_hello.c` for minimum viable page.

## Service Convention

Services live in `main/app/services/`. Each service exposes:

```c
esp_err_t svc_xxx_init(void);
void svc_xxx_register_cb(svc_xxx_cb_t cb, void *ctx);
```

Rules:
- Init in `main.c` boot sequence
- Push data to UI via `app_manager_update_*()` functions
- Direct UI updates require `lv_port_lock()` / `lv_port_unlock()`

Example — see `svc_storage.c` for minimum viable service.

## Data Flow

```
RP2040 →UART/COBS→ svc_rp2040.c → app_manager_update_sensor() → page_*_update()
WiFi event        → svc_wifi.c   → app_manager_update_wifi_info() → LVGL timer → UI
LoRa uplink       → svc_lorawan  reads app_manager_get_sensor_data()
```

WiFi status uses async→sync pattern: event handler sets pending flag,
LVGL timer (`wifi_ui_sync_cb`) reads and updates labels on LVGL thread.

## Key sdkconfig Settings

```
CONFIG_SPIRAM=y                              # 8MB PSRAM
CONFIG_SPIRAM_FETCH_INSTRUCTIONS=y           # Code in PSRAM
CONFIG_BT_NIMBLE_MEM_ALLOC_MODE_EXTERNAL=y   # BLE heap in PSRAM
CONFIG_BT_NIMBLE_HOST_TASK_STACK_SIZE=5120    # BLE task stack
```

## Hardware Summary

- **Screen**: 480×480 RGB LCD (ST7701S), 16-bit RGB565
- **Touch**: FT6336U via I2C
- **LoRa**: SX1262 via IO Expander (D1L/D1Pro only), bit-bang SPI
- **RP2040**: UART2 @ 115200, COBS framing
- **Provisioning**: BLE with QR code, POP "sensecap123"
- **PSRAM**: 8MB, required for BLE stack (external memory allocation)

## Dual-Chip Architecture: ESP32-S3 vs RP2040

SenseCAP Indicator 采用双芯片架构，职责明确分离：

| 芯片 | 职责 | 外设 |
|------|------|------|
| **RP2040** | 传感器采集、Grove 口管理 | GPIO/I2C Grove 接口、板载传感器（SHT/SCD/TVOC） |
| **ESP32-S3** | 显示、数据处理、网络通信 | LCD、Touch、WiFi/BLE、LoRa SPI |

**关键设计决策：Grove 口（GPIO/I2C）物理连接在 RP2040 侧，不在 ESP32-S3 侧。**

因此：
- 新增 Grove 传感器驱动 → 改 RP2040 固件（Arduino/PlatformIO）
- 传感器数据通过 UART/COBS 协议传给 ESP32-S3
- ESP32-S3 侧通过 `svc_rp2040` 接收数据，经 `app_manager` 路由到 UI pages
- 不要尝试在 ESP32-S3 侧直接读取 Grove 传感器——硬件上无法连接
