/*******************************************************************************
 * Size: 14 px
 * Bpp: 4
 * Opts: --no-compress --no-prefilter --bpp 4 --size 14 --font Montserrat-Medium.ttf -r 0xB2-0xB5,0xB0 --format lvgl -o /Users/spencer/Seeed/ops/chaihuo-car/mcv-indicator-screen/main/app/fonts/font_unit_14.c --force-fast-kern-format
 ******************************************************************************/

#include "lvgl.h"

#ifndef FONT_UNIT_14
#define FONT_UNIT_14 1
#endif

#if FONT_UNIT_14

/*-----------------
 *    BITMAPS
 *----------------*/

/*Store the image of the glyphs*/
static LV_ATTRIBUTE_LARGE_CONST const uint8_t glyph_bitmap[] = {
    /* U+00B0 "°" */
    0x4, 0xcc, 0x30, 0x2b, 0x1, 0xc0, 0x57, 0x0,
    0x93, 0x2b, 0x1, 0xc0, 0x5, 0xcc, 0x30,

    /* U+00B2 "²" */
    0x2b, 0xdd, 0x70, 0x11, 0x0, 0xf0, 0x0, 0xa,
    0x60, 0x3, 0xb3, 0x0, 0x4f, 0xdd, 0xd4,

    /* U+00B3 "³" */
    0x5d, 0xdd, 0xe0, 0x0, 0x2b, 0x20, 0x0, 0x8a,
    0x90, 0x10, 0x0, 0xc4, 0x4c, 0xdd, 0x90,

    /* U+00B4 "´" */
    0x6, 0xe3, 0x6d, 0x20,

    /* U+00B5 "µ" */
    0xba, 0x0, 0x1, 0xf4, 0xba, 0x0, 0x1, 0xf4,
    0xba, 0x0, 0x1, 0xf4, 0xba, 0x0, 0x1, 0xf4,
    0xba, 0x0, 0x2, 0xf4, 0xbc, 0x0, 0x6, 0xf4,
    0xbf, 0x93, 0x6e, 0xf4, 0xbb, 0xbf, 0xd5, 0xf4,
    0xba, 0x0, 0x0, 0x0, 0xba, 0x0, 0x0, 0x0,
    0xba, 0x0, 0x0, 0x0
};


/*---------------------
 *  GLYPH DESCRIPTION
 *--------------------*/

static const lv_font_fmt_txt_glyph_dsc_t glyph_dsc[] = {
    {.bitmap_index = 0, .adv_w = 0, .box_w = 0, .box_h = 0, .ofs_x = 0, .ofs_y = 0} /* id = 0 reserved */,
    {.bitmap_index = 0, .adv_w = 94, .box_w = 6, .box_h = 5, .ofs_x = 0, .ofs_y = 6},
    {.bitmap_index = 15, .adv_w = 96, .box_w = 6, .box_h = 5, .ofs_x = 0, .ofs_y = 5},
    {.bitmap_index = 30, .adv_w = 96, .box_w = 6, .box_h = 5, .ofs_x = 0, .ofs_y = 5},
    {.bitmap_index = 45, .adv_w = 134, .box_w = 4, .box_h = 2, .ofs_x = 3, .ofs_y = 9},
    {.bitmap_index = 49, .adv_w = 153, .box_w = 8, .box_h = 11, .ofs_x = 1, .ofs_y = -3}
};

/*---------------------
 *  CHARACTER MAPPING
 *--------------------*/

static const uint8_t glyph_id_ofs_list_0[] = {
    0, 0, 1, 2, 3, 4
};

/*Collect the unicode lists and glyph_id offsets*/
static const lv_font_fmt_txt_cmap_t cmaps[] =
{
    {
        .range_start = 176, .range_length = 6, .glyph_id_start = 1,
        .unicode_list = NULL, .glyph_id_ofs_list = glyph_id_ofs_list_0, .list_length = 6, .type = LV_FONT_FMT_TXT_CMAP_FORMAT0_FULL
    }
};



/*--------------------
 *  ALL CUSTOM DATA
 *--------------------*/

#if LVGL_VERSION_MAJOR == 8
/*Store all the custom data of the font*/
static  lv_font_fmt_txt_glyph_cache_t cache;
#endif

#if LVGL_VERSION_MAJOR >= 8
static const lv_font_fmt_txt_dsc_t font_dsc = {
#else
static lv_font_fmt_txt_dsc_t font_dsc = {
#endif
    .glyph_bitmap = glyph_bitmap,
    .glyph_dsc = glyph_dsc,
    .cmaps = cmaps,
    .kern_dsc = NULL,
    .kern_scale = 0,
    .cmap_num = 1,
    .bpp = 4,
    .kern_classes = 0,
    .bitmap_format = 0,
#if LVGL_VERSION_MAJOR == 8
    .cache = &cache
#endif
};



/*-----------------
 *  PUBLIC FONT
 *----------------*/

/*Initialize a public general font descriptor*/
#if LVGL_VERSION_MAJOR >= 8
const lv_font_t font_unit_14 = {
#else
lv_font_t font_unit_14 = {
#endif
    .get_glyph_dsc = lv_font_get_glyph_dsc_fmt_txt,    /*Function pointer to get glyph's data*/
    .get_glyph_bitmap = lv_font_get_bitmap_fmt_txt,    /*Function pointer to get glyph's bitmap*/
    .line_height = 14,          /*The maximum line height required by the font*/
    .base_line = 3,             /*Baseline measured from the bottom of the line*/
#if !(LVGL_VERSION_MAJOR == 6 && LVGL_VERSION_MINOR == 0)
    .subpx = LV_FONT_SUBPX_NONE,
#endif
#if LV_VERSION_CHECK(7, 4, 0) || LVGL_VERSION_MAJOR >= 8
    .underline_position = -1,
    .underline_thickness = 1,
#endif
    .dsc = &font_dsc,          /*The custom font data. Will be accessed by `get_glyph_bitmap/dsc` */
#if LV_VERSION_CHECK(8, 2, 0) || LVGL_VERSION_MAJOR >= 9
    .fallback = NULL,
#endif
    .user_data = NULL,
};



#endif /*#if FONT_UNIT_14*/

