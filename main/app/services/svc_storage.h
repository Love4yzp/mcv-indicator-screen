/*
 * Service: NVS persistent storage for app settings.
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Spencer (love4yzp)
 */
#pragma once

#include <stdint.h>

#include "esp_err.h"

esp_err_t svc_storage_init(void);
int svc_storage_load_pm25_threshold(int default_value);
esp_err_t svc_storage_save_pm25_threshold(int value);

/* MQTT broker config persisted in NVS (HA upload).
 * On first boot fields are empty → caller falls back to compiled defaults. */
typedef struct {
    char     host[48];   // broker hostname or IP
    uint16_t port;       // 0 → use default 1883
    char     user[32];   // empty → anonymous
    char     pass[64];
} svc_storage_mqtt_cfg_t;

esp_err_t svc_storage_load_mqtt_cfg(svc_storage_mqtt_cfg_t *out);
esp_err_t svc_storage_save_mqtt_cfg(const svc_storage_mqtt_cfg_t *cfg);
