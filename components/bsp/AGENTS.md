# BSP — AGENTS.md

**Scope:** `components/bsp/` — 纯硬件抽象层，无 app 逻辑

跨模块约束（LoRa/LCD GPIO 共享、IO Expander 竞态等）见**根 AGENTS.md → CROSS-MODULE CONSTRAINTS**。

## BSP-SPECIFIC RULES

- Pin 定义全在 `include/bsp_indicator.h`，不要在 app 代码里硬编码
- LCD resync：BSP 周期性自动 resync + `bsp_display_rgb_restart()` 供 LoRa HAL 在 SPI 后触发立即 resync
- 使用 GPIO 41/48 的代码必须持 `bsp_shared_spi_mutex()`

## WHERE TO LOOK

| Need | File |
|------|------|
| Pin definitions, IO expander pins | `include/bsp_indicator.h` |
| Backlight + RGB restart | `include/bsp_display.h`, `src/bsp_display.c` |
| Thread-safe IO expander API | `bsp_io_expander_*_safe()` in `bsp_indicator.h` |
| Init sequence | `src/bsp_init.c` |
| Internal shared state (mutex, handles) | `src/bsp_internal.h` |
