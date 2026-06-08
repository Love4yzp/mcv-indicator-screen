/*
 * Custom fonts: built-in Montserrat with extended-Latin fallback (µ ² ³ °).
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "lvgl.h"

/* Non-const copies of the built-in Montserrat fonts with fallback set. */
extern lv_font_t app_font_14;
extern lv_font_t app_font_20;
extern lv_font_t app_font_32;

/* Chinese fonts (Noto Sans SC, subset of ~53 glyphs for UI labels). */
extern lv_font_t app_font_cn_14;
extern lv_font_t app_font_cn_20;

/* Must be called once before any UI creation. */
void app_fonts_init(void);
