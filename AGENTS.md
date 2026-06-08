# PROJECT KNOWLEDGE BASE

**Stack:** ESP-IDF 5.4+ / LVGL 9 / ESP32-S3 / RP2040 coprocessor

## WHY THIS ARCHITECTURE

Convention over framework：薄 app 层（pages/ + services/），不用 esp-brookesia 也不写自定义 display driver。
用 `esp_lcd` + `esp_lvgl_port` 驱动显示，`app_manager` 做 page 注册和数据路由。

## WHERE TO LOOK

| Task | Start here |
|------|-----------|
| Add UI page | `main/app/pages/page_hello.c`（照抄改），注册到 `app_manager.c` 的 `s_pages[]` |
| Add service | `main/app/services/svc_storage.c`（照抄改），在 `main.c` 中 init |
| Pin / hardware config | `components/bsp/include/bsp_indicator.h` |
| Boot sequence | `main/main.c` — `app_main()` |
| LoRaWAN | `main/app/services/svc_lorawan.cpp` + `indicator_hal.h` |
| HA MQTT uplink | `main/app/services/svc_ha_mqtt.c` — 30s publish + HA Discovery |
| Add/modify Grove sensor | `rp2040/`；ESP32-S3 侧只收数据 via `svc_rp2040` |
| Partition layout | `partitions.csv` |

## CONVENTIONS

- **Pages = pure LVGL**：不 include BSP headers，通过 `update()` callback 接收数据
- **Services = 业务逻辑**：可访问 BSP，通过 `app_manager_*()` 推数据给 pages
- **LVGL 线程安全**：service/task 中触碰 UI 必须 `lv_port_lock()` / `lv_port_unlock()`
- **IO Expander 线程安全**：只用 `bsp_io_expander_*_safe()` 系列，禁止直接调 `esp_io_expander_set_level()`

## DATA FLOW

```
RP2040 →UART/COBS→ svc_rp2040 → app_manager_update_sensor() → page_*_update()
WiFi event        → svc_wifi   → app_manager_update_wifi_info()  → timer → UI
LoRa event        → main.c cb  → app_manager_update_lora_*()     → timer → page_lora
LoRa uplink       → svc_lorawan reads app_manager_get_sensor_data()
```

## CROSS-MODULE CONSTRAINTS

修改相关代码前**必须理解**这些跨模块陷阱：

1. **LoRa SPI ↔ LCD 共享 GPIO 41/48**：LoRa SPI 会篡改 ST7701S 寄存器导致黑屏。BSP 层已自动 resync。新增使用 GPIO 41/48 的代码必须持 `bsp_shared_spi_mutex()`。详见 `docs/lora-lcd-coexist-debug-notes.md`。
2. **IO Expander 竞态**：多任务访问 TCA9535 必须用 `bsp_io_expander_*_safe()`（mutex 保护读-改-写），直接调底层 API 会导致 port 寄存器竞态。
3. **LoRaWAN 条件编译**：`CONFIG_INDICATOR_LORAWAN_ENABLED` Kconfig 开关。无 LoRa 硬件时 `svc_lorawan_init()` 返回 `ESP_ERR_NOT_FOUND` 并清理，不 crash。
4. **WiFi 重新配网必须重启**：BLE 内存释放后无法运行时重建，`svc_wifi_reprovision()` 用 `esp_restart()` 是有意为之。
5. **LoRa scanGuard 必须在 join 前设置**：IO Expander I2C 延迟 ~5ms/toggle，RadioLib 默认 scanGuard (10ms) 不够。`s_node->scanGuard = 50` 必须在 `activateOTAA()` 之前。
6. **Grove 传感器归 RP2040**：Grove 口（GPIO/I2C/ADC）物理连在 RP2040。新增传感器 → 改 `rp2040/`；ESP32-S3 **不能直接访问 Grove**，只通过 `svc_rp2040` UART/COBS 收数据。
