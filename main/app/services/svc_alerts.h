/*
 * Service: PM2.5 threshold alert + buzzer control.
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Spencer (love4yzp)
 */
#pragma once

#include "app/app_manager.h"

void svc_alerts_init(void);
void svc_alerts_check(const sensor_data_t *data);
void svc_alerts_dismiss(void);
