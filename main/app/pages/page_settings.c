/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Spencer (love4yzp)
 */

#include "app/pages/page_settings.h"

#include <stdio.h>
#include <string.h>

#include "app/app_manager.h"
#include "app/fonts/app_fonts.h"

#define C_BG lv_color_hex(0x0D1117)
#define C_CARD lv_color_hex(0x161B22)
#define C_TEXT lv_color_hex(0xE6EDF3)
#define C_DIM lv_color_hex(0x8B949E)
#define C_ACCENT lv_color_hex(0x4CC9F0)

static char s_sys_info[128] = "SenseCAP Indicator";
static lv_obj_t *s_lbl_wifi_info;
static lv_obj_t *s_lbl_qr_info;
static lv_obj_t *s_lbl_mqtt_status;
static lv_obj_t *s_slider_value;

static void wifi_prov_btn_cb(lv_event_t *event)
{
    (void)event;
    const platform_callbacks_t *cbs = app_manager_get_callbacks();
    if (cbs != NULL && cbs->start_wifi_prov != NULL) {
        cbs->start_wifi_prov();
    }
}

static void brightness_changed_cb(lv_event_t *event)
{
    lv_obj_t *slider = lv_event_get_target(event);
    int value = (int)lv_slider_get_value(slider);

    if (s_slider_value != NULL) {
        static char buf[16];
        snprintf(buf, sizeof(buf), "%d%%", value);
        lv_label_set_text(s_slider_value, buf);
    }

    const platform_callbacks_t *cbs = app_manager_get_callbacks();
    if (cbs != NULL && cbs->set_brightness != NULL) {
        cbs->set_brightness(value);
    }
}

void page_settings_create(lv_obj_t *parent)
{
    lv_obj_set_style_bg_color(parent, C_BG, 0);
    lv_obj_set_style_pad_all(parent, 20, 0);
    lv_obj_clear_flag(parent, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *title = lv_label_create(parent);
    lv_label_set_text(title, "Settings");
    lv_obj_set_style_text_color(title, C_TEXT, 0);
    lv_obj_set_style_text_font(title, &app_font_20, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 10);

    lv_obj_t *brightness_card = lv_obj_create(parent);
    lv_obj_set_size(brightness_card, 380, 110);
    lv_obj_set_style_bg_color(brightness_card, C_CARD, 0);
    lv_obj_set_style_radius(brightness_card, 16, 0);
    lv_obj_set_style_border_width(brightness_card, 0, 0);
    lv_obj_set_style_pad_all(brightness_card, 16, 0);
    lv_obj_align(brightness_card, LV_ALIGN_TOP_MID, 0, 52);

    lv_obj_t *brightness_label = lv_label_create(brightness_card);
    lv_label_set_text(brightness_label, "Brightness");
    lv_obj_set_style_text_color(brightness_label, C_TEXT, 0);
    lv_obj_set_style_text_font(brightness_label, &app_font_14, 0);
    lv_obj_align(brightness_label, LV_ALIGN_TOP_LEFT, 0, 0);

    s_slider_value = lv_label_create(brightness_card);
    lv_label_set_text(s_slider_value, "100%");
    lv_obj_set_style_text_color(s_slider_value, C_ACCENT, 0);
    lv_obj_set_style_text_font(s_slider_value, &app_font_14, 0);
    lv_obj_align(s_slider_value, LV_ALIGN_TOP_RIGHT, 0, 0);

    lv_obj_t *slider = lv_slider_create(brightness_card);
    lv_obj_set_width(slider, 340);
    lv_slider_set_range(slider, 5, 100);
    lv_slider_set_value(slider, 100, LV_ANIM_OFF);
    lv_obj_align(slider, LV_ALIGN_BOTTOM_MID, 0, -8);
    lv_obj_add_event_cb(slider, brightness_changed_cb, LV_EVENT_VALUE_CHANGED, NULL);

    lv_obj_t *btn = lv_button_create(parent);
    lv_obj_set_size(btn, 300, 50);
    lv_obj_align(btn, LV_ALIGN_CENTER, 0, 6);
    lv_obj_set_style_bg_color(btn, C_CARD, 0);
    lv_obj_set_style_radius(btn, 12, 0);
    lv_obj_add_event_cb(btn, wifi_prov_btn_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *bl = lv_label_create(btn);
    lv_label_set_text(bl, LV_SYMBOL_WIFI "  BLE Provisioning");
    lv_obj_set_style_text_color(bl, C_TEXT, 0);
    lv_obj_center(bl);

    s_lbl_wifi_info = lv_label_create(parent);
    lv_label_set_text(s_lbl_wifi_info, "WiFi: initializing...");
    lv_obj_set_style_text_color(s_lbl_wifi_info, C_DIM, 0);
    lv_obj_set_style_text_font(s_lbl_wifi_info, &app_font_14, 0);
    lv_obj_set_style_text_align(s_lbl_wifi_info, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(s_lbl_wifi_info, 380);
    lv_obj_align(s_lbl_wifi_info, LV_ALIGN_CENTER, 0, 72);

    s_lbl_mqtt_status = lv_label_create(parent);
    lv_label_set_text(s_lbl_mqtt_status, "MQTT: idle");
    lv_obj_set_style_text_color(s_lbl_mqtt_status, C_DIM, 0);
    lv_obj_set_style_text_font(s_lbl_mqtt_status, &app_font_14, 0);
    lv_obj_set_style_text_align(s_lbl_mqtt_status, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(s_lbl_mqtt_status, 380);
    lv_obj_align(s_lbl_mqtt_status, LV_ALIGN_CENTER, 0, 112);

    s_lbl_qr_info = lv_label_create(parent);
    lv_label_set_text(s_lbl_qr_info, "");
    lv_obj_set_width(s_lbl_qr_info, 380);
    lv_obj_set_style_text_color(s_lbl_qr_info, C_ACCENT, 0);
    lv_obj_set_style_text_font(s_lbl_qr_info, &app_font_14, 0);
    lv_obj_set_style_text_align(s_lbl_qr_info, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(s_lbl_qr_info, LV_ALIGN_CENTER, 0, 142);

    lv_obj_t *sys = lv_label_create(parent);
    lv_label_set_text(sys, s_sys_info);
    lv_obj_set_style_text_color(sys, C_DIM, 0);
    lv_obj_set_style_text_font(sys, &app_font_14, 0);
    lv_obj_set_style_text_align(sys, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(sys, 420);
    lv_obj_align(sys, LV_ALIGN_BOTTOM_MID, 0, -10);
}

void page_settings_set_sys_info(const char *info)
{
    if (info != NULL) {
        strlcpy(s_sys_info, info, sizeof(s_sys_info));
    }
}

void page_settings_update_wifi_info(const char *line1, const char *line2)
{
    if (s_lbl_wifi_info == NULL) {
        return;
    }
    char text[192];
    snprintf(text, sizeof(text), "%s\n%s", line1 != NULL ? line1 : "", line2 != NULL ? line2 : "");
    lv_label_set_text(s_lbl_wifi_info, text);
}

void page_settings_show_wifi_qr(const char *payload, const char *service, const char *pop)
{
    if (s_lbl_qr_info == NULL) {
        return;
    }
    char text[256];
    snprintf(text, sizeof(text), "BLE QR payload ready\nservice=%s\npop=%s\n%s", service != NULL ? service : "",
             pop != NULL ? pop : "", payload != NULL ? payload : "");
    lv_label_set_text(s_lbl_qr_info, text);
}

void page_settings_hide_wifi_qr(void)
{
    if (s_lbl_qr_info != NULL) {
        lv_label_set_text(s_lbl_qr_info, "");
    }
}

void page_settings_update_mqtt_status(const char *text)
{
    if (s_lbl_mqtt_status == NULL) return;
    lv_label_set_text(s_lbl_mqtt_status, text != NULL ? text : "");
}
