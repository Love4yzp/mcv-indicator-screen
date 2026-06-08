/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Spencer (love4yzp)
 */

#include "app/pages/page_hello.h"

#include <stdio.h>

#include "app/fonts/app_fonts.h"
#include "lvgl.h" // IWYU pragma: keep

#define COLOR_BG        lv_color_hex(0x050816)
#define COLOR_PANEL     lv_color_hex(0x10192A)
#define COLOR_TEXT      lv_color_hex(0xF5F7FA)
#define COLOR_MUTED     lv_color_hex(0x93A1B5)
#define COLOR_ACCENT    lv_color_hex(0x4CC9F0)
#define COLOR_GOOD      lv_color_hex(0x5BD5A8)

static lv_obj_t *s_status_value;
static lv_obj_t *s_sensor_value;

static lv_obj_t *create_info_row(lv_obj_t *parent, const char *label_text, const char *value_text)
{
    lv_obj_t *row = lv_obj_create(parent);
    lv_obj_remove_style_all(row);
    lv_obj_set_width(row, lv_pct(100));
    lv_obj_set_height(row, LV_SIZE_CONTENT);
    lv_obj_set_layout(row, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_ver(row, 6, 0);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *label = lv_label_create(row);
    lv_label_set_text(label, label_text);
    lv_obj_set_style_text_color(label, COLOR_MUTED, 0);
    lv_obj_set_style_text_font(label, &app_font_14, 0);

    lv_obj_t *value = lv_label_create(row);
    lv_label_set_text(value, value_text);
    lv_obj_set_style_text_color(value, COLOR_TEXT, 0);
    lv_obj_set_style_text_font(value, &app_font_14, 0);

    return value;
}

void page_hello_create(lv_obj_t *parent)
{
    lv_obj_set_style_bg_color(parent, COLOR_BG, 0);
    lv_obj_set_style_bg_opa(parent, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(parent, 24, 0);
    lv_obj_clear_flag(parent, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *content = lv_obj_create(parent);
    lv_obj_remove_style_all(content);
    lv_obj_set_size(content, lv_pct(100), lv_pct(100));
    lv_obj_set_layout(content, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(content, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(content, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_row(content, 16, 0);
    lv_obj_clear_flag(content, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *eyebrow = lv_label_create(content);
    lv_label_set_text(eyebrow, "Phase 2 app layer");
    lv_obj_set_style_text_color(eyebrow, COLOR_ACCENT, 0);
    lv_obj_set_style_text_font(eyebrow, &app_font_14, 0);

    lv_obj_t *title = lv_label_create(content);
    lv_label_set_text(title, "Hello Indicator");
    lv_obj_set_style_text_color(title, COLOR_TEXT, 0);
    lv_obj_set_style_text_font(title, &app_font_32, 0);

    lv_obj_t *subtitle = lv_label_create(content);
    lv_obj_set_width(subtitle, lv_pct(100));
    lv_label_set_text(subtitle,
                      "app_manager now owns the page container so new pages can follow the same create/update convention.");
    lv_label_set_long_mode(subtitle, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_color(subtitle, COLOR_MUTED, 0);
    lv_obj_set_style_text_font(subtitle, &app_font_14, 0);

    lv_obj_t *panel = lv_obj_create(content);
    lv_obj_set_width(panel, lv_pct(100));
    lv_obj_set_height(panel, LV_SIZE_CONTENT);
    lv_obj_set_style_radius(panel, 20, 0);
    lv_obj_set_style_bg_color(panel, COLOR_PANEL, 0);
    lv_obj_set_style_bg_opa(panel, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(panel, 0, 0);
    lv_obj_set_style_pad_all(panel, 20, 0);
    lv_obj_set_layout(panel, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(panel, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(panel, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_row(panel, 4, 0);
    lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *panel_title = lv_label_create(panel);
    lv_label_set_text(panel_title, "System info");
    lv_obj_set_style_text_color(panel_title, COLOR_TEXT, 0);
    lv_obj_set_style_text_font(panel_title, &app_font_20, 0);

    create_info_row(panel, "Display", "480x480 RGB");
    create_info_row(panel, "Pages", "1 registered");
    create_info_row(panel, "Services", "reserved for platform logic");
    s_status_value = create_info_row(panel, "RP2040 link", "not connected yet");
    s_sensor_value = create_info_row(panel, "Latest sample", "waiting for sensor data");
}

void page_hello_update(const sensor_data_t *data)
{
    if (data == NULL || s_status_value == NULL || s_sensor_value == NULL) {
        return;
    }

    if (data->rp2040_connected) {
        lv_label_set_text(s_status_value, "connected");
        lv_obj_set_style_text_color(s_status_value, COLOR_GOOD, 0);
    } else {
        lv_label_set_text(s_status_value, "not connected yet");
        lv_obj_set_style_text_color(s_status_value, COLOR_TEXT, 0);
    }

    if (data->pm2_5 > 0.0f || data->temp > 0.0f || data->humidity > 0.0f || data->voc_index > 0.0f) {
        static char sensor_text[96];
        snprintf(sensor_text, sizeof(sensor_text), "PM2.5 %.1f \xC2\xB5g/m\xC2\xB3 | T %.1f \xC2\xB0" "C | RH %.1f %%",
                 data->pm2_5, data->temp, data->humidity);
        lv_label_set_text(s_sensor_value, sensor_text);
    } else {
        lv_label_set_text(s_sensor_value, "waiting for sensor data");
    }
}
