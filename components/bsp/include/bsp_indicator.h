/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Spencer (love4yzp)
 */

#pragma once

#include "driver/i2c_master.h"
#include "driver/uart.h"
#include "esp_err.h"
#include "esp_io_expander.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "iot_button.h"

#ifdef __cplusplus
extern "C" {
#endif

#define BSP_I2C_SCL_IO          (40)
#define BSP_I2C_SDA_IO          (39)
#define BSP_I2C_NUM             (I2C_NUM_0)
#define BSP_I2C_FREQ_HZ         (400000)

#define BSP_IO_EXPANDER_I2C_ADDR_PRIMARY    (0x20)
#define BSP_IO_EXPANDER_I2C_ADDR_FALLBACK   (0x39)

#define BSP_IO_EXP_TOUCH_RST    (7)
#define BSP_IO_EXP_LCD_CS       (4)
#define BSP_IO_EXP_LCD_RST      (5)
#define BSP_IO_EXP_RP2040_RST   (8)
#define BSP_IO_EXP_BMP_PWR      (10)

#define BSP_LCD_IO_DATA0        (15)
#define BSP_LCD_IO_DATA1        (14)
#define BSP_LCD_IO_DATA2        (13)
#define BSP_LCD_IO_DATA3        (12)
#define BSP_LCD_IO_DATA4        (11)
#define BSP_LCD_IO_DATA5        (10)
#define BSP_LCD_IO_DATA6        (9)
#define BSP_LCD_IO_DATA7        (8)
#define BSP_LCD_IO_DATA8        (7)
#define BSP_LCD_IO_DATA9        (6)
#define BSP_LCD_IO_DATA10       (5)
#define BSP_LCD_IO_DATA11       (4)
#define BSP_LCD_IO_DATA12       (3)
#define BSP_LCD_IO_DATA13       (2)
#define BSP_LCD_IO_DATA14       (1)
#define BSP_LCD_IO_DATA15       (0)
#define BSP_LCD_IO_VSYNC        (17)
#define BSP_LCD_IO_HSYNC        (16)
#define BSP_LCD_IO_DE           (18)
#define BSP_LCD_IO_PCLK         (21)
#define BSP_LCD_IO_BL           (45)

#define BSP_LCD_H_RES               (480)
#define BSP_LCD_V_RES               (480)
#define BSP_LCD_PCLK_HZ             (18 * 1000 * 1000)
#define BSP_LCD_HSYNC_BACK_PORCH    (50)
#define BSP_LCD_HSYNC_FRONT_PORCH   (10)
#define BSP_LCD_HSYNC_PULSE_WIDTH   (8)
#define BSP_LCD_VSYNC_BACK_PORCH    (20)
#define BSP_LCD_VSYNC_FRONT_PORCH   (10)
#define BSP_LCD_VSYNC_PULSE_WIDTH   (8)

#define BSP_TOUCH_I2C_ADDR      (0x48)

#define BSP_BUTTON_IO           (38)

/* LoRa radio (SX1262) — D1L / D1Pro only */
#define BSP_LORA_SPI_HOST       SPI3_HOST
#define BSP_LORA_SPI_MOSI_IO    (48)
#define BSP_LORA_SPI_MISO_IO    (47)
#define BSP_LORA_SPI_SCLK_IO    (41)
#define BSP_LORA_SPI_FREQ_HZ    (2000000)

#define BSP_IO_EXP_LORA_NSS     (0)   /* SPI chip select (active low) */
#define BSP_IO_EXP_LORA_RST     (1)   /* radio reset (active low) */
#define BSP_IO_EXP_LORA_BUSY    (2)   /* radio busy flag (input) */
#define BSP_IO_EXP_LORA_DIO1    (3)   /* radio IRQ line (input) */
#define BSP_IO_EXP_LORA_VER     (11)  /* pull-up = TCXO present (input) */
#define BSP_IO_EXP_INT_IO       (42)  /* IO expander interrupt GPIO */

#define BSP_RP2040_UART_NUM     (UART_NUM_2)
#define BSP_RP2040_UART_TX_IO   (19)
#define BSP_RP2040_UART_RX_IO   (20)
#define BSP_RP2040_UART_BAUD    (115200)

extern esp_io_expander_handle_t bsp_io_expander_handle;

esp_err_t bsp_indicator_init(void);
esp_io_expander_handle_t bsp_get_io_expander(void);
esp_err_t bsp_button_init(button_handle_t *btn_handle_out);

/* Thread-safe IO expander operations (mutex-protected read-modify-write) */
esp_err_t bsp_io_expander_set_level_safe(uint32_t pin_mask, uint8_t level);
esp_err_t bsp_io_expander_set_dir_safe(uint32_t pin_mask, esp_io_expander_dir_t dir);
esp_err_t bsp_io_expander_get_level_safe(uint32_t pin_mask, uint32_t *level);

/**
 * Mutex guarding GPIO 41 (CLK) / 48 (MOSI) which are shared between
 * ST7701S LCD 3-wire SPI and SX1262 LoRa bit-bang SPI.
 * Both LCD resync and LoRa SPI transactions MUST hold this mutex.
 * Returns NULL before bsp_io_expander_init() completes.
 */
SemaphoreHandle_t bsp_shared_spi_mutex(void);

#ifdef __cplusplus
}
#endif
