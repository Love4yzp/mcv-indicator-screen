#pragma once

#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t svc_ha_mqtt_init(void);

/* Apply a new broker config and reconnect. host/user/pass are copied,
 * port=0 falls back to 1883. Persists to NVS. Safe to call from any task. */
esp_err_t svc_ha_mqtt_set_broker(const char *host, uint16_t port,
                                 const char *user, const char *pass);

/* Returns the device id (lowercase hex of MAC[3..5]) — populated after init. */
const char *svc_ha_mqtt_device_id(void);

#ifdef __cplusplus
}
#endif
