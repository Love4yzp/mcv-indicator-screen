/*
 * Simulator platform stubs — replaces ESP-IDF lv_port on desktop.
 * SPDX-License-Identifier: MIT
 */
#include "lv_port.h"

void lv_port_lock(void)
{
    /* single-threaded simulator — no locking needed */
}

void lv_port_unlock(void)
{
    /* single-threaded simulator — no locking needed */
}
