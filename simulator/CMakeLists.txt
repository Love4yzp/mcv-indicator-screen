cmake_minimum_required(VERSION 3.16)
project(sim C CXX ASM)

set(CMAKE_C_STANDARD 99)
set(CMAKE_CXX_STANDARD 17)

# --- SDL2 ---
list(APPEND CMAKE_PREFIX_PATH "/opt/homebrew")
find_package(SDL2 REQUIRED)

# --- LVGL (as subdirectory, uses env_support/cmake/os_desktop.cmake) ---
set(CONFIG_LV_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(CONFIG_LV_BUILD_DEMOS OFF CACHE BOOL "" FORCE)
set(LVGL_ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../managed_components/lvgl__lvgl")
add_subdirectory("${LVGL_ROOT_DIR}" lvgl)

# Let LVGL find our lv_conf.h
target_include_directories(lvgl PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_include_directories(lvgl PUBLIC "${SDL2_INCLUDE_DIRS}")
target_compile_definitions(lvgl PUBLIC LV_CONF_INCLUDE_SIMPLE)

# --- App sources (pages + app_manager) ---
set(APP_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../main/app")

set(APP_SOURCES
    "${APP_DIR}/app_manager.c"
    "${APP_DIR}/app_inspect.c"
    "${APP_DIR}/pages/page_sensor.c"
    "${APP_DIR}/pages/page_control.c"
    "${APP_DIR}/pages/page_hello.c"
    "${APP_DIR}/pages/page_settings.c"
    "${APP_DIR}/pages/page_lora.c"
    "${APP_DIR}/fonts/app_fonts.c"
    "${APP_DIR}/fonts/font_unit_14.c"
    "${APP_DIR}/fonts/font_unit_32.c"
    "${APP_DIR}/fonts/font_cn_14.c"
    "${APP_DIR}/fonts/font_cn_20.c"
)

# --- Simulator executable ---
add_executable(sim
    main.c
    sim_platform.c
    ${APP_SOURCES}
)

target_include_directories(sim PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"          # simulator's lv_port.h
    "${CMAKE_CURRENT_SOURCE_DIR}/shims"    # ESP-IDF stubs (esp_err.h, freertos/)
    "${CMAKE_CURRENT_SOURCE_DIR}/../main"  # app/ headers via "app/..."
    "${APP_DIR}/services"                  # svc_lorawan.h for page_lora
)

target_compile_definitions(sim PRIVATE CONFIG_INDICATOR_LORAWAN_ENABLED=1)

target_link_libraries(sim PRIVATE lvgl ${SDL2_LIBRARIES} m pthread)
