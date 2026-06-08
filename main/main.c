/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Spencer (love4yzp)
 */

#include <stdio.h>

#include "app/app_manager.h"
#include "app/pages/page_settings.h"
#include "bsp_display.h"
#include "bsp_indicator.h"
#include "esp_idf_version.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "iot_button.h"
#include "lv_port.h"
#include "nvs_flash.h"
#include "svc_alerts.h"
#include "svc_console.h"
#include "svc_ha_mqtt.h"
#include "svc_rp2040.h"
#include "svc_storage.h"
#include "svc_wifi.h"
#if CONFIG_INDICATOR_LORAWAN_ENABLED
#include "svc_lorawan.h"
#endif

static const char *TAG = "main";

static void sensor_cb(const rp2040_sensor_data_t *data, void *ctx)
{
    (void)ctx;
    if (data == NULL) return;
    if ((uint8_t)data->type == PKT_TYPE_SENSOR_BATCH_END) {
        app_manager_flush_sensor();
        svc_alerts_check(app_manager_get_sensor_data());
        return;
    }
    app_manager_update_sensor((uint8_t)data->type, data->value);
}

#if CONFIG_INDICATOR_LORAWAN_ENABLED
#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"
#include <string.h>

static uint32_t s_lora_ul_count = 0;

static void lorawan_event_cb(const svc_lorawan_evt_t *evt, void *ctx)
{
    (void)ctx;
    switch (evt->type) {
    case SVC_LORAWAN_EVT_JOINED:
        app_manager_update_lora_status(SVC_LORAWAN_STATE_JOINED);
        break;
    case SVC_LORAWAN_EVT_JOIN_FAILED:
        app_manager_update_lora_status(SVC_LORAWAN_STATE_FAILED);
        break;
    case SVC_LORAWAN_EVT_TX_DONE:
        s_lora_ul_count++;
        app_manager_update_lora_uplink(s_lora_ul_count, true);
        break;
    case SVC_LORAWAN_EVT_DOWNLINK:
        app_manager_update_lora_downlink(evt->downlink.port, evt->downlink.data,
                                         evt->downlink.len, evt->downlink.rssi,
                                         evt->downlink.snr);
        break;
    }
}

static void lorawan_uplink_timer_cb(TimerHandle_t timer)
{
    (void)timer;
    svc_lorawan_request_uplink();
}
#endif

static void button_cb(void *handle, void *usr_data)
{
    (void)handle;
    (void)usr_data;
    svc_alerts_dismiss();
}

static void cb_brightness(int pct) { bsp_display_backlight_set(pct); }
static void cb_beep(void) { svc_rp2040_send_cmd(PKT_TYPE_CMD_BEEP_ON, NULL, 0); }
static void cb_beep_off(void) { svc_rp2040_send_cmd(PKT_TYPE_CMD_BEEP_OFF, NULL, 0); }
static void cb_wifi_prov(void) { svc_wifi_reprovision(); }
static void cb_save_pm25(int value) { svc_storage_save_pm25_threshold(value); }
#if CONFIG_INDICATOR_LORAWAN_ENABLED
static TimerHandle_t s_lora_timer = NULL;

static void cb_lora_uplink(void) { svc_lorawan_request_uplink(); }

static void cb_configure_lora_uplink(bool auto_mode, uint32_t interval_s)
{
    if (!s_lora_timer) return;

    if (auto_mode) {
        xTimerChangePeriod(s_lora_timer, pdMS_TO_TICKS(interval_s * 1000), 0);
        xTimerStart(s_lora_timer, 0);
        ESP_LOGI(TAG, "LoRa uplink: auto every %lus", (unsigned long)interval_s);
    } else {
        xTimerStop(s_lora_timer, 0);
        ESP_LOGI(TAG, "LoRa uplink: manual mode");
    }
}
#endif

void app_main(void)
{
    ESP_LOGI(TAG, "SenseCAP Indicator Starter Phase 3 | IDF %s", esp_get_idf_version());

    esp_err_t nvs_err = nvs_flash_init();
    if (nvs_err == ESP_ERR_NVS_NO_FREE_PAGES || nvs_err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    } else {
        ESP_ERROR_CHECK(nvs_err);
    }

    ESP_ERROR_CHECK(bsp_indicator_init());
    ESP_ERROR_CHECK(lv_port_init());
    ESP_ERROR_CHECK(svc_rp2040_init());
    svc_rp2040_register_cb(sensor_cb, NULL);
    ESP_ERROR_CHECK(svc_storage_init());
    svc_alerts_init();

    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    char info[128];
    snprintf(info, sizeof(info), "ESP32-S3 | IDF %s | MAC %02X:%02X:%02X:%02X:%02X:%02X", esp_get_idf_version(),
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    page_settings_set_sys_info(info);

    platform_callbacks_t callbacks = {
        .set_brightness = cb_brightness,
        .send_beep = cb_beep,
        .stop_beep = cb_beep_off,
        .start_wifi_prov = cb_wifi_prov,
        .save_pm25_threshold = cb_save_pm25,
#if CONFIG_INDICATOR_LORAWAN_ENABLED
        .request_lora_uplink = cb_lora_uplink,
        .configure_lora_uplink = cb_configure_lora_uplink,
#endif
    };

    lv_port_lock();
    app_manager_init(&callbacks);
    lv_port_unlock();

    ESP_ERROR_CHECK(svc_wifi_init());

    esp_err_t mqtt_err = svc_ha_mqtt_init();
    if (mqtt_err != ESP_OK) {
        ESP_LOGW(TAG, "HA MQTT init failed: %s", esp_err_to_name(mqtt_err));
    }

    button_handle_t button = NULL;
    ESP_ERROR_CHECK(bsp_button_init(&button));
    if (button != NULL) {
        ESP_ERROR_CHECK(iot_button_register_cb(button, BUTTON_PRESS_DOWN, NULL, button_cb, NULL));
    }

#if CONFIG_INDICATOR_LORAWAN_ENABLED
    if (svc_lorawan_init() == ESP_OK) {
        svc_lorawan_register_cb(lorawan_event_cb, NULL);
        app_manager_update_lora_status(SVC_LORAWAN_STATE_JOINING);
        svc_lorawan_join();

        /* Periodic sensor uplink every 60 seconds (auto mode default) */
        s_lora_timer = xTimerCreate("lora_ul", pdMS_TO_TICKS(60000),
                                     pdTRUE, NULL, lorawan_uplink_timer_cb);
        if (s_lora_timer) xTimerStart(s_lora_timer, 0);
    } else {
        ESP_LOGW(TAG, "LoRaWAN not available (no SX1262 hardware?)");
    }
#endif

    esp_err_t console_err = svc_console_start();
    if (console_err != ESP_OK) {
        ESP_LOGW(TAG, "console start failed: %s", esp_err_to_name(console_err));
    }

    ESP_LOGI(TAG, "Boot complete — full starter running");
}
