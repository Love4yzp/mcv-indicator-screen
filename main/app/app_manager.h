/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Spencer (love4yzp)
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef struct _lv_obj_t lv_obj_t;

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    float pm1_0;       // µg/m³
    float pm2_5;       // µg/m³
    float pm4_0;       // µg/m³
    float pm10;        // µg/m³
    float temp;        // °C
    float humidity;    // %RH
    float voc_index;   // 1-500
    bool  rp2040_connected;
} sensor_data_t;

typedef struct {
    const char *name;
    void (*create)(lv_obj_t *parent);
    void (*update)(const sensor_data_t *data);
} page_entry_t;

typedef struct {
    void (*set_brightness)(int pct);
    void (*send_beep)(void);
    void (*stop_beep)(void);
    void (*start_wifi_prov)(void);
    void (*save_pm25_threshold)(int value);
    void (*request_lora_uplink)(void);
    void (*configure_lora_uplink)(bool auto_mode, uint32_t interval_s);
} platform_callbacks_t;

void app_manager_init(const platform_callbacks_t *cbs);
void app_manager_update_sensor(uint8_t type, float value);
void app_manager_flush_sensor(void);
const platform_callbacks_t *app_manager_get_callbacks(void);
const sensor_data_t *app_manager_get_sensor_data(void);
void app_manager_update_wifi_info(const char *line1, const char *line2);
void app_manager_show_wifi_qr(const char *payload, const char *service, const char *pop);
void app_manager_hide_wifi_qr(void);
void app_manager_update_mqtt_status(const char *text);

bool app_manager_set_page(uint32_t idx);
uint32_t app_manager_get_page_count(void);

/* LoRa UI updates (thread-safe, queued to LVGL timer) */
void app_manager_update_lora_status(int state);
void app_manager_update_lora_uplink(uint32_t count, bool last_ok);
void app_manager_update_lora_downlink(uint8_t port, const uint8_t *data, uint16_t len,
                                      int16_t rssi, int8_t snr);

#ifdef __cplusplus
}
#endif
