/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Spencer (love4yzp)
 */
#pragma once

#include "app/app_manager.h"
#include "svc_lorawan.h"

void page_lora_create(lv_obj_t *parent);
void page_lora_update(const sensor_data_t *data);
void page_lora_update_status(svc_lorawan_state_t state);
void page_lora_update_uplink(uint32_t count, bool last_ok);
void page_lora_update_downlink(uint8_t port, const uint8_t *data, uint16_t len,
                               int16_t rssi, int8_t snr);
