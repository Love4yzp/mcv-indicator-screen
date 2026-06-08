# LoRaWAN Downlink 接收问题（未解决）

## 状态

**Open** — Uplink 正常，Downlink（包括 JoinAccept）接收不稳定。

## 现象

- SX1262 uplink 正常：gateway 收到所有 JoinRequest 和数据帧。
- Downlink 100% 丢失（RX1/RX2 窗口内均未检测到 RF 信号）。
- 10 秒连续 RX 监听测试也收不到任何信号。
- RSSI 读数异常：-17 ~ -49 dBm（正常噪底应为 -110 ~ -120 dBm）。
- Join 曾成功过一次（间歇性），说明 RX 硬件不是完全损坏。

## 已排除的原因

| 排查项 | 结论 |
|--------|------|
| RF 参数（频率/BW/SF/IQ） | RadioLib 配置正确，与 ChirpStack 完全匹配 |
| RX 窗口时序 | `tUplinkEnd + delay` 计算正确，误差 <6ms |
| scanGuard 不足 | 已从默认 10ms 提升到 50ms（join 前设置） |
| ChirpStack Class C `imme:true` | 已改回 Class A，downlink 使用 tmst 调度 |
| RadioLib LoRaWAN 版本 | v7.6.0 支持 1.1.0，与 ChirpStack 配置匹配 |
| DIO2 RF Switch | 测试了启用/禁用两种，均无法稳定接收 |

## 待排查方向

1. **RSSI 异常高**（-17 ~ -49 dBm）：可能是 SX1262 RF 前端在 RX 模式下配置不正确，或板级 EMI 干扰。
2. **SX1262 内部 RX 路径**：需要对比 SenseCAP Indicator 官方 SDK（`SenseCAP_Indicator_ESP32`）的 LoRa 初始化序列，确认是否有特殊的寄存器配置。
3. **I2C IO Expander 延迟**：NSS 通过 TCA9535（~5ms/toggle），可能导致 SPI 时序边界条件。I2C 当前运行在 100kHz，提速到 400kHz 可减少开销。
4. **SPI 总线争用**：LoRa SPI（bit-bang GPIO 41/48）与 ST7701S LCD 共享。display resync 可能在 RX 窗口期间干扰 SX1262。
5. **硬件级确认**：用示波器或频谱仪验证 RX 时 RF 路径是否导通。

## 硬件参考

- SX1262 (U16, SX1262IMLTRT) — 单端口设计，TX/RX 共用 RFI_P
- DIO2 连接到 VBAT_IO（非 RF switch）
- NSS / DIO1 / RST / BUSY 通过 TCA9535 IO Expander
- TCXO 2.4V（VER pin 确认存在）
- 原理图：`SENSECAP INDICATOR D1PRO V1.3_sch.pdf` Page 6

## 关键修复（已合入）

- `scanGuard = 50` 在 join 之前设置（解决 IO Expander 延迟导致的 RX 窗口不足）
- `LoRaWANEvent_t` 初始化为 `{0}`（避免垃圾值误判 downlink）
- ChirpStack Device Profile 需设为 Class A（Class C 的 `imme:true` 会导致所有 downlink miss）
