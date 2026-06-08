/*
 * Service: PM2.5 threshold alert + buzzer control.
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Spencer (love4yzp)
 */

#include "svc_alerts.h"

#include <stdio.h>

#include "app/pages/page_sensor.h"
#include "lv_port.h"
#include "lvgl.h"

static bool s_active = false;

void svc_alerts_init(void) { s_active = false; }

void svc_alerts_check(const sensor_data_t *data)
{
    if (data == NULL || !data->rp2040_connected || data->pm2_5 <= 0) return;

    int threshold = 35;
    bool over = (data->pm2_5 > threshold);

    if (over && !s_active) {
        s_active = true;
        char buf[64];
        snprintf(buf, sizeof(buf), LV_SYMBOL_WARNING " PM2.5 %.1f > %d \xC2\xB5g/m\xC2\xB3",
                 data->pm2_5, threshold);
        lv_port_lock();
        page_sensor_show_alert(buf);
        lv_port_unlock();
        const platform_callbacks_t *cbs = app_manager_get_callbacks();
        if (cbs != NULL && cbs->send_beep != NULL) cbs->send_beep();
    } else if (!over && s_active) {
        s_active = false;
        lv_port_lock();
        page_sensor_hide_alert();
        lv_port_unlock();
        const platform_callbacks_t *cbs = app_manager_get_callbacks();
        if (cbs != NULL && cbs->stop_beep != NULL) cbs->stop_beep();
    }
}

void svc_alerts_dismiss(void)
{
    if (!s_active) return;
    s_active = false;
    lv_port_lock();
    page_sensor_hide_alert();
    lv_port_unlock();
    const platform_callbacks_t *cbs = app_manager_get_callbacks();
    if (cbs != NULL && cbs->stop_beep != NULL) cbs->stop_beep();
}
