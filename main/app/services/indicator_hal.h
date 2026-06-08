/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Spencer (love4yzp)
 *
 * RadioLib HAL for SenseCAP Indicator (ESP32-S3 + TCA9535 IO Expander).
 *
 * Pin numbering convention:
 *   0..99   = direct ESP32-S3 GPIO
 *   100+    = IO Expander pin (pin_number - 100)
 *
 * This header-only class is included from svc_lorawan.cpp.
 */

#pragma once

#include <cstring>
#include "RadioLib.h"

#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_rom_sys.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include "bsp_display.h"
#include "bsp_indicator.h"

#define INDICATOR_HAL_TAG "indicator_hal"

/* Threshold for IO expander virtual pins */
#define IO_EXP_PIN_BASE 100

/* Arduino-style constants used by RadioLib HAL base class */
#define HAL_INPUT   0x01
#define HAL_OUTPUT  0x02
#define HAL_LOW     0x00
#define HAL_HIGH    0x01
#define HAL_RISING  0x01
#define HAL_FALLING 0x02

class IndicatorHal : public RadioLibHal {
public:
    IndicatorHal(int8_t sck, int8_t miso, int8_t mosi)
        : RadioLibHal(HAL_INPUT, HAL_OUTPUT, HAL_LOW, HAL_HIGH, HAL_RISING, HAL_FALLING),
          _sck(sck), _miso(miso), _mosi(mosi),
          _spi_mutex(nullptr),
          _irq_cb(nullptr), _irq_gpio_queue(nullptr)
    {
    }

    /* ---- Lifecycle --------------------------------------------------- */

    void init() override
    {
        if (_spi_mutex) return; /* already initialised */
        _spi_mutex = xSemaphoreCreateMutex();
        assert(_spi_mutex);

        spiBegin();

        /* Configure IO expander pin directions for LoRa radio */
        bsp_io_expander_set_dir_safe(BIT(BSP_IO_EXP_LORA_NSS),  IO_EXPANDER_OUTPUT);
        bsp_io_expander_set_dir_safe(BIT(BSP_IO_EXP_LORA_RST),  IO_EXPANDER_OUTPUT);
        bsp_io_expander_set_dir_safe(BIT(BSP_IO_EXP_LORA_BUSY), IO_EXPANDER_INPUT);
        bsp_io_expander_set_dir_safe(BIT(BSP_IO_EXP_LORA_DIO1), IO_EXPANDER_INPUT);
        bsp_io_expander_set_dir_safe(BIT(BSP_IO_EXP_LORA_VER),  IO_EXPANDER_INPUT);

        /* NSS HIGH (deselected) before any SPI traffic */
        bsp_io_expander_set_level_safe(BIT(BSP_IO_EXP_LORA_NSS), 1);

        /* Detect TCXO via VER pin */
        uint32_t ver = 0;
        bsp_io_expander_get_level_safe(BIT(BSP_IO_EXP_LORA_VER), &ver);
        ESP_LOGI(INDICATOR_HAL_TAG, "LoRa VER pin (TCXO detect): %s", ver ? "HIGH (present)" : "LOW (absent)");
    }

    void term() override
    {
        spiEnd();
        if (_spi_mutex) {
            vSemaphoreDelete(_spi_mutex);
            _spi_mutex = nullptr;
        }
    }

    /* ---- GPIO -------------------------------------------------------- */

    void pinMode(uint32_t pin, uint32_t mode) override
    {
        if (pin == RADIOLIB_NC) return;

        if (pin >= IO_EXP_PIN_BASE) {
            uint8_t exp_pin = pin - IO_EXP_PIN_BASE;
            esp_io_expander_dir_t dir = (mode == HAL_OUTPUT)
                ? IO_EXPANDER_OUTPUT : IO_EXPANDER_INPUT;
            bsp_io_expander_set_dir_safe(BIT(exp_pin), dir);
        } else {
            gpio_config_t cfg = {
                .pin_bit_mask = (1ULL << pin),
                .mode = (mode == HAL_OUTPUT) ? GPIO_MODE_OUTPUT : GPIO_MODE_INPUT,
                .pull_up_en = GPIO_PULLUP_DISABLE,
                .pull_down_en = GPIO_PULLDOWN_DISABLE,
                .intr_type = GPIO_INTR_DISABLE,
            };
            gpio_config(&cfg);
        }
    }

    void digitalWrite(uint32_t pin, uint32_t value) override
    {
        if (pin == RADIOLIB_NC) return;

        if (pin >= IO_EXP_PIN_BASE) {
            uint8_t exp_pin = pin - IO_EXP_PIN_BASE;
            bsp_io_expander_set_level_safe(BIT(exp_pin), value);
        } else {
            gpio_set_level((gpio_num_t)pin, value);
        }
    }

    uint32_t digitalRead(uint32_t pin) override
    {
        if (pin == RADIOLIB_NC) return 0;

        if (pin >= IO_EXP_PIN_BASE) {
            uint8_t exp_pin = pin - IO_EXP_PIN_BASE;
            uint32_t level = 0;
            bsp_io_expander_get_level_safe(BIT(exp_pin), &level);
            return (level != 0) ? 1 : 0;
        }
        return (uint32_t)gpio_get_level((gpio_num_t)pin);
    }

    /* ---- Interrupts -------------------------------------------------- */

    void attachInterrupt(uint32_t interruptNum, void (*interruptCb)(void), uint32_t mode) override
    {
        if (interruptNum == RADIOLIB_NC) return;

        _irq_cb = interruptCb;

        if (_irq_gpio_queue) return; /* already initialised */
        _irq_gpio_queue = xQueueCreate(10, sizeof(uint32_t));

        xTaskCreate(_irq_task_trampoline, "lorawan_irq", 4096, this, 10, nullptr);

        gpio_config_t cfg = {
            .pin_bit_mask = (1ULL << BSP_IO_EXP_INT_IO),
            .mode = GPIO_MODE_INPUT,
            .pull_up_en = GPIO_PULLUP_ENABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type = GPIO_INTR_NEGEDGE,
        };
        gpio_config(&cfg);
        gpio_install_isr_service(0);
        gpio_isr_handler_add((gpio_num_t)BSP_IO_EXP_INT_IO, _gpio_isr, this);
    }

    void detachInterrupt(uint32_t interruptNum) override
    {
        if (interruptNum == RADIOLIB_NC) return;
        _irq_cb = nullptr;
        gpio_isr_handler_remove((gpio_num_t)BSP_IO_EXP_INT_IO);
    }

    /* ---- Timing ------------------------------------------------------ */

    void delay(RadioLibTime_t ms) override
    {
        vTaskDelay(pdMS_TO_TICKS(ms));
    }

    void delayMicroseconds(RadioLibTime_t us) override
    {
        esp_rom_delay_us((uint32_t)us);
    }

    RadioLibTime_t millis() override
    {
        return (RadioLibTime_t)(esp_timer_get_time() / 1000ULL);
    }

    RadioLibTime_t micros() override
    {
        return (RadioLibTime_t)esp_timer_get_time();
    }

    long pulseIn(uint32_t pin, uint32_t state, RadioLibTime_t timeout) override
    {
        (void)pin; (void)state; (void)timeout;
        return 0; /* not used by LoRaWAN */
    }

    /* ---- SPI --------------------------------------------------------- */

    void spiBegin() override
    {
        /* Bit-bang SPI: configure GPIO pins manually.
         * We do NOT use the ESP-IDF SPI peripheral because it holds
         * GPIO 41 (SCLK) LOW when idle (SPI mode 0), which causes
         * the ST7701S LCD controller to misinterpret LoRa SPI traffic
         * as display commands (shared physical lines). */
        gpio_set_direction((gpio_num_t)_sck, GPIO_MODE_OUTPUT);
        gpio_set_direction((gpio_num_t)_mosi, GPIO_MODE_OUTPUT);
        gpio_set_direction((gpio_num_t)_miso, GPIO_MODE_INPUT);
        gpio_set_level((gpio_num_t)_sck, 1);   /* idle HIGH = safe for LCD */
        gpio_set_level((gpio_num_t)_mosi, 1);
    }

    void spiBeginTransaction() override
    {
        SemaphoreHandle_t shared = bsp_shared_spi_mutex();
        if (shared) {
            xSemaphoreTake(shared, portMAX_DELAY);
        }
        xSemaphoreTake(_spi_mutex, portMAX_DELAY);
    }

    void spiTransfer(uint8_t *out, size_t len, uint8_t *in) override
    {
        /* Bit-bang SPI mode 0 (CPOL=0, CPHA=0) with CLK idle HIGH.
         * We invert the clock polarity from standard mode 0:
         *   - idle: CLK=HIGH (LCD-safe)
         *   - active: CLK goes LOW then HIGH per bit
         * The SX1262 samples on rising edge regardless of idle state. */
        for (size_t i = 0; i < len; i++) {
            uint8_t tx = out ? out[i] : 0x00;
            uint8_t rx = 0;
            for (int bit = 7; bit >= 0; bit--) {
                /* Set MOSI */
                gpio_set_level((gpio_num_t)_mosi, (tx >> bit) & 1);
                /* Falling edge (CLK HIGH→LOW) */
                gpio_set_level((gpio_num_t)_sck, 0);
                esp_rom_delay_us(1);
                /* Rising edge (CLK LOW→HIGH): SX1262 samples MOSI, we sample MISO */
                gpio_set_level((gpio_num_t)_sck, 1);
                rx |= (gpio_get_level((gpio_num_t)_miso) << bit);
                esp_rom_delay_us(1);
            }
            if (in) in[i] = rx;
        }
    }

    void spiEndTransaction() override
    {
        /* CLK and MOSI stay HIGH (LCD-safe idle state) */
        gpio_set_level((gpio_num_t)_sck, 1);
        gpio_set_level((gpio_num_t)_mosi, 1);
        xSemaphoreGive(_spi_mutex);
        SemaphoreHandle_t shared = bsp_shared_spi_mutex();
        if (shared) {
            xSemaphoreGive(shared);
        }
        /* Every SPI transaction corrupts ST7701S registers (shared GPIO).
         * Signal the debounced resync task — it waits for SPI activity to
         * settle before doing the 151 ms register rewrite. */
        bsp_display_rgb_restart();
    }

    void spiEnd() override
    {
        gpio_set_level((gpio_num_t)_sck, 1);
        gpio_set_level((gpio_num_t)_mosi, 1);
    }

    /* ---- Misc overrides ---------------------------------------------- */

    void yield() override
    {
        taskYIELD();
    }

private:
    int8_t _sck, _miso, _mosi;
    SemaphoreHandle_t _spi_mutex;
    void (*_irq_cb)(void);
    QueueHandle_t _irq_gpio_queue;

    static void IRAM_ATTR _gpio_isr(void *arg)
    {
        auto *self = static_cast<IndicatorHal *>(arg);
        uint32_t dummy = 1;
        xQueueSendFromISR(self->_irq_gpio_queue, &dummy, nullptr);
    }

    static void _irq_task_trampoline(void *arg)
    {
        auto *self = static_cast<IndicatorHal *>(arg);
        uint32_t dummy;
        for (;;) {
            if (xQueueReceive(self->_irq_gpio_queue, &dummy, portMAX_DELAY)) {
                /* Verify DIO1 is actually high (filter spurious IO exp interrupts) */
                uint32_t dio1 = 0;
                bsp_io_expander_get_level_safe(BIT(BSP_IO_EXP_LORA_DIO1), &dio1);
                if (dio1 && self->_irq_cb) {
                    self->_irq_cb();
                }
            }
        }
    }
};
