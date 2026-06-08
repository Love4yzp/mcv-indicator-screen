#include "svc_ha_mqtt.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "app/app_manager.h"
#include "esp_console.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_timer.h"
#include "mqtt_client.h"
#include "svc_storage.h"

static const char *TAG = "svc_ha_mqtt";

#define DEFAULT_HOST "172.16.0.4"
#define DEFAULT_PORT 1883
#define PUBLISH_PERIOD_US (30LL * 1000 * 1000)

#define DEV_ID_LEN 7  // 6 hex chars + NUL
#define TOPIC_BUF  96
#define UNIQ_BUF   48
#define JSON_BUF   512

typedef struct {
    const char *key;       // value_json key + uniq_id suffix + discovery node id
    const char *name;      // friendly name in HA
    const char *unit;      // unit_of_measurement (NULL → omit)
    const char *dev_cla;   // device_class (NULL → omit)
} sensor_meta_t;

/* HA recognised device_class values: pm1/pm25/pm10/temperature/humidity/aqi.
 * pm4 has no device_class — omit; HA renders as plain numeric sensor. */
static const sensor_meta_t s_sensors[] = {
    {"pm1",      "PM1.0",       "µg/m³", "pm1"},
    {"pm25",     "PM2.5",       "µg/m³", "pm25"},
    {"pm4",      "PM4.0",       "µg/m³", NULL},
    {"pm10",     "PM10",        "µg/m³", "pm10"},
    {"temp",     "Temperature", "°C",    "temperature"},
    {"humidity", "Humidity",    "%",     "humidity"},
    {"voc",      "VOC Index",   NULL,    "aqi"},
};
#define SENSOR_COUNT (sizeof(s_sensors) / sizeof(s_sensors[0]))

static esp_mqtt_client_handle_t s_client = NULL;
static esp_timer_handle_t       s_pub_timer = NULL;
static char  s_dev_id[DEV_ID_LEN] = {0};
static char  s_topic_state[TOPIC_BUF] = {0};
static char  s_topic_avail[TOPIC_BUF] = {0};
static char  s_broker_str[56] = {0};   // "host:port" for status display
static bool  s_connected = false;

static void register_console_cmd(void);

static void push_status(bool connected)
{
    char text[80];
    snprintf(text, sizeof(text), "MQTT: %s · %s",
             connected ? "connected" : "offline", s_broker_str);
    app_manager_update_mqtt_status(text);
}

static void make_dev_id(char *out)
{
    uint8_t mac[6] = {0};
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    snprintf(out, DEV_ID_LEN, "%02x%02x%02x", mac[3], mac[4], mac[5]);
}

static void publish_discovery(void)
{
    char topic[TOPIC_BUF];
    char json[JSON_BUF];
    char uniq[UNIQ_BUF];

    for (size_t i = 0; i < SENSOR_COUNT; ++i) {
        const sensor_meta_t *s = &s_sensors[i];
        snprintf(topic, sizeof(topic),
                 "homeassistant/sensor/chaihuo_indicator_%s/%s/config",
                 s_dev_id, s->key);
        snprintf(uniq, sizeof(uniq),
                 "chaihuo_indicator_%s_%s", s_dev_id, s->key);

        int n = snprintf(json, sizeof(json),
            "{\"name\":\"%s\","
            "\"uniq_id\":\"%s\","
            "\"stat_t\":\"%s\","
            "\"avty_t\":\"%s\","
            "\"val_tpl\":\"{{ value_json.%s }}\","
            "\"stat_cla\":\"measurement\"",
            s->name, uniq, s_topic_state, s_topic_avail, s->key);

        if (s->unit && n < (int)sizeof(json)) {
            n += snprintf(json + n, sizeof(json) - n,
                          ",\"unit_of_meas\":\"%s\"", s->unit);
        }
        if (s->dev_cla && n < (int)sizeof(json)) {
            n += snprintf(json + n, sizeof(json) - n,
                          ",\"dev_cla\":\"%s\"", s->dev_cla);
        }
        if (n < (int)sizeof(json)) {
            snprintf(json + n, sizeof(json) - n,
                     ",\"dev\":{\"ids\":[\"chaihuo_indicator_%s\"],"
                     "\"name\":\"Indicator %s\","
                     "\"mf\":\"Seeed\","
                     "\"mdl\":\"SenseCAP Indicator\"}}",
                     s_dev_id, s_dev_id);
        }

        int msg_id = esp_mqtt_client_publish(s_client, topic, json, 0, 1, 1);
        if (msg_id < 0) {
            ESP_LOGW(TAG, "discovery publish failed: %s", topic);
        }
    }
    ESP_LOGI(TAG, "HA discovery sent for %u sensors (id=%s)",
             (unsigned)SENSOR_COUNT, s_dev_id);
}

static void publish_state(void)
{
    if (!s_connected) return;

    const sensor_data_t *d = app_manager_get_sensor_data();
    if (!d || !d->rp2040_connected) {
        ESP_LOGD(TAG, "skip publish: no RP2040 data yet");
        return;
    }

    char json[160];
    int n = snprintf(json, sizeof(json),
        "{\"pm1\":%.1f,\"pm25\":%.1f,\"pm4\":%.1f,\"pm10\":%.1f,"
        "\"temp\":%.1f,\"humidity\":%.1f,\"voc\":%.0f}",
        d->pm1_0, d->pm2_5, d->pm4_0, d->pm10,
        d->temp, d->humidity, d->voc_index);
    if (n <= 0) return;

    int msg_id = esp_mqtt_client_publish(s_client, s_topic_state, json, n, 1, 1);
    if (msg_id < 0) {
        ESP_LOGW(TAG, "state publish failed");
    } else {
        ESP_LOGD(TAG, "state published (%d bytes)", n);
    }
}

static void publish_timer_cb(void *arg)
{
    (void)arg;
    publish_state();
}

static void mqtt_event_handler(void *arg, esp_event_base_t base,
                               int32_t event_id, void *event_data)
{
    (void)arg;
    (void)base;
    (void)event_data;
    switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "broker connected");
        s_connected = true;
        esp_mqtt_client_publish(s_client, s_topic_avail, "online", 0, 1, 1);
        publish_discovery();
        publish_state();  // immediate first sample if available
        push_status(true);
        break;
    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGW(TAG, "broker disconnected");
        s_connected = false;
        push_status(false);
        break;
    case MQTT_EVENT_ERROR:
        ESP_LOGW(TAG, "mqtt event error");
        break;
    default:
        break;
    }
}

static void resolve_cfg(svc_storage_mqtt_cfg_t *cfg)
{
    svc_storage_load_mqtt_cfg(cfg);
    if (cfg->host[0] == '\0') strlcpy(cfg->host, DEFAULT_HOST, sizeof(cfg->host));
    if (cfg->port == 0)       cfg->port = DEFAULT_PORT;
}

static esp_err_t start_client(const svc_storage_mqtt_cfg_t *cfg)
{
    char uri[80];
    snprintf(uri, sizeof(uri), "mqtt://%s:%u", cfg->host, (unsigned)cfg->port);
    snprintf(s_broker_str, sizeof(s_broker_str), "%s:%u", cfg->host, (unsigned)cfg->port);

    esp_mqtt_client_config_t mcfg = {
        .broker.address.uri = uri,
        .credentials.client_id = NULL,           // let esp-mqtt auto-generate
        .session.last_will = {
            .topic   = s_topic_avail,
            .msg     = "offline",
            .msg_len = 7,
            .qos     = 1,
            .retain  = 1,
        },
        .session.keepalive = 60,
    };
    if (cfg->user[0] != '\0') {
        mcfg.credentials.username = cfg->user;
        mcfg.credentials.authentication.password = cfg->pass;
    }

    s_client = esp_mqtt_client_init(&mcfg);
    if (!s_client) return ESP_FAIL;

    esp_mqtt_client_register_event(s_client, ESP_EVENT_ANY_ID,
                                   mqtt_event_handler, NULL);
    esp_err_t err = esp_mqtt_client_start(s_client);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "client_start failed: %s", esp_err_to_name(err));
        esp_mqtt_client_destroy(s_client);
        s_client = NULL;
        return err;
    }
    ESP_LOGI(TAG, "client started uri=%s user=%s", uri,
             cfg->user[0] ? cfg->user : "<anonymous>");
    return ESP_OK;
}

esp_err_t svc_ha_mqtt_init(void)
{
    if (s_client) return ESP_OK;

    make_dev_id(s_dev_id);
    snprintf(s_topic_state, sizeof(s_topic_state),
             "chaihuo/indicator/%s/state", s_dev_id);
    snprintf(s_topic_avail, sizeof(s_topic_avail),
             "chaihuo/indicator/%s/avail", s_dev_id);

    svc_storage_mqtt_cfg_t cfg = {0};
    resolve_cfg(&cfg);

    esp_err_t err = start_client(&cfg);
    if (err != ESP_OK) return err;

    esp_timer_create_args_t targs = {
        .callback = publish_timer_cb,
        .name     = "ha_mqtt_pub",
    };
    err = esp_timer_create(&targs, &s_pub_timer);
    if (err != ESP_OK) return err;
    err = esp_timer_start_periodic(s_pub_timer, PUBLISH_PERIOD_US);
    if (err != ESP_OK) return err;

    register_console_cmd();
    push_status(false);  // initial state until CONNECTED event arrives
    return ESP_OK;
}

static int cmd_mqtt(int argc, char **argv)
{
    svc_storage_mqtt_cfg_t cfg = {0};
    svc_storage_load_mqtt_cfg(&cfg);

    if (argc < 2 || strcmp(argv[1], "show") == 0) {
        svc_storage_mqtt_cfg_t eff = cfg;
        if (eff.host[0] == '\0') strlcpy(eff.host, DEFAULT_HOST, sizeof(eff.host));
        if (eff.port == 0)       eff.port = DEFAULT_PORT;
        printf("host=%s port=%u user=%s connected=%s id=%s\n",
               eff.host, (unsigned)eff.port,
               eff.user[0] ? eff.user : "<anon>",
               s_connected ? "yes" : "no",
               s_dev_id);
        return 0;
    }

    if (strcmp(argv[1], "host") == 0) {
        if (argc < 3) {
            printf("usage: mqtt host <addr> [port]\n");
            return 1;
        }
        strlcpy(cfg.host, argv[2], sizeof(cfg.host));
        if (argc >= 4) cfg.port = (uint16_t)atoi(argv[3]);
    } else if (strcmp(argv[1], "user") == 0) {
        if (argc < 3) {
            printf("usage: mqtt user <name> [pass]\n");
            return 1;
        }
        strlcpy(cfg.user, argv[2], sizeof(cfg.user));
        if (argc >= 4) strlcpy(cfg.pass, argv[3], sizeof(cfg.pass));
        else           cfg.pass[0] = '\0';
    } else if (strcmp(argv[1], "reset") == 0) {
        memset(&cfg, 0, sizeof(cfg));
    } else {
        printf("usage: mqtt [show | host <addr> [port] | user <name> [pass] | reset]\n");
        return 1;
    }

    esp_err_t err = svc_ha_mqtt_set_broker(cfg.host, cfg.port, cfg.user, cfg.pass);
    printf("%s\n", err == ESP_OK ? "OK (reconnecting)" : esp_err_to_name(err));
    return err == ESP_OK ? 0 : 1;
}

static void register_console_cmd(void)
{
    const esp_console_cmd_t cmd = {
        .command = "mqtt",
        .help    = "show or change MQTT broker config (persists in NVS)",
        .hint    = "[show | host <addr> [port] | user <name> [pass] | reset]",
        .func    = &cmd_mqtt,
    };
    esp_err_t err = esp_console_cmd_register(&cmd);
    if (err != ESP_OK) ESP_LOGW(TAG, "console cmd register failed: %s",
                                esp_err_to_name(err));
}

esp_err_t svc_ha_mqtt_set_broker(const char *host, uint16_t port,
                                 const char *user, const char *pass)
{
    svc_storage_mqtt_cfg_t cfg = {0};
    if (host) strlcpy(cfg.host, host, sizeof(cfg.host));
    cfg.port = port;
    if (user) strlcpy(cfg.user, user, sizeof(cfg.user));
    if (pass) strlcpy(cfg.pass, pass, sizeof(cfg.pass));

    esp_err_t err = svc_storage_save_mqtt_cfg(&cfg);
    if (err != ESP_OK) return err;

    if (s_client) {
        esp_mqtt_client_stop(s_client);
        esp_mqtt_client_destroy(s_client);
        s_client = NULL;
        s_connected = false;
    }

    svc_storage_mqtt_cfg_t resolved = cfg;
    if (resolved.host[0] == '\0') strlcpy(resolved.host, DEFAULT_HOST, sizeof(resolved.host));
    if (resolved.port == 0)       resolved.port = DEFAULT_PORT;
    return start_client(&resolved);
}

const char *svc_ha_mqtt_device_id(void) { return s_dev_id; }
