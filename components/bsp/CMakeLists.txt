# SPDX-License-Identifier: MIT

idf_component_register(
    SRC_DIRS "src"
    INCLUDE_DIRS "include"
    REQUIRES
        driver
        esp_hw_support
        esp_timer
        freertos
        log
        nvs_flash
        esp_io_expander_tca95xx_16bit
        button
        esp_lcd
        esp_lcd_touch
        esp_lcd_touch_ft5x06
)
