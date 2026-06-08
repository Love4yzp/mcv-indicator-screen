/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Spencer (love4yzp)
 *
 * P1 Data Page — Air Quality Dashboard
 * Tailored to SEN54 capabilities: 2 primary cards (PM2.5, Temp) +
 * 4 secondary cards (RH, VOC, PM10, PM1.0).
 * Values are threshold-colored: green (good), orange (warn), red (bad).
 */

#include "app/pages/page_sensor.h"

#include <stdio.h>

#include "app/fonts/app_fonts.h"
#include "lvgl.h"

/* ═══════ Colors ═══════ */
#define C_BG       lv_color_hex(0x050816)
#define C_CARD     lv_color_hex(0x10192A)
#define C_CARD_BRD lv_color_hex(0x1A2744)  /* subtle card border */
#define C_TEXT     lv_color_hex(0xF5F7FA)
#define C_DIM      lv_color_hex(0x93A1B5)
#define C_GOOD     lv_color_hex(0x5BD5A8)
#define C_WARN     lv_color_hex(0xF97316)
#define C_BAD      lv_color_hex(0xF44336)

/* ═══════ Thresholds ═══════ */
typedef enum { LEVEL_GOOD, LEVEL_WARN, LEVEL_BAD } air_level_t;

static air_level_t level_pm25(float v)   { return v < 35 ? LEVEL_GOOD : v < 75  ? LEVEL_WARN : LEVEL_BAD; }
static air_level_t level_pm10(float v)   { return v < 50 ? LEVEL_GOOD : v < 150 ? LEVEL_WARN : LEVEL_BAD; }
static air_level_t level_voc(float v)    { return v < 100 ? LEVEL_GOOD : v < 250 ? LEVEL_WARN : LEVEL_BAD; }

static air_level_t level_temp(float v)
{
    if (v >= 20 && v <= 26) return LEVEL_GOOD;
    if (v >= 18 && v <= 28) return LEVEL_WARN;
    return LEVEL_BAD;
}

static air_level_t level_rh(float v)
{
    if (v >= 30 && v <= 60) return LEVEL_GOOD;
    if (v >= 20 && v <= 70) return LEVEL_WARN;
    return LEVEL_BAD;
}

static lv_color_t level_color(air_level_t l)
{
    switch (l) {
        case LEVEL_GOOD: return C_GOOD;
        case LEVEL_WARN: return C_WARN;
        case LEVEL_BAD:  return C_BAD;
    }
    return C_DIM;
}

static air_level_t worst(air_level_t a, air_level_t b) { return a > b ? a : b; }

/* Chinese level labels: 优 / 注意 / 行动 */
static const char *level_text_cn(air_level_t l)
{
    switch (l) {
        case LEVEL_GOOD: return "\xE4\xBC\x98";          /* 优 */
        case LEVEL_WARN: return "\xE6\xB3\xA8\xE6\x84\x8F"; /* 注意 */
        case LEVEL_BAD:  return "\xE8\xA1\x8C\xE5\x8A\xA8"; /* 行动 */
    }
    return "?";
}

/* ═══════ Static UI refs ═══════ */
static lv_obj_t *s_alert;
static lv_obj_t *s_badge;
static lv_obj_t *s_badge_label;
static lv_obj_t *s_val[6];  /* 0=PM2.5, 1=Temp, 2=RH, 3=VOC, 4=PM10, 5=PM1.0 */
static lv_obj_t *s_dot[6];  /* status dots per metric */

/* ═══════ Card helpers ═══════ */

static lv_obj_t *s_unit[6]; /* unit labels (updated with color alongside value) */

static lv_obj_t *create_card(lv_obj_t *parent, int w, int h, int pad, int radius,
                             const char *label_text, const char *unit_text,
                             const char *initial, int idx)
{
    lv_obj_t *card = lv_obj_create(parent);
    lv_obj_set_size(card, w, h);
    lv_obj_set_style_bg_color(card, C_CARD, 0);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(card, 1, 0);
    lv_obj_set_style_border_color(card, C_CARD_BRD, 0);
    lv_obj_set_style_radius(card, radius, 0);
    lv_obj_set_style_pad_all(card, pad, 0);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);

    /* Metric name (top-left) */
    lv_obj_t *lbl = lv_label_create(card);
    lv_label_set_text(lbl, label_text);
    lv_obj_set_style_text_color(lbl, C_DIM, 0);
    lv_obj_set_style_text_font(lbl, &app_font_14, 0);
    lv_obj_set_style_text_letter_space(lbl, 1, 0);
    lv_obj_align(lbl, LV_ALIGN_TOP_LEFT, 0, 0);

    /* Status dot (top-right, colored by threshold) */
    lv_obj_t *dot = lv_obj_create(card);
    lv_obj_remove_style_all(dot);
    lv_obj_set_size(dot, 8, 8);
    lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(dot, C_DIM, 0);
    lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, 0);
    lv_obj_align(dot, LV_ALIGN_TOP_RIGHT, 0, 4);

    /* Value (bottom-left, dominant) */
    lv_obj_t *val = lv_label_create(card);
    lv_label_set_text(val, initial);
    lv_obj_set_style_text_color(val, C_DIM, 0);
    lv_obj_set_style_text_font(val, &app_font_32, 0);
    lv_obj_align(val, LV_ALIGN_BOTTOM_LEFT, 0, -16);

    /* Unit (bottom-left, below value) */
    lv_obj_t *unit = lv_label_create(card);
    lv_label_set_text(unit, unit_text);
    lv_obj_set_style_text_color(unit, lv_color_hex(0x4A5A6E), 0);
    lv_obj_set_style_text_font(unit, &app_font_14, 0);
    lv_obj_align(unit, LV_ALIGN_BOTTOM_LEFT, 0, 0);

    if (idx >= 0 && idx < 6) {
        s_unit[idx] = unit;
        s_dot[idx] = dot;
    }
    return val;
}

/* ═══════ Page lifecycle ═══════ */

void page_sensor_create(lv_obj_t *parent)
{
    lv_obj_set_style_bg_color(parent, C_BG, 0);
    lv_obj_set_style_bg_opa(parent, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(parent, 20, 0);
    lv_obj_clear_flag(parent, LV_OBJ_FLAG_SCROLLABLE);

    /* ── Header ── */
    /* Chinese title: 空气质量 */
    lv_obj_t *title = lv_label_create(parent);
    lv_label_set_text(title, "\xE7\xA9\xBA\xE6\xB0\x94\xE8\xB4\xA8\xE9\x87\x8F");
    lv_obj_set_style_text_color(title, C_TEXT, 0);
    lv_obj_set_style_text_font(title, &app_font_cn_20, 0);
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, 0, 0);

    /* Status badge (top-right pill) */
    s_badge = lv_obj_create(parent);
    lv_obj_set_size(s_badge, LV_SIZE_CONTENT, 28);
    lv_obj_set_style_bg_color(s_badge, C_DIM, 0);
    lv_obj_set_style_bg_opa(s_badge, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(s_badge, 0, 0);
    lv_obj_set_style_radius(s_badge, 14, 0);
    lv_obj_set_style_pad_hor(s_badge, 12, 0);
    lv_obj_set_style_pad_ver(s_badge, 4, 0);
    lv_obj_clear_flag(s_badge, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align(s_badge, LV_ALIGN_TOP_RIGHT, 0, -2);

    s_badge_label = lv_label_create(s_badge);
    lv_label_set_text(s_badge_label, "Offline");
    lv_obj_set_style_text_color(s_badge_label, lv_color_white(), 0);
    lv_obj_set_style_text_font(s_badge_label, &app_font_cn_14, 0);
    lv_obj_center(s_badge_label);

    /* ── Alert (hidden by default, overlays primary cards) ── */
    s_alert = lv_label_create(parent);
    lv_label_set_text(s_alert, "");
    lv_obj_set_size(s_alert, 440, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_color(s_alert, C_WARN, 0);
    lv_obj_set_style_bg_opa(s_alert, LV_OPA_COVER, 0);
    lv_obj_set_style_text_color(s_alert, lv_color_white(), 0);
    lv_obj_set_style_text_font(s_alert, &app_font_cn_14, 0);
    lv_obj_set_style_radius(s_alert, 14, 0);
    lv_obj_set_style_pad_all(s_alert, 12, 0);
    lv_obj_align(s_alert, LV_ALIGN_TOP_LEFT, 0, 40);
    lv_obj_add_flag(s_alert, LV_OBJ_FLAG_HIDDEN);

    /* ── Primary cards row (PM2.5 + CO₂) ── */
    lv_obj_t *primary = lv_obj_create(parent);
    lv_obj_remove_style_all(primary);
    lv_obj_set_size(primary, 440, 180);
    lv_obj_set_layout(primary, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(primary, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(primary, 12, 0);
    lv_obj_align(primary, LV_ALIGN_TOP_LEFT, 0, 40);
    lv_obj_clear_flag(primary, LV_OBJ_FLAG_SCROLLABLE);

    s_val[0] = create_card(primary, 214, 180, 16, 20, "PM2.5", "\xC2\xB5g/m\xC2\xB3", "--", 0);
    s_val[1] = create_card(primary, 214, 180, 16, 20, "Temp", "\xC2\xB0" "C", "--", 1);

    /* ── Secondary cards row (RH + VOC + PM10 + PM1.0) ── */
    lv_obj_t *secondary = lv_obj_create(parent);
    lv_obj_remove_style_all(secondary);
    lv_obj_set_size(secondary, 440, 150);
    lv_obj_set_layout(secondary, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(secondary, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(secondary, 8, 0);
    lv_obj_align(secondary, LV_ALIGN_TOP_LEFT, 0, 232);
    lv_obj_clear_flag(secondary, LV_OBJ_FLAG_SCROLLABLE);

    s_val[2] = create_card(secondary, 103, 150, 12, 16, "RH",    "%",              "--", 2);
    s_val[3] = create_card(secondary, 103, 150, 12, 16, "VOC",   "idx",            "--", 3);
    s_val[4] = create_card(secondary, 103, 150, 12, 16, "PM10",  "\xC2\xB5g/m\xC2\xB3", "--", 4);
    s_val[5] = create_card(secondary, 103, 150, 12, 16, "PM1.0", "\xC2\xB5g/m\xC2\xB3", "--", 5);
}

void page_sensor_update(const sensor_data_t *data)
{
    if (data == NULL) {
        return;
    }

    if (!data->rp2040_connected) {
        for (int i = 0; i < 6; i++) {
            lv_label_set_text(s_val[i], "--");
            lv_obj_set_style_text_color(s_val[i], C_DIM, 0);
            if (s_dot[i]) lv_obj_set_style_bg_color(s_dot[i], C_DIM, 0);
        }
        lv_label_set_text(s_badge_label, "Offline");
        lv_obj_set_style_bg_color(s_badge, C_DIM, 0);
        return;
    }

    static char buf[6][32];

    snprintf(buf[0], sizeof(buf[0]), "%.1f", data->pm2_5);
    snprintf(buf[1], sizeof(buf[1]), "%.1f", data->temp);
    snprintf(buf[2], sizeof(buf[2]), "%.0f", data->humidity);
    snprintf(buf[3], sizeof(buf[3]), "%.0f", data->voc_index);
    snprintf(buf[4], sizeof(buf[4]), "%.1f", data->pm10);
    snprintf(buf[5], sizeof(buf[5]), "%.1f", data->pm1_0);

    air_level_t levels[6];
    levels[0] = level_pm25(data->pm2_5);
    levels[1] = level_temp(data->temp);
    levels[2] = level_rh(data->humidity);
    levels[3] = level_voc(data->voc_index);
    levels[4] = level_pm10(data->pm10);
    levels[5] = level_pm25(data->pm1_0);  /* PM1.0 uses same thresholds as PM2.5 (WHO-aligned) */

    /* Update labels, colors, and dots */
    for (int i = 0; i < 6; i++) {
        lv_label_set_text(s_val[i], buf[i]);
        lv_color_t c = level_color(levels[i]);
        lv_obj_set_style_text_color(s_val[i], c, 0);
        if (s_dot[i]) lv_obj_set_style_bg_color(s_dot[i], c, 0);
    }

    /* Badge: worst of all */
    air_level_t overall = LEVEL_GOOD;
    for (int i = 0; i < 6; i++) {
        overall = worst(overall, levels[i]);
    }
    lv_label_set_text(s_badge_label, level_text_cn(overall));
    lv_obj_set_style_bg_color(s_badge, level_color(overall), 0);
}

void page_sensor_show_alert(const char *message)
{
    if (s_alert == NULL || message == NULL) {
        return;
    }
    lv_label_set_text(s_alert, message);
    lv_obj_clear_flag(s_alert, LV_OBJ_FLAG_HIDDEN);
}

void page_sensor_hide_alert(void)
{
    if (s_alert != NULL) {
        lv_obj_add_flag(s_alert, LV_OBJ_FLAG_HIDDEN);
    }
}
