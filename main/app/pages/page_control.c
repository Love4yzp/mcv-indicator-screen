/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Spencer (love4yzp)
 *
 * P2 Control Page — 2×2 card grid + hero header + overlay panels.
 * Cards: 灯光, 空调, 遮阳帘, 模式
 * Hero: 当前模式 chip · 室温
 * Card power dot toggles on/off without opening overlay; tap card body opens overlay.
 */

#include "app/pages/page_control.h"

#include <stdio.h>

#include "app/fonts/app_fonts.h"
#include "app/app_manager.h"
#include "lvgl.h"

/* ═══════ Colors ═══════ */
#define C_BG          lv_color_hex(0x050816)
#define C_CARD        lv_color_hex(0x10192A)
#define C_CARD_ON     lv_color_hex(0x122A4A)
#define C_CARD_BRD    lv_color_hex(0x1A2744)
#define C_TEXT        lv_color_hex(0xF5F7FA)
#define C_DIM         lv_color_hex(0x93A1B5)
#define C_FAINT       lv_color_hex(0x5A6B80)
#define C_ACCENT      lv_color_hex(0x4CC9F0)
#define C_ACCENT_DIM  lv_color_hex(0x1F4A66)
#define C_GOOD        lv_color_hex(0x5BD5A8)
#define C_WARM        lv_color_hex(0xF8B36C)
#define C_COOL        lv_color_hex(0x6FCFE8)
#define C_TRACK       lv_color_hex(0x223049)

/* ═══════ UTF-8 Chinese strings ═══════ */
#define S_LIGHT    "\xE7\x81\xAF\xE5\x85\x89"           /* 灯光 */
#define S_AC       "\xE7\xA9\xBA\xE8\xB0\x83"           /* 空调 */
#define S_BLINDS   "\xE9\x81\xAE\xE9\x98\xB3\xE5\xB8\x98" /* 遮阳帘 */
#define S_MODE     "\xE6\xA8\xA1\xE5\xBC\x8F"           /* 模式 */
#define S_CONTROL  "\xE6\x8E\xA7\xE5\x88\xB6"           /* 控制 */
#define S_BRIGHT   "\xE4\xBA\xAE\xE5\xBA\xA6"           /* 亮度 */
#define S_OPENED   "\xE5\xB7\xB2\xE6\x89\x93\xE5\xBC\x80" /* 已打开 */
#define S_CLOSED   "\xE5\xB7\xB2\xE5\x85\xB3\xE9\x97\xAD" /* 已关闭 */
#define S_OPEN     "\xE6\x89\x93\xE5\xBC\x80"           /* 打开 */
#define S_CLOSE    "\xE5\x85\xB3\xE9\x97\xAD"           /* 关闭 */
#define S_OFF      "\xE5\xBE\x85\xE6\x9C\xBA"           /* 待机 */
#define S_ROOM     "\xE5\xAE\xA4\xE6\xB8\xA9"           /* 室温 */

/* AC mode/fan */
#define S_COOL_M   "\xE5\x88\xB6\xE5\x86\xB7"           /* 制冷 */
#define S_HEAT_M   "\xE5\x88\xB6\xE7\x83\xAD"           /* 制热 */
#define S_AUTO     "\xE8\x87\xAA\xE5\x8A\xA8"           /* 自动 */
#define S_FAN      "\xE9\xA3\x8E\xE9\x80\x9F"           /* 风速 */
#define S_LOW      "\xE4\xBD\x8E"                       /* 低 */
#define S_MID      "\xE4\xB8\xAD"                       /* 中 */
#define S_HIGH     "\xE9\xAB\x98"                       /* 高 */
#define S_TARGET   "\xE7\x9B\xAE\xE6\xA0\x87"           /* 目标 */

/* Mode names & descriptions */
#define S_M_SHOW   "\xE5\xB1\x95\xE7\xA4\xBA"           /* 展示 */
#define S_M_DAILY  "\xE6\x97\xA5\xE5\xB8\xB8"           /* 日常 */
#define S_M_ECO    "\xE8\x8A\x82\xE8\x83\xBD"           /* 节能 */
#define S_M_AWAY   "\xE7\xA6\xBB\xE8\xBD\xA6"           /* 离车 */
#define S_D_SHOW   "\xE5\x85\xA8\xE4\xBA\xAE \xC2\xB7 \xE6\xBC\x94\xE7\xA4\xBA" /* 全亮 · 演示 */
#define S_D_DAILY  "\xE6\xAD\xA3\xE5\xB8\xB8 \xC2\xB7 \xE8\x88\x92\xE9\x80\x82" /* 正常 · 舒适 */
#define S_D_ECO    "\xE6\x9A\x97\xE7\x81\xAF \xC2\xB7 \xE4\xBD\x8E\xE8\x80\x97" /* 暗灯 · 低耗 */
#define S_D_AWAY   "\xE5\x85\xA8\xE5\x85\xB3 \xC2\xB7 \xE7\x9B\x91\xE6\x8E\xA7" /* 全关 · 监控 */

/* ═══════ Control state ═══════ */
typedef struct {
    bool  light_on;
    int   light_brightness; /* 0-100 */
    bool  ac_on;
    float ac_temp;          /* 16.0-30.0 */
    int   ac_mode;          /* 0=cool, 1=heat, 2=auto */
    int   ac_fan;           /* 0=low, 1=mid, 2=high, 3=auto */
    bool  blinds_open;
    int   mode;             /* 0=展示, 1=日常, 2=节能, 3=离车 */
} ctrl_state_t;

static ctrl_state_t s_ctrl = {
    .light_on = false,
    .light_brightness = 80,
    .ac_on = true,
    .ac_temp = 23.0f,
    .ac_mode = 0, /* cool */
    .ac_fan = 3,
    .blinds_open = false,
    .mode = 1, /* 默认日常 */
};

/* ═══════ Static refs ═══════ */
static lv_obj_t *s_cards[4];
static lv_obj_t *s_card_icon[4];
static lv_obj_t *s_card_value[4];   /* 主值 */
static lv_obj_t *s_card_sub[4];     /* 副状态 */
static lv_obj_t *s_card_bar[4];     /* 进度条（仅灯光用） */
static lv_obj_t *s_card_dot[4];     /* power dot button (mode 卡片为 NULL) */
static lv_obj_t *s_card_dot_icon[4];

static lv_obj_t *s_hero_mode;       /* hero chip: 当前模式 */
static lv_obj_t *s_hero_temp;       /* hero: 室温 */

static lv_obj_t *s_overlay;
static lv_obj_t *s_ac_temp_label;
static lv_obj_t *s_mode_items[4];

static const char *MODE_NAMES[] = {S_M_SHOW, S_M_DAILY, S_M_ECO, S_M_AWAY};
static const char *MODE_DESCS[] = {S_D_SHOW, S_D_DAILY, S_D_ECO, S_D_AWAY};
static const char *AC_MODE_CN[] = {S_COOL_M, S_HEAT_M, S_AUTO};

static const char *CARD_NAMES[] = {S_LIGHT, S_AC, S_BLINDS, S_MODE};
static const char *CARD_ICONS[] = {
    LV_SYMBOL_EYE_OPEN,    /* 灯光 → 眼睛(亮度) */
    LV_SYMBOL_REFRESH,     /* 空调 → 循环(气流) */
    LV_SYMBOL_DOWN,        /* 遮阳帘 → 上/下 (动态切换) */
    LV_SYMBOL_HOME,        /* 模式 → 家(场景) */
};

/* ═══════ fwd decl ═══════ */
static void update_card_status(void);
static void refresh_hero(void);

/* ═══════ Overlay infrastructure ═══════ */

static void close_overlay(void)
{
    if (s_overlay != NULL) {
        lv_obj_delete(s_overlay);
        s_overlay = NULL;
    }
}

static void overlay_bg_click_cb(lv_event_t *e)
{
    if (lv_event_get_target(e) == lv_event_get_current_target(e)) {
        close_overlay();
    }
}

static void overlay_close_btn_cb(lv_event_t *e)
{
    (void)e;
    close_overlay();
}

static lv_obj_t *create_overlay_panel(const char *title)
{
    close_overlay();

    s_overlay = lv_obj_create(lv_layer_top());
    lv_obj_remove_style_all(s_overlay);
    lv_obj_set_size(s_overlay, 480, 480);
    lv_obj_set_style_bg_color(s_overlay, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(s_overlay, LV_OPA_70, 0);
    lv_obj_add_event_cb(s_overlay, overlay_bg_click_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_clear_flag(s_overlay, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(s_overlay, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t *panel = lv_obj_create(s_overlay);
    lv_obj_set_size(panel, 410, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_color(panel, lv_color_hex(0x0C1525), 0);
    lv_obj_set_style_bg_opa(panel, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(panel, 1, 0);
    lv_obj_set_style_border_color(panel, C_CARD_BRD, 0);
    lv_obj_set_style_radius(panel, 24, 0);
    lv_obj_set_style_pad_all(panel, 24, 0);
    lv_obj_set_style_pad_row(panel, 14, 0);
    lv_obj_set_style_shadow_width(panel, 30, 0);
    lv_obj_set_style_shadow_color(panel, lv_color_black(), 0);
    lv_obj_set_style_shadow_opa(panel, LV_OPA_50, 0);
    lv_obj_set_layout(panel, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(panel, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(panel, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER);
    lv_obj_center(panel);
    lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);

    /* Header row: title + close button */
    lv_obj_t *header = lv_obj_create(panel);
    lv_obj_remove_style_all(header);
    lv_obj_set_size(header, LV_PCT(100), 36);
    lv_obj_set_layout(header, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(header, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(header, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *lbl = lv_label_create(header);
    lv_label_set_text(lbl, title);
    lv_obj_set_style_text_color(lbl, C_TEXT, 0);
    lv_obj_set_style_text_font(lbl, &app_font_cn_20, 0);

    lv_obj_t *cbtn = lv_button_create(header);
    lv_obj_set_size(cbtn, 36, 36);
    lv_obj_set_style_bg_color(cbtn, lv_color_hex(0x1A2744), 0);
    lv_obj_set_style_bg_color(cbtn, lv_color_hex(0x2A3B55), LV_STATE_PRESSED);
    lv_obj_set_style_border_width(cbtn, 0, 0);
    lv_obj_set_style_radius(cbtn, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_shadow_opa(cbtn, LV_OPA_TRANSP, 0);
    lv_obj_add_event_cb(cbtn, overlay_close_btn_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *clbl = lv_label_create(cbtn);
    lv_label_set_text(clbl, LV_SYMBOL_CLOSE);
    lv_obj_set_style_text_color(clbl, C_DIM, 0);
    lv_obj_set_style_text_font(clbl, &app_font_14, 0);
    lv_obj_center(clbl);

    /* Separator */
    lv_obj_t *sep = lv_obj_create(panel);
    lv_obj_remove_style_all(sep);
    lv_obj_set_size(sep, LV_PCT(100), 1);
    lv_obj_set_style_bg_color(sep, C_CARD_BRD, 0);
    lv_obj_set_style_bg_opa(sep, LV_OPA_COVER, 0);

    return panel;
}

/* ═══════ Shared UI helpers ═══════ */

static lv_obj_t *create_toggle_row(lv_obj_t *parent, const char *label_text, bool initial,
                                    lv_event_cb_t cb, void *user_data)
{
    lv_obj_t *row = lv_obj_create(parent);
    lv_obj_remove_style_all(row);
    lv_obj_set_size(row, LV_PCT(100), 44);
    lv_obj_set_layout(row, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *lbl = lv_label_create(row);
    lv_label_set_text(lbl, label_text);
    lv_obj_set_style_text_color(lbl, C_TEXT, 0);
    lv_obj_set_style_text_font(lbl, &app_font_cn_14, 0);

    lv_obj_t *sw = lv_switch_create(row);
    lv_obj_set_size(sw, 60, 32);
    if (initial) lv_obj_add_state(sw, LV_STATE_CHECKED);
    lv_obj_set_style_bg_color(sw, lv_color_hex(0x2A3441), LV_PART_MAIN);
    lv_obj_set_style_bg_color(sw, C_ACCENT, LV_PART_INDICATOR | LV_STATE_CHECKED);
    lv_obj_set_style_radius(sw, 16, LV_PART_MAIN);
    lv_obj_set_style_radius(sw, 16, LV_PART_INDICATOR);
    lv_obj_set_style_radius(sw, LV_RADIUS_CIRCLE, LV_PART_KNOB);
    lv_obj_add_event_cb(sw, cb, LV_EVENT_VALUE_CHANGED, user_data);
    return sw;
}

/* ═══════ Light overlay ═══════ */

static void light_toggle_cb(lv_event_t *e)
{
    s_ctrl.light_on = lv_obj_has_state(lv_event_get_target(e), LV_STATE_CHECKED);
    update_card_status();
}

static void light_slider_cb(lv_event_t *e)
{
    s_ctrl.light_brightness = (int)lv_slider_get_value(lv_event_get_target(e));
    update_card_status();
}

static void open_light_overlay(void)
{
    lv_obj_t *panel = create_overlay_panel(S_LIGHT);

    create_toggle_row(panel, S_OPEN, s_ctrl.light_on, light_toggle_cb, NULL);

    lv_obj_t *blbl = lv_label_create(panel);
    lv_label_set_text(blbl, S_BRIGHT);
    lv_obj_set_style_text_color(blbl, C_DIM, 0);
    lv_obj_set_style_text_font(blbl, &app_font_cn_14, 0);

    lv_obj_t *slider = lv_slider_create(panel);
    lv_obj_set_width(slider, LV_PCT(100));
    lv_obj_set_height(slider, 10);
    lv_slider_set_range(slider, 0, 100);
    lv_slider_set_value(slider, s_ctrl.light_brightness, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(slider, C_TRACK, LV_PART_MAIN);
    lv_obj_set_style_bg_color(slider, C_ACCENT, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(slider, C_TEXT, LV_PART_KNOB);
    lv_obj_set_style_radius(slider, 5, LV_PART_MAIN);
    lv_obj_set_style_radius(slider, 5, LV_PART_INDICATOR);
    lv_obj_set_style_pad_all(slider, 8, LV_PART_KNOB);
    lv_obj_add_event_cb(slider, light_slider_cb, LV_EVENT_VALUE_CHANGED, NULL);
}

/* ═══════ AC overlay ═══════ */

static void ac_toggle_cb(lv_event_t *e)
{
    s_ctrl.ac_on = lv_obj_has_state(lv_event_get_target(e), LV_STATE_CHECKED);
    update_card_status();
}

static void ac_temp_btn_cb(lv_event_t *e)
{
    int delta = (int)(intptr_t)lv_event_get_user_data(e);
    s_ctrl.ac_temp += delta * 0.5f;
    if (s_ctrl.ac_temp < 16.0f) s_ctrl.ac_temp = 16.0f;
    if (s_ctrl.ac_temp > 30.0f) s_ctrl.ac_temp = 30.0f;

    static char buf[16];
    snprintf(buf, sizeof(buf), "%.1f\xC2\xB0" "C", s_ctrl.ac_temp);
    lv_label_set_text(s_ac_temp_label, buf);
    update_card_status();
}

static void ac_mode_cb(lv_event_t *e)
{
    uint32_t id = lv_buttonmatrix_get_selected_button(lv_event_get_target(e));
    if (id != LV_BUTTONMATRIX_BUTTON_NONE) {
        s_ctrl.ac_mode = (int)id;
        update_card_status();
    }
}

static void ac_fan_cb(lv_event_t *e)
{
    uint32_t id = lv_buttonmatrix_get_selected_button(lv_event_get_target(e));
    if (id != LV_BUTTONMATRIX_BUTTON_NONE) s_ctrl.ac_fan = (int)id;
}

static lv_obj_t *create_round_btn(lv_obj_t *parent, const char *text, lv_event_cb_t cb, void *ud)
{
    lv_obj_t *btn = lv_button_create(parent);
    lv_obj_set_size(btn, 56, 56);
    lv_obj_set_style_bg_color(btn, lv_color_hex(0x1A2744), 0);
    lv_obj_set_style_bg_color(btn, C_ACCENT_DIM, LV_STATE_PRESSED);
    lv_obj_set_style_radius(btn, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(btn, 1, 0);
    lv_obj_set_style_border_color(btn, C_CARD_BRD, 0);
    lv_obj_set_style_shadow_opa(btn, LV_OPA_TRANSP, 0);
    lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, ud);
    lv_obj_t *lbl = lv_label_create(btn);
    lv_label_set_text(lbl, text);
    lv_obj_set_style_text_color(lbl, C_TEXT, 0);
    lv_obj_set_style_text_font(lbl, &app_font_20, 0);
    lv_obj_center(lbl);
    return btn;
}

static lv_obj_t *create_btnmatrix_cn(lv_obj_t *parent, const char **map, int count,
                                      int checked, lv_event_cb_t cb)
{
    lv_obj_t *bm = lv_buttonmatrix_create(parent);
    lv_buttonmatrix_set_map(bm, map);
    lv_obj_set_size(bm, LV_PCT(100), 42);
    lv_buttonmatrix_set_one_checked(bm, true);
    if (checked >= 0 && checked < count) {
        lv_buttonmatrix_set_button_ctrl(bm, (uint16_t)checked, LV_BUTTONMATRIX_CTRL_CHECKED);
    }
    lv_obj_set_style_bg_opa(bm, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(bm, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_gap(bm, 8, LV_PART_MAIN);
    lv_obj_set_style_bg_color(bm, lv_color_hex(0x1A2744), LV_PART_ITEMS);
    lv_obj_set_style_bg_color(bm, C_ACCENT, LV_PART_ITEMS | LV_STATE_CHECKED);
    lv_obj_set_style_text_color(bm, C_DIM, LV_PART_ITEMS);
    lv_obj_set_style_text_color(bm, C_TEXT, LV_PART_ITEMS | LV_STATE_CHECKED);
    lv_obj_set_style_text_font(bm, &app_font_cn_14, LV_PART_ITEMS);
    lv_obj_set_style_radius(bm, 10, LV_PART_ITEMS);
    lv_obj_set_style_border_width(bm, 0, LV_PART_ITEMS);
    lv_obj_add_event_cb(bm, cb, LV_EVENT_VALUE_CHANGED, NULL);
    return bm;
}

static void open_ac_overlay(void)
{
    lv_obj_t *panel = create_overlay_panel(S_AC);

    create_toggle_row(panel, S_OPEN, s_ctrl.ac_on, ac_toggle_cb, NULL);

    /* Temperature row */
    lv_obj_t *temp_row = lv_obj_create(panel);
    lv_obj_remove_style_all(temp_row);
    lv_obj_set_size(temp_row, LV_PCT(100), 70);
    lv_obj_set_layout(temp_row, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(temp_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(temp_row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(temp_row, 28, 0);
    lv_obj_clear_flag(temp_row, LV_OBJ_FLAG_SCROLLABLE);

    create_round_btn(temp_row, LV_SYMBOL_MINUS, ac_temp_btn_cb, (void *)(intptr_t)-1);

    s_ac_temp_label = lv_label_create(temp_row);
    static char tbuf[16];
    snprintf(tbuf, sizeof(tbuf), "%.1f\xC2\xB0" "C", s_ctrl.ac_temp);
    lv_label_set_text(s_ac_temp_label, tbuf);
    lv_obj_set_style_text_color(s_ac_temp_label, C_TEXT, 0);
    lv_obj_set_style_text_font(s_ac_temp_label, &app_font_32, 0);

    create_round_btn(temp_row, LV_SYMBOL_PLUS, ac_temp_btn_cb, (void *)(intptr_t)1);

    /* Mode */
    lv_obj_t *mlbl = lv_label_create(panel);
    lv_label_set_text(mlbl, S_MODE);
    lv_obj_set_style_text_color(mlbl, C_DIM, 0);
    lv_obj_set_style_text_font(mlbl, &app_font_cn_14, 0);

    static const char *mode_map[] = {S_COOL_M, S_HEAT_M, S_AUTO, ""};
    create_btnmatrix_cn(panel, mode_map, 3, s_ctrl.ac_mode, ac_mode_cb);

    /* Fan */
    lv_obj_t *flbl = lv_label_create(panel);
    lv_label_set_text(flbl, S_FAN);
    lv_obj_set_style_text_color(flbl, C_DIM, 0);
    lv_obj_set_style_text_font(flbl, &app_font_cn_14, 0);

    static const char *fan_map[] = {S_LOW, S_MID, S_HIGH, S_AUTO, ""};
    create_btnmatrix_cn(panel, fan_map, 4, s_ctrl.ac_fan, ac_fan_cb);
}

/* ═══════ Blinds overlay ═══════ */

static void blinds_toggle_cb(lv_event_t *e)
{
    s_ctrl.blinds_open = lv_obj_has_state(lv_event_get_target(e), LV_STATE_CHECKED);
    update_card_status();
}

static void open_blinds_overlay(void)
{
    lv_obj_t *panel = create_overlay_panel(S_BLINDS);
    create_toggle_row(panel, S_OPEN, s_ctrl.blinds_open, blinds_toggle_cb, NULL);

    /* helper hint */
    lv_obj_t *hint = lv_label_create(panel);
    lv_label_set_text(hint, S_BLINDS);
    lv_obj_set_style_text_color(hint, C_FAINT, 0);
    lv_obj_set_style_text_font(hint, &app_font_cn_14, 0);
    lv_label_set_long_mode(hint, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(hint, LV_PCT(100));
}

/* ═══════ Mode overlay ═══════ */

static void mode_item_click_cb(lv_event_t *e)
{
    int idx = (int)(intptr_t)lv_event_get_user_data(e);
    s_ctrl.mode = idx;

    for (int i = 0; i < 4; i++) {
        bool sel = (i == idx);
        lv_obj_set_style_bg_color(s_mode_items[i], sel ? C_CARD_ON : lv_color_hex(0x0C1525), 0);
        lv_obj_set_style_border_color(s_mode_items[i], sel ? C_GOOD : C_CARD_BRD, 0);
        lv_obj_set_style_border_width(s_mode_items[i], sel ? 2 : 1, 0);
    }
    update_card_status();
}

static void open_mode_overlay(void)
{
    lv_obj_t *panel = create_overlay_panel(S_MODE);

    for (int i = 0; i < 4; i++) {
        bool sel = (i == s_ctrl.mode);

        lv_obj_t *item = lv_obj_create(panel);
        lv_obj_set_size(item, LV_PCT(100), 60);
        lv_obj_set_style_bg_color(item, sel ? C_CARD_ON : lv_color_hex(0x0C1525), 0);
        lv_obj_set_style_bg_opa(item, LV_OPA_COVER, 0);
        lv_obj_set_style_border_color(item, sel ? C_GOOD : C_CARD_BRD, 0);
        lv_obj_set_style_border_width(item, sel ? 2 : 1, 0);
        lv_obj_set_style_radius(item, 14, 0);
        lv_obj_set_style_pad_all(item, 14, 0);
        lv_obj_clear_flag(item, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(item, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(item, mode_item_click_cb, LV_EVENT_CLICKED, (void *)(intptr_t)i);
        s_mode_items[i] = item;

        lv_obj_t *name = lv_label_create(item);
        lv_label_set_text(name, MODE_NAMES[i]);
        lv_obj_set_style_text_color(name, sel ? C_TEXT : C_DIM, 0);
        lv_obj_set_style_text_font(name, &app_font_cn_20, 0);
        lv_obj_align(name, LV_ALIGN_LEFT_MID, 0, -10);

        lv_obj_t *desc = lv_label_create(item);
        lv_label_set_text(desc, MODE_DESCS[i]);
        lv_obj_set_style_text_color(desc, C_FAINT, 0);
        lv_obj_set_style_text_font(desc, &app_font_cn_14, 0);
        lv_obj_align(desc, LV_ALIGN_LEFT_MID, 0, 14);

        if (sel) {
            lv_obj_t *check = lv_label_create(item);
            lv_label_set_text(check, LV_SYMBOL_OK);
            lv_obj_set_style_text_color(check, C_GOOD, 0);
            lv_obj_set_style_text_font(check, &app_font_20, 0);
            lv_obj_align(check, LV_ALIGN_RIGHT_MID, 0, 0);
        }
    }
}

/* ═══════ Card click handlers ═══════ */

static void card_click_cb(lv_event_t *e)
{
    int idx = (int)(intptr_t)lv_event_get_user_data(e);
    switch (idx) {
        case 0: open_light_overlay(); break;
        case 1: open_ac_overlay(); break;
        case 2: open_blinds_overlay(); break;
        case 3: open_mode_overlay(); break;
    }
}

static void power_dot_click_cb(lv_event_t *e)
{
    int idx = (int)(intptr_t)lv_event_get_user_data(e);
    switch (idx) {
        case 0: s_ctrl.light_on   = !s_ctrl.light_on;   break;
        case 1: s_ctrl.ac_on      = !s_ctrl.ac_on;      break;
        case 2: s_ctrl.blinds_open = !s_ctrl.blinds_open; break;
        default: return;
    }
    update_card_status();
    lv_event_stop_processing(e); /* don't bubble to card */
}

/* ═══════ Card status update ═══════ */

static void update_card_status(void)
{
    static char vbuf[4][32];
    static char sbuf[4][48];

    bool on_states[4] = {s_ctrl.light_on, s_ctrl.ac_on, s_ctrl.blinds_open, true};

    /* === Light === */
    if (s_ctrl.light_on) {
        snprintf(vbuf[0], sizeof(vbuf[0]), "%d%%", s_ctrl.light_brightness);
        snprintf(sbuf[0], sizeof(sbuf[0]), "%s", S_BRIGHT);
    } else {
        snprintf(vbuf[0], sizeof(vbuf[0]), "%s", S_OFF);
        snprintf(sbuf[0], sizeof(sbuf[0]), "%s", S_BRIGHT);
    }
    lv_label_set_text(s_card_value[0], vbuf[0]);
    lv_label_set_text(s_card_sub[0], sbuf[0]);
    lv_obj_set_style_text_font(s_card_value[0], s_ctrl.light_on ? &app_font_32 : &app_font_cn_20, 0);
    if (s_card_bar[0]) {
        lv_bar_set_value(s_card_bar[0], s_ctrl.light_on ? s_ctrl.light_brightness : 0, LV_ANIM_OFF);
        lv_obj_set_style_bg_color(s_card_bar[0], s_ctrl.light_on ? C_ACCENT : C_TRACK, LV_PART_INDICATOR);
    }

    /* === AC === */
    if (s_ctrl.ac_on) {
        snprintf(vbuf[1], sizeof(vbuf[1]), "%.0f\xC2\xB0" "C", s_ctrl.ac_temp);
        snprintf(sbuf[1], sizeof(sbuf[1]), "%s \xC2\xB7 %s", AC_MODE_CN[s_ctrl.ac_mode], S_TARGET);
    } else {
        snprintf(vbuf[1], sizeof(vbuf[1]), "%s", S_OFF);
        snprintf(sbuf[1], sizeof(sbuf[1]), "%s", AC_MODE_CN[s_ctrl.ac_mode]);
    }
    lv_label_set_text(s_card_value[1], vbuf[1]);
    lv_label_set_text(s_card_sub[1], sbuf[1]);
    lv_obj_set_style_text_font(s_card_value[1], s_ctrl.ac_on ? &app_font_32 : &app_font_cn_20, 0);

    /* AC mode color hint */
    lv_color_t ac_accent = (s_ctrl.ac_mode == 1) ? C_WARM : C_COOL;
    if (s_ctrl.ac_on) {
        lv_obj_set_style_text_color(s_card_value[1], ac_accent, 0);
    } else {
        lv_obj_set_style_text_color(s_card_value[1], C_DIM, 0);
    }

    /* === Blinds === */
    snprintf(vbuf[2], sizeof(vbuf[2]), "%s", s_ctrl.blinds_open ? S_OPENED : S_CLOSED);
    lv_label_set_text(s_card_value[2], vbuf[2]);
    lv_obj_set_style_text_font(s_card_value[2], &app_font_cn_20, 0);
    lv_label_set_text(s_card_sub[2], "\xE9\x81\xAE\xE9\x98\xB3"); /* 遮阳 */
    /* dynamic icon: 开=UP, 关=DOWN */
    lv_label_set_text(s_card_icon[2], s_ctrl.blinds_open ? LV_SYMBOL_UP : LV_SYMBOL_DOWN);

    /* === Mode === */
    lv_label_set_text(s_card_value[3], MODE_NAMES[s_ctrl.mode]);
    lv_obj_set_style_text_font(s_card_value[3], &app_font_cn_20, 0);
    lv_label_set_text(s_card_sub[3], MODE_DESCS[s_ctrl.mode]);

    /* Per-card active accent color */
    lv_color_t accents[4] = {C_ACCENT, ac_accent, C_ACCENT, C_GOOD};

    /* === Card visual states === */
    for (int i = 0; i < 4; i++) {
        bool on = on_states[i];
        lv_color_t accent = accents[i];
        lv_obj_set_style_bg_color(s_cards[i], on ? C_CARD_ON : C_CARD, 0);
        lv_obj_set_style_border_color(s_cards[i], on ? accent : C_CARD_BRD, 0);
        lv_obj_set_style_border_width(s_cards[i], on ? 2 : 1, 0);
        lv_obj_set_style_shadow_color(s_cards[i], on ? accent : lv_color_black(), 0);
        lv_obj_set_style_shadow_opa(s_cards[i], on ? LV_OPA_30 : LV_OPA_TRANSP, 0);
        lv_obj_set_style_shadow_width(s_cards[i], on ? 18 : 0, 0);
        lv_obj_set_style_shadow_spread(s_cards[i], 0, 0);

        /* icon color */
        lv_obj_set_style_text_color(s_card_icon[i], on ? accent : C_FAINT, 0);

        /* power dot (mode 卡片无) */
        if (s_card_dot[i]) {
            lv_obj_set_style_bg_color(s_card_dot[i], on ? C_ACCENT : lv_color_hex(0x202B40), 0);
            lv_obj_set_style_border_color(s_card_dot[i], on ? C_ACCENT : C_CARD_BRD, 0);
            lv_obj_set_style_text_color(s_card_dot_icon[i], on ? lv_color_hex(0x06121F) : C_DIM, 0);
        }

        /* sub label color */
        lv_obj_set_style_text_color(s_card_sub[i], on ? C_DIM : C_FAINT, 0);

        /* main value default color (AC handled above) */
        if (i != 1) {
            lv_obj_set_style_text_color(s_card_value[i],
                                        on ? C_TEXT : C_DIM, 0);
        }
    }

    refresh_hero();
}

/* ═══════ Hero header ═══════ */

static void refresh_hero(void)
{
    if (s_hero_mode) {
        lv_label_set_text(s_hero_mode, MODE_NAMES[s_ctrl.mode]);
    }
    if (s_hero_temp) {
        const sensor_data_t *sd = app_manager_get_sensor_data();
        static char tbuf[24];
        if (sd && sd->temp > -50.0f && sd->temp < 100.0f) {
            snprintf(tbuf, sizeof(tbuf), "%s %.1f\xC2\xB0" "C", S_ROOM, sd->temp);
        } else {
            snprintf(tbuf, sizeof(tbuf), "%s --", S_ROOM);
        }
        lv_label_set_text(s_hero_temp, tbuf);
    }
}

/* ═══════ Card builder ═══════ */

static void build_card(lv_obj_t *parent, int idx, int x, int y, int w, int h)
{
    lv_obj_t *card = lv_obj_create(parent);
    lv_obj_set_size(card, w, h);
    lv_obj_set_style_bg_color(card, C_CARD, 0);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(card, 1, 0);
    lv_obj_set_style_border_color(card, C_CARD_BRD, 0);
    lv_obj_set_style_radius(card, 20, 0);
    lv_obj_set_style_pad_all(card, 14, 0);
    lv_obj_set_style_bg_color(card, lv_color_hex(0x152040), LV_STATE_PRESSED);
    lv_obj_align(card, LV_ALIGN_TOP_LEFT, x, y);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(card, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(card, card_click_cb, LV_EVENT_CLICKED, (void *)(intptr_t)idx);
    s_cards[idx] = card;

    /* Top row: icon + name (left), power dot (right) */
    lv_obj_t *icon = lv_label_create(card);
    lv_label_set_text(icon, CARD_ICONS[idx]);
    lv_obj_set_style_text_color(icon, C_FAINT, 0);
    lv_obj_set_style_text_font(icon, &app_font_20, 0);
    lv_obj_align(icon, LV_ALIGN_TOP_LEFT, 0, 0);
    s_card_icon[idx] = icon;

    lv_obj_t *name = lv_label_create(card);
    lv_label_set_text(name, CARD_NAMES[idx]);
    lv_obj_set_style_text_color(name, C_TEXT, 0);
    lv_obj_set_style_text_font(name, &app_font_cn_14, 0);
    lv_obj_align(name, LV_ALIGN_TOP_LEFT, 28, 3);

    /* Power dot (skip for mode card) */
    if (idx != 3) {
        lv_obj_t *dot = lv_button_create(card);
        lv_obj_set_size(dot, 32, 32);
        lv_obj_set_style_bg_color(dot, lv_color_hex(0x202B40), 0);
        lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_border_width(dot, 1, 0);
        lv_obj_set_style_border_color(dot, C_CARD_BRD, 0);
        lv_obj_set_style_shadow_opa(dot, LV_OPA_TRANSP, 0);
        lv_obj_align(dot, LV_ALIGN_TOP_RIGHT, 0, -2);
        lv_obj_add_event_cb(dot, power_dot_click_cb, LV_EVENT_CLICKED, (void *)(intptr_t)idx);
        s_card_dot[idx] = dot;

        lv_obj_t *dot_icon = lv_label_create(dot);
        lv_label_set_text(dot_icon, LV_SYMBOL_POWER);
        lv_obj_set_style_text_color(dot_icon, C_DIM, 0);
        lv_obj_set_style_text_font(dot_icon, &app_font_14, 0);
        lv_obj_center(dot_icon);
        s_card_dot_icon[idx] = dot_icon;
    } else {
        s_card_dot[idx] = NULL;
        s_card_dot_icon[idx] = NULL;
    }

    /* Main value (centered-ish) */
    lv_obj_t *value = lv_label_create(card);
    lv_label_set_text(value, "--");
    lv_obj_set_style_text_color(value, C_TEXT, 0);
    lv_obj_set_style_text_font(value, &app_font_32, 0);
    lv_obj_align(value, LV_ALIGN_LEFT_MID, 0, 6);
    s_card_value[idx] = value;

    /* Optional bar (light only) */
    if (idx == 0) {
        lv_obj_t *bar = lv_bar_create(card);
        lv_obj_set_size(bar, w - 28, 4);
        lv_obj_set_style_bg_color(bar, C_TRACK, LV_PART_MAIN);
        lv_obj_set_style_bg_color(bar, C_ACCENT, LV_PART_INDICATOR);
        lv_obj_set_style_radius(bar, 2, LV_PART_MAIN);
        lv_obj_set_style_radius(bar, 2, LV_PART_INDICATOR);
        lv_bar_set_range(bar, 0, 100);
        lv_obj_align(bar, LV_ALIGN_BOTTOM_LEFT, 0, -22);
        s_card_bar[idx] = bar;
    } else {
        s_card_bar[idx] = NULL;
    }

    /* Sub status (bottom) */
    lv_obj_t *sub = lv_label_create(card);
    lv_label_set_text(sub, "--");
    lv_obj_set_style_text_color(sub, C_FAINT, 0);
    lv_obj_set_style_text_font(sub, &app_font_cn_14, 0);
    lv_obj_align(sub, LV_ALIGN_BOTTOM_LEFT, 0, 0);
    s_card_sub[idx] = sub;
}

/* ═══════ Page lifecycle ═══════ */

void page_control_create(lv_obj_t *parent)
{
    lv_obj_set_style_bg_color(parent, C_BG, 0);
    lv_obj_set_style_bg_opa(parent, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(parent, 18, 0);
    lv_obj_clear_flag(parent, LV_OBJ_FLAG_SCROLLABLE);

    /* ═══════ Hero header ═══════ */
    lv_obj_t *header = lv_obj_create(parent);
    lv_obj_remove_style_all(header);
    lv_obj_set_size(header, 444, 44);
    lv_obj_align(header, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_set_layout(header, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(header, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(header, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);

    /* Left side: title */
    lv_obj_t *title = lv_label_create(header);
    lv_label_set_text(title, S_CONTROL);
    lv_obj_set_style_text_color(title, C_TEXT, 0);
    lv_obj_set_style_text_font(title, &app_font_cn_20, 0);

    /* Right side: a small group with mode chip + temp */
    lv_obj_t *right = lv_obj_create(header);
    lv_obj_remove_style_all(right);
    lv_obj_set_size(right, LV_SIZE_CONTENT, 36);
    lv_obj_set_layout(right, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(right, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(right, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(right, 10, 0);
    lv_obj_clear_flag(right, LV_OBJ_FLAG_SCROLLABLE);

    /* Room temp */
    lv_obj_t *room = lv_label_create(right);
    lv_label_set_text(room, "\xE5\xAE\xA4\xE6\xB8\xA9 --"); /* 室温 -- */
    lv_obj_set_style_text_color(room, C_DIM, 0);
    lv_obj_set_style_text_font(room, &app_font_cn_14, 0);
    s_hero_temp = room;

    /* Mode chip */
    lv_obj_t *chip = lv_obj_create(right);
    lv_obj_remove_style_all(chip);
    lv_obj_set_size(chip, LV_SIZE_CONTENT, 30);
    lv_obj_set_style_bg_color(chip, lv_color_hex(0x14223D), 0);
    lv_obj_set_style_bg_opa(chip, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(chip, 1, 0);
    lv_obj_set_style_border_color(chip, C_ACCENT_DIM, 0);
    lv_obj_set_style_radius(chip, 15, 0);
    lv_obj_set_style_pad_hor(chip, 12, 0);
    lv_obj_set_layout(chip, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(chip, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(chip, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(chip, 6, 0);
    lv_obj_clear_flag(chip, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *chip_dot = lv_obj_create(chip);
    lv_obj_remove_style_all(chip_dot);
    lv_obj_set_size(chip_dot, 6, 6);
    lv_obj_set_style_bg_color(chip_dot, C_GOOD, 0);
    lv_obj_set_style_bg_opa(chip_dot, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(chip_dot, LV_RADIUS_CIRCLE, 0);

    lv_obj_t *chip_lbl = lv_label_create(chip);
    lv_label_set_text(chip_lbl, MODE_NAMES[s_ctrl.mode]);
    lv_obj_set_style_text_color(chip_lbl, C_TEXT, 0);
    lv_obj_set_style_text_font(chip_lbl, &app_font_cn_14, 0);
    s_hero_mode = chip_lbl;

    /* ═══════ Card grid: 2×2 ═══════ */
    /* Cards: 444 px wide / 2 = 222, with 12 gap → each card 210 wide */
    /* Vertical: hero 44 + 12 gap = top y=56; remaining 480-18*2-56-15(dots)=355; each row 171 + 12 gap */
    const int card_w = 210;
    const int card_h = 171;
    const int gap    = 12;
    const int top_y  = 56;

    for (int i = 0; i < 4; i++) {
        int col = i % 2;
        int row = i / 2;
        int x = col * (card_w + gap);
        int y = top_y + row * (card_h + gap);
        build_card(parent, i, x, y, card_w, card_h);
    }

    update_card_status();
    refresh_hero();
}

void page_control_update(const sensor_data_t *data)
{
    (void)data;
    /* Hero header refresh on sensor change */
    refresh_hero();
}
