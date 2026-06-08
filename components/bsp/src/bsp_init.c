/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Spencer (love4yzp)
 */

#include "bsp_indicator.h"
#include "bsp_internal.h"

#include "esp_check.h"
#include "esp_log.h"

static const char *TAG = "bsp_init";

esp_err_t bsp_indicator_init(void)
{
    ESP_LOGI(TAG, "Initializing SenseCAP Indicator BSP");
    ESP_RETURN_ON_ERROR(bsp_i2c_init(), TAG, "I2C init failed");
    ESP_RETURN_ON_ERROR(bsp_io_expander_init(), TAG, "IO expander init failed");
    ESP_RETURN_ON_ERROR(bsp_button_init(NULL), TAG, "Button init failed");
    ESP_LOGI(TAG, "BSP initialization complete");
    return ESP_OK;
}
