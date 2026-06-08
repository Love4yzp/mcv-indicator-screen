/*
 * Shared protocol definitions for ESP32-S3 <-> RP2040 communication.
 *
 * This is the single source of truth for packet types used over UART + COBS.
 * Keep in sync with: main/app/services/svc_rp2040.h (ESP32 side)
 *
 * SPDX-License-Identifier: MIT
 */
#pragma once

/* --- Commands (ESP32 -> RP2040) --- */
#define PKT_TYPE_ACK                    0x00
#define PKT_TYPE_CMD_COLLECT_INTERVAL   0xA0
#define PKT_TYPE_CMD_BEEP_ON            0xA1
#define PKT_TYPE_CMD_BEEP_OFF           0xA2
#define PKT_TYPE_CMD_SHUTDOWN           0xA3
#define PKT_TYPE_CMD_POWER_ON           0xA4

/* --- deprecated — SCD41/AHT20/SGP40, replaced by SEN54 --- */
#define PKT_TYPE_SENSOR_SCD41_TEMP      0xB0
#define PKT_TYPE_SENSOR_SCD41_HUMIDITY  0xB1
#define PKT_TYPE_SENSOR_SCD41_CO2       0xB2
#define PKT_TYPE_SENSOR_AHT20_TEMP      0xB3
#define PKT_TYPE_SENSOR_AHT20_HUMIDITY  0xB4
#define PKT_TYPE_SENSOR_TVOC_INDEX      0xB5

/* --- SEN54 (RP2040 -> ESP32) --- */
#define PKT_TYPE_SENSOR_SEN54_PM1_0     0xC0
#define PKT_TYPE_SENSOR_SEN54_PM2_5     0xC1
#define PKT_TYPE_SENSOR_SEN54_PM4_0     0xC2
#define PKT_TYPE_SENSOR_SEN54_PM10      0xC3
#define PKT_TYPE_SENSOR_SEN54_TEMP      0xC4
#define PKT_TYPE_SENSOR_SEN54_HUMIDITY  0xC5
#define PKT_TYPE_SENSOR_SEN54_VOC_INDEX 0xC6
/* 0xC7 (SEN54_NOX_INDEX) unused: SEN54 has no NOx sensor, reserved for SEN55 */
#define PKT_TYPE_SENSOR_BATCH_END       0xFE

/*
 * Packet format (after COBS decode):
 *   Byte 0:   packet type (one of the defines above)
 *   Bytes 1-4: float value (little-endian, IEEE 754)
 *   Total: 5 bytes
 */
