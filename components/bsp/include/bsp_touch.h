/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Spencer (love4yzp)
 */

#pragma once

#include "esp_err.h"
#include "esp_lcd_touch.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t bsp_touch_init(esp_lcd_touch_handle_t *tp_handle_out);

#ifdef __cplusplus
}
#endif
