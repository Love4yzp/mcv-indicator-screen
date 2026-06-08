/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Spencer (love4yzp)
 */

#include "bsp_indicator.h"
#include "bsp_internal.h"

#include "driver/i2c_master.h"
#include "esp_bit_defs.h"
#include "esp_check.h"
#include "esp_io_expander_tca95xx_16bit.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

static const char *TAG = "bsp_io_exp";
esp_io_expander_handle_t bsp_io_expander_handle = NULL;
static SemaphoreHandle_t s_io_exp_mutex = NULL;
static SemaphoreHandle_t s_shared_spi_mutex = NULL;

static esp_err_t probe_expander_addr(i2c_master_bus_handle_t bus, uint8_t *addr_out)
{
    esp_err_t ret = i2c_master_probe(bus, BSP_IO_EXPANDER_I2C_ADDR_PRIMARY, -1);
    if (ret == ESP_OK) {
        *addr_out = BSP_IO_EXPANDER_I2C_ADDR_PRIMARY;
        return ESP_OK;
    }
    if (ret != ESP_ERR_NOT_FOUND) {
        return ret;
    }

    ret = i2c_master_probe(bus, BSP_IO_EXPANDER_I2C_ADDR_FALLBACK, -1);
    if (ret == ESP_OK) {
        *addr_out = BSP_IO_EXPANDER_I2C_ADDR_FALLBACK;
    }
    return ret;
}

static esp_err_t configure_output_pin(uint32_t pin_mask, uint8_t level)
{
    ESP_RETURN_ON_ERROR(esp_io_expander_set_dir(bsp_io_expander_handle, pin_mask, IO_EXPANDER_OUTPUT), TAG,
                        "Failed to set output direction");
    return esp_io_expander_set_level(bsp_io_expander_handle, pin_mask, level);
}

esp_io_expander_handle_t bsp_get_io_expander(void)
{
    return bsp_io_expander_handle;
}

esp_err_t bsp_io_expander_init(void)
{
    if (bsp_io_expander_handle != NULL) {
        return ESP_OK;
    }

    i2c_master_bus_handle_t bus = bsp_get_i2c_bus();
    ESP_RETURN_ON_FALSE(bus != NULL, ESP_ERR_INVALID_STATE, TAG, "I2C bus not initialized");

    uint8_t expander_addr = 0;
    ESP_RETURN_ON_ERROR(probe_expander_addr(bus, &expander_addr), TAG, "TCA9535 probe failed");
    ESP_RETURN_ON_ERROR(esp_io_expander_new_i2c_tca95xx_16bit(bus, expander_addr, &bsp_io_expander_handle), TAG,
                        "Failed to create TCA9535 handle");
    ESP_LOGI(TAG, "TCA9535 detected at 0x%02X", expander_addr);

    ESP_RETURN_ON_ERROR(configure_output_pin(BIT(BSP_IO_EXP_TOUCH_RST), 0), TAG, "Touch reset low failed");
    ESP_RETURN_ON_ERROR(configure_output_pin(BIT(BSP_IO_EXP_LCD_CS), 1), TAG, "LCD CS init failed");
    ESP_RETURN_ON_ERROR(configure_output_pin(BIT(BSP_IO_EXP_LCD_RST), 1), TAG, "LCD reset init failed");
    ESP_RETURN_ON_ERROR(configure_output_pin(BIT(BSP_IO_EXP_RP2040_RST), 1), TAG, "RP2040 reset init failed");
    ESP_RETURN_ON_ERROR(configure_output_pin(BIT(BSP_IO_EXP_BMP_PWR), 1), TAG, "BMP power init failed");

    vTaskDelay(pdMS_TO_TICKS(5));
    ESP_RETURN_ON_ERROR(esp_io_expander_set_level(bsp_io_expander_handle, BIT(BSP_IO_EXP_TOUCH_RST), 1), TAG,
                        "Touch reset release failed");

    s_io_exp_mutex = xSemaphoreCreateMutex();
    assert(s_io_exp_mutex);

    s_shared_spi_mutex = xSemaphoreCreateMutex();
    assert(s_shared_spi_mutex);

    return ESP_OK;
}

SemaphoreHandle_t bsp_shared_spi_mutex(void)
{
    return s_shared_spi_mutex;
}

esp_err_t bsp_io_expander_set_level_safe(uint32_t pin_mask, uint8_t level)
{
    if (!bsp_io_expander_handle || !s_io_exp_mutex) {
        return esp_io_expander_set_level(bsp_io_expander_handle, pin_mask, level);
    }
    xSemaphoreTake(s_io_exp_mutex, portMAX_DELAY);
    esp_err_t ret = esp_io_expander_set_level(bsp_io_expander_handle, pin_mask, level);
    xSemaphoreGive(s_io_exp_mutex);
    return ret;
}

esp_err_t bsp_io_expander_set_dir_safe(uint32_t pin_mask, esp_io_expander_dir_t dir)
{
    if (!bsp_io_expander_handle || !s_io_exp_mutex) {
        return esp_io_expander_set_dir(bsp_io_expander_handle, pin_mask, dir);
    }
    xSemaphoreTake(s_io_exp_mutex, portMAX_DELAY);
    esp_err_t ret = esp_io_expander_set_dir(bsp_io_expander_handle, pin_mask, dir);
    xSemaphoreGive(s_io_exp_mutex);
    return ret;
}

esp_err_t bsp_io_expander_get_level_safe(uint32_t pin_mask, uint32_t *level)
{
    if (!bsp_io_expander_handle || !s_io_exp_mutex) {
        return esp_io_expander_get_level(bsp_io_expander_handle, pin_mask, level);
    }
    xSemaphoreTake(s_io_exp_mutex, portMAX_DELAY);
    esp_err_t ret = esp_io_expander_get_level(bsp_io_expander_handle, pin_mask, level);
    xSemaphoreGive(s_io_exp_mutex);
    return ret;
}
