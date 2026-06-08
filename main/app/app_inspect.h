/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Spencer (love4yzp)
 *
 * Widget-tree inspection for test / simulator feedback.
 * Emits a flat JSON list of visible LVGL objects (class, coords, text, value)
 * so callers can assert UI state by grep/jq without parsing a PNG.
 */

#pragma once

#include <stdbool.h>
#include <stdio.h>

#include "app/app_manager.h"  /* for lv_obj_t forward decl */

#ifdef __cplusplus
extern "C" {
#endif

/* Walk `root` and write a self-contained JSON object to `out`. */
void app_inspect_dump_json(lv_obj_t *root, FILE *out);

/* Walker primitive used when composing a larger JSON document. Emits JSON
 * object entries (comma-prefixed after the first) into an already-open array.
 * `*first` must start true on the outer call and is cleared by the walker. */
void app_inspect_walk(lv_obj_t *obj, int depth, FILE *out, bool *first);

/* Dump the currently-active page (tileview tile) plus screen-level overlays.
 * Adds page metadata so a screenshot+tree pair is self-describing. */
void app_manager_inspect_json(FILE *out);

#ifdef __cplusplus
}
#endif
