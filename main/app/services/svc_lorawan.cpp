/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Spencer (love4yzp)
 */

#include "sdkconfig.h"
#if CONFIG_INDICATOR_LORAWAN_ENABLED

#include "svc_lorawan.h"
#include "indicator_hal.h"

#include <cstring>
#include <cstdlib>
#include "app_manager.h"
#include "bsp_display.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define TAG "svc_lorawan"

/* ------------------------------------------------------------------ */
/*  NVS persistence for LoRaWAN session & nonces                        */
/* ------------------------------------------------------------------ */

#define NVS_NAMESPACE   "lorawan"
#define NVS_KEY_NONCES  "nonces"
#define NVS_KEY_SESSION "session"

static esp_err_t nvs_save_blob(const char *key, const uint8_t *data, size_t len)
{
    nvs_handle_t h;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &h);
    if (err != ESP_OK) return err;
    err = nvs_set_blob(h, key, data, len);
    if (err == ESP_OK) err = nvs_commit(h);
    nvs_close(h);
    return err;
}

static esp_err_t nvs_load_blob(const char *key, uint8_t *data, size_t len)
{
    nvs_handle_t h;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &h);
    if (err != ESP_OK) return err;
    size_t stored_len = len;
    err = nvs_get_blob(h, key, data, &stored_len);
    nvs_close(h);
    return (err == ESP_OK && stored_len == len) ? ESP_OK : ESP_ERR_NOT_FOUND;
}

/* ------------------------------------------------------------------ */
/*  Region mapping from Kconfig                                         */
/* ------------------------------------------------------------------ */

#if defined(CONFIG_LORAWAN_REGION_EU868)
static const LoRaWANBand_t &s_region = EU868;
#elif defined(CONFIG_LORAWAN_REGION_US915)
static const LoRaWANBand_t &s_region = US915;
#elif defined(CONFIG_LORAWAN_REGION_CN470)
static const LoRaWANBand_t &s_region = CN470;
#elif defined(CONFIG_LORAWAN_REGION_AS923)
static const LoRaWANBand_t &s_region = AS923;
#elif defined(CONFIG_LORAWAN_REGION_AU915)
static const LoRaWANBand_t &s_region = AU915;
#else
#error "No LoRaWAN region selected in Kconfig"
#endif

/* ------------------------------------------------------------------ */
/*  Key parsing                                                         */
/* ------------------------------------------------------------------ */

/** Parse a 16-char hex string to uint64_t (big-endian). */
static uint64_t parse_eui(const char *hex)
{
    uint64_t val = 0;
    for (int i = 0; i < 16 && hex[i]; i++) {
        char c = hex[i];
        uint8_t nibble;
        if (c >= '0' && c <= '9')      nibble = c - '0';
        else if (c >= 'a' && c <= 'f') nibble = c - 'a' + 10;
        else if (c >= 'A' && c <= 'F') nibble = c - 'A' + 10;
        else continue;
        val = (val << 4) | nibble;
    }
    return val;
}

/** Parse a 32-char hex string to 16-byte array. */
static void parse_key(const char *hex, uint8_t *out)
{
    memset(out, 0, 16);
    for (int i = 0; i < 32 && hex[i]; i += 2) {
        char buf[3] = { hex[i], hex[i + 1], '\0' };
        out[i / 2] = (uint8_t)strtoul(buf, nullptr, 16);
    }
}

/**
 * Derive DevEUI from ESP32 WiFi STA MAC address (EUI-64).
 * MAC AA:BB:CC:DD:EE:FF → DevEUI AA:BB:CC:FF:FE:DD:EE:FF
 */
static uint64_t derive_deveui_from_mac(void)
{
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);

    uint8_t eui64[8] = {
        mac[0], mac[1], mac[2],
        0xFF, 0xFE,
        mac[3], mac[4], mac[5],
    };

    uint64_t val = 0;
    for (int i = 0; i < 8; i++) {
        val = (val << 8) | eui64[i];
    }
    return val;
}

/* ------------------------------------------------------------------ */
/*  Module state                                                        */
/* ------------------------------------------------------------------ */

static IndicatorHal       *s_hal   = nullptr;
static SX1262             *s_radio = nullptr;
static LoRaWANNode        *s_node  = nullptr;

static svc_lorawan_cb_t    s_cb     = nullptr;
static void               *s_cb_ctx = nullptr;
static svc_lorawan_state_t s_state  = SVC_LORAWAN_STATE_IDLE;

static TaskHandle_t        s_task_handle = nullptr;

/* ------------------------------------------------------------------ */
/*  NVS persistence helpers                                             */
/* ------------------------------------------------------------------ */

static void persist_save_nonces(void)
{
    uint8_t buf[RADIOLIB_LORAWAN_NONCES_BUF_SIZE];
    uint8_t *p = s_node->getBufferNonces();
    memcpy(buf, p, sizeof(buf));
    nvs_save_blob(NVS_KEY_NONCES, buf, sizeof(buf));
    ESP_LOGI(TAG, "Nonces saved to NVS");
}

static void persist_save_session(void)
{
    uint8_t buf[RADIOLIB_LORAWAN_SESSION_BUF_SIZE];
    uint8_t *p = s_node->getBufferSession();
    memcpy(buf, p, sizeof(buf));
    nvs_save_blob(NVS_KEY_SESSION, buf, sizeof(buf));
    ESP_LOGI(TAG, "Session saved to NVS");
}

static bool persist_restore(void)
{
    uint8_t nonces[RADIOLIB_LORAWAN_NONCES_BUF_SIZE];
    if (nvs_load_blob(NVS_KEY_NONCES, nonces, sizeof(nonces)) != ESP_OK) {
        ESP_LOGI(TAG, "No saved nonces in NVS — fresh join required");
        return false;
    }
    int16_t state = s_node->setBufferNonces(nonces);
    if (state != RADIOLIB_ERR_NONE) {
        ESP_LOGW(TAG, "setBufferNonces failed: %d", state);
        return false;
    }

    uint8_t session[RADIOLIB_LORAWAN_SESSION_BUF_SIZE];
    if (nvs_load_blob(NVS_KEY_SESSION, session, sizeof(session)) != ESP_OK) {
        ESP_LOGI(TAG, "Nonces restored, but no session — will rejoin");
        return false;
    }
    state = s_node->setBufferSession(session);
    if (state != RADIOLIB_ERR_NONE) {
        ESP_LOGW(TAG, "setBufferSession failed: %d", state);
        return false;
    }

    ESP_LOGI(TAG, "Session + nonces restored from NVS");
    return true;
}

/* ------------------------------------------------------------------ */
/*  Event helpers                                                       */
/* ------------------------------------------------------------------ */

static void emit_event(svc_lorawan_evt_type_t type)
{
    if (!s_cb) return;
    svc_lorawan_evt_t evt = {};
    evt.type = type;
    s_cb(&evt, s_cb_ctx);
}

/* ------------------------------------------------------------------ */
/*  LoRaWAN task                                                        */
/* ------------------------------------------------------------------ */

#define JOIN_RETRY_BASE_MS   10000
#define JOIN_RETRY_MAX_MS   300000

static void lorawan_task(void *arg)
{
    (void)arg;

    /* --- Parse credentials from Kconfig --- */
    uint64_t joinEUI = parse_eui(CONFIG_LORAWAN_JOIN_EUI);
    uint64_t devEUI  = parse_eui(CONFIG_LORAWAN_DEV_EUI);
    uint8_t  appKey[16], nwkKey[16];
    parse_key(CONFIG_LORAWAN_APP_KEY, appKey);
    parse_key(CONFIG_LORAWAN_NWK_KEY, nwkKey);

    /* If DevEUI is all zeros, derive from WiFi MAC address */
    if (devEUI == 0) {
        devEUI = derive_deveui_from_mac();
        ESP_LOGI(TAG, "DevEUI derived from MAC: %016llX", (unsigned long long)devEUI);
    }

    ESP_LOGI(TAG, "DevEUI: %016llX  JoinEUI: %016llX",
             (unsigned long long)devEUI, (unsigned long long)joinEUI);

    /* --- OTAA credentials (LoRaWAN 1.1.0 — two root keys) --- */
    int16_t state = s_node->beginOTAA(joinEUI, devEUI, nwkKey, appKey);
    if (state != RADIOLIB_ERR_NONE) {
        ESP_LOGE(TAG, "beginOTAA failed: %d", state);
        s_state = SVC_LORAWAN_STATE_FAILED;
        emit_event(SVC_LORAWAN_EVT_JOIN_FAILED);
        vTaskDelete(nullptr);
        return;
    }

    /* --- Try to restore saved session from NVS --- */
    bool restored = persist_restore();

    /* Compensate for IO Expander latency BEFORE join — RadioLib default
     * scanGuard (10ms) is too tight when NSS goes through I2C (~5ms/toggle).
     * Must be set before activateOTAA so RX windows during join are wide enough. */
    s_node->scanGuard = 50;

    /* --- Activate (restore or new join) --- */
    s_state = SVC_LORAWAN_STATE_JOINING;
    uint32_t retry_ms = JOIN_RETRY_BASE_MS;

    ESP_LOGI(TAG, "Activating OTAA (%s, scanGuard=%lu ms)...",
             restored ? "restoring session" : "new join",
             (unsigned long)s_node->scanGuard);
    while (true) {
        state = s_node->activateOTAA();
        if (state == RADIOLIB_LORAWAN_NEW_SESSION) {
            ESP_LOGI(TAG, "New session established");
            persist_save_nonces();
            persist_save_session();
            s_state = SVC_LORAWAN_STATE_JOINED;
            emit_event(SVC_LORAWAN_EVT_JOINED);
            break;
        }
        if (state == RADIOLIB_LORAWAN_SESSION_RESTORED) {
            ESP_LOGI(TAG, "Session restored from NVS — no rejoin needed");
            s_state = SVC_LORAWAN_STATE_JOINED;
            emit_event(SVC_LORAWAN_EVT_JOINED);
            break;
        }

        ESP_LOGW(TAG, "Join failed (err=%d), retry in %lu ms", state, (unsigned long)retry_ms);
        vTaskDelay(pdMS_TO_TICKS(retry_ms));
        retry_ms = (retry_ms * 2 > JOIN_RETRY_MAX_MS) ? JOIN_RETRY_MAX_MS : retry_ms * 2;
    }

#if defined(CONFIG_LORAWAN_CLASS_C)
    ESP_LOGI(TAG, "Switching to Class C (scanGuard=%lu ms)", (unsigned long)s_node->scanGuard);
    s_node->setClass(RADIOLIB_LORAWAN_CLASS_C);
#endif

    /* --- Main loop: process uplink requests --- */
    for (;;) {
        /* Wait for uplink request (task notification) */
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        if (s_state != SVC_LORAWAN_STATE_JOINED) continue;

        /* Build sensor payload */
        const sensor_data_t *s = app_manager_get_sensor_data();
        uint8_t payload[12] = {0};

        uint16_t pm25     = (uint16_t)(s->pm2_5 * 10.0f);     // 0.1 µg/m³
        int16_t  temp     = (int16_t)(s->temp * 100.0f);
        uint16_t hum      = (uint16_t)(s->humidity * 100.0f);
        uint16_t voc      = (uint16_t)s->voc_index;
        uint16_t pm10_val = (uint16_t)(s->pm10 * 10.0f);

        payload[0]  = pm25 >> 8;     payload[1]  = pm25 & 0xFF;
        payload[2]  = temp >> 8;     payload[3]  = temp & 0xFF;
        payload[4]  = hum >> 8;      payload[5]  = hum & 0xFF;
        payload[6]  = voc >> 8;      payload[7]  = voc & 0xFF;
        payload[8]  = pm10_val >> 8; payload[9]  = pm10_val & 0xFF;
        payload[10] = s->rp2040_connected ? 0x01 : 0x00;

        /* sendReceive: uplink + check for downlink */
        uint8_t  dl_buf[256];
        size_t   dl_len = sizeof(dl_buf);
        LoRaWANEvent_t ul_event = {0};
        LoRaWANEvent_t dl_event = {0};

        int16_t result = s_node->sendReceive(
            payload, sizeof(payload), 1,
            dl_buf, &dl_len,
            false,
            &ul_event, &dl_event
        );

        /* LoRa SPI bit-bang shares GPIO 41/48 with ST7701S — the clock
         * transitions can desync the RGB panel horizontal counter.
         * Restart the panel to re-sync from VSYNC. */
        bsp_display_rgb_restart();

        /* Always persist session after sendReceive — frame counter increments
         * regardless of TX success. Not saving causes FCntUp retransmission warnings. */
        persist_save_session();

        if (result >= RADIOLIB_ERR_NONE) {
            ESP_LOGI(TAG, "Uplink OK: PM2.5=%u.%u T=%.1f H=%.1f VOC=%u PM10=%u.%u",
                     pm25 / 10, pm25 % 10, s->temp, s->humidity,
                     voc, pm10_val / 10, pm10_val % 10);
            emit_event(SVC_LORAWAN_EVT_TX_DONE);

            /* Check for downlink — result > 0 means application downlink present */
            if (result > 0) {
                ESP_LOGI(TAG, "Downlink received: port=%u len=%u",
                         dl_event.fPort, (unsigned)dl_len);
                if (dl_event.fPort > 0 && s_cb) {
                    svc_lorawan_evt_t evt = {};
                    evt.type = SVC_LORAWAN_EVT_DOWNLINK;
                    evt.downlink.port = dl_event.fPort;
                    evt.downlink.data = dl_buf;
                    evt.downlink.len  = (uint16_t)dl_len;
                    s_cb(&evt, s_cb_ctx);
                }
            }
        } else {
            ESP_LOGW(TAG, "Uplink failed: %d", result);
        }
    }
}

/* ------------------------------------------------------------------ */
/*  Public API (extern "C")                                             */
/* ------------------------------------------------------------------ */

extern "C" {

esp_err_t svc_lorawan_init(void)
{
    if (s_hal) return ESP_OK; /* already initialised */

    /* Create HAL with SPI pins */
    s_hal = new IndicatorHal(BSP_LORA_SPI_SCLK_IO, BSP_LORA_SPI_MISO_IO, BSP_LORA_SPI_MOSI_IO);

    /* Create radio module — using virtual pin numbers for IO Expander pins */
    Module *mod = new Module(
        s_hal,
        IO_EXP_PIN_BASE + BSP_IO_EXP_LORA_NSS,   /* NSS  = 100 */
        IO_EXP_PIN_BASE + BSP_IO_EXP_LORA_DIO1,   /* DIO1 = 103 */
        IO_EXP_PIN_BASE + BSP_IO_EXP_LORA_RST,    /* RST  = 101 */
        IO_EXP_PIN_BASE + BSP_IO_EXP_LORA_BUSY    /* BUSY = 102 */
    );
    s_radio = new SX1262(mod);

    /* Initialise the radio.
     * SenseCAP Indicator D1L/D1Pro uses TCXO at 2.4V — must be set here
     * or the crystal oscillator won't start and SPI returns no response.
     *
     * begin(freq, bw, sf, cr, syncWord, power, preambleLength, tcxoVoltage, useRegulatorLDO)
     * These RF params will be overridden by LoRaWAN MAC, but tcxoVoltage is critical.
     */
    int16_t state = s_radio->begin(868.0, 125.0, 12, 5,
                                   RADIOLIB_SX126X_SYNC_WORD_PRIVATE,
                                   22, 8, 2.4, false);
    if (state != RADIOLIB_ERR_NONE) {
        ESP_LOGW(TAG, "SX1262 begin() failed: %d — no LoRa hardware? (D1/D1S)", state);
        /* Clean up — this is not a fatal error on boards without LoRa */
        delete s_radio; s_radio = nullptr;
        delete s_hal; s_hal = nullptr;
        return ESP_ERR_NOT_FOUND;
    }

    /* DIO2 as RF switch — required for SX1262 internal TX/RX path control.
     * On SenseCAP Indicator, DIO2 is wired to VBAT_IO but the SX1262 still
     * needs this mode enabled for proper PA/LNA routing. */
    s_radio->setDio2AsRfSwitch(true);

    /* Create LoRaWAN node */
    s_node = new LoRaWANNode(s_radio, &s_region, CONFIG_LORAWAN_SUBBAND);

    ESP_LOGI(TAG, "LoRaWAN stack initialised (RadioLib)");
    return ESP_OK;
}

void svc_lorawan_register_cb(svc_lorawan_cb_t cb, void *user_ctx)
{
    s_cb     = cb;
    s_cb_ctx = user_ctx;
}

esp_err_t svc_lorawan_join(void)
{
    if (!s_node) return ESP_ERR_INVALID_STATE;
    if (s_state == SVC_LORAWAN_STATE_JOINING) return ESP_OK;

    /* Launch the LoRaWAN task which handles join + Class C RX */
    BaseType_t ret = xTaskCreate(lorawan_task, "lorawan", 8192, nullptr, 5, &s_task_handle);
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create lorawan task");
        return ESP_FAIL;
    }
    return ESP_OK;
}

esp_err_t svc_lorawan_send(uint8_t port, const uint8_t *data, size_t len, bool confirmed)
{
    if (!s_node || s_state != SVC_LORAWAN_STATE_JOINED) {
        return ESP_ERR_INVALID_STATE;
    }

    /* sendReceive: uplink + receive any pending downlink */
    uint8_t  dl_buf[256];
    size_t   dl_len = sizeof(dl_buf);
    LoRaWANEvent_t ul_event, dl_event;

    int16_t state = s_node->sendReceive(
        data, len, port,
        dl_buf, &dl_len,
        confirmed,
        &ul_event, &dl_event
    );

    /* Re-sync RGB panel after LoRa SPI burst (shared GPIO 41/48) */
    bsp_display_rgb_restart();

    if (state < RADIOLIB_ERR_NONE) {
        ESP_LOGW(TAG, "sendReceive failed: %d", state);
        return ESP_FAIL;
    }

    /* Persist session after each uplink (frame counters changed) */
    persist_save_session();

    /* Notify TX done */
    emit_event(SVC_LORAWAN_EVT_TX_DONE);

    /* Check for downlink */
    if (state > 0 && dl_len > 0 && s_cb) {
        svc_lorawan_evt_t evt = {};
        evt.type = SVC_LORAWAN_EVT_DOWNLINK;
        evt.downlink.port = dl_event.fPort;
        evt.downlink.data = dl_buf;
        evt.downlink.len  = (uint16_t)dl_len;
        evt.downlink.rssi = 0;
        evt.downlink.snr  = 0;
        s_cb(&evt, s_cb_ctx);
    }

    return ESP_OK;
}

svc_lorawan_state_t svc_lorawan_get_state(void)
{
    return s_state;
}

void svc_lorawan_request_uplink(void)
{
    if (s_task_handle) {
        xTaskNotifyGive(s_task_handle);
    }
}

} /* extern "C" */

#endif /* CONFIG_INDICATOR_LORAWAN_ENABLED */
