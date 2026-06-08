/*
 * Service: RP2040 UART + COBS communication.
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Spencer (love4yzp)
 */

#include "svc_rp2040.h"

#include <string.h>

#include "bsp_indicator.h"
#include "cobs.h"
#include "driver/uart.h"
#include "esp_check.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define BUF_SIZE 512

static const char *TAG = "svc_rp2040";
static rp2040_data_cb_t s_cb;
static void *s_cb_ctx;

static esp_err_t send_packet(uint8_t type, const void *data, uint8_t len)
{
    uint8_t raw[32] = {0};
    uint8_t enc[40] = {0};
    if (len > 30) return ESP_ERR_INVALID_ARG;
    raw[0] = type;
    if (len > 0 && data != NULL) memcpy(&raw[1], data, len);
    cobs_encode_result result = cobs_encode(enc, sizeof(enc), raw, 1 + (size_t)len);
    if (result.status != COBS_ENCODE_OK) return ESP_FAIL;
    enc[result.out_len] = 0x00;
    uart_write_bytes(BSP_RP2040_UART_NUM, enc, result.out_len + 1);
    return ESP_OK;
}

static void handle_packet(const uint8_t *data, size_t len)
{
    if (len < 5 || s_cb == NULL) return;
    rp2040_sensor_data_t sensor_data = {.type = (rp2040_pkt_type_t)data[0]};
    memcpy(&sensor_data.value, &data[1], sizeof(float));
    s_cb(&sensor_data, s_cb_ctx);
}

static void comm_task(void *arg)
{
    (void)arg;
    uint8_t buf[BUF_SIZE];
    uint8_t dec[BUF_SIZE];
    while (true) {
        int len = uart_read_bytes(BSP_RP2040_UART_NUM, buf, BUF_SIZE - 1, pdMS_TO_TICKS(100));
        if (len <= 0) continue;
        uint8_t *start = buf;
        uint8_t *stop = buf + len;
        while (start < stop) {
            uint8_t *p = start;
            while (p < stop && *p != 0x00) p++;
            if (p > start) {
                cobs_decode_result result = cobs_decode(dec, sizeof(dec), start, (size_t)(p - start));
                if (result.status == COBS_DECODE_OK && result.out_len >= 5) handle_packet(dec, result.out_len);
            }
            start = p + 1;
        }
    }
}

esp_err_t svc_rp2040_init(void)
{
    uart_config_t cfg = {
        .baud_rate = BSP_RP2040_UART_BAUD,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    ESP_RETURN_ON_ERROR(uart_driver_install(BSP_RP2040_UART_NUM, BUF_SIZE * 2, 0, 0, NULL, 0), TAG,
                        "UART install failed");
    ESP_RETURN_ON_ERROR(uart_param_config(BSP_RP2040_UART_NUM, &cfg), TAG, "UART config failed");
    ESP_RETURN_ON_ERROR(uart_set_pin(BSP_RP2040_UART_NUM, BSP_RP2040_UART_TX_IO, BSP_RP2040_UART_RX_IO,
                                     UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE),
                        TAG, "UART pin failed");
    xTaskCreate(comm_task, "rp2040_comm", 4096, NULL, 5, NULL);
    send_packet(PKT_TYPE_CMD_POWER_ON, NULL, 0);
    ESP_LOGI(TAG, "RP2040 UART2 TX=%d RX=%d initialized", BSP_RP2040_UART_TX_IO, BSP_RP2040_UART_RX_IO);
    return ESP_OK;
}

void svc_rp2040_register_cb(rp2040_data_cb_t cb, void *user_ctx)
{
    s_cb = cb;
    s_cb_ctx = user_ctx;
}

esp_err_t svc_rp2040_send_cmd(rp2040_pkt_type_t cmd, const void *data, uint8_t len)
{
    return send_packet((uint8_t)cmd, data, len);
}
