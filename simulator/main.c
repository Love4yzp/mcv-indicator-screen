/*
 * LVGL PC simulator entry point for SenseCAP Indicator UI.
 *
 * Usage:
 *   ./sim                                    # interactive mode
 *   ./sim --screenshot output.bmp            # capture page 0
 *   ./sim --screenshot output.png --page 1   # capture page 1 as PNG
 *
 * SPDX-License-Identifier: MIT
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <SDL2/SDL.h>
#include "app/app_manager.h"
#include "app/app_inspect.h"
#include "lvgl.h"

/* Stub platform callbacks — no real hardware on desktop */
static void stub_brightness(int pct) { printf("[sim] brightness=%d%%\n", pct); }
static void stub_beep(void) { printf("[sim] beep on\n"); }
static void stub_beep_off(void) { printf("[sim] beep off\n"); }
static void stub_wifi_prov(void) { printf("[sim] wifi provisioning requested\n"); }
static void stub_save_pm25(int value) { printf("[sim] save pm25 threshold=%d\n", value); }
static void stub_lora_uplink(void) { printf("[sim] LoRa uplink requested\n"); }
static void stub_lora_configure(bool auto_mode, uint32_t interval_s) {
    printf("[sim] LoRa configure: auto=%d interval=%us\n", auto_mode, interval_s);
}

#define SIM_WIDTH  480
#define SIM_HEIGHT 480

static int save_screenshot(const char *path)
{
    /* Use LVGL snapshot to capture the active screen — works regardless of SDL buffer state */
    lv_obj_t *screen = lv_screen_active();
    lv_draw_buf_t *buf = lv_snapshot_take(screen, LV_COLOR_FORMAT_RGB888);
    if (buf == NULL) {
        fprintf(stderr, "error: lv_snapshot_take returned NULL\n");
        return -1;
    }

    uint32_t w = buf->header.w;
    uint32_t h = buf->header.h;
    uint8_t *pixels = buf->data;
    uint32_t stride = buf->header.stride;

    /* Write BMP */
    char bmp_path[1024];
    size_t len = strlen(path);
    int want_png = (len >= 4 && strcasecmp(path + len - 4, ".png") == 0);
    if (want_png) {
        snprintf(bmp_path, sizeof(bmp_path), "%.*s.bmp", (int)(len - 4), path);
    } else {
        snprintf(bmp_path, sizeof(bmp_path), "%s", path);
    }

    /* Manual BMP write (bottom-up, BGR) */
    FILE *fp = fopen(bmp_path, "wb");
    if (!fp) {
        fprintf(stderr, "error: can't open %s\n", bmp_path);
        lv_draw_buf_destroy(buf);
        return -1;
    }

    uint32_t row_size = (w * 3 + 3) & ~3u;
    uint32_t img_size = row_size * h;
    uint32_t file_size = 54 + img_size;

    /* BMP header */
    uint8_t hdr[54] = {0};
    hdr[0] = 'B'; hdr[1] = 'M';
    memcpy(hdr + 2, &file_size, 4);
    uint32_t offset = 54; memcpy(hdr + 10, &offset, 4);
    uint32_t dib = 40; memcpy(hdr + 14, &dib, 4);
    memcpy(hdr + 18, &w, 4);
    memcpy(hdr + 22, &h, 4);
    uint16_t planes = 1; memcpy(hdr + 26, &planes, 2);
    uint16_t bpp = 24; memcpy(hdr + 28, &bpp, 2);
    memcpy(hdr + 34, &img_size, 4);

    fwrite(hdr, 1, 54, fp);

    /* Write rows bottom-up (LVGL RGB888 is stored as B,G,R in memory = BMP native order) */
    uint8_t pad[3] = {0};
    uint32_t pad_bytes = row_size - w * 3;
    for (int y = (int)h - 1; y >= 0; y--) {
        uint8_t *row = pixels + y * stride;
        fwrite(row, 1, w * 3, fp);
        if (pad_bytes) fwrite(pad, 1, pad_bytes, fp);
    }
    fclose(fp);
    lv_draw_buf_destroy(buf);

    if (want_png) {
        char cmd[2048];
        snprintf(cmd, sizeof(cmd),
                 "sips -s format png '%s' --out '%s' >/dev/null 2>&1 && rm -f '%s'",
                 bmp_path, path, bmp_path);
        if (system(cmd) != 0) {
            fprintf(stderr, "warning: PNG conversion failed, BMP at %s\n", bmp_path);
        }
    }

    printf("[sim] screenshot saved: %s\n", path);
    return 0;
}

static void render_frames(int count)
{
    for (int i = 0; i < count; i++) {
        lv_timer_handler();
        SDL_Delay(30);
    }
}

static int save_tree(const char *path)
{
    FILE *fp = fopen(path, "w");
    if (!fp) {
        fprintf(stderr, "error: can't open %s\n", path);
        return -1;
    }
    app_manager_inspect_json(fp);
    fclose(fp);
    printf("[sim] widget tree saved: %s\n", path);
    return 0;
}

static lv_display_t *sim_init(void)
{
    lv_init();

    lv_display_t *disp = lv_sdl_window_create(SIM_WIDTH, SIM_HEIGHT);
    lv_display_set_default(disp);

    lv_indev_t *mouse = lv_sdl_mouse_create();
    lv_indev_set_display(mouse, disp);

    platform_callbacks_t callbacks = {
        .set_brightness = stub_brightness,
        .send_beep = stub_beep,
        .stop_beep = stub_beep_off,
        .start_wifi_prov = stub_wifi_prov,
        .save_pm25_threshold = stub_save_pm25,
        .request_lora_uplink = stub_lora_uplink,
        .configure_lora_uplink = stub_lora_configure,
    };
    app_manager_init(&callbacks);

    /* Seed plausible sensor values so hero/header widgets are not blank. */
    app_manager_update_sensor(0xC0, 12.0f); /* PM1.0 */
    app_manager_update_sensor(0xC1, 18.0f); /* PM2.5 */
    app_manager_update_sensor(0xC2, 24.0f); /* PM4.0 */
    app_manager_update_sensor(0xC3, 32.0f); /* PM10 */
    app_manager_update_sensor(0xC4, 24.5f); /* temperature °C */
    app_manager_update_sensor(0xC5, 52.0f); /* humidity %RH */
    app_manager_update_sensor(0xC6, 80.0f); /* VOC index */
    app_manager_flush_sensor();

    return disp;
}

int main(int argc, char *argv[])
{
    const char *screenshot_path = NULL;
    const char *screenshot_dir = NULL;
    const char *tree_path = NULL;
    const char *inspect_dir = NULL;
    int page = -1;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--screenshot") == 0 && i + 1 < argc) {
            screenshot_path = argv[++i];
        } else if (strcmp(argv[i], "--screenshot-all") == 0 && i + 1 < argc) {
            screenshot_dir = argv[++i];
        } else if (strcmp(argv[i], "--dump-tree") == 0 && i + 1 < argc) {
            tree_path = argv[++i];
        } else if (strcmp(argv[i], "--inspect-all") == 0 && i + 1 < argc) {
            inspect_dir = argv[++i];
        } else if (strcmp(argv[i], "--page") == 0 && i + 1 < argc) {
            page = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            printf("Usage: %s [OPTIONS]\n", argv[0]);
            printf("  (no args)                    Interactive mode\n");
            printf("  --screenshot <path>          Capture one page (.png or .bmp)\n");
            printf("  --screenshot-all <dir>       Capture all pages as PNG\n");
            printf("  --dump-tree <path>           Dump active page widget tree as JSON\n");
            printf("  --inspect-all <dir>          Capture PNG + JSON for every page\n");
            printf("  --page <n>                   Page index (default: 0)\n");
            return 0;
        }
    }

    lv_display_t *disp = sim_init();
    (void)disp;

    if (inspect_dir != NULL) {
        uint32_t count = app_manager_get_page_count();
        int rc = 0;
        for (uint32_t p = 0; p < count; p++) {
            app_manager_set_page(p);
            for (int f = 0; f < 20; f++) { lv_timer_handler(); SDL_Delay(16); }
            char png[1024], json[1024];
            snprintf(png, sizeof(png), "%s/page_%u.png", inspect_dir, p);
            snprintf(json, sizeof(json), "%s/page_%u.json", inspect_dir, p);
            if (save_screenshot(png) != 0) rc = 1;
            if (save_tree(json) != 0) rc = 1;
        }
        lv_deinit();
        return rc;
    }

    if (screenshot_dir != NULL) {
        /* Capture all pages */
        uint32_t count = app_manager_get_page_count();
        int rc = 0;
        for (uint32_t p = 0; p < count; p++) {
            app_manager_set_page(p);
            /* Let LVGL process the scroll + re-render */
            for (int f = 0; f < 20; f++) {
                lv_timer_handler();
                SDL_Delay(16);
            }
            char path[1024];
            snprintf(path, sizeof(path), "%s/page_%u.png", screenshot_dir, p);
            if (save_screenshot(path) != 0) rc = 1;
        }
        lv_deinit();
        return rc;
    }

    if (screenshot_path != NULL || tree_path != NULL) {
        if (page < 0) page = 0;
        if (!app_manager_set_page((uint32_t)page)) {
            fprintf(stderr, "error: page %d out of range (0-%u)\n",
                    page, app_manager_get_page_count() - 1);
            lv_deinit();
            return 1;
        }
        lv_refr_now(NULL);
        render_frames(15);
        int rc = 0;
        if (screenshot_path) rc |= save_screenshot(screenshot_path);
        if (tree_path) rc |= save_tree(tree_path);
        lv_deinit();
        return rc;
    }

    /* Interactive mode */
    if (page >= 0) app_manager_set_page((uint32_t)page);
    printf("[sim] SenseCAP Indicator simulator running — close window to exit\n");
    printf("[sim] %u pages registered\n", app_manager_get_page_count());

    while (1) {
        uint32_t time_until_next = lv_timer_handler();
        lv_delay_ms(time_until_next);
    }

    return 0;
}
