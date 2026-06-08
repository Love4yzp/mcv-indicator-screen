# SenseCAP Indicator LoRa / LCD 共存排障记录

## 背景

SenseCAP Indicator 的 ST7701S LCD 初始化 3-wire SPI 与 SX1262 LoRa bit-bang SPI 物理共享 GPIO 41 (`CLK`) / 48 (`MOSI`)。

已知硬件约束：

- LoRa SPI 的时钟脉冲会污染 ST7701S 内部寄存器状态，导致显示水平错位。
- 仅拉高 `LCD_CS` 不能阻止 ST7701S 被共享时钟干扰。
- 项目约束要求每次 LoRa SPI 批量操作后调用 `bsp_display_rgb_restart()` 做显示恢复。

## 本次排障中确认的事实

- 现有代码最初使用的是“`VSYNC` 延迟恢复 + 只写 critical registers”的方案。
- `components/bsp/src/bsp_display.c` 中 `io_exp_set()` 原本直接调用非线程安全的 `esp_io_expander_set_level(...)`。
- IO expander 与触摸控制器都在同一条 I2C 总线上。
- `svc_lorawan.cpp` 已经在 `sendReceive()` 返回后调用 `bsp_display_rgb_restart()`，所以问题主要在恢复实现本身，不在调用点缺失。

## 试过的方案与结果

### 1. 同步 full resync

做法：

- `io_exp_set()` 改为 `bsp_io_expander_set_level_safe()`
- `bsp_display_rgb_restart()` 改成同步执行
- 加入 SPI 状态 flush
- 每次恢复时全量重写 ST7701S 寄存器，然后 `esp_lcd_rgb_panel_restart()`

结果：

- 编译、烧录通过
- 设备在 uplink 后出现重启

结论：

- “在 LoRa 发送路径里同步做完整显示恢复”风险过高，不适合作为最终方案。

### 2. `VSYNC ISR + 高优先级 resync task + full resync`

做法：

- 保留 full resync
- 把恢复放回异步 task
- 由 `VSYNC` callback 唤醒

结果：

- 错位可以恢复，但系统仍会出现“突然死掉/重启”
- 串口抓到真实致因并不是 LoRa 自身崩溃，而是触摸读取超时触发 abort

抓到的关键日志：

```text
ESP_ERROR_CHECK failed: esp_err_t 0x107 (ESP_ERR_TIMEOUT)
file: "./managed_components/espressif__esp_lvgl_port/src/lvgl9/esp_lvgl_port_touch.c" line 127
func: lvgl_port_touchpad_read
expression: esp_lcd_touch_read_data(touch_ctx->handle)
```

结论：

- 显示恢复期间大量 IO expander/I2C 操作会和触摸读取冲突。
- `esp_lvgl_port` 默认 touch read 失败即 `ESP_ERROR_CHECK` abort，导致整机被拉死。

### 3. 普通优先级异步 worker 直接触发 full resync

做法：

- 移除 `VSYNC ISR` 依赖
- 保留异步 resync task
- 由 `bsp_display_rgb_restart()` 直接 `xTaskNotifyGive()` 唤醒 worker
- worker 中执行 `flush + full register rewrite + rgb restart`

结果：

- 行为比同步恢复稳定
- 但如果触摸路径仍然沿用 vendor 默认实现，I2C 冲突时仍会 abort

结论：

- 显示恢复策略需要和触摸容错一起处理，单改显示侧不够。

## 本次最终收敛方案

### 显示侧

文件：[components/bsp/src/bsp_display.c](/Users/spencer/Seeed/dev/indicator/sensecap-indicator-starter/components/bsp/src/bsp_display.c)

- `io_exp_set()` 使用 `bsp_io_expander_set_level_safe()`，避免 IO expander 竞态
- 保留 runtime `flush + full register rewrite + esp_lcd_rgb_panel_restart()`
- 通过普通优先级异步 worker 执行恢复，不在 LoRa 发送路径里同步重编程显示
- 不再依赖 `VSYNC ISR` 或超高优先级恢复任务

### 触摸侧

文件：[main/lv_port.c](/Users/spencer/Seeed/dev/indicator/sensecap-indicator-starter/main/lv_port.c)

- 不修改三方 `esp_lvgl_port` 组件
- 不再使用 `lvgl_port_add_touch()` 的默认 touch read 路径
- 在本项目里注册自定义 LVGL touch indev
- 自定义 read callback 中：
  - `esp_lcd_touch_read_data()` / `esp_lcd_touch_get_data()` 失败时仅记录 warning
  - 返回 `LV_INDEV_STATE_RELEASED`
  - 不再 abort 整机

## 当前验证结果

- 构建通过
- 烧录通过
- 监控 130 秒内连续抓到两次 uplink 成功
- 期间未再出现 touch timeout abort、panic、watchdog 或自动重启

样例日志：

```text
I (64468) svc_lorawan: Session saved to NVS
I (64468) svc_lorawan: Uplink OK: CO2=0 T=28.3 H=59.9 TVOC=0

I (124525) svc_lorawan: Session saved to NVS
I (124525) svc_lorawan: Uplink OK: CO2=0 T=28.2 H=59.6 TVOC=0
```

## 当前仍存在但可接受的现象

- uplink 后屏幕有时会先错位一小段时间，然后闪一下恢复正常

这说明：

- 显示恢复已经生效
- 恢复是异步发生的，不是 LoRa 事务结束瞬间同步完成

当前判断是该现象可接受，但它仍说明“共享 SPI + runtime full resync”本质上是补救方案，不是完全无感的硬件隔离方案。

## 这次排障得到的结论

1. 真正导致“设备突然死掉”的关键问题不是 LoRaWAN 本身，而是触摸读超时后的默认 abort 行为。
2. `io_expander` 的线程安全问题确实需要修，不然显示恢复路径本身不可靠。
3. 把 full resync 放进 LoRa 发送同步路径风险太大。
4. 完全依赖 `VSYNC ISR + 高优先级 task` 也不够稳。
5. 当前最稳妥的做法是：
   - 显示恢复异步执行
   - 触摸读失败本地容错
   - 不污染三方组件

## 后续可继续探索的方向

### 1. 缩短错位持续时间

- 优化恢复触发时机
- 评估是否能在不引入死锁/超时的前提下进一步提前 resync

### 2. 降低恢复期间 I2C 压力

- 评估是否能减少 runtime full resync 使用的 IO expander 翻转次数
- 评估是否需要在恢复期间暂时抑制触摸轮询

### 3. 显示恢复必要寄存器集合——已验证

**实验结论（2026-04-13）**：

- 跳过 `st7701s_write_registers()`，只调 `esp_lcd_rgb_panel_restart()` → **屏幕黑屏，无法恢复**
- 确认故障模式是 **模式 A（SPI 寄存器被篡改）**，不是单纯的 RGB 行计数器失步
- LoRa SPI 时钟边沿在 LCD_CS=HIGH 时仍会被 ST7701S 部分接收，导致内部寄存器被随机改写
- **full register rewrite 是必要的**，不能精简为仅 RGB restart
- 实测 full register rewrite 耗时 ~151ms（瓶颈是 CS 翻转经 I2C→TCA9535，每次约 2-5ms）

### 4. LCD resync 与 LoRa SPI 互斥（已修复）

**问题（2026-04-13 发现并修复）**：

- `resync_task()` 操作 GPIO 41/48 写 ST7701S 寄存器时，LoRa HAL 的 `spiTransfer()` 也可能在用相同 GPIO
- 两者之间没有共享 mutex
- Class A 碰巧安全（sendReceive 阻塞完成后才 resync），但 Class C 下 LoRa 随时可能有 DIO1 中断触发 SPI 读取

**修复**：

- BSP 新增 `bsp_shared_spi_mutex()`，在 `bsp_io_expander_init()` 中创建
- `resync_task()` 在操作 GPIO 41/48 期间持锁
- `indicator_hal.h` 的 `spiBeginTransaction/EndTransaction` 也取/放该锁
- `esp_lcd_rgb_panel_restart()` 放在锁外面（它不操作 GPIO 41/48）

### 4. 若追求彻底无错位

- 需要重新审视硬件层隔离手段，而不是继续堆软件补偿

## 后续排障中新发现的问题

### 1. Wi-Fi 重新配网路径存在独立 crash 风险

现象：

- 在设置页触发 `svc_wifi_reprovision()` 后，设备可能在 BLE/Wi-Fi provisioning 重新切换过程中黑屏重启

串口中抓到的关键线索：

- `wifi_prov_scheme_ble: BT memory released`
- `BTDM memory released`
- 随后重新 `BLE_INIT`
- 接着出现 `Guru Meditation Error: Core 1 panic'ed (LoadProhibited)`

结论：

- 这不是 LoRa/LCD 共存问题本身，而是 `svc_wifi_reprovision()` 的 provisioning manager / BLE 资源生命周期处理有缺陷
- 当前仓库已经恢复到之前满意的版本，因此该问题仅记录，不保留中间试验性修复

### 2. 触摸门控会破坏点击语义

现象：

- 为了降低 display resync 与 touch I2C 冲突，尝试在 display resync 窗口内直接跳过 touch read
- 实机表现为“页面仍可滑动，但按钮点击失效或不稳定”

结论：

- 简单地把 resync 窗口内的 touch 一律返回 `RELEASED` 会打断 LVGL pointer 序列
- 该实验性方案已放弃，代码已回退

### 3. BLE Provision 成功后的黑屏不等于系统真的死机

现象：

- 某些场景下，BLE Provision 完成、IP 已获取后，屏幕先错位甚至发黑
- 等待一段时间或下一次 LoRa uplink 后，屏幕又恢复正常

结论：

- 这更像显示控制器状态异常，而不是整个系统已经停止工作
- 后续若继续处理这一类问题，应把“系统 crash”和“显示链路异常”分开看待
