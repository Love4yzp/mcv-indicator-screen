/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Spencer (love4yzp)
 */

#include "app/app_manager.h"
#include "app/app_inspect.h"

#include <string.h>

#include "freertos/portmacro.h"
#include "lv_port.h"
#include "lvgl.h"
#include "pages/page_hello.h"
#include "pages/page_sensor.h"
#include "pages/page_control.h"
#include "pages/page_settings.h"
#include "app/fonts/app_fonts.h"
#if CONFIG_INDICATOR_LORAWAN_ENABLED
#include "pages/page_lora.h"
#endif

#define PAGE_WIDTH 480
#define PAGE_HEIGHT 480

static const page_entry_t s_pages[] = {
    {"Data", page_sensor_create, page_sensor_update},
    {"Control", page_control_create, page_control_update},
    {"Hello", page_hello_create, page_hello_update},
    {"Settings", page_settings_create, NULL},
#if CONFIG_INDICATOR_LORAWAN_ENABLED
    {"LoRa", page_lora_create, page_lora_update},
#endif
};

#define PAGE_COUNT (sizeof(s_pages) / sizeof(s_pages[0]))

static sensor_data_t s_sensor_data = {0};
static platform_callbacks_t s_callbacks = {0};
static lv_obj_t *s_tileview = NULL;
static lv_obj_t *s_tiles[PAGE_COUNT] = {0};
static lv_obj_t *s_page_dots[PAGE_COUNT] = {0};

static portMUX_TYPE s_mux = portMUX_INITIALIZER_UNLOCKED;
#define LOCK() portENTER_CRITICAL(&s_mux)
#define UNLOCK() portEXIT_CRITICAL(&s_mux)

static bool s_wifi_info_pending = false;
static bool s_wifi_qr_show_pending = false;
static bool s_wifi_qr_hide_pending = false;
static char s_wifi_line1[96];
static char s_wifi_line2[96];
static char s_wifi_qr_payload[192];
static char s_wifi_qr_service[64];
static char s_wifi_qr_pop[64];

static bool s_mqtt_pending = false;
static char s_mqtt_text[96];

#if CONFIG_INDICATOR_LORAWAN_ENABLED
static bool s_lora_status_pending = false;
static bool s_lora_uplink_pending = false;
static bool s_lora_downlink_pending = false;
static int s_lora_state;
static uint32_t s_lora_ul_count;
static bool s_lora_ul_ok;
static uint8_t s_lora_dl_port;
static uint8_t s_lora_dl_data[64];
static uint16_t s_lora_dl_len;
static int16_t s_lora_dl_rssi;
static int8_t s_lora_dl_snr;
#endif

static void update_page_dots(uint32_t active_col)
{
    for (uint32_t i = 0; i < PAGE_COUNT; ++i) {
        if (s_page_dots[i] == NULL) continue;
        lv_color_t color = (i == active_col) ? lv_color_hex(0x4CC9F0) : lv_color_hex(0x2A3441);
        lv_obj_set_style_bg_color(s_page_dots[i], color, 0);
    }
}

static void tile_changed_cb(lv_event_t *event)
{
    lv_obj_t *tileview = lv_event_get_target(event);
    lv_obj_t *active_tile = lv_tileview_get_tile_active(tileview);
    int32_t x = lv_obj_get_x(active_tile);
    uint32_t active_col = (x >= 0) ? (uint32_t)(x / PAGE_WIDTH) : 0;
    update_page_dots(active_col);
}

static void wifi_ui_sync_cb(lv_timer_t *timer)
{
    (void)timer;
    static char line1[96];
    static char line2[96];
    static char payload[192];
    static char service[64];
    static char pop[64];
    bool do_info = false;
    bool do_show = false;
    bool do_hide = false;

    LOCK();
    if (s_wifi_info_pending) {
        s_wifi_info_pending = false;
        do_info = true;
        strlcpy(line1, s_wifi_line1, sizeof(line1));
        strlcpy(line2, s_wifi_line2, sizeof(line2));
    }
    if (s_wifi_qr_show_pending) {
        s_wifi_qr_show_pending = false;
        do_show = true;
        strlcpy(payload, s_wifi_qr_payload, sizeof(payload));
        strlcpy(service, s_wifi_qr_service, sizeof(service));
        strlcpy(pop, s_wifi_qr_pop, sizeof(pop));
    }
    if (s_wifi_qr_hide_pending) {
        s_wifi_qr_hide_pending = false;
        do_hide = true;
    }
    UNLOCK();

    if (do_info) page_settings_update_wifi_info(line1, line2);
    if (do_hide) page_settings_hide_wifi_qr();
    if (do_show) page_settings_show_wifi_qr(payload, service, pop);

    bool do_mqtt = false;
    static char mqtt_text[96];
    LOCK();
    if (s_mqtt_pending) {
        s_mqtt_pending = false;
        do_mqtt = true;
        strlcpy(mqtt_text, s_mqtt_text, sizeof(mqtt_text));
    }
    UNLOCK();
    if (do_mqtt) page_settings_update_mqtt_status(mqtt_text);

#if CONFIG_INDICATOR_LORAWAN_ENABLED
    bool do_lora_status = false, do_lora_ul = false, do_lora_dl = false;
    int lora_state = 0;
    uint32_t ul_count = 0;
    bool ul_ok = false;
    uint8_t dl_port = 0;
    uint8_t dl_data[64];
    uint16_t dl_len = 0;
    int16_t dl_rssi = 0;
    int8_t dl_snr = 0;

    LOCK();
    if (s_lora_status_pending) {
        s_lora_status_pending = false;
        do_lora_status = true;
        lora_state = s_lora_state;
    }
    if (s_lora_uplink_pending) {
        s_lora_uplink_pending = false;
        do_lora_ul = true;
        ul_count = s_lora_ul_count;
        ul_ok = s_lora_ul_ok;
    }
    if (s_lora_downlink_pending) {
        s_lora_downlink_pending = false;
        do_lora_dl = true;
        dl_port = s_lora_dl_port;
        dl_len = s_lora_dl_len;
        dl_rssi = s_lora_dl_rssi;
        dl_snr = s_lora_dl_snr;
        memcpy(dl_data, s_lora_dl_data, dl_len);
    }
    UNLOCK();

    if (do_lora_status) page_lora_update_status((svc_lorawan_state_t)lora_state);
    if (do_lora_ul) page_lora_update_uplink(ul_count, ul_ok);
    if (do_lora_dl) page_lora_update_downlink(dl_port, dl_data, dl_len, dl_rssi, dl_snr);
#endif
}

void app_manager_init(const platform_callbacks_t *cbs)
{
    if (cbs != NULL) s_callbacks = *cbs;

    app_fonts_init();

    lv_obj_t *screen = lv_screen_active();
    lv_obj_set_style_bg_color(screen, lv_color_hex(0x050816), 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);

    s_tileview = lv_tileview_create(screen);
    lv_obj_t *tileview = s_tileview;
    lv_obj_remove_style_all(tileview);
    lv_obj_set_size(tileview, PAGE_WIDTH, PAGE_HEIGHT);
    lv_obj_set_style_bg_color(tileview, lv_color_hex(0x050816), 0);
    lv_obj_set_style_bg_opa(tileview, LV_OPA_COVER, 0);
    lv_obj_set_scrollbar_mode(tileview, LV_SCROLLBAR_MODE_OFF);
    lv_obj_center(tileview);
    lv_obj_add_event_cb(tileview, tile_changed_cb, LV_EVENT_VALUE_CHANGED, NULL);

    for (uint32_t i = 0; i < PAGE_COUNT; ++i) {
        lv_dir_t dir = LV_DIR_NONE;
        if (i > 0) dir |= LV_DIR_LEFT;
        if (i + 1U < PAGE_COUNT) dir |= LV_DIR_RIGHT;
        lv_obj_t *tile = lv_tileview_add_tile(tileview, i, 0, dir);
        s_tiles[i] = tile;
        s_pages[i].create(tile);
        if (s_pages[i].update != NULL) s_pages[i].update(&s_sensor_data);
    }

    lv_obj_t *dot_row = lv_obj_create(screen);
    lv_obj_remove_style_all(dot_row);
    lv_obj_set_size(dot_row, 80, 12);
    lv_obj_set_flex_flow(dot_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(dot_row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(dot_row, 8, 0);
    lv_obj_align(dot_row, LV_ALIGN_BOTTOM_MID, 0, -18);
    lv_obj_clear_flag(dot_row, LV_OBJ_FLAG_SCROLLABLE);

    for (uint32_t i = 0; i < PAGE_COUNT; ++i) {
        s_page_dots[i] = lv_obj_create(dot_row);
        lv_obj_remove_style_all(s_page_dots[i]);
        lv_obj_set_size(s_page_dots[i], 8, 8);
        lv_obj_set_style_radius(s_page_dots[i], LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_opa(s_page_dots[i], LV_OPA_COVER, 0);
        lv_obj_clear_flag(s_page_dots[i], LV_OBJ_FLAG_SCROLLABLE);
    }

    update_page_dots(0);
    lv_timer_create(wifi_ui_sync_cb, 100, NULL);
}

void app_manager_update_sensor(uint8_t type, float value)
{
    s_sensor_data.rp2040_connected = true;
    switch (type) {
    case 0xC0: s_sensor_data.pm1_0     = value; break;
    case 0xC1: s_sensor_data.pm2_5     = value; break;
    case 0xC2: s_sensor_data.pm4_0     = value; break;
    case 0xC3: s_sensor_data.pm10      = value; break;
    case 0xC4: s_sensor_data.temp      = value; break;
    case 0xC5: s_sensor_data.humidity  = value; break;
    case 0xC6: s_sensor_data.voc_index = value; break;
    default: break;
    }
}

void app_manager_flush_sensor(void)
{
    lv_port_lock();
    for (uint32_t i = 0; i < PAGE_COUNT; ++i) {
        if (s_pages[i].update != NULL) s_pages[i].update(&s_sensor_data);
    }
    lv_port_unlock();
}

const platform_callbacks_t *app_manager_get_callbacks(void) { return &s_callbacks; }
const sensor_data_t *app_manager_get_sensor_data(void) { return &s_sensor_data; }

void app_manager_update_wifi_info(const char *line1, const char *line2)
{
    LOCK();
    strlcpy(s_wifi_line1, line1 != NULL ? line1 : "", sizeof(s_wifi_line1));
    strlcpy(s_wifi_line2, line2 != NULL ? line2 : "", sizeof(s_wifi_line2));
    s_wifi_info_pending = true;
    UNLOCK();
}

void app_manager_show_wifi_qr(const char *payload, const char *service, const char *pop)
{
    LOCK();
    strlcpy(s_wifi_qr_payload, payload != NULL ? payload : "", sizeof(s_wifi_qr_payload));
    strlcpy(s_wifi_qr_service, service != NULL ? service : "", sizeof(s_wifi_qr_service));
    strlcpy(s_wifi_qr_pop, pop != NULL ? pop : "", sizeof(s_wifi_qr_pop));
    s_wifi_qr_show_pending = true;
    s_wifi_qr_hide_pending = false;
    UNLOCK();
}

void app_manager_hide_wifi_qr(void)
{
    LOCK();
    s_wifi_qr_hide_pending = true;
    s_wifi_qr_show_pending = false;
    UNLOCK();
}

void app_manager_update_mqtt_status(const char *text)
{
    LOCK();
    strlcpy(s_mqtt_text, text != NULL ? text : "", sizeof(s_mqtt_text));
    s_mqtt_pending = true;
    UNLOCK();
}

void app_manager_update_lora_status(int state)
{
#if CONFIG_INDICATOR_LORAWAN_ENABLED
    LOCK();
    s_lora_state = state;
    s_lora_status_pending = true;
    UNLOCK();
#else
    (void)state;
#endif
}

void app_manager_update_lora_uplink(uint32_t count, bool last_ok)
{
#if CONFIG_INDICATOR_LORAWAN_ENABLED
    LOCK();
    s_lora_ul_count = count;
    s_lora_ul_ok = last_ok;
    s_lora_uplink_pending = true;
    UNLOCK();
#else
    (void)count;
    (void)last_ok;
#endif
}

void app_manager_update_lora_downlink(uint8_t port, const uint8_t *data, uint16_t len,
                                      int16_t rssi, int8_t snr)
{
#if CONFIG_INDICATOR_LORAWAN_ENABLED
    LOCK();
    s_lora_dl_port = port;
    s_lora_dl_len = (len > sizeof(s_lora_dl_data)) ? sizeof(s_lora_dl_data) : len;
    memcpy(s_lora_dl_data, data, s_lora_dl_len);
    s_lora_dl_rssi = rssi;
    s_lora_dl_snr = snr;
    s_lora_downlink_pending = true;
    UNLOCK();
#else
    (void)port;
    (void)data;
    (void)len;
    (void)rssi;
    (void)snr;
#endif
}

bool app_manager_set_page(uint32_t idx)
{
    if (s_tileview == NULL || idx >= PAGE_COUNT) return false;
    if (s_tiles[idx] == NULL) return false;
    lv_tileview_set_tile(s_tileview, s_tiles[idx], LV_ANIM_OFF);
    update_page_dots(idx);
    return true;
}

uint32_t app_manager_get_page_count(void) { return PAGE_COUNT; }

void app_manager_inspect_json(FILE *out)
{
    if (out == NULL || s_tileview == NULL) return;

    lv_obj_t *active = lv_tileview_get_tile_active(s_tileview);
    int32_t x = active ? lv_obj_get_x(active) : 0;
    uint32_t idx = (x >= 0) ? (uint32_t)(x / PAGE_WIDTH) : 0;
    const char *name = (idx < PAGE_COUNT) ? s_pages[idx].name : "?";

    fprintf(out, "{\n  \"page_index\": %u,\n  \"page_name\": \"%s\",\n",
            (unsigned)idx, name);

    lv_obj_t *screen = lv_screen_active();
    int32_t sw = lv_obj_get_width(screen);
    int32_t sh = lv_obj_get_height(screen);
    fprintf(out, "  \"viewport\": {\"w\":%ld,\"h\":%ld},\n",
            (long)sw, (long)sh);

    /* Emit active tile's subtree + any screen-level overlays that sit outside
     * the tileview (QR code popup, status toasts, etc.). */
    fputs("  \"objects\": [\n", out);
    bool first = true;
    if (active) app_inspect_walk(active, 0, out, &first);
    uint32_t n = lv_obj_get_child_count(screen);
    for (uint32_t i = 0; i < n; i++) {
        lv_obj_t *c = lv_obj_get_child(screen, i);
        if (c == s_tileview) continue;
        app_inspect_walk(c, 0, out, &first);
    }
    fputs("\n  ]\n}\n", out);
}
