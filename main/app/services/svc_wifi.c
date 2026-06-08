/*
 * Service: WiFi + BLE provisioning.
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Spencer (love4yzp)
 */

#include "svc_wifi.h"

#include <string.h>

#include "app/app_manager.h"
#include "esp_check.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_netif.h"
#include "esp_system.h"
#include "esp_sntp.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "wifi_provisioning/manager.h"
#include "wifi_provisioning/scheme_ble.h"

static const char *TAG = "svc_wifi";

#define PROV_POP "sensecap123"
#define PROV_QR_VER "v1"
#define PROV_TRANSPORT "ble"
#define BACKOFF_INIT_MS 2000
#define BACKOFF_MAX_MS 30000

static svc_wifi_state_t s_state = WIFI_ST_IDLE;
static int s_retry_count;
static int s_backoff_ms = BACKOFF_INIT_MS;
static bool s_prov_mgr_inited;
static esp_timer_handle_t s_reconnect_timer;

static void make_service_name(char *buf, size_t len)
{
    uint8_t mac[6] = {0};
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    snprintf(buf, len, "PROV_%02X%02X%02X", mac[3], mac[4], mac[5]);
}

static void make_qr_payload(char *buf, size_t len, const char *svc)
{
    snprintf(buf, len, "{\"ver\":\"%s\",\"name\":\"%s\",\"pop\":\"%s\",\"transport\":\"%s\"}", PROV_QR_VER,
             svc, PROV_POP, PROV_TRANSPORT);
}

static void start_ntp(void)
{
    if (esp_sntp_enabled()) return;
    esp_sntp_setoperatingmode(ESP_SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, "pool.ntp.org");
    esp_sntp_init();
}

static void reconnect_timer_cb(void *arg)
{
    (void)arg;
    ESP_LOGI(TAG, "Reconnect attempt %d (backoff %dms)", s_retry_count, s_backoff_ms);
    esp_wifi_connect();
}

static void schedule_reconnect(void)
{
    if (s_reconnect_timer != NULL) esp_timer_start_once(s_reconnect_timer, (uint64_t)s_backoff_ms * 1000);
    s_backoff_ms = (s_backoff_ms * 2 > BACKOFF_MAX_MS) ? BACKOFF_MAX_MS : s_backoff_ms * 2;
}

static void reset_backoff(void)
{
    s_backoff_ms = BACKOFF_INIT_MS;
    s_retry_count = 0;
}

static void event_handler(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    (void)arg;
    if (base == WIFI_PROV_EVENT) {
        switch (id) {
        case WIFI_PROV_START:
            s_state = WIFI_ST_PROVISIONING;
            app_manager_update_wifi_info("WiFi: BLE provisioning active", "Scan QR with ESP BLE Prov app");
            break;
        case WIFI_PROV_CRED_RECV: {
            wifi_sta_config_t *cfg = (wifi_sta_config_t *)data;
            char line[96];
            snprintf(line, sizeof(line), "Received SSID: %s", (const char *)cfg->ssid);
            app_manager_update_wifi_info("WiFi credentials received", line);
            break;
        }
        case WIFI_PROV_CRED_FAIL:
            app_manager_update_wifi_info("Provisioning failed", "Check SSID/password, scan again");
            break;
        case WIFI_PROV_CRED_SUCCESS:
            if (s_state != WIFI_ST_CONNECTED) {
                app_manager_update_wifi_info("Credentials saved", "Connecting...");
            }
            break;
        case WIFI_PROV_END:
            wifi_prov_mgr_deinit();
            s_prov_mgr_inited = false;
            break;
        default:
            break;
        }
        return;
    }

    if (base == WIFI_EVENT) {
        switch (id) {
        case WIFI_EVENT_STA_START:
            reset_backoff();
            s_state = WIFI_ST_CONNECTING;
            esp_wifi_connect();
            app_manager_update_wifi_info("WiFi: connecting...", "");
            break;
        case WIFI_EVENT_STA_DISCONNECTED: {
            s_state = WIFI_ST_DISCONNECTED;
            s_retry_count++;
            wifi_event_sta_disconnected_t *d = (wifi_event_sta_disconnected_t *)data;
            char detail[64];
            snprintf(detail, sizeof(detail), "Retry %d in %ds (reason=%d)", s_retry_count, s_backoff_ms / 1000,
                     d != NULL ? d->reason : -1);
            app_manager_update_wifi_info("WiFi disconnected", detail);
            schedule_reconnect();
            break;
        }
        default:
            break;
        }
        return;
    }

    if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)data;
        char ip[32];
        snprintf(ip, sizeof(ip), IPSTR, IP2STR(&event->ip_info.ip));
        s_state = WIFI_ST_CONNECTED;
        reset_backoff();
        app_manager_hide_wifi_qr();
        app_manager_update_wifi_info("WiFi connected", ip);
        start_ntp();
    }
}

static esp_err_t start_ble_prov(void)
{
    if (!s_prov_mgr_inited) {
        wifi_prov_mgr_config_t cfg = {
            .scheme = wifi_prov_scheme_ble,
            .scheme_event_handler = WIFI_PROV_SCHEME_BLE_EVENT_HANDLER_FREE_BTDM,
        };
        ESP_RETURN_ON_ERROR(wifi_prov_mgr_init(cfg), TAG, "prov mgr init");
        ESP_RETURN_ON_ERROR(wifi_prov_mgr_disable_auto_stop(1000), TAG, "disable auto stop");
        s_prov_mgr_inited = true;
    }

    char svc[16];
    char qr[192];
    make_service_name(svc, sizeof(svc));
    make_qr_payload(qr, sizeof(qr), svc);
    ESP_RETURN_ON_ERROR(wifi_prov_mgr_start_provisioning(WIFI_PROV_SECURITY_1, PROV_POP, svc, NULL), TAG,
                        "start prov");
    app_manager_show_wifi_qr(qr, svc, PROV_POP);
    app_manager_update_wifi_info("WiFi: BLE provisioning ready", svc);
    return ESP_OK;
}

esp_err_t svc_wifi_init(void)
{
    esp_timer_create_args_t timer_args = {.callback = reconnect_timer_cb, .name = "wifi_reconn"};
    ESP_RETURN_ON_ERROR(esp_timer_create(&timer_args, &s_reconnect_timer), TAG, "timer create");

    esp_err_t err = esp_netif_init();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) return err;
    err = esp_event_loop_create_default();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) return err;

    esp_netif_create_default_wifi_sta();
    wifi_init_config_t wifi_cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_RETURN_ON_ERROR(esp_wifi_init(&wifi_cfg), TAG, "wifi init");
    ESP_RETURN_ON_ERROR(esp_event_handler_register(WIFI_PROV_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL), TAG,
                        "prov event reg");
    ESP_RETURN_ON_ERROR(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL), TAG,
                        "wifi event reg");
    ESP_RETURN_ON_ERROR(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL), TAG,
                        "ip event reg");

    bool provisioned = false;
    ESP_RETURN_ON_ERROR(wifi_prov_mgr_is_provisioned(&provisioned), TAG, "is_provisioned");

    if (provisioned) {
        ESP_RETURN_ON_ERROR(esp_wifi_set_mode(WIFI_MODE_STA), TAG, "set mode");
        ESP_RETURN_ON_ERROR(esp_wifi_start(), TAG, "wifi start");
        app_manager_update_wifi_info("WiFi: saved credentials", "Connecting...");
    } else {
        ESP_RETURN_ON_ERROR(start_ble_prov(), TAG, "start BLE prov");
    }
    return ESP_OK;
}

esp_err_t svc_wifi_reprovision(void)
{
    /* Clear saved credentials so the next boot enters provisioning mode.
     * We must restart because wifi_prov_scheme_ble with FREE_BTDM releases
     * all BT memory on deinit — it cannot be re-initialized at runtime. */
    if (!s_prov_mgr_inited) {
        wifi_prov_mgr_config_t cfg = {
            .scheme = wifi_prov_scheme_ble,
            .scheme_event_handler = WIFI_PROV_SCHEME_BLE_EVENT_HANDLER_FREE_BTDM,
        };
        ESP_RETURN_ON_ERROR(wifi_prov_mgr_init(cfg), TAG, "prov mgr init");
        s_prov_mgr_inited = true;
    }
    ESP_RETURN_ON_ERROR(wifi_prov_mgr_reset_provisioning(), TAG, "reset prov");
    ESP_LOGI(TAG, "Provisioning data cleared, restarting...");
    esp_restart();
    __builtin_unreachable();
}

svc_wifi_state_t svc_wifi_get_state(void) { return s_state; }
