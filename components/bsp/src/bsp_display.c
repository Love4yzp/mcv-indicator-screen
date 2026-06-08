/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Spencer (love4yzp)
 */

#include "bsp_display.h"
#include "bsp_indicator.h"

#include <stdatomic.h>
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_check.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_rgb.h"
#include "esp_log.h"
#include "esp_rom_sys.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

static const char *TAG = "bsp_display";

#define LCD_SPI_CLK_IO  41
#define LCD_SPI_SDO_IO  48

static esp_lcd_panel_handle_t s_panel = NULL;
static void *s_fb0 = NULL;
static void *s_fb1 = NULL;
static atomic_bool s_resync_pending = false;
static TaskHandle_t s_resync_task = NULL;

static void resync_task(void *arg);

static inline void io_exp_set(int pin, int level)
{
    bsp_io_expander_set_level_safe((uint32_t)(1ULL << pin), level);
    esp_rom_delay_us(10);
}

static void spi_send9(unsigned short bits)
{
    for (int n = 0; n < 9; n++) {
        gpio_set_level(LCD_SPI_SDO_IO, (bits & 0x0100U) ? 1 : 0);
        bits <<= 1;
        gpio_set_level(LCD_SPI_CLK_IO, 1);
        esp_rom_delay_us(10);
        gpio_set_level(LCD_SPI_CLK_IO, 0);
        esp_rom_delay_us(10);
    }
}

static void spi_write_cmd(uint8_t cmd)
{
    io_exp_set(BSP_IO_EXP_LCD_CS, 0);
    esp_rom_delay_us(10);
    gpio_set_level(LCD_SPI_CLK_IO, 0);
    esp_rom_delay_us(10);
    spi_send9((unsigned short)(0x2000U | ((cmd >> 8) & 0xFFU)));
    gpio_set_level(LCD_SPI_CLK_IO, 1);
    esp_rom_delay_us(10);
    gpio_set_level(LCD_SPI_CLK_IO, 0);
    io_exp_set(BSP_IO_EXP_LCD_CS, 1);
    esp_rom_delay_us(10);
    io_exp_set(BSP_IO_EXP_LCD_CS, 0);
    esp_rom_delay_us(10);
    spi_send9(cmd & 0xFFU);
    io_exp_set(BSP_IO_EXP_LCD_CS, 1);
    esp_rom_delay_us(10);
}

static void spi_write_data(uint8_t data)
{
    io_exp_set(BSP_IO_EXP_LCD_CS, 0);
    esp_rom_delay_us(10);
    gpio_set_level(LCD_SPI_CLK_IO, 0);
    esp_rom_delay_us(10);
    spi_send9((unsigned short)(0x0100U | data));
    gpio_set_level(LCD_SPI_CLK_IO, 1);
    esp_rom_delay_us(10);
    gpio_set_level(LCD_SPI_CLK_IO, 0);
    esp_rom_delay_us(10);
    io_exp_set(BSP_IO_EXP_LCD_CS, 1);
    esp_rom_delay_us(10);
}

/**
 * Program all ST7701S registers (no reset, no sleep-out / display-on delays).
 * Called from both cold-init and runtime resync.
 */
static void st7701s_write_registers(bool hot_resync)
{
    (void)hot_resync;

    /* Page 0 — display timing */
    spi_write_cmd(0xFF); spi_write_data(0x77); spi_write_data(0x01); spi_write_data(0x00); spi_write_data(0x00); spi_write_data(0x10);
    spi_write_cmd(0xC0); spi_write_data(0x3B); spi_write_data(0x00);
    spi_write_cmd(0xC1); spi_write_data(0x0D); spi_write_data(0x02);
    spi_write_cmd(0xC2); spi_write_data(0x31); spi_write_data(0x05);
    spi_write_cmd(0xC7); spi_write_data(0x04);
    spi_write_cmd(0xCD); spi_write_data(0x08);
    /* Positive gamma */
    spi_write_cmd(0xB0);
    spi_write_data(0x00); spi_write_data(0x11); spi_write_data(0x18); spi_write_data(0x0E); spi_write_data(0x11); spi_write_data(0x06);
    spi_write_data(0x07); spi_write_data(0x08); spi_write_data(0x07); spi_write_data(0x22); spi_write_data(0x04); spi_write_data(0x12);
    spi_write_data(0x0F); spi_write_data(0xAA); spi_write_data(0x31); spi_write_data(0x18);
    /* Negative gamma */
    spi_write_cmd(0xB1);
    spi_write_data(0x00); spi_write_data(0x11); spi_write_data(0x19); spi_write_data(0x0E); spi_write_data(0x12); spi_write_data(0x07);
    spi_write_data(0x08); spi_write_data(0x08); spi_write_data(0x08); spi_write_data(0x22); spi_write_data(0x04); spi_write_data(0x11);
    spi_write_data(0x11); spi_write_data(0xA9); spi_write_data(0x32); spi_write_data(0x18);
    /* Page 1 — power & GIP */
    spi_write_cmd(0xFF); spi_write_data(0x77); spi_write_data(0x01); spi_write_data(0x00); spi_write_data(0x00); spi_write_data(0x11);
    spi_write_cmd(0xB0); spi_write_data(0x60);
    spi_write_cmd(0xB1); spi_write_data(0x32);
    spi_write_cmd(0xB2); spi_write_data(0x07);
    spi_write_cmd(0xB3); spi_write_data(0x80);
    spi_write_cmd(0xB5); spi_write_data(0x49);
    spi_write_cmd(0xB7); spi_write_data(0x85);
    spi_write_cmd(0xB8); spi_write_data(0x21);
    spi_write_cmd(0xC1); spi_write_data(0x78);
    spi_write_cmd(0xC2); spi_write_data(0x78);
    vTaskDelay(pdMS_TO_TICKS(20));
    /* GIP timing */
    spi_write_cmd(0xE0); spi_write_data(0x00); spi_write_data(0x1B); spi_write_data(0x02);
    spi_write_cmd(0xE1);
    spi_write_data(0x08); spi_write_data(0xA0); spi_write_data(0x00); spi_write_data(0x00); spi_write_data(0x07); spi_write_data(0xA0);
    spi_write_data(0x00); spi_write_data(0x00); spi_write_data(0x00); spi_write_data(0x44); spi_write_data(0x44);
    spi_write_cmd(0xE2);
    spi_write_data(0x11); spi_write_data(0x11); spi_write_data(0x44); spi_write_data(0x44); spi_write_data(0xED); spi_write_data(0xA0);
    spi_write_data(0x00); spi_write_data(0x00); spi_write_data(0xEC); spi_write_data(0xA0); spi_write_data(0x00); spi_write_data(0x00);
    spi_write_cmd(0xE3); spi_write_data(0x00); spi_write_data(0x00); spi_write_data(0x11); spi_write_data(0x11);
    spi_write_cmd(0xE4); spi_write_data(0x44); spi_write_data(0x44);
    spi_write_cmd(0xE5);
    spi_write_data(0x0A); spi_write_data(0xE9); spi_write_data(0xD8); spi_write_data(0xA0); spi_write_data(0x0C); spi_write_data(0xEB);
    spi_write_data(0xD8); spi_write_data(0xA0); spi_write_data(0x0E); spi_write_data(0xED); spi_write_data(0xD8); spi_write_data(0xA0);
    spi_write_data(0x10); spi_write_data(0xEF); spi_write_data(0xD8); spi_write_data(0xA0);
    spi_write_cmd(0xE6); spi_write_data(0x00); spi_write_data(0x00); spi_write_data(0x11); spi_write_data(0x11);
    spi_write_cmd(0xE7); spi_write_data(0x44); spi_write_data(0x44);
    spi_write_cmd(0xE8);
    spi_write_data(0x09); spi_write_data(0xE8); spi_write_data(0xD8); spi_write_data(0xA0); spi_write_data(0x0B); spi_write_data(0xEA);
    spi_write_data(0xD8); spi_write_data(0xA0); spi_write_data(0x0D); spi_write_data(0xEC); spi_write_data(0xD8); spi_write_data(0xA0);
    spi_write_data(0x0F); spi_write_data(0xEE); spi_write_data(0xD8); spi_write_data(0xA0);
    spi_write_cmd(0xEB); spi_write_data(0x02); spi_write_data(0x00); spi_write_data(0xE4); spi_write_data(0xE4); spi_write_data(0x88); spi_write_data(0x00); spi_write_data(0x40);
    spi_write_cmd(0xEC); spi_write_data(0x3C); spi_write_data(0x00);
    spi_write_cmd(0xED);
    spi_write_data(0xAB); spi_write_data(0x89); spi_write_data(0x76); spi_write_data(0x54); spi_write_data(0x02); spi_write_data(0xFF);
    spi_write_data(0xFF); spi_write_data(0xFF); spi_write_data(0xFF); spi_write_data(0xFF); spi_write_data(0xFF); spi_write_data(0x20);
    spi_write_data(0x45); spi_write_data(0x67); spi_write_data(0x98); spi_write_data(0xBA);
    /* User command page */
    spi_write_cmd(0x36); spi_write_data(0x10);
    spi_write_cmd(0xFF); spi_write_data(0x77); spi_write_data(0x01); spi_write_data(0x00); spi_write_data(0x00); spi_write_data(0x13);
    spi_write_cmd(0xE5); spi_write_data(0xE4);
    spi_write_cmd(0xFF); spi_write_data(0x77); spi_write_data(0x01); spi_write_data(0x00); spi_write_data(0x00); spi_write_data(0x00);
    spi_write_cmd(0x3A); spi_write_data(0x60);
    spi_write_cmd(0x21);
}

static void st7701s_flush_spi_state(void)
{
    /* Best-effort reset of the ST7701S 3-wire SPI framing state. */
    gpio_set_level(LCD_SPI_CLK_IO, 1);
    gpio_set_level(LCD_SPI_SDO_IO, 1);

    io_exp_set(BSP_IO_EXP_LCD_CS, 0);
    esp_rom_delay_us(20);
    io_exp_set(BSP_IO_EXP_LCD_CS, 1);
    esp_rom_delay_us(20);
}

static void st7701s_init(void)
{
    const gpio_config_t gpio_config_spi = {
        .pin_bit_mask = (1ULL << LCD_SPI_CLK_IO) | (1ULL << LCD_SPI_SDO_IO),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = 0,
        .pull_down_en = 0,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&gpio_config_spi);

    io_exp_set(BSP_IO_EXP_LCD_CS, 1);
    gpio_set_level(LCD_SPI_CLK_IO, 1);
    gpio_set_level(LCD_SPI_SDO_IO, 1);

    io_exp_set(BSP_IO_EXP_LCD_RST, 0);
    vTaskDelay(pdMS_TO_TICKS(20));
    io_exp_set(BSP_IO_EXP_LCD_RST, 1);
    vTaskDelay(pdMS_TO_TICKS(120));

    st7701s_write_registers(false);

    spi_write_cmd(0x11);
    vTaskDelay(pdMS_TO_TICKS(120));
    spi_write_cmd(0x29);
    vTaskDelay(pdMS_TO_TICKS(120));

    io_exp_set(BSP_IO_EXP_LCD_CS, 1);
    gpio_set_level(LCD_SPI_CLK_IO, 1);
    gpio_set_level(LCD_SPI_SDO_IO, 1);
    ESP_LOGI(TAG, "ST7701S init sequence complete");
}

esp_err_t bsp_display_init(esp_lcd_panel_handle_t *panel_handle_out)
{
    ESP_RETURN_ON_FALSE(bsp_get_io_expander() != NULL, ESP_ERR_INVALID_STATE, TAG, "IO expander not initialized");

    if (s_panel != NULL) {
        if (panel_handle_out != NULL) {
            *panel_handle_out = s_panel;
        }
        return ESP_OK;
    }

    esp_lcd_rgb_panel_config_t rgb_cfg = {
        .clk_src = LCD_CLK_SRC_DEFAULT,
        .data_width = 16,
        .disp_gpio_num = GPIO_NUM_NC,
        .pclk_gpio_num = BSP_LCD_IO_PCLK,
        .vsync_gpio_num = BSP_LCD_IO_VSYNC,
        .hsync_gpio_num = BSP_LCD_IO_HSYNC,
        .de_gpio_num = BSP_LCD_IO_DE,
        .data_gpio_nums = {
            BSP_LCD_IO_DATA0, BSP_LCD_IO_DATA1, BSP_LCD_IO_DATA2, BSP_LCD_IO_DATA3,
            BSP_LCD_IO_DATA4, BSP_LCD_IO_DATA5, BSP_LCD_IO_DATA6, BSP_LCD_IO_DATA7,
            BSP_LCD_IO_DATA8, BSP_LCD_IO_DATA9, BSP_LCD_IO_DATA10, BSP_LCD_IO_DATA11,
            BSP_LCD_IO_DATA12, BSP_LCD_IO_DATA13, BSP_LCD_IO_DATA14, BSP_LCD_IO_DATA15,
        },
        .timings = {
            .pclk_hz = BSP_LCD_PCLK_HZ,
            .h_res = BSP_LCD_H_RES,
            .v_res = BSP_LCD_V_RES,
            .hsync_back_porch = BSP_LCD_HSYNC_BACK_PORCH,
            .hsync_front_porch = BSP_LCD_HSYNC_FRONT_PORCH,
            .hsync_pulse_width = BSP_LCD_HSYNC_PULSE_WIDTH,
            .vsync_back_porch = BSP_LCD_VSYNC_BACK_PORCH,
            .vsync_front_porch = BSP_LCD_VSYNC_FRONT_PORCH,
            .vsync_pulse_width = BSP_LCD_VSYNC_PULSE_WIDTH,
            .flags.pclk_active_neg = 0,
        },
        .flags.fb_in_psram = 1,
        .flags.double_fb = 1,
    };

    ESP_RETURN_ON_ERROR(esp_lcd_new_rgb_panel(&rgb_cfg, &s_panel), TAG, "RGB panel create failed");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_reset(s_panel), TAG, "Panel reset failed");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_init(s_panel), TAG, "Panel init failed");
    ESP_RETURN_ON_ERROR(esp_lcd_rgb_panel_get_frame_buffer(s_panel, 2, &s_fb0, &s_fb1), TAG,
                        "Get frame buffers failed");

    st7701s_init();

    /* Run runtime display recovery in a dedicated task so LoRa sendReceive()
     * does not block on the ST7701S reprogramming sequence. Keep the task at
     * a normal application priority to avoid starving WiFi/BLE/system work. */
    xTaskCreate(resync_task, "lcd_resync", 3072, NULL, 6, &s_resync_task);

    const ledc_timer_config_t ledc_timer = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_10_BIT,
        .timer_num = LEDC_TIMER_0,
        .freq_hz = 5000,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ESP_RETURN_ON_ERROR(ledc_timer_config(&ledc_timer), TAG, "Backlight timer config failed");

    const ledc_channel_config_t ledc_channel = {
        .gpio_num = BSP_LCD_IO_BL,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = LEDC_CHANNEL_0,
        .timer_sel = LEDC_TIMER_0,
        .duty = 1023,
        .hpoint = 0,
    };
    ESP_RETURN_ON_ERROR(ledc_channel_config(&ledc_channel), TAG, "Backlight channel config failed");

    if (panel_handle_out != NULL) {
        *panel_handle_out = s_panel;
    }

    ESP_LOGI(TAG, "Display ready (%dx%d, double buffer PSRAM)", BSP_LCD_H_RES, BSP_LCD_V_RES);
    return ESP_OK;
}

void bsp_display_backlight_set(int brightness_pct)
{
    if (brightness_pct < 0) {
        brightness_pct = 0;
    }
    if (brightness_pct > 100) {
        brightness_pct = 100;
    }

    const uint32_t duty = (uint32_t)(brightness_pct * 1023 / 100);
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, duty);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
}

int bsp_display_get_h_res(void)
{
    return BSP_LCD_H_RES;
}

int bsp_display_get_v_res(void)
{
    return BSP_LCD_V_RES;
}

void bsp_display_get_frame_buffers(void **buf1, void **buf2)
{
    if (buf1 != NULL) {
        *buf1 = s_fb0;
    }
    if (buf2 != NULL) {
        *buf2 = s_fb1;
    }
}

/* Periodic resync interval (ms).  Catches any stray SPI corruption from
 * sources we don't explicitly track (Class C mode transitions, IO expander
 * interrupt glitches, etc.).  Keep this long enough to avoid unnecessary
 * flicker but short enough that a black screen doesn't last too long. */
#define RESYNC_PERIODIC_MS  5000

static void resync_task(void *arg)
{
    (void)arg;

    for (;;) {
        /* Wait for an explicit signal OR the periodic timeout. */
        uint32_t got = ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(RESYNC_PERIODIC_MS));

        if (got > 0) {
            /* Debounce: wait for SPI activity to settle.  sendReceive()
             * does many rapid SPI transactions — we don't want to resync
             * between each one. */
            while (ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(200)) > 0) {
                /* Another SPI burst arrived, reset the timer. */
            }
        }

        /* On periodic timeout with no pending flag, skip the resync —
         * no SPI interference was detected. */
        if (got == 0 && !atomic_exchange(&s_resync_pending, false)) {
            continue;
        }
        atomic_exchange(&s_resync_pending, false);

        SemaphoreHandle_t spi_mtx = bsp_shared_spi_mutex();
        if (spi_mtx) {
            xSemaphoreTake(spi_mtx, portMAX_DELAY);
        }

        int64_t t0 = esp_timer_get_time();
        st7701s_flush_spi_state();
        st7701s_write_registers(true);
        io_exp_set(BSP_IO_EXP_LCD_CS, 1);
        gpio_set_level(LCD_SPI_CLK_IO, 1);
        gpio_set_level(LCD_SPI_SDO_IO, 1);
        int64_t t1 = esp_timer_get_time();

        if (spi_mtx) {
            xSemaphoreGive(spi_mtx);
        }

        esp_lcd_rgb_panel_restart(s_panel);
        ESP_LOGI(TAG, "Display resync (%s): SPI reprogram %lld us",
                 got > 0 ? "triggered" : "periodic", (long long)(t1 - t0));
    }
}

esp_err_t bsp_display_rgb_restart(void)
{
    if (s_panel == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    atomic_store(&s_resync_pending, true);
    if (s_resync_task) {
        xTaskNotifyGive(s_resync_task);
    }
    return ESP_OK;
}
