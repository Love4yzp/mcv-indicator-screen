/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Spencer (love4yzp)
 */
#pragma once

#include "app/app_manager.h"

void page_sensor_create(lv_obj_t *parent);
void page_sensor_update(const sensor_data_t *data);
void page_sensor_show_alert(const char *message);
void page_sensor_hide_alert(void);
