/*
 * Service: RP2040 UART + COBS communication.
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Spencer (love4yzp)
 */
#pragma once

#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Packet types — keep in sync with firmware/rp2040/include/protocol.h
 */
typedef enum {
    PKT_TYPE_ACK = 0x00,
    PKT_TYPE_CMD_COLLECT_INTERVAL = 0xA0,
    PKT_TYPE_CMD_BEEP_ON = 0xA1,
    PKT_TYPE_CMD_BEEP_OFF = 0xA2,
    PKT_TYPE_CMD_SHUTDOWN = 0xA3,
    PKT_TYPE_CMD_POWER_ON = 0xA4,
    /* deprecated — SCD41/AHT20/SGP40 */
    PKT_TYPE_SENSOR_SCD41_TEMP = 0xB0,
    PKT_TYPE_SENSOR_SCD41_HUMIDITY = 0xB1,
    PKT_TYPE_SENSOR_SCD41_CO2 = 0xB2,
    PKT_TYPE_SENSOR_AHT20_TEMP = 0xB3,
    PKT_TYPE_SENSOR_AHT20_HUMIDITY = 0xB4,
    PKT_TYPE_SENSOR_TVOC_INDEX = 0xB5,
    /* SEN54 */
    PKT_TYPE_SENSOR_SEN54_PM1_0 = 0xC0,
    PKT_TYPE_SENSOR_SEN54_PM2_5 = 0xC1,
    PKT_TYPE_SENSOR_SEN54_PM4_0 = 0xC2,
    PKT_TYPE_SENSOR_SEN54_PM10 = 0xC3,
    PKT_TYPE_SENSOR_SEN54_TEMP = 0xC4,
    PKT_TYPE_SENSOR_SEN54_HUMIDITY = 0xC5,
    PKT_TYPE_SENSOR_SEN54_VOC_INDEX = 0xC6,
    /* 0xC7 (SEN54_NOX_INDEX) unused: SEN54 has no NOx sensor, reserved for SEN55 */
    PKT_TYPE_SENSOR_BATCH_END = 0xFE,
} rp2040_pkt_type_t;

typedef struct {
    rp2040_pkt_type_t type;
    float value;
} rp2040_sensor_data_t;

typedef void (*rp2040_data_cb_t)(const rp2040_sensor_data_t *data, void *user_ctx);

esp_err_t svc_rp2040_init(void);
void svc_rp2040_register_cb(rp2040_data_cb_t cb, void *user_ctx);
esp_err_t svc_rp2040_send_cmd(rp2040_pkt_type_t cmd, const void *data, uint8_t len);

#ifdef __cplusplus
}
#endif
