# Hardware Reference

## ESP32-S3 Pin Map

From `components/bsp/include/bsp_indicator.h`:

### I2C
| Signal | GPIO | Notes |
|--------|------|-------|
| SCL | 40 | |
| SDA | 39 | |
| Bus | I2C_NUM_0 | 400kHz |

### RGB LCD (16-bit parallel, ST7701S)
| Signal | GPIO |
|--------|------|
| DATA0–15 | 15,14,13,12,11,10,9,8,7,6,5,4,3,2,1,0 |
| PCLK | 21 |
| HSYNC | 16 |
| VSYNC | 17 |
| DE | 18 |
| Backlight (PWM) | 45 |

**Timing** (480×480 @ 18MHz PCLK, RGB565):
```
HSYNC: BP=50, FP=10, PW=8
VSYNC: BP=20, FP=10, PW=8
```

### Touch
| Parameter | Value |
|-----------|-------|
| IC | FT6336U |
| I2C addr | 0x48 |

### Button
| Signal | GPIO |
|--------|------|
| User button | 38 |

### RP2040 UART
| Signal | GPIO |
|--------|------|
| TX | 19 |
| RX | 20 |
| Baud | 115200 |

### LoRa SPI (D1L / D1Pro only, bit-bang)
| Signal | GPIO | Notes |
|--------|------|-------|
| MOSI | 48 | **Shared with LCD 3-wire SPI** |
| MISO | 47 | |
| SCLK | 41 | **Shared with LCD 3-wire SPI** |
| Freq | — | 2MHz |

## IO Expander (TCA9535)

Primary I2C addr: 0x20, fallback: 0x39

| Pin | Name | Direction | Notes |
|-----|------|-----------|-------|
| 0 | LORA_NSS | Output | SPI chip select (active low) |
| 1 | LORA_RST | Output | Radio reset (active low) |
| 2 | LORA_BUSY | Input | Radio busy flag |
| 3 | LORA_DIO1 | Input | Radio IRQ line |
| 4 | LCD_CS | Output | LCD SPI CS (init only) |
| 5 | LCD_RST | Output | LCD reset |
| 7 | TOUCH_RST | Output | Touch reset |
| 8 | RP2040_RST | Output | RP2040 reset |
| 10 | BMP_PWR | Output | BMP sensor power |
| 11 | LORA_VER | Input | Pull-up = TCXO present |

IO Expander interrupt: GPIO 42 (ESP32 side)

## Grove Connectors (RP2040 Side)

**重要：Grove 接口连接在 RP2040，不是 ESP32-S3。**

方位以**背面朝向自己**（USB Type-C 在底部中央）为准：

| Grove 口 | 位置（背面视角） | RP2040 Pin | 能力 |
|----------|----------------|------------|------|
| Grove(IIC) | 右侧 | GPIO20, GPIO21 | GPIO / 硬件 I2C |
| Grove(ADC) | 左侧 | GPIO26 (ADC0), GPIO27 (ADC1) | GPIO / I2C (PIO) / **硬件 ADC** |

两个 Grove 口都是通用 GPIO，也都能跑 I2C。区别在于只有 GPIO26/27 有硬件 ADC（RP2040 ADC 仅限 GPIO26-29）。
丝印标注 "IIC" / "ADC" 是推荐用途，不是硬性限制。

板载传感器（SHT4x 温湿度、SCD4x CO2、SGP40 TVOC）也连接在 RP2040 的 I2C 总线上。

传感器数据流：`RP2040 (采集) → UART/COBS → ESP32-S3 (svc_rp2040) → app_manager → UI`

如需接入新 Grove 传感器，需修改 RP2040 固件（`rp2040/`）。

## BSP Init Sequence

```
bsp_indicator_init()
├── bsp_i2c_init()              // I2C master @ 400kHz
├── bsp_io_expander_init()      // TCA9535, set pin directions
├── bsp_display_init()          // RGB panel, ST7701S register init
│   └── bsp_display_backlight_set(100)
└── bsp_touch_init()            // FT6336U via I2C
```

## BSP Dependencies

From `components/bsp/idf_component.yml`:
```yaml
idf: ">=5.4"
esp_lcd: "~1.0"
esp_io_expander: "~1.0"
espressif/esp_lvgl_port: "~2.0"
lvgl/lvgl: "~9.0"
button: "~3.0"
```
