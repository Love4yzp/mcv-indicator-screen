/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Spencer (love4yzp)
 */

#pragma once

#include "driver/i2c_master.h"
#include "iot_button.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t bsp_i2c_init(void);
i2c_master_bus_handle_t bsp_get_i2c_bus(void);
esp_err_t bsp_io_expander_init(void);
esp_err_t bsp_button_init(button_handle_t *btn_handle_out);

#ifdef __cplusplus
}
#endif
