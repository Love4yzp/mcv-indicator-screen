/*
 * Service: WiFi + BLE provisioning.
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Spencer (love4yzp)
 */
#pragma once

#include "esp_err.h"

typedef enum {
    WIFI_ST_IDLE,
    WIFI_ST_PROVISIONING,
    WIFI_ST_CONNECTING,
    WIFI_ST_CONNECTED,
    WIFI_ST_DISCONNECTED,
} svc_wifi_state_t;

esp_err_t svc_wifi_init(void);
esp_err_t svc_wifi_reprovision(void);
svc_wifi_state_t svc_wifi_get_state(void);
