/* Simulator shim for FreeRTOS portmacro.h — single-threaded, no-op */
#pragma once

typedef int portMUX_TYPE;
#define portMUX_INITIALIZER_UNLOCKED 0
#define portENTER_CRITICAL(mux) (void)(mux)
#define portEXIT_CRITICAL(mux) (void)(mux)
