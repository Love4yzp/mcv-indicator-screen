/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Spencer (love4yzp)
 */

#include "bsp_indicator.h"
#include "bsp_internal.h"

#include "button_gpio.h"
#include "esp_check.h"
#include "esp_log.h"
#include "iot_button.h"

static const char *TAG = "bsp_button";
static button_handle_t s_button_handle = NULL;

static void bsp_button_press_cb(void *button_handle, void *usr_data)
{
    (void)button_handle;
    (void)usr_data;
    ESP_LOGI(TAG, "Button pressed");
}

esp_err_t bsp_button_init(button_handle_t *btn_handle_out)
{
    if (s_button_handle == NULL) {
        const button_config_t button_config = {0};
        const button_gpio_config_t gpio_config = {
            .gpio_num = BSP_BUTTON_IO,
            .active_level = 0,
            .enable_power_save = false,
            .disable_pull = false,
        };

        ESP_RETURN_ON_ERROR(iot_button_new_gpio_device(&button_config, &gpio_config, &s_button_handle), TAG,
                            "Failed to create button");
        ESP_RETURN_ON_ERROR(iot_button_register_cb(s_button_handle, BUTTON_PRESS_DOWN, NULL, bsp_button_press_cb, NULL),
                            TAG, "Failed to register button callback");
    }

    if (btn_handle_out != NULL) {
        *btn_handle_out = s_button_handle;
    }
    return ESP_OK;
}
