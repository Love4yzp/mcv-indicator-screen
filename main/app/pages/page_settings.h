/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Spencer (love4yzp)
 */
#pragma once

#include "lvgl.h"

void page_settings_create(lv_obj_t *parent);
void page_settings_update_wifi_info(const char *line1, const char *line2);
void page_settings_show_wifi_qr(const char *payload, const char *service, const char *pop);
void page_settings_hide_wifi_qr(void);
void page_settings_update_mqtt_status(const char *text);
void page_settings_set_sys_info(const char *info);
