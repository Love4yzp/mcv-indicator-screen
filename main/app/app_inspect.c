/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Spencer (love4yzp)
 */

#include "app/app_inspect.h"

#include <string.h>

#include "lvgl.h"

static void json_escape(FILE *out, const char *s)
{
    if (s == NULL) { fputs("\"\"", out); return; }
    fputc('"', out);
    for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
        unsigned char c = *p;
        switch (c) {
        case '"':  fputs("\\\"", out); break;
        case '\\': fputs("\\\\", out); break;
        case '\n': fputs("\\n", out);  break;
        case '\r': fputs("\\r", out);  break;
        case '\t': fputs("\\t", out);  break;
        default:
            if (c < 0x20) fprintf(out, "\\u%04x", c);
            else fputc(c, out);
        }
    }
    fputc('"', out);
}

static const char *class_name(const lv_obj_t *obj)
{
    if (lv_obj_check_type((lv_obj_t *)obj, &lv_label_class))       return "label";
    if (lv_obj_check_type((lv_obj_t *)obj, &lv_button_class))      return "button";
    if (lv_obj_check_type((lv_obj_t *)obj, &lv_arc_class))         return "arc";
    if (lv_obj_check_type((lv_obj_t *)obj, &lv_bar_class))         return "bar";
    if (lv_obj_check_type((lv_obj_t *)obj, &lv_slider_class))      return "slider";
    if (lv_obj_check_type((lv_obj_t *)obj, &lv_switch_class))      return "switch";
    if (lv_obj_check_type((lv_obj_t *)obj, &lv_image_class))       return "image";
    if (lv_obj_check_type((lv_obj_t *)obj, &lv_roller_class))      return "roller";
    if (lv_obj_check_type((lv_obj_t *)obj, &lv_dropdown_class))    return "dropdown";
    if (lv_obj_check_type((lv_obj_t *)obj, &lv_checkbox_class))    return "checkbox";
    if (lv_obj_check_type((lv_obj_t *)obj, &lv_tileview_class))    return "tileview";
    if (lv_obj_check_type((lv_obj_t *)obj, &lv_tileview_tile_class)) return "tile";
    return "obj";
}

/* If a button has exactly one label child, return its text — avoids the
 * common pattern where callers store button text on a child label. */
static const char *button_text(const lv_obj_t *btn)
{
    uint32_t n = lv_obj_get_child_count(btn);
    for (uint32_t i = 0; i < n; i++) {
        lv_obj_t *child = lv_obj_get_child((lv_obj_t *)btn, i);
        if (lv_obj_check_type(child, &lv_label_class)) {
            return lv_label_get_text(child);
        }
    }
    return NULL;
}

void app_inspect_walk(lv_obj_t *obj, int depth, FILE *out, bool *first)
{
    if (obj == NULL) return;
    if (lv_obj_has_flag(obj, LV_OBJ_FLAG_HIDDEN)) return;

    lv_area_t area;
    lv_obj_get_coords(obj, &area);
    int32_t w = area.x2 - area.x1 + 1;
    int32_t h = area.y2 - area.y1 + 1;
    if (w <= 0 || h <= 0) return;

    const char *klass = class_name(obj);

    /* Skip pure layout containers with no useful data to reduce noise. */
    bool is_plain_obj = (strcmp(klass, "obj") == 0);
    bool has_children = lv_obj_get_child_count(obj) > 0;

    if (!is_plain_obj || !has_children) {
        if (!*first) fputs(",\n", out);
        *first = false;
        fprintf(out, "    {\"depth\":%d,\"class\":\"%s\",\"coords\":[%ld,%ld,%ld,%ld]",
                depth, klass,
                (long)area.x1, (long)area.y1, (long)area.x2, (long)area.y2);

        if (strcmp(klass, "label") == 0) {
            fputs(",\"text\":", out);
            json_escape(out, lv_label_get_text(obj));
        } else if (strcmp(klass, "button") == 0) {
            const char *t = button_text(obj);
            if (t != NULL) { fputs(",\"text\":", out); json_escape(out, t); }
        } else if (strcmp(klass, "arc") == 0) {
            fprintf(out, ",\"value\":%d,\"range\":[%d,%d]",
                    (int)lv_arc_get_value(obj),
                    (int)lv_arc_get_min_value(obj),
                    (int)lv_arc_get_max_value(obj));
        } else if (strcmp(klass, "bar") == 0) {
            fprintf(out, ",\"value\":%d,\"range\":[%d,%d]",
                    (int)lv_bar_get_value(obj),
                    (int)lv_bar_get_min_value(obj),
                    (int)lv_bar_get_max_value(obj));
        } else if (strcmp(klass, "slider") == 0) {
            fprintf(out, ",\"value\":%d,\"range\":[%d,%d]",
                    (int)lv_slider_get_value(obj),
                    (int)lv_slider_get_min_value(obj),
                    (int)lv_slider_get_max_value(obj));
        } else if (strcmp(klass, "switch") == 0 || strcmp(klass, "checkbox") == 0) {
            fprintf(out, ",\"checked\":%s",
                    lv_obj_has_state(obj, LV_STATE_CHECKED) ? "true" : "false");
        }
        fputc('}', out);
    }

    uint32_t n = lv_obj_get_child_count(obj);
    for (uint32_t i = 0; i < n; i++) {
        app_inspect_walk(lv_obj_get_child(obj, i), depth + 1, out, first);
    }
}

void app_inspect_dump_json(lv_obj_t *root, FILE *out)
{
    if (root == NULL || out == NULL) return;
    lv_area_t area;
    lv_obj_get_coords(root, &area);
    int32_t w = area.x2 - area.x1 + 1;
    int32_t h = area.y2 - area.y1 + 1;

    fprintf(out, "{\n  \"viewport\": {\"w\":%ld,\"h\":%ld},\n  \"objects\": [\n",
            (long)w, (long)h);
    bool first = true;
    app_inspect_walk(root, 0, out, &first);
    fputs("\n  ]\n}\n", out);
}
