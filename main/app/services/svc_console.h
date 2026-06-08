#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Start the UART0 REPL @115200. Call AFTER all services have registered
 * their commands so help/tab-completion sees them on first prompt. */
esp_err_t svc_console_start(void);

#ifdef __cplusplus
}
#endif
