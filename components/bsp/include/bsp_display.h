/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Spencer (love4yzp)
 */

#pragma once

#include <stdbool.h>

#include "esp_err.h"

typedef struct esp_lcd_panel_t *esp_lcd_panel_handle_t;

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t bsp_display_init(esp_lcd_panel_handle_t *panel_handle_out);
void bsp_display_backlight_set(int brightness_pct);
esp_err_t bsp_display_rgb_restart(void);
int bsp_display_get_h_res(void);
int bsp_display_get_v_res(void);
void bsp_display_get_frame_buffers(void **buf1, void **buf2);

#ifdef __cplusplus
}
#endif
