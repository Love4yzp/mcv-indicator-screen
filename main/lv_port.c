/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Spencer (love4yzp)
 */

#include "lv_port.h"

#include "bsp_display.h"
#include "bsp_touch.h"
#include "esp_check.h"
#include "esp_lcd_touch.h"
#include "esp_log.h"
#include "esp_lvgl_port.h"
#include "esp_timer.h"
#include "sdkconfig.h"

static const char *TAG = "lv_port";

typedef struct {
    esp_lcd_touch_handle_t handle;
    lv_display_t *disp;
} local_touch_ctx_t;

static void local_touch_read(lv_indev_t *indev, lv_indev_data_t *data)
{
    local_touch_ctx_t *ctx = (local_touch_ctx_t *)lv_indev_get_driver_data(indev);
    static int64_t s_last_touch_warn_us = 0;
    uint8_t touch_cnt = 0;
    esp_lcd_touch_point_data_t touch_data[CONFIG_ESP_LCD_TOUCH_MAX_POINTS] = {0};

    if (ctx == NULL || ctx->handle == NULL) {
        data->state = LV_INDEV_STATE_RELEASED;
        return;
    }

    esp_err_t ret = esp_lcd_touch_read_data(ctx->handle);
    if (ret != ESP_OK) {
        int64_t now = esp_timer_get_time();
        if (now - s_last_touch_warn_us > 1000000) {
            ESP_LOGW(TAG, "Touch read failed: %s", esp_err_to_name(ret));
            s_last_touch_warn_us = now;
        }
        data->state = LV_INDEV_STATE_RELEASED;
        return;
    }

    ret = esp_lcd_touch_get_data(ctx->handle, touch_data, &touch_cnt, CONFIG_ESP_LCD_TOUCH_MAX_POINTS);
    if (ret != ESP_OK) {
        int64_t now = esp_timer_get_time();
        if (now - s_last_touch_warn_us > 1000000) {
            ESP_LOGW(TAG, "Touch get-data failed: %s", esp_err_to_name(ret));
            s_last_touch_warn_us = now;
        }
        data->state = LV_INDEV_STATE_RELEASED;
        return;
    }

    if (touch_cnt > 0) {
        data->point.x = touch_data[0].x;
        data->point.y = touch_data[0].y;
        data->state = LV_INDEV_STATE_PRESSED;
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
    }
}

esp_err_t lv_port_init(void)
{
    static bool s_initialized = false;

    if (s_initialized) {
        return ESP_OK;
    }

    esp_lcd_panel_handle_t panel = NULL;
    esp_lcd_touch_handle_t touch = NULL;

    ESP_RETURN_ON_ERROR(bsp_display_init(&panel), TAG, "Display init failed");

    esp_err_t touch_err = bsp_touch_init(&touch);
    if (touch_err != ESP_OK) {
        ESP_LOGW(TAG, "Touch init failed (0x%x)", touch_err);
        touch = NULL;
    }

    lvgl_port_cfg_t port_cfg = ESP_LVGL_PORT_INIT_CONFIG();
    port_cfg.task_priority = 4;
    port_cfg.task_stack = 16384;
    port_cfg.timer_period_ms = 5;
    ESP_RETURN_ON_ERROR(lvgl_port_init(&port_cfg), TAG, "LVGL port init failed");

    const lvgl_port_display_cfg_t disp_cfg = {
        .panel_handle = panel,
        .hres = (uint32_t)bsp_display_get_h_res(),
        .vres = (uint32_t)bsp_display_get_v_res(),
        .buffer_size = (uint32_t)(bsp_display_get_h_res() * bsp_display_get_v_res()),
        .double_buffer = true,
        .color_format = LV_COLOR_FORMAT_RGB565,
        .flags = {
            .buff_spiram = true,
            .buff_dma = false,
            .direct_mode = true,
        },
    };
    const lvgl_port_display_rgb_cfg_t rgb_cfg = {
        .flags = {
            .avoid_tearing = true,
        },
    };

    lv_display_t *disp = lvgl_port_add_disp_rgb(&disp_cfg, &rgb_cfg);
    ESP_RETURN_ON_FALSE(disp != NULL, ESP_FAIL, TAG, "LVGL display add failed");

    if (touch != NULL) {
        local_touch_ctx_t *touch_ctx = calloc(1, sizeof(local_touch_ctx_t));
        ESP_RETURN_ON_FALSE(touch_ctx != NULL, ESP_ERR_NO_MEM, TAG, "Touch ctx alloc failed");

        touch_ctx->handle = touch;
        touch_ctx->disp = disp;

        lvgl_port_lock(0);
        lv_indev_t *indev = lv_indev_create();
        if (indev != NULL) {
            lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
            lv_indev_set_read_cb(indev, local_touch_read);
            lv_indev_set_disp(indev, disp);
            lv_indev_set_driver_data(indev, touch_ctx);
        }
        lvgl_port_unlock();

        if (indev != NULL) {
            ESP_LOGI(TAG, "Touch input registered");
        } else {
            free(touch_ctx);
            ESP_LOGW(TAG, "LVGL touch registration failed");
        }
    }

    s_initialized = true;
    ESP_LOGI(TAG, "LVGL 9 initialized (%dx%d, touch=%s)", bsp_display_get_h_res(), bsp_display_get_v_res(),
             touch != NULL ? "yes" : "no");
    return ESP_OK;
}

void lv_port_lock(void)
{
    lvgl_port_lock(0);
}

void lv_port_unlock(void)
{
    lvgl_port_unlock();
}
