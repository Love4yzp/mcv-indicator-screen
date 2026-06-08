# APP LAYER — AGENTS.md

**Scope:** `main/app/` — Pages（纯 UI）+ Services（业务逻辑）

全局 conventions 和 data flow 见**根 AGENTS.md**，以下只补充 app 层独有的细节。

## APP-SPECIFIC RULES

- 新 page 注册到 `app_manager.c` 的 `s_pages[]`；新 service 在 `main.c` 中 init
- Page 里不放业务逻辑 — service 处理后通过 `app_manager_update_*()` 推给 page
- **async→sync 模式**（WiFi、LoRa 共用）：非 LVGL 线程设 pending flag，LVGL timer 读取并更新 UI
- WiFi 重新配网（`svc_wifi_reprovision()`）会 `esp_restart()`——这是有意为之，见根 AGENTS.md 约束 #4

## WHERE TO LOOK

| Need | File |
|------|------|
| Page 模板 | `pages/page_hello.c` |
| Service 模板 | `services/svc_storage.c` |
| sensor_data_t / page_entry_t | `app_manager.h` |
| platform_callbacks_t | `app_manager.h` |
| LVGL locking | `lv_port.h` |
| Boot order + wiring | `main/main.c` |
