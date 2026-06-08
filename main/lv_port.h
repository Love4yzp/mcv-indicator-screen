/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Spencer (love4yzp)
 */

#pragma once

#include "esp_err.h"

esp_err_t lv_port_init(void);
void lv_port_lock(void);
void lv_port_unlock(void);
