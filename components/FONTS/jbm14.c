/*******************************************************************************
 * Size: 14 px
 * Bpp: 1
 * Opts: --bpp 1 --size 14 --no-compress --stride 1 --align 1 --font JetBrainsMonoNL-Regular.ttf --range 32-127 --format lvgl -o jbm14.c
 ******************************************************************************/
#include "jbm14.h"
#ifdef __has_include
    #if __has_include("lvgl.h")
        #ifndef LV_LVGL_H_INCLUDE_SIMPLE
            #define LV_LVGL_H_INCLUDE_SIMPLE
        #endif
    #endif
#endif

#ifdef LV_LVGL_H_INCLUDE_SIMPLE
    #include "lvgl.h"
#else
    #include "lvgl/lvgl.h"
#endif



#ifndef JBM14
#define JBM14 1
#endif

#if JBM14

/*-----------------
 *    BITMAPS
 *----------------*/

/*Store the image of the glyphs*/
static LV_ATTRIBUTE_LARGE_CONST const uint8_t glyph_bitmap[] = {
    /* U+0020 " " */
    0x0,

    /* U+0021 "!" */
    0x55, 0x54, 0x30,

    /* U+0022 "\"" */
    0x99, 0x99,

    /* U+0023 "#" */
    0x12, 0x12, 0x12, 0x7f, 0x24, 0x24, 0xfe, 0x24,
    0x44, 0x48,

    /* U+0024 "$" */
    0x10, 0x43, 0x9f, 0x96, 0x4d, 0x1e, 0x1c, 0x59,
    0x65, 0x78, 0x41, 0x0,

    /* U+0025 "%" */
    0xf1, 0x92, 0x94, 0xf4, 0x8, 0x16, 0x29, 0x29,
    0x49, 0x86,

    /* U+0026 "&" */
    0x38, 0x44, 0x40, 0x20, 0x70, 0x9b, 0x8e, 0x84,
    0x8a, 0x73,

    /* U+0027 "'" */
    0xf0,

    /* U+0028 "(" */
    0x16, 0x48, 0x88, 0x88, 0x88, 0xc4, 0x31,

    /* U+0029 ")" */
    0x86, 0x21, 0x11, 0x11, 0x11, 0x32, 0xc8,

    /* U+002A "*" */
    0x10, 0x23, 0x5b, 0xe3, 0x5, 0x11, 0x0,

    /* U+002B "+" */
    0x10, 0x23, 0xf8, 0x81, 0x2, 0x0,

    /* U+002C "," */
    0x7e,

    /* U+002D "-" */
    0xf0,

    /* U+002E "." */
    0xf0,

    /* U+002F "/" */
    0x4, 0x10, 0xc2, 0x8, 0x61, 0xc, 0x20, 0x86,
    0x10, 0x43, 0x0,

    /* U+0030 "0" */
    0x7b, 0x38, 0x61, 0x96, 0x18, 0x61, 0xcd, 0xe0,

    /* U+0031 "1" */
    0x33, 0x49, 0x4, 0x10, 0x41, 0x4, 0x13, 0xf0,

    /* U+0032 "2" */
    0x7b, 0x38, 0x41, 0xc, 0x63, 0x18, 0xc3, 0xf0,

    /* U+0033 "3" */
    0xfc, 0x31, 0x8c, 0x38, 0x10, 0x61, 0xcd, 0xe0,

    /* U+0034 "4" */
    0x8, 0x43, 0x8, 0x47, 0x18, 0x7f, 0x4, 0x10,

    /* U+0035 "5" */
    0xfe, 0x8, 0x3e, 0xc, 0x10, 0x61, 0xcd, 0xe0,

    /* U+0036 "6" */
    0x10, 0x82, 0x1e, 0x4e, 0x18, 0x61, 0xc9, 0xe0,

    /* U+0037 "7" */
    0xff, 0xa, 0x30, 0x40, 0x83, 0x4, 0x18, 0x20,
    0xc0,

    /* U+0038 "8" */
    0x7a, 0x18, 0x61, 0x7b, 0x38, 0x61, 0xcd, 0xe0,

    /* U+0039 "9" */
    0x7b, 0x38, 0x61, 0xcd, 0xe1, 0x84, 0x30, 0x80,

    /* U+003A ":" */
    0xf0, 0xf,

    /* U+003B ";" */
    0xf0, 0x3, 0xa8,

    /* U+003C "<" */
    0x0, 0x33, 0xb8, 0x81, 0x81, 0x81,

    /* U+003D "=" */
    0xfc, 0x0, 0x0, 0xfc,

    /* U+003E ">" */
    0x3, 0x7, 0x7, 0x4, 0x66, 0x20,

    /* U+003F "?" */
    0xf0, 0x42, 0x13, 0x10, 0x80, 0x3, 0x0,

    /* U+0040 "@" */
    0x3c, 0x8e, 0xc, 0x79, 0x32, 0x64, 0xc9, 0x93,
    0x1a, 0x2, 0x3, 0x80,

    /* U+0041 "A" */
    0x30, 0x60, 0xa3, 0x44, 0x89, 0x9f, 0x62, 0x85,
    0xc,

    /* U+0042 "B" */
    0xfa, 0x18, 0x61, 0xfa, 0x38, 0x61, 0x8f, 0xe0,

    /* U+0043 "C" */
    0x7a, 0x18, 0x60, 0x82, 0x8, 0x21, 0xc5, 0xe0,

    /* U+0044 "D" */
    0xfa, 0x38, 0x61, 0x86, 0x18, 0x61, 0x8f, 0xe0,

    /* U+0045 "E" */
    0xfe, 0x8, 0x20, 0xfa, 0x8, 0x20, 0x83, 0xf0,

    /* U+0046 "F" */
    0xfe, 0x8, 0x20, 0xfa, 0x8, 0x20, 0x82, 0x0,

    /* U+0047 "G" */
    0x7b, 0x18, 0x20, 0x9e, 0x18, 0x61, 0xcd, 0xe0,

    /* U+0048 "H" */
    0x86, 0x18, 0x61, 0xfe, 0x18, 0x61, 0x86, 0x10,

    /* U+0049 "I" */
    0xf9, 0x8, 0x42, 0x10, 0x84, 0x27, 0xc0,

    /* U+004A "J" */
    0x4, 0x10, 0x41, 0x4, 0x10, 0x61, 0xcd, 0xe0,

    /* U+004B "K" */
    0x8e, 0x29, 0xa4, 0xf2, 0x49, 0xa2, 0x8e, 0x10,

    /* U+004C "L" */
    0x82, 0x8, 0x20, 0x82, 0x8, 0x20, 0x83, 0xf0,

    /* U+004D "M" */
    0xcf, 0x3d, 0x6d, 0xb6, 0xd8, 0x61, 0x86, 0x10,

    /* U+004E "N" */
    0xc7, 0x1e, 0x69, 0xa6, 0x59, 0x67, 0x8e, 0x30,

    /* U+004F "O" */
    0x7a, 0x18, 0x61, 0x86, 0x18, 0x61, 0xc5, 0xe0,

    /* U+0050 "P" */
    0xfa, 0x38, 0x61, 0x8f, 0xe8, 0x20, 0x82, 0x0,

    /* U+0051 "Q" */
    0x7b, 0x38, 0x61, 0x86, 0x18, 0x61, 0xcd, 0xe0,
    0x82, 0x4,

    /* U+0052 "R" */
    0xfa, 0x18, 0x61, 0xfa, 0x68, 0xa2, 0x8e, 0x10,

    /* U+0053 "S" */
    0x7a, 0x38, 0x60, 0x70, 0x60, 0x61, 0xcd, 0xe0,

    /* U+0054 "T" */
    0xfe, 0x20, 0x40, 0x81, 0x2, 0x4, 0x8, 0x10,
    0x20,

    /* U+0055 "U" */
    0x86, 0x18, 0x61, 0x86, 0x18, 0x61, 0x85, 0xe0,

    /* U+0056 "V" */
    0x87, 0xb, 0x12, 0x24, 0xc9, 0x1a, 0x14, 0x30,
    0x60,

    /* U+0057 "W" */
    0x99, 0x99, 0x99, 0xd9, 0xda, 0x7a, 0x66, 0x66,
    0x66, 0x66,

    /* U+0058 "X" */
    0x84, 0x99, 0xa1, 0xc3, 0x6, 0xe, 0x24, 0xc5,
    0xc,

    /* U+0059 "Y" */
    0xc2, 0x89, 0x11, 0x42, 0x82, 0x4, 0x8, 0x10,
    0x20,

    /* U+005A "Z" */
    0xfc, 0x30, 0x86, 0x10, 0x86, 0x10, 0xc3, 0xf0,

    /* U+005B "[" */
    0xf2, 0x49, 0x24, 0x92, 0x4e,

    /* U+005C "\\" */
    0x83, 0x4, 0x10, 0x20, 0x83, 0x4, 0x10, 0x60,
    0x82, 0xc, 0x10,

    /* U+005D "]" */
    0xe4, 0x92, 0x49, 0x24, 0x9e,

    /* U+005E "^" */
    0x10, 0xc2, 0x92, 0x4e, 0x10,

    /* U+005F "_" */
    0xfe,

    /* U+0060 "`" */
    0x4c,

    /* U+0061 "a" */
    0x7b, 0x10, 0x5f, 0x86, 0x18, 0xdd,

    /* U+0062 "b" */
    0x82, 0xb, 0xb1, 0x86, 0x18, 0x61, 0xc6, 0xe0,

    /* U+0063 "c" */
    0x7b, 0x18, 0x60, 0x82, 0x1c, 0x5e,

    /* U+0064 "d" */
    0x4, 0x17, 0x63, 0x86, 0x18, 0x61, 0x8d, 0xd0,

    /* U+0065 "e" */
    0x7a, 0x18, 0x7f, 0x82, 0xc, 0xde,

    /* U+0066 "f" */
    0x1e, 0x40, 0x87, 0xf2, 0x4, 0x8, 0x10, 0x20,
    0x40,

    /* U+0067 "g" */
    0x77, 0x38, 0x61, 0x86, 0x1c, 0xdd, 0x4, 0x17,
    0x80,

    /* U+0068 "h" */
    0x82, 0xb, 0xb3, 0x86, 0x18, 0x61, 0x86, 0x10,

    /* U+0069 "i" */
    0x0, 0x60, 0x7, 0x81, 0x2, 0x4, 0x8, 0x10,
    0x23, 0xf8,

    /* U+006A "j" */
    0x0, 0xc1, 0xf0, 0x84, 0x21, 0x8, 0x42, 0x11,
    0xf8,

    /* U+006B "k" */
    0x82, 0x8, 0xe2, 0x93, 0xc9, 0x26, 0x8a, 0x30,

    /* U+006C "l" */
    0xf0, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10,
    0x10, 0xf,

    /* U+006D "m" */
    0xfd, 0x26, 0x4c, 0x99, 0x32, 0x64, 0xc9,

    /* U+006E "n" */
    0xbb, 0x38, 0x61, 0x86, 0x18, 0x61,

    /* U+006F "o" */
    0x7b, 0x38, 0x61, 0x86, 0x1c, 0xde,

    /* U+0070 "p" */
    0xbb, 0x18, 0x61, 0x86, 0x1c, 0x6e, 0x82, 0x8,
    0x0,

    /* U+0071 "q" */
    0x76, 0x38, 0x61, 0x86, 0x18, 0xdd, 0x4, 0x10,
    0x40,

    /* U+0072 "r" */
    0xbb, 0x18, 0x60, 0x82, 0x8, 0x20,

    /* U+0073 "s" */
    0x7a, 0x18, 0x3e, 0x1c, 0x18, 0x5e,

    /* U+0074 "t" */
    0x20, 0x43, 0xf9, 0x2, 0x4, 0x8, 0x10, 0x20,
    0x3c,

    /* U+0075 "u" */
    0x86, 0x18, 0x61, 0x86, 0x1c, 0xde,

    /* U+0076 "v" */
    0x85, 0x89, 0x12, 0x66, 0x85, 0xe, 0x18,

    /* U+0077 "w" */
    0x59, 0x59, 0x59, 0x5a, 0x56, 0x56, 0x66, 0x26,

    /* U+0078 "x" */
    0xc4, 0x98, 0xe1, 0x83, 0xd, 0x13, 0x62,

    /* U+0079 "y" */
    0x85, 0x89, 0x32, 0x46, 0x87, 0xc, 0x8, 0x30,
    0x40, 0x80,

    /* U+007A "z" */
    0xfc, 0x31, 0x84, 0x21, 0x8c, 0x3f,

    /* U+007B "{" */
    0xc, 0x41, 0x4, 0x10, 0x4e, 0x4, 0x10, 0x41,
    0x4, 0xc,

    /* U+007C "|" */
    0xff, 0xfc,

    /* U+007D "}" */
    0xc0, 0x82, 0x8, 0x20, 0x81, 0xc8, 0x20, 0x82,
    0x8, 0xc0,

    /* U+007E "~" */
    0x66, 0xd9, 0x80
};


/*---------------------
 *  GLYPH DESCRIPTION
 *--------------------*/

static const lv_font_fmt_txt_glyph_dsc_t glyph_dsc[] = {
    {.bitmap_index = 0, .adv_w = 0, .box_w = 0, .box_h = 0, .ofs_x = 0, .ofs_y = 0} /* id = 0 reserved */,
    {.bitmap_index = 0, .adv_w = 134, .box_w = 1, .box_h = 1, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1, .adv_w = 134, .box_w = 2, .box_h = 10, .ofs_x = 3, .ofs_y = 0},
    {.bitmap_index = 4, .adv_w = 134, .box_w = 4, .box_h = 4, .ofs_x = 2, .ofs_y = 6},
    {.bitmap_index = 6, .adv_w = 134, .box_w = 8, .box_h = 10, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 16, .adv_w = 134, .box_w = 6, .box_h = 15, .ofs_x = 1, .ofs_y = -2},
    {.bitmap_index = 28, .adv_w = 134, .box_w = 8, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 38, .adv_w = 134, .box_w = 8, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 48, .adv_w = 134, .box_w = 1, .box_h = 4, .ofs_x = 4, .ofs_y = 6},
    {.bitmap_index = 49, .adv_w = 134, .box_w = 4, .box_h = 14, .ofs_x = 3, .ofs_y = -2},
    {.bitmap_index = 56, .adv_w = 134, .box_w = 4, .box_h = 14, .ofs_x = 2, .ofs_y = -2},
    {.bitmap_index = 63, .adv_w = 134, .box_w = 7, .box_h = 7, .ofs_x = 1, .ofs_y = 2},
    {.bitmap_index = 70, .adv_w = 134, .box_w = 7, .box_h = 6, .ofs_x = 1, .ofs_y = 2},
    {.bitmap_index = 76, .adv_w = 134, .box_w = 2, .box_h = 4, .ofs_x = 3, .ofs_y = -3},
    {.bitmap_index = 77, .adv_w = 134, .box_w = 4, .box_h = 1, .ofs_x = 2, .ofs_y = 4},
    {.bitmap_index = 78, .adv_w = 134, .box_w = 2, .box_h = 2, .ofs_x = 3, .ofs_y = 0},
    {.bitmap_index = 79, .adv_w = 134, .box_w = 6, .box_h = 14, .ofs_x = 1, .ofs_y = -2},
    {.bitmap_index = 90, .adv_w = 134, .box_w = 6, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 98, .adv_w = 134, .box_w = 6, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 106, .adv_w = 134, .box_w = 6, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 114, .adv_w = 134, .box_w = 6, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 122, .adv_w = 134, .box_w = 6, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 130, .adv_w = 134, .box_w = 6, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 138, .adv_w = 134, .box_w = 6, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 146, .adv_w = 134, .box_w = 7, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 155, .adv_w = 134, .box_w = 6, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 163, .adv_w = 134, .box_w = 6, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 171, .adv_w = 134, .box_w = 2, .box_h = 8, .ofs_x = 3, .ofs_y = 0},
    {.bitmap_index = 173, .adv_w = 134, .box_w = 2, .box_h = 11, .ofs_x = 3, .ofs_y = -3},
    {.bitmap_index = 176, .adv_w = 134, .box_w = 6, .box_h = 8, .ofs_x = 1, .ofs_y = 1},
    {.bitmap_index = 182, .adv_w = 134, .box_w = 6, .box_h = 5, .ofs_x = 1, .ofs_y = 2},
    {.bitmap_index = 186, .adv_w = 134, .box_w = 6, .box_h = 8, .ofs_x = 1, .ofs_y = 1},
    {.bitmap_index = 192, .adv_w = 134, .box_w = 5, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 199, .adv_w = 134, .box_w = 7, .box_h = 13, .ofs_x = 1, .ofs_y = -3},
    {.bitmap_index = 211, .adv_w = 134, .box_w = 7, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 220, .adv_w = 134, .box_w = 6, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 228, .adv_w = 134, .box_w = 6, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 236, .adv_w = 134, .box_w = 6, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 244, .adv_w = 134, .box_w = 6, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 252, .adv_w = 134, .box_w = 6, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 260, .adv_w = 134, .box_w = 6, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 268, .adv_w = 134, .box_w = 6, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 276, .adv_w = 134, .box_w = 5, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 283, .adv_w = 134, .box_w = 6, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 291, .adv_w = 134, .box_w = 6, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 299, .adv_w = 134, .box_w = 6, .box_h = 10, .ofs_x = 2, .ofs_y = 0},
    {.bitmap_index = 307, .adv_w = 134, .box_w = 6, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 315, .adv_w = 134, .box_w = 6, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 323, .adv_w = 134, .box_w = 6, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 331, .adv_w = 134, .box_w = 6, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 339, .adv_w = 134, .box_w = 6, .box_h = 13, .ofs_x = 1, .ofs_y = -3},
    {.bitmap_index = 349, .adv_w = 134, .box_w = 6, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 357, .adv_w = 134, .box_w = 6, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 365, .adv_w = 134, .box_w = 7, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 374, .adv_w = 134, .box_w = 6, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 382, .adv_w = 134, .box_w = 7, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 391, .adv_w = 134, .box_w = 8, .box_h = 10, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 401, .adv_w = 134, .box_w = 7, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 410, .adv_w = 134, .box_w = 7, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 419, .adv_w = 134, .box_w = 6, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 427, .adv_w = 134, .box_w = 3, .box_h = 13, .ofs_x = 3, .ofs_y = -1},
    {.bitmap_index = 432, .adv_w = 134, .box_w = 6, .box_h = 14, .ofs_x = 1, .ofs_y = -2},
    {.bitmap_index = 443, .adv_w = 134, .box_w = 3, .box_h = 13, .ofs_x = 2, .ofs_y = -1},
    {.bitmap_index = 448, .adv_w = 134, .box_w = 6, .box_h = 6, .ofs_x = 1, .ofs_y = 5},
    {.bitmap_index = 453, .adv_w = 134, .box_w = 7, .box_h = 1, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 454, .adv_w = 134, .box_w = 3, .box_h = 2, .ofs_x = 2, .ofs_y = 9},
    {.bitmap_index = 455, .adv_w = 134, .box_w = 6, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 461, .adv_w = 134, .box_w = 6, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 469, .adv_w = 134, .box_w = 6, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 475, .adv_w = 134, .box_w = 6, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 483, .adv_w = 134, .box_w = 6, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 489, .adv_w = 134, .box_w = 7, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 498, .adv_w = 134, .box_w = 6, .box_h = 11, .ofs_x = 1, .ofs_y = -3},
    {.bitmap_index = 507, .adv_w = 134, .box_w = 6, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 515, .adv_w = 134, .box_w = 7, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 525, .adv_w = 134, .box_w = 5, .box_h = 14, .ofs_x = 1, .ofs_y = -3},
    {.bitmap_index = 534, .adv_w = 134, .box_w = 6, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 542, .adv_w = 134, .box_w = 8, .box_h = 10, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 552, .adv_w = 134, .box_w = 7, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 559, .adv_w = 134, .box_w = 6, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 565, .adv_w = 134, .box_w = 6, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 571, .adv_w = 134, .box_w = 6, .box_h = 11, .ofs_x = 1, .ofs_y = -3},
    {.bitmap_index = 580, .adv_w = 134, .box_w = 6, .box_h = 11, .ofs_x = 1, .ofs_y = -3},
    {.bitmap_index = 589, .adv_w = 134, .box_w = 6, .box_h = 8, .ofs_x = 2, .ofs_y = 0},
    {.bitmap_index = 595, .adv_w = 134, .box_w = 6, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 601, .adv_w = 134, .box_w = 7, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 610, .adv_w = 134, .box_w = 6, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 616, .adv_w = 134, .box_w = 7, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 623, .adv_w = 134, .box_w = 8, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 631, .adv_w = 134, .box_w = 7, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 638, .adv_w = 134, .box_w = 7, .box_h = 11, .ofs_x = 1, .ofs_y = -3},
    {.bitmap_index = 648, .adv_w = 134, .box_w = 6, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 654, .adv_w = 134, .box_w = 6, .box_h = 13, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 664, .adv_w = 134, .box_w = 1, .box_h = 14, .ofs_x = 4, .ofs_y = -2},
    {.bitmap_index = 666, .adv_w = 134, .box_w = 6, .box_h = 13, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 676, .adv_w = 134, .box_w = 6, .box_h = 3, .ofs_x = 1, .ofs_y = 4}
};

/*---------------------
 *  CHARACTER MAPPING
 *--------------------*/



/*Collect the unicode lists and glyph_id offsets*/
static const lv_font_fmt_txt_cmap_t cmaps[] =
{
    {
        .range_start = 32, .range_length = 95, .glyph_id_start = 1,
        .unicode_list = NULL, .glyph_id_ofs_list = NULL, .list_length = 0, .type = LV_FONT_FMT_TXT_CMAP_FORMAT0_TINY
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
    .bpp = 1,
    .kern_classes = 0,
    .bitmap_format = 0,
#if LVGL_VERSION_MAJOR == 8
    .cache = &cache
#endif

};

extern const lv_font_t lv_font_montserrat_14;


/*-----------------
 *  PUBLIC FONT
 *----------------*/

/*Initialize a public general font descriptor*/
#if LVGL_VERSION_MAJOR >= 8
const lv_font_t jbm14 = {
#else
lv_font_t jbm14 = {
#endif
    .get_glyph_dsc = lv_font_get_glyph_dsc_fmt_txt,    /*Function pointer to get glyph's data*/
    .get_glyph_bitmap = lv_font_get_bitmap_fmt_txt,    /*Function pointer to get glyph's bitmap*/
    .line_height = 16,          /*The maximum line height required by the font*/
    .base_line = 3,             /*Baseline measured from the bottom of the line*/
#if !(LVGL_VERSION_MAJOR == 6 && LVGL_VERSION_MINOR == 0)
    .subpx = LV_FONT_SUBPX_NONE,
#endif
#if LV_VERSION_CHECK(7, 4, 0) || LVGL_VERSION_MAJOR >= 8
    .underline_position = -2,
    .underline_thickness = 1,
#endif
    //.static_bitmap = 0,
    .dsc = &font_dsc,          /*The custom font data. Will be accessed by `get_glyph_bitmap/dsc` */
#if LV_VERSION_CHECK(8, 2, 0) || LVGL_VERSION_MAJOR >= 9
    .fallback = &lv_font_montserrat_14,
#endif
    .user_data = NULL,
};



#endif /*#if JBM14*/
