/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Spencer (love4yzp)
 *
 * LoRaWAN service for SenseCAP Indicator D1L / D1Pro (SX1262 radio).
 * Uses RadioLib as the LoRaWAN MAC stack. Supports Class A and Class C.
 *
 * Typical usage:
 *
 *   svc_lorawan_init(NULL);                      // uses Kconfig defaults
 *   svc_lorawan_register_cb(my_cb, NULL);
 *   svc_lorawan_join();                           // OTAA, blocks until done
 *
 *   uint8_t payload[] = {0x01, 0x02};
 *   svc_lorawan_send(1, payload, sizeof(payload)); // uplink on port 1
 *
 *   // Downlink arrives via callback:
 *   void my_cb(const svc_lorawan_evt_t *e, void *ctx) {
 *       if (e->type == SVC_LORAWAN_EVT_DOWNLINK)
 *           handle(e->downlink.port, e->downlink.data, e->downlink.len);
 *   }
 *
 * Only compiled when CONFIG_INDICATOR_LORAWAN_ENABLED=y.
 */

#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------ */
/*  State                                                               */
/* ------------------------------------------------------------------ */

typedef enum {
    SVC_LORAWAN_STATE_IDLE,       /**< Not initialised or no join attempted */
    SVC_LORAWAN_STATE_JOINING,    /**< OTAA join in progress */
    SVC_LORAWAN_STATE_JOINED,     /**< Successfully joined */
    SVC_LORAWAN_STATE_FAILED,     /**< Join failed or link lost */
} svc_lorawan_state_t;

/* ------------------------------------------------------------------ */
/*  Events                                                              */
/* ------------------------------------------------------------------ */

typedef enum {
    SVC_LORAWAN_EVT_JOINED,       /**< Successfully joined the network */
    SVC_LORAWAN_EVT_JOIN_FAILED,  /**< Join attempt failed */
    SVC_LORAWAN_EVT_TX_DONE,      /**< Uplink accepted */
    SVC_LORAWAN_EVT_DOWNLINK,     /**< Downlink data received */
} svc_lorawan_evt_type_t;

typedef struct {
    svc_lorawan_evt_type_t type;
    union {
        /** Valid when type == SVC_LORAWAN_EVT_DOWNLINK */
        struct {
            uint8_t        port;
            const uint8_t *data;
            uint16_t       len;
            int16_t        rssi;
            int8_t         snr;
        } downlink;
    };
} svc_lorawan_evt_t;

typedef void (*svc_lorawan_cb_t)(const svc_lorawan_evt_t *evt, void *user_ctx);

/* ------------------------------------------------------------------ */
/*  API                                                                 */
/* ------------------------------------------------------------------ */

#if CONFIG_INDICATOR_LORAWAN_ENABLED

/**
 * Initialise the LoRaWAN stack and radio hardware.
 * Must be called after bsp_display_init() (SPI pins shared with LCD init).
 * Pass NULL to use Kconfig defaults for all parameters.
 */
esp_err_t svc_lorawan_init(void);

/**
 * Register a callback for LoRaWAN events (downlink, join status, etc.).
 * Only one callback supported; calling again replaces the previous one.
 */
void svc_lorawan_register_cb(svc_lorawan_cb_t cb, void *user_ctx);

/**
 * Start OTAA join. Non-blocking — result delivered via callback.
 * The join is retried automatically with exponential backoff.
 */
esp_err_t svc_lorawan_join(void);

/**
 * Send an uplink message.
 *
 * @param port     LoRaWAN FPort (1-223).
 * @param data     Payload bytes.
 * @param len      Payload length (max ~242 bytes depending on DR).
 * @param confirmed  True for confirmed uplink, false for unconfirmed.
 * @return ESP_OK if queued, ESP_ERR_INVALID_STATE if not joined.
 */
esp_err_t svc_lorawan_send(uint8_t port, const uint8_t *data, size_t len, bool confirmed);

/**
 * Get current LoRaWAN connection state.
 */
svc_lorawan_state_t svc_lorawan_get_state(void);

/**
 * Request a sensor data uplink (non-blocking, safe from any context).
 * The actual send happens in the LoRaWAN task.
 */
void svc_lorawan_request_uplink(void);

#else /* CONFIG_INDICATOR_LORAWAN_ENABLED not set */

static inline esp_err_t svc_lorawan_init(void)
    { return ESP_ERR_NOT_SUPPORTED; }
static inline void svc_lorawan_register_cb(svc_lorawan_cb_t cb, void *ctx)
    { (void)cb; (void)ctx; }
static inline esp_err_t svc_lorawan_join(void)
    { return ESP_ERR_NOT_SUPPORTED; }
static inline esp_err_t svc_lorawan_send(uint8_t p, const uint8_t *d, size_t l, bool c)
    { (void)p; (void)d; (void)l; (void)c; return ESP_ERR_NOT_SUPPORTED; }
static inline svc_lorawan_state_t svc_lorawan_get_state(void)
    { return SVC_LORAWAN_STATE_IDLE; }
static inline void svc_lorawan_request_uplink(void) {}

#endif /* CONFIG_INDICATOR_LORAWAN_ENABLED */

#ifdef __cplusplus
}
#endif
