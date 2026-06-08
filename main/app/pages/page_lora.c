/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Spencer (love4yzp)
 */

#include "app/pages/page_lora.h"

#include <stdio.h>
#include <string.h>

#include "app/fonts/app_fonts.h"
#include "lvgl.h"

#define C_BG     lv_color_hex(0x050816)
#define C_CARD   lv_color_hex(0x10192A)
#define C_TEXT   lv_color_hex(0xF5F7FA)
#define C_DIM    lv_color_hex(0x93A1B5)
#define C_GREEN  lv_color_hex(0x2DD4BF)
#define C_YELLOW lv_color_hex(0xFBBF24)
#define C_RED    lv_color_hex(0xEF4444)
#define C_ACCENT lv_color_hex(0x4CC9F0)

/* Interval presets (seconds) */
static const uint32_t s_intervals[] = {30, 60, 120, 300};
#define INTERVAL_COUNT (sizeof(s_intervals) / sizeof(s_intervals[0]))

/* Status display */
static lv_obj_t *s_status_dot;
static lv_obj_t *s_status_label;

/* Uplink card */
static lv_obj_t *s_uplink_count;
static lv_obj_t *s_uplink_result;

/* Downlink card */
static lv_obj_t *s_downlink_info;

/* Control area */
static lv_obj_t *s_mode_switch;
static lv_obj_t *s_mode_label;
static lv_obj_t *s_auto_panel;     /* interval buttons container */
static lv_obj_t *s_manual_panel;   /* send button container */
static lv_obj_t *s_interval_btns[INTERVAL_COUNT];
static lv_obj_t *s_apply_btn;
static uint32_t s_selected_interval = 60;  /* visual selection */
static uint32_t s_applied_interval = 60;   /* actually running */

static lv_obj_t *create_card(lv_obj_t *parent, int32_t w, int32_t h)
{
    lv_obj_t *card = lv_obj_create(parent);
    lv_obj_set_size(card, w, h);
    lv_obj_set_style_bg_color(card, C_CARD, 0);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(card, 0, 0);
    lv_obj_set_style_radius(card, 20, 0);
    lv_obj_set_style_pad_all(card, 16, 0);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
    return card;
}

static void update_interval_btn_styles(void)
{
    for (uint32_t i = 0; i < INTERVAL_COUNT; i++) {
        if (s_intervals[i] == s_selected_interval) {
            lv_obj_set_style_bg_color(s_interval_btns[i], C_ACCENT, 0);
            lv_obj_set_style_text_color(lv_obj_get_child(s_interval_btns[i], 0),
                                        lv_color_hex(0x050816), 0);
        } else {
            lv_obj_set_style_bg_color(s_interval_btns[i], C_CARD, 0);
            lv_obj_set_style_text_color(lv_obj_get_child(s_interval_btns[i], 0), C_DIM, 0);
        }
    }
}

static void update_apply_btn_state(void)
{
    if (s_apply_btn == NULL) return;
    bool changed = (s_selected_interval != s_applied_interval);
    lv_obj_set_style_bg_color(s_apply_btn, changed ? C_ACCENT : C_CARD, 0);
    lv_obj_set_style_text_color(lv_obj_get_child(s_apply_btn, 0),
                                changed ? lv_color_hex(0x050816) : C_DIM, 0);
}

static void interval_btn_cb(lv_event_t *event)
{
    uint32_t idx = (uint32_t)(uintptr_t)lv_event_get_user_data(event);
    if (idx >= INTERVAL_COUNT) return;
    s_selected_interval = s_intervals[idx];
    update_interval_btn_styles();
    update_apply_btn_state();
}

static void apply_btn_cb(lv_event_t *event)
{
    (void)event;
    if (s_selected_interval == s_applied_interval) return;
    s_applied_interval = s_selected_interval;
    update_apply_btn_state();

    const platform_callbacks_t *cbs = app_manager_get_callbacks();
    if (cbs != NULL && cbs->configure_lora_uplink != NULL) {
        cbs->configure_lora_uplink(true, s_applied_interval);
    }
}

static void send_btn_cb(lv_event_t *event)
{
    (void)event;
    const platform_callbacks_t *cbs = app_manager_get_callbacks();
    if (cbs != NULL && cbs->request_lora_uplink != NULL) {
        cbs->request_lora_uplink();
    }
}

static void mode_switch_cb(lv_event_t *event)
{
    lv_obj_t *sw = lv_event_get_target(event);
    bool is_auto = lv_obj_has_state(sw, LV_STATE_CHECKED);

    lv_label_set_text(s_mode_label, is_auto ? "Auto" : "Manual");

    if (is_auto) {
        /* Reset selection to match applied when switching back */
        s_selected_interval = s_applied_interval;
        update_interval_btn_styles();
        update_apply_btn_state();
        lv_obj_clear_flag(s_auto_panel, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(s_manual_panel, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(s_auto_panel, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(s_manual_panel, LV_OBJ_FLAG_HIDDEN);
    }

    const platform_callbacks_t *cbs = app_manager_get_callbacks();
    if (cbs != NULL && cbs->configure_lora_uplink != NULL) {
        cbs->configure_lora_uplink(is_auto, s_applied_interval);
    }
}

void page_lora_create(lv_obj_t *parent)
{
    lv_obj_set_style_bg_color(parent, C_BG, 0);
    lv_obj_set_style_bg_opa(parent, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(parent, 20, 0);
    lv_obj_clear_flag(parent, LV_OBJ_FLAG_SCROLLABLE);

    /* Title */
    lv_obj_t *title = lv_label_create(parent);
    lv_label_set_text(title, "LoRa");
    lv_obj_set_style_text_color(title, C_TEXT, 0);
    lv_obj_set_style_text_font(title, &app_font_20, 0);
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, 0, 0);

    lv_obj_t *subtitle = lv_label_create(parent);
    lv_label_set_text(subtitle, "LoRaWAN OTAA | D1L / D1Pro");
    lv_obj_set_style_text_color(subtitle, C_DIM, 0);
    lv_obj_set_style_text_font(subtitle, &app_font_14, 0);
    lv_obj_align(subtitle, LV_ALIGN_TOP_LEFT, 0, 30);

    /* ── Status card ── */
    lv_obj_t *status_card = create_card(parent, 420, 60);
    lv_obj_align(status_card, LV_ALIGN_TOP_MID, 0, 64);

    s_status_dot = lv_obj_create(status_card);
    lv_obj_remove_style_all(s_status_dot);
    lv_obj_set_size(s_status_dot, 12, 12);
    lv_obj_set_style_radius(s_status_dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(s_status_dot, C_DIM, 0);
    lv_obj_set_style_bg_opa(s_status_dot, LV_OPA_COVER, 0);
    lv_obj_align(s_status_dot, LV_ALIGN_LEFT_MID, 0, 0);

    s_status_label = lv_label_create(status_card);
    lv_label_set_text(s_status_label, "Idle");
    lv_obj_set_style_text_color(s_status_label, C_TEXT, 0);
    lv_obj_set_style_text_font(s_status_label, &app_font_20, 0);
    lv_obj_align(s_status_label, LV_ALIGN_LEFT_MID, 22, 0);

    /* ── Metric cards row ── */
    lv_obj_t *row = lv_obj_create(parent);
    lv_obj_remove_style_all(row);
    lv_obj_set_size(row, 420, 120);
    lv_obj_set_layout(row, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER);
    lv_obj_align(row, LV_ALIGN_TOP_MID, 0, 140);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

    /* Uplink card */
    lv_obj_t *ul_card = create_card(row, 200, 120);

    lv_obj_t *ul_title = lv_label_create(ul_card);
    lv_label_set_text(ul_title, "Uplinks");
    lv_obj_set_style_text_color(ul_title, C_DIM, 0);
    lv_obj_set_style_text_font(ul_title, &app_font_14, 0);
    lv_obj_align(ul_title, LV_ALIGN_TOP_LEFT, 0, 0);

    s_uplink_count = lv_label_create(ul_card);
    lv_label_set_text(s_uplink_count, "0");
    lv_obj_set_style_text_color(s_uplink_count, C_TEXT, 0);
    lv_obj_set_style_text_font(s_uplink_count, &app_font_32, 0);
    lv_obj_align(s_uplink_count, LV_ALIGN_LEFT_MID, 0, 4);

    s_uplink_result = lv_label_create(ul_card);
    lv_label_set_text(s_uplink_result, "");
    lv_obj_set_style_text_color(s_uplink_result, C_DIM, 0);
    lv_obj_set_style_text_font(s_uplink_result, &app_font_14, 0);
    lv_obj_align(s_uplink_result, LV_ALIGN_BOTTOM_LEFT, 0, 0);

    /* Downlink card */
    lv_obj_t *dl_card = create_card(row, 200, 120);

    lv_obj_t *dl_title = lv_label_create(dl_card);
    lv_label_set_text(dl_title, "Last Downlink");
    lv_obj_set_style_text_color(dl_title, C_DIM, 0);
    lv_obj_set_style_text_font(dl_title, &app_font_14, 0);
    lv_obj_align(dl_title, LV_ALIGN_TOP_LEFT, 0, 0);

    s_downlink_info = lv_label_create(dl_card);
    lv_label_set_text(s_downlink_info, "None");
    lv_obj_set_style_text_color(s_downlink_info, C_TEXT, 0);
    lv_obj_set_style_text_font(s_downlink_info, &app_font_14, 0);
    lv_obj_set_width(s_downlink_info, 168);
    lv_label_set_long_mode(s_downlink_info, LV_LABEL_LONG_WRAP);
    lv_obj_align(s_downlink_info, LV_ALIGN_BOTTOM_LEFT, 0, 0);

    /* ── Mode switch row ── */
    lv_obj_t *mode_row = create_card(parent, 420, 50);
    lv_obj_align(mode_row, LV_ALIGN_TOP_MID, 0, 276);
    lv_obj_set_style_pad_ver(mode_row, 8, 0);

    s_mode_label = lv_label_create(mode_row);
    lv_label_set_text(s_mode_label, "Auto");
    lv_obj_set_style_text_color(s_mode_label, C_TEXT, 0);
    lv_obj_set_style_text_font(s_mode_label, &app_font_14, 0);
    lv_obj_align(s_mode_label, LV_ALIGN_LEFT_MID, 0, 0);

    s_mode_switch = lv_switch_create(mode_row);
    lv_obj_set_size(s_mode_switch, 44, 24);
    lv_obj_add_state(s_mode_switch, LV_STATE_CHECKED); /* default: Auto */
    lv_obj_set_style_bg_color(s_mode_switch, C_DIM, 0);
    lv_obj_set_style_bg_color(s_mode_switch, C_ACCENT, LV_PART_INDICATOR | LV_STATE_CHECKED);
    lv_obj_align(s_mode_switch, LV_ALIGN_RIGHT_MID, 0, 0);
    lv_obj_add_event_cb(s_mode_switch, mode_switch_cb, LV_EVENT_VALUE_CHANGED, NULL);

    /* ── Auto panel: interval buttons + Apply ── */
    s_auto_panel = lv_obj_create(parent);
    lv_obj_remove_style_all(s_auto_panel);
    lv_obj_set_size(s_auto_panel, 420, 100);
    lv_obj_set_layout(s_auto_panel, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(s_auto_panel, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_flex_align(s_auto_panel, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(s_auto_panel, 8, 0);
    lv_obj_align(s_auto_panel, LV_ALIGN_TOP_MID, 0, 336);
    lv_obj_clear_flag(s_auto_panel, LV_OBJ_FLAG_SCROLLABLE);

    const char *labels[] = {"30s", "60s", "2m", "5m"};
    for (uint32_t i = 0; i < INTERVAL_COUNT; i++) {
        lv_obj_t *btn = lv_button_create(s_auto_panel);
        lv_obj_set_size(btn, 95, 40);
        lv_obj_set_style_radius(btn, 10, 0);
        lv_obj_set_style_border_width(btn, 0, 0);
        lv_obj_add_event_cb(btn, interval_btn_cb, LV_EVENT_CLICKED, (void *)(uintptr_t)i);
        s_interval_btns[i] = btn;

        lv_obj_t *lbl = lv_label_create(btn);
        lv_label_set_text(lbl, labels[i]);
        lv_obj_set_style_text_font(lbl, &app_font_14, 0);
        lv_obj_center(lbl);
    }
    update_interval_btn_styles();

    /* Apply button — full width, dimmed when selection == applied */
    s_apply_btn = lv_button_create(s_auto_panel);
    lv_obj_set_size(s_apply_btn, 420, 40);
    lv_obj_set_style_radius(s_apply_btn, 10, 0);
    lv_obj_set_style_border_width(s_apply_btn, 0, 0);
    lv_obj_add_event_cb(s_apply_btn, apply_btn_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *apply_lbl = lv_label_create(s_apply_btn);
    lv_label_set_text(apply_lbl, "Apply");
    lv_obj_set_style_text_font(apply_lbl, &app_font_14, 0);
    lv_obj_center(apply_lbl);
    update_apply_btn_state();

    /* ── Manual panel: send button ── */
    s_manual_panel = lv_obj_create(parent);
    lv_obj_remove_style_all(s_manual_panel);
    lv_obj_set_size(s_manual_panel, 420, 50);
    lv_obj_align(s_manual_panel, LV_ALIGN_TOP_MID, 0, 336);
    lv_obj_clear_flag(s_manual_panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(s_manual_panel, LV_OBJ_FLAG_HIDDEN); /* hidden by default */

    lv_obj_t *send_btn = lv_button_create(s_manual_panel);
    lv_obj_set_size(send_btn, 420, 42);
    lv_obj_set_style_bg_color(send_btn, C_ACCENT, 0);
    lv_obj_set_style_radius(send_btn, 10, 0);
    lv_obj_add_event_cb(send_btn, send_btn_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_center(send_btn);

    lv_obj_t *btn_label = lv_label_create(send_btn);
    lv_label_set_text(btn_label, LV_SYMBOL_UPLOAD "  Send Uplink");
    lv_obj_set_style_text_color(btn_label, lv_color_hex(0x050816), 0);
    lv_obj_set_style_text_font(btn_label, &app_font_14, 0);
    lv_obj_center(btn_label);
}

void page_lora_update(const sensor_data_t *data)
{
    (void)data;
}

void page_lora_update_status(svc_lorawan_state_t state)
{
    if (s_status_label == NULL) return;

    const char *text;
    lv_color_t dot_color;

    switch (state) {
    case SVC_LORAWAN_STATE_JOINING:
        text = "Joining...";
        dot_color = C_YELLOW;
        break;
    case SVC_LORAWAN_STATE_JOINED:
        text = "Joined";
        dot_color = C_GREEN;
        break;
    case SVC_LORAWAN_STATE_FAILED:
        text = "Failed";
        dot_color = C_RED;
        break;
    default:
        text = "Idle";
        dot_color = C_DIM;
        break;
    }

    lv_label_set_text(s_status_label, text);
    lv_obj_set_style_bg_color(s_status_dot, dot_color, 0);
}

void page_lora_update_uplink(uint32_t count, bool last_ok)
{
    if (s_uplink_count == NULL) return;

    static char buf[16];
    snprintf(buf, sizeof(buf), "%lu", (unsigned long)count);
    lv_label_set_text(s_uplink_count, buf);
    lv_label_set_text(s_uplink_result, last_ok ? "Last: OK" : "Last: Failed");
    lv_obj_set_style_text_color(s_uplink_result, last_ok ? C_GREEN : C_RED, 0);
}

void page_lora_update_downlink(uint8_t port, const uint8_t *data, uint16_t len,
                               int16_t rssi, int8_t snr)
{
    if (s_downlink_info == NULL) return;

    static char buf[128];
    int off = snprintf(buf, sizeof(buf), "Port %u  %u bytes\n", port, len);

    /* Hex dump, truncate if needed */
    uint16_t show = (len > 16) ? 16 : len;
    for (uint16_t i = 0; i < show && off < (int)sizeof(buf) - 4; i++) {
        off += snprintf(buf + off, sizeof(buf) - off, "%02X ", data[i]);
    }
    if (len > 16) {
        snprintf(buf + off, sizeof(buf) - off, "...");
    }

    lv_label_set_text(s_downlink_info, buf);
}
