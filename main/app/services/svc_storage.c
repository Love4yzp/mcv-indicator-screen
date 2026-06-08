/*
 * Service: NVS persistent storage for app settings.
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Spencer (love4yzp)
 */

#include "svc_storage.h"

#include <string.h>

#include "nvs.h"

#define NS "app"
#define KEY_PM25     "pm25_th"
#define KEY_MQTT_HOST "mq_host"
#define KEY_MQTT_PORT "mq_port"
#define KEY_MQTT_USER "mq_user"
#define KEY_MQTT_PASS "mq_pass"

esp_err_t svc_storage_init(void) { return ESP_OK; }

int svc_storage_load_pm25_threshold(int def)
{
    nvs_handle_t handle;
    int32_t value = def;
    if (nvs_open(NS, NVS_READONLY, &handle) != ESP_OK) return def;
    if (nvs_get_i32(handle, KEY_PM25, &value) != ESP_OK) value = def;
    nvs_close(handle);
    return (int)value;
}

esp_err_t svc_storage_save_pm25_threshold(int value)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NS, NVS_READWRITE, &handle);
    if (err != ESP_OK) return err;
    err = nvs_set_i32(handle, KEY_PM25, value);
    if (err == ESP_OK) err = nvs_commit(handle);
    nvs_close(handle);
    return err;
}

esp_err_t svc_storage_load_mqtt_cfg(svc_storage_mqtt_cfg_t *out)
{
    if (out == NULL) return ESP_ERR_INVALID_ARG;
    memset(out, 0, sizeof(*out));

    nvs_handle_t handle;
    if (nvs_open(NS, NVS_READONLY, &handle) != ESP_OK) return ESP_ERR_NOT_FOUND;

    size_t len;

    len = sizeof(out->host);
    nvs_get_str(handle, KEY_MQTT_HOST, out->host, &len);

    uint16_t port = 0;
    nvs_get_u16(handle, KEY_MQTT_PORT, &port);
    out->port = port;

    len = sizeof(out->user);
    nvs_get_str(handle, KEY_MQTT_USER, out->user, &len);

    len = sizeof(out->pass);
    nvs_get_str(handle, KEY_MQTT_PASS, out->pass, &len);

    nvs_close(handle);
    return ESP_OK;
}

esp_err_t svc_storage_save_mqtt_cfg(const svc_storage_mqtt_cfg_t *cfg)
{
    if (cfg == NULL) return ESP_ERR_INVALID_ARG;

    nvs_handle_t handle;
    esp_err_t err = nvs_open(NS, NVS_READWRITE, &handle);
    if (err != ESP_OK) return err;

    err = nvs_set_str(handle, KEY_MQTT_HOST, cfg->host);
    if (err == ESP_OK) err = nvs_set_u16(handle, KEY_MQTT_PORT, cfg->port);
    if (err == ESP_OK) err = nvs_set_str(handle, KEY_MQTT_USER, cfg->user);
    if (err == ESP_OK) err = nvs_set_str(handle, KEY_MQTT_PASS, cfg->pass);
    if (err == ESP_OK) err = nvs_commit(handle);
    nvs_close(handle);
    return err;
}
