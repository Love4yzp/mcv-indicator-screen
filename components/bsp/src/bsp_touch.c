/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Spencer (love4yzp)
 */

#include "bsp_touch.h"
#include "bsp_indicator.h"
#include "bsp_internal.h"

#include <stdlib.h>

#include "driver/i2c_master.h"
#include "esp_check.h"
#include "esp_lcd_touch.h"
#include "esp_log.h"

static const char *TAG = "bsp_touch";

#define FT_REG_TOUCH_POINTS  0x02
#define FT_REG_TOUCH1_XH     0x03
#define FT_REG_THGROUP       0x80
#define FT_REG_THPEAK        0x81
#define FT_REG_PERIODACTIVE  0x88

static i2c_master_dev_handle_t s_dev = NULL;
static esp_lcd_touch_handle_t s_tp = NULL;
static uint16_t s_last_x = 0;
static uint16_t s_last_y = 0;
static bool s_pressed = false;

static esp_err_t ft_read_reg(uint8_t reg, uint8_t *buf, size_t len)
{
    return i2c_master_transmit_receive(s_dev, &reg, 1, buf, len, 100);
}

static esp_err_t ft_write_reg(uint8_t reg, uint8_t val)
{
    uint8_t buf[2] = {reg, val};
    return i2c_master_transmit(s_dev, buf, sizeof(buf), 100);
}

static esp_err_t touch_read_data(esp_lcd_touch_handle_t tp)
{
    (void)tp;

    uint8_t num = 0;
    esp_err_t ret = ft_read_reg(FT_REG_TOUCH_POINTS, &num, 1);
    if (ret != ESP_OK) {
        return ret;
    }

    num &= 0x0FU;
    if (num > 0 && num < 6) {
        uint8_t data[4];
        ret = ft_read_reg(FT_REG_TOUCH1_XH, data, sizeof(data));
        if (ret != ESP_OK) {
            return ret;
        }

        uint16_t raw_x = (uint16_t)(((data[0] & 0x0FU) << 8) | data[1]);
        uint16_t raw_y = (uint16_t)(((data[2] & 0x0FU) << 8) | data[3]);

        if (raw_x >= BSP_LCD_H_RES) {
            raw_x = BSP_LCD_H_RES - 1;
        }
        if (raw_y >= BSP_LCD_V_RES) {
            raw_y = BSP_LCD_V_RES - 1;
        }

        s_last_x = raw_x;
        s_last_y = raw_y;
        s_pressed = true;
    } else {
        s_pressed = false;
    }

    return ESP_OK;
}

static bool touch_get_xy(esp_lcd_touch_handle_t tp, uint16_t *x, uint16_t *y,
                         uint16_t *strength, uint8_t *cnt, uint8_t max)
{
    (void)tp;

    if (s_pressed && max > 0) {
        *x = s_last_x;
        *y = s_last_y;
        if (strength != NULL) {
            *strength = 0;
        }
        *cnt = 1;
        return true;
    }

    *cnt = 0;
    return false;
}

static esp_err_t touch_del(esp_lcd_touch_handle_t tp)
{
    if (tp != NULL) {
        free(tp);
        s_tp = NULL;
    }
    return ESP_OK;
}

esp_err_t bsp_touch_init(esp_lcd_touch_handle_t *tp_handle_out)
{
    if (s_tp != NULL) {
        if (tp_handle_out != NULL) {
            *tp_handle_out = s_tp;
        }
        return ESP_OK;
    }

    const i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = BSP_TOUCH_I2C_ADDR,
        .scl_speed_hz = BSP_I2C_FREQ_HZ,
    };
    ESP_RETURN_ON_ERROR(i2c_master_bus_add_device(bsp_get_i2c_bus(), &dev_cfg, &s_dev), TAG,
                        "I2C device add failed");

    (void)ft_write_reg(FT_REG_THGROUP, 70);
    (void)ft_write_reg(FT_REG_THPEAK, 60);
    (void)ft_write_reg(FT_REG_PERIODACTIVE, 12);

    s_tp = calloc(1, sizeof(esp_lcd_touch_t));
    ESP_RETURN_ON_FALSE(s_tp != NULL, ESP_ERR_NO_MEM, TAG, "alloc failed");

    s_tp->config = (esp_lcd_touch_config_t){
        .x_max = BSP_LCD_H_RES,
        .y_max = BSP_LCD_V_RES,
        .rst_gpio_num = GPIO_NUM_NC,
        .int_gpio_num = GPIO_NUM_NC,
        .flags = {
            .swap_xy = 0,
            .mirror_x = 1,
            .mirror_y = 1,
        },
    };
    s_tp->read_data = touch_read_data;
    s_tp->get_xy = touch_get_xy;
    s_tp->del = touch_del;

    if (tp_handle_out != NULL) {
        *tp_handle_out = s_tp;
    }

    ESP_LOGI(TAG, "FT6336U touch ready (I2C 0x%02X)", BSP_TOUCH_I2C_ADDR);
    return ESP_OK;
}
