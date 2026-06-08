/*
 * LVGL configuration for PC simulator (SDL2 backend).
 * Mirrors the ESP-IDF sdkconfig settings for SenseCAP Indicator.
 * SPDX-License-Identifier: MIT
 */
#ifndef LV_CONF_H
#define LV_CONF_H

/* clang-format off */

/*====================
   COLOR SETTINGS
 *====================*/
#define LV_COLOR_DEPTH          16

/*====================
   MEMORY SETTINGS
 *====================*/
#define LV_MEM_SIZE             (2 * 1024 * 1024)

/*====================
   DISPLAY SETTINGS
 *====================*/
#define LV_DPI_DEF              130

/*====================
   DRAW SETTINGS
 *====================*/
#define LV_USE_DRAW_SW          1

/*====================
   SDL DRIVER
 *====================*/
#define LV_USE_SDL              1
#define LV_SDL_INCLUDE_PATH     <SDL2/SDL.h>
#define LV_SDL_RENDER_MODE      LV_DISPLAY_RENDER_MODE_DIRECT
#define LV_SDL_BUF_COUNT        1
#define LV_SDL_ACCELERATED      1
#define LV_SDL_FULLSCREEN       0
#define LV_SDL_DIRECT_EXIT      1
#define LV_SDL_MOUSEWHEEL_MODE  LV_SDL_MOUSEWHEEL_MODE_ENCODER

/*====================
   FONT SETTINGS
 *====================*/
#define LV_FONT_MONTSERRAT_14   1
#define LV_FONT_MONTSERRAT_20   1
#define LV_FONT_MONTSERRAT_32   1
#define LV_FONT_DEFAULT         &lv_font_montserrat_20

/*====================
   WIDGETS
 *====================*/
#define LV_USE_LABEL            1
#define LV_USE_BTN              1
#define LV_USE_SLIDER           1
#define LV_USE_ARC              1
#define LV_USE_SWITCH           1
#define LV_USE_TEXTAREA         1
#define LV_USE_TABLE            1
#define LV_USE_CHECKBOX         1
#define LV_USE_DROPDOWN         1
#define LV_USE_ROLLER           1
#define LV_USE_BAR              1
#define LV_USE_LINE             1
#define LV_USE_IMAGE            1
#define LV_USE_CANVAS           1
#define LV_USE_ANIMIMG          0
#define LV_USE_TILEVIEW         1
#define LV_USE_TABVIEW          1
#define LV_USE_SPAN             1
#define LV_USE_CALENDAR         0
#define LV_USE_CHART            0
#define LV_USE_COLORWHEEL       0
#define LV_USE_KEYBOARD         1
#define LV_USE_LED              0
#define LV_USE_LIST             1
#define LV_USE_MENU             1
#define LV_USE_MSGBOX           1
#define LV_USE_SPINBOX          0
#define LV_USE_SPINNER          1
#define LV_USE_WIN              0
#define LV_USE_SCALE            0

/*====================
   STDLIB
 *====================*/
#define LV_USE_STDLIB_MALLOC    LV_STDLIB_BUILTIN
#define LV_USE_STDLIB_STRING    LV_STDLIB_BUILTIN
#define LV_USE_STDLIB_SPRINTF   LV_STDLIB_BUILTIN

/*====================
   OS ABSTRACTION
 *====================*/
#define LV_USE_OS               LV_OS_NONE

/*====================
   LOGGING
 *====================*/
#define LV_USE_LOG              1
#define LV_LOG_LEVEL            LV_LOG_LEVEL_WARN
#define LV_LOG_PRINTF           1

/*====================
   OTHERS
 *====================*/
#define LV_USE_ASSERT_NULL          1
#define LV_USE_ASSERT_MALLOC        1
#define LV_USE_ASSERT_OBJ           0
#define LV_USE_OBSERVER             1
#define LV_USE_SYSMON               0
#define LV_USE_PROFILER             0
#define LV_USE_OBJ_ID              0
#define LV_USE_FLOAT               0
#define LV_USE_QRCODE              1
#define LV_USE_SNAPSHOT            1

/* Disable unused drivers */
#define LV_USE_LINUX_FBDEV      0
#define LV_USE_LINUX_DRM        0
#define LV_USE_NUTTX            0
#define LV_USE_WINDOWS          0
#define LV_USE_OPENGLES         0

/* clang-format on */

#endif /* LV_CONF_H */
