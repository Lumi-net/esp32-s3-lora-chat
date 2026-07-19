/*******************************************************************************
 * Size: 12 px
 * Bpp: 1
 * Opts: --bpp 1 --size 12 --no-compress --stride 1 --align 1 --font JetBrainsMonoNL-Regular.ttf --range 32-127 --format lvgl -o jbm12.c
 ******************************************************************************/
#include "jbm12.h"
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



#ifndef JBM12
#define JBM12 1
#endif

#if JBM12

/*-----------------
 *    BITMAPS
 *----------------*/

/*Store the image of the glyphs*/
static LV_ATTRIBUTE_LARGE_CONST const uint8_t glyph_bitmap[] = {
    /* U+0020 " " */
    0x0,

    /* U+0021 "!" */
    0xaa, 0xa0, 0xc0,

    /* U+0022 "\"" */
    0x99, 0x99,

    /* U+0023 "#" */
    0x24, 0x49, 0xf9, 0x22, 0x9f, 0x92, 0x24, 0x48,

    /* U+0024 "$" */
    0x21, 0x1d, 0x5a, 0xd1, 0xc7, 0x2d, 0x7e, 0x42,
    0x0,

    /* U+0025 "%" */
    0xe3, 0x4a, 0xa7, 0x41, 0x7, 0xcc, 0xa9, 0x9e,

    /* U+0026 "&" */
    0x30, 0x91, 0x1, 0x6, 0x12, 0xa2, 0x4a, 0x74,

    /* U+0027 "'" */
    0xf0,

    /* U+0028 "(" */
    0x13, 0x48, 0x88, 0x88, 0x88, 0x43, 0x10,

    /* U+0029 ")" */
    0x8c, 0x21, 0x11, 0x11, 0x11, 0x2c, 0x80,

    /* U+002A "*" */
    0x10, 0x23, 0xf9, 0x82, 0x8d, 0x80,

    /* U+002B "+" */
    0x20, 0x82, 0x3f, 0x20, 0x80,

    /* U+002C "," */
    0xf0,

    /* U+002D "-" */
    0xf0,

    /* U+002E "." */
    0xf0,

    /* U+002F "/" */
    0x8, 0x44, 0x21, 0x10, 0x8c, 0x42, 0x31, 0x0,

    /* U+0030 "0" */
    0x74, 0x63, 0x1a, 0xc6, 0x31, 0x70,

    /* U+0031 "1" */
    0x65, 0x8, 0x42, 0x10, 0x84, 0xf8,

    /* U+0032 "2" */
    0x74, 0x62, 0x11, 0x19, 0x98, 0xf8,

    /* U+0033 "3" */
    0xf8, 0xcc, 0x61, 0x86, 0x31, 0x70,

    /* U+0034 "4" */
    0x11, 0x88, 0x8c, 0xc7, 0xe1, 0x8,

    /* U+0035 "5" */
    0xfc, 0x21, 0xe0, 0x86, 0x31, 0x70,

    /* U+0036 "6" */
    0x10, 0x82, 0x1e, 0xce, 0x18, 0x73, 0x78,

    /* U+0037 "7" */
    0xfe, 0x20, 0x86, 0x10, 0x42, 0x8, 0x60,

    /* U+0038 "8" */
    0x72, 0x28, 0xa2, 0x72, 0x38, 0x61, 0x78,

    /* U+0039 "9" */
    0x7b, 0x38, 0x61, 0xcd, 0xe1, 0x4, 0x20,

    /* U+003A ":" */
    0xf0, 0x3c,

    /* U+003B ";" */
    0xf0, 0x3a, 0x80,

    /* U+003C "<" */
    0x1b, 0x21, 0x83, 0x4,

    /* U+003D "=" */
    0xf8, 0x1, 0xf0,

    /* U+003E ">" */
    0xc3, 0x86, 0x36, 0x40,

    /* U+003F "?" */
    0xf0, 0x42, 0x37, 0x20, 0x0, 0x60,

    /* U+0040 "@" */
    0x7b, 0x38, 0x6f, 0xa6, 0x9a, 0x66, 0x83, 0x7,
    0x0,

    /* U+0041 "A" */
    0x21, 0xc5, 0x14, 0x53, 0x2f, 0xa2, 0x88,

    /* U+0042 "B" */
    0xf4, 0x63, 0x1f, 0x46, 0x31, 0xf0,

    /* U+0043 "C" */
    0x74, 0x63, 0x8, 0x42, 0x31, 0x70,

    /* U+0044 "D" */
    0xf4, 0x63, 0x18, 0xc6, 0x31, 0xf0,

    /* U+0045 "E" */
    0xfc, 0x21, 0xf, 0xc2, 0x10, 0xf8,

    /* U+0046 "F" */
    0xfc, 0x21, 0xf, 0xc2, 0x10, 0x80,

    /* U+0047 "G" */
    0x74, 0x61, 0xb, 0xc6, 0x31, 0x70,

    /* U+0048 "H" */
    0x8c, 0x63, 0x1f, 0xc6, 0x31, 0x88,

    /* U+0049 "I" */
    0xf9, 0x8, 0x42, 0x10, 0x84, 0xf8,

    /* U+004A "J" */
    0x8, 0x42, 0x10, 0x86, 0x31, 0x70,

    /* U+004B "K" */
    0x8a, 0x29, 0x24, 0xe2, 0x49, 0x22, 0x88,

    /* U+004C "L" */
    0x84, 0x21, 0x8, 0x42, 0x10, 0xf8,

    /* U+004D "M" */
    0xde, 0xf7, 0xda, 0xc6, 0x31, 0x88,

    /* U+004E "N" */
    0xce, 0x73, 0x5a, 0xd6, 0x73, 0x98,

    /* U+004F "O" */
    0x74, 0x63, 0x18, 0xc6, 0x31, 0x70,

    /* U+0050 "P" */
    0xf4, 0x63, 0x1f, 0x42, 0x10, 0x80,

    /* U+0051 "Q" */
    0x74, 0x63, 0x18, 0xc6, 0x31, 0x70, 0x82,

    /* U+0052 "R" */
    0xf4, 0x63, 0x1f, 0x4a, 0x53, 0x88,

    /* U+0053 "S" */
    0x74, 0x61, 0x87, 0x6, 0x31, 0x70,

    /* U+0054 "T" */
    0xfc, 0x82, 0x8, 0x20, 0x82, 0x8, 0x20,

    /* U+0055 "U" */
    0x8c, 0x63, 0x18, 0xc6, 0x31, 0x70,

    /* U+0056 "V" */
    0x8a, 0x28, 0xb2, 0x51, 0x45, 0x14, 0x20,

    /* U+0057 "W" */
    0x93, 0x25, 0xd3, 0xa6, 0xcd, 0x9b, 0x36, 0x6c,

    /* U+0058 "X" */
    0x44, 0xc8, 0xa1, 0xc1, 0x7, 0xa, 0x22, 0x44,

    /* U+0059 "Y" */
    0x44, 0x88, 0xa1, 0x43, 0x82, 0x4, 0x8, 0x10,

    /* U+005A "Z" */
    0xf8, 0x44, 0x62, 0x31, 0x10, 0xf8,

    /* U+005B "[" */
    0xf2, 0x49, 0x24, 0x92, 0x70,

    /* U+005C "\\" */
    0x86, 0x10, 0x86, 0x10, 0x82, 0x10, 0x82, 0x10,

    /* U+005D "]" */
    0xe4, 0x92, 0x49, 0x24, 0xf0,

    /* U+005E "^" */
    0x22, 0x94, 0x98, 0x80,

    /* U+005F "_" */
    0xf8,

    /* U+0060 "`" */
    0x90,

    /* U+0061 "a" */
    0x74, 0x42, 0xf8, 0xc5, 0xe0,

    /* U+0062 "b" */
    0x84, 0x2d, 0x98, 0xc6, 0x31, 0xf0,

    /* U+0063 "c" */
    0x74, 0x61, 0x8, 0x45, 0xc0,

    /* U+0064 "d" */
    0x8, 0x5b, 0x38, 0xc6, 0x31, 0x78,

    /* U+0065 "e" */
    0x74, 0x63, 0xf8, 0x45, 0xc0,

    /* U+0066 "f" */
    0x3c, 0x82, 0x3f, 0x20, 0x82, 0x8, 0x20,

    /* U+0067 "g" */
    0x7c, 0x63, 0x18, 0xbc, 0x21, 0x70,

    /* U+0068 "h" */
    0x84, 0x3d, 0x18, 0xc6, 0x31, 0x88,

    /* U+0069 "i" */
    0x20, 0x80, 0x38, 0x20, 0x82, 0x8, 0x23, 0xf0,

    /* U+006A "j" */
    0x11, 0xf, 0x11, 0x11, 0x11, 0x1e,

    /* U+006B "k" */
    0x84, 0x23, 0x29, 0x72, 0x53, 0x88,

    /* U+006C "l" */
    0xe0, 0x82, 0x8, 0x20, 0x82, 0x8, 0x1c,

    /* U+006D "m" */
    0xfe, 0x9a, 0x69, 0xa6, 0x9a, 0x40,

    /* U+006E "n" */
    0xf4, 0x63, 0x18, 0xc6, 0x20,

    /* U+006F "o" */
    0x74, 0x63, 0x18, 0xc5, 0xc0,

    /* U+0070 "p" */
    0xf4, 0x63, 0x18, 0xc7, 0xd0, 0x80,

    /* U+0071 "q" */
    0x7c, 0x63, 0x18, 0xc5, 0xe1, 0x8,

    /* U+0072 "r" */
    0xf4, 0x63, 0x8, 0x42, 0x0,

    /* U+0073 "s" */
    0x74, 0x60, 0xe0, 0xc5, 0xc0,

    /* U+0074 "t" */
    0x20, 0x8f, 0xc8, 0x20, 0x82, 0x8, 0x1c,

    /* U+0075 "u" */
    0x8c, 0x63, 0x18, 0xc5, 0xc0,

    /* U+0076 "v" */
    0x8a, 0x2c, 0x94, 0x51, 0x42, 0x0,

    /* U+0077 "w" */
    0x92, 0xa5, 0xb3, 0x66, 0xcd, 0x9b, 0x0,

    /* U+0078 "x" */
    0x89, 0x65, 0x8, 0x51, 0x68, 0x80,

    /* U+0079 "y" */
    0x8a, 0x24, 0x94, 0x50, 0xc2, 0x8, 0x40,

    /* U+007A "z" */
    0xf8, 0xc4, 0x44, 0x63, 0xe0,

    /* U+007B "{" */
    0x19, 0x88, 0x43, 0x13, 0x4, 0x31, 0x8, 0x30,

    /* U+007C "|" */
    0xff, 0xf0,

    /* U+007D "}" */
    0xc3, 0x8, 0xc4, 0x20, 0xe8, 0x41, 0x9, 0x80,

    /* U+007E "~" */
    0x66, 0xd9, 0x80
};


/*---------------------
 *  GLYPH DESCRIPTION
 *--------------------*/

static const lv_font_fmt_txt_glyph_dsc_t glyph_dsc[] = {
    {.bitmap_index = 0, .adv_w = 0, .box_w = 0, .box_h = 0, .ofs_x = 0, .ofs_y = 0} /* id = 0 reserved */,
    {.bitmap_index = 0, .adv_w = 115, .box_w = 1, .box_h = 1, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1, .adv_w = 115, .box_w = 2, .box_h = 9, .ofs_x = 3, .ofs_y = 0},
    {.bitmap_index = 4, .adv_w = 115, .box_w = 4, .box_h = 4, .ofs_x = 2, .ofs_y = 5},
    {.bitmap_index = 6, .adv_w = 115, .box_w = 7, .box_h = 9, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 14, .adv_w = 115, .box_w = 5, .box_h = 13, .ofs_x = 1, .ofs_y = -2},
    {.bitmap_index = 23, .adv_w = 115, .box_w = 7, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 31, .adv_w = 115, .box_w = 7, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 39, .adv_w = 115, .box_w = 1, .box_h = 4, .ofs_x = 3, .ofs_y = 5},
    {.bitmap_index = 40, .adv_w = 115, .box_w = 4, .box_h = 13, .ofs_x = 2, .ofs_y = -2},
    {.bitmap_index = 47, .adv_w = 115, .box_w = 4, .box_h = 13, .ofs_x = 1, .ofs_y = -2},
    {.bitmap_index = 54, .adv_w = 115, .box_w = 7, .box_h = 6, .ofs_x = 0, .ofs_y = 2},
    {.bitmap_index = 60, .adv_w = 115, .box_w = 6, .box_h = 6, .ofs_x = 1, .ofs_y = 1},
    {.bitmap_index = 65, .adv_w = 115, .box_w = 1, .box_h = 4, .ofs_x = 3, .ofs_y = -2},
    {.bitmap_index = 66, .adv_w = 115, .box_w = 4, .box_h = 1, .ofs_x = 2, .ofs_y = 4},
    {.bitmap_index = 67, .adv_w = 115, .box_w = 2, .box_h = 2, .ofs_x = 3, .ofs_y = 0},
    {.bitmap_index = 68, .adv_w = 115, .box_w = 5, .box_h = 12, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 76, .adv_w = 115, .box_w = 5, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 82, .adv_w = 115, .box_w = 5, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 88, .adv_w = 115, .box_w = 5, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 94, .adv_w = 115, .box_w = 5, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 100, .adv_w = 115, .box_w = 5, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 106, .adv_w = 115, .box_w = 5, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 112, .adv_w = 115, .box_w = 6, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 119, .adv_w = 115, .box_w = 6, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 126, .adv_w = 115, .box_w = 6, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 133, .adv_w = 115, .box_w = 6, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 140, .adv_w = 115, .box_w = 2, .box_h = 7, .ofs_x = 3, .ofs_y = 0},
    {.bitmap_index = 142, .adv_w = 115, .box_w = 2, .box_h = 9, .ofs_x = 3, .ofs_y = -2},
    {.bitmap_index = 145, .adv_w = 115, .box_w = 5, .box_h = 6, .ofs_x = 1, .ofs_y = 1},
    {.bitmap_index = 149, .adv_w = 115, .box_w = 5, .box_h = 4, .ofs_x = 1, .ofs_y = 2},
    {.bitmap_index = 152, .adv_w = 115, .box_w = 5, .box_h = 6, .ofs_x = 1, .ofs_y = 1},
    {.bitmap_index = 156, .adv_w = 115, .box_w = 5, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 162, .adv_w = 115, .box_w = 6, .box_h = 11, .ofs_x = 1, .ofs_y = -2},
    {.bitmap_index = 171, .adv_w = 115, .box_w = 6, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 178, .adv_w = 115, .box_w = 5, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 184, .adv_w = 115, .box_w = 5, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 190, .adv_w = 115, .box_w = 5, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 196, .adv_w = 115, .box_w = 5, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 202, .adv_w = 115, .box_w = 5, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 208, .adv_w = 115, .box_w = 5, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 214, .adv_w = 115, .box_w = 5, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 220, .adv_w = 115, .box_w = 5, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 226, .adv_w = 115, .box_w = 5, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 232, .adv_w = 115, .box_w = 6, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 239, .adv_w = 115, .box_w = 5, .box_h = 9, .ofs_x = 2, .ofs_y = 0},
    {.bitmap_index = 245, .adv_w = 115, .box_w = 5, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 251, .adv_w = 115, .box_w = 5, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 257, .adv_w = 115, .box_w = 5, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 263, .adv_w = 115, .box_w = 5, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 269, .adv_w = 115, .box_w = 5, .box_h = 11, .ofs_x = 1, .ofs_y = -2},
    {.bitmap_index = 276, .adv_w = 115, .box_w = 5, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 282, .adv_w = 115, .box_w = 5, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 288, .adv_w = 115, .box_w = 6, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 295, .adv_w = 115, .box_w = 5, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 301, .adv_w = 115, .box_w = 6, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 308, .adv_w = 115, .box_w = 7, .box_h = 9, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 316, .adv_w = 115, .box_w = 7, .box_h = 9, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 324, .adv_w = 115, .box_w = 7, .box_h = 9, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 332, .adv_w = 115, .box_w = 5, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 338, .adv_w = 115, .box_w = 3, .box_h = 12, .ofs_x = 2, .ofs_y = -1},
    {.bitmap_index = 343, .adv_w = 115, .box_w = 5, .box_h = 12, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 351, .adv_w = 115, .box_w = 3, .box_h = 12, .ofs_x = 2, .ofs_y = -1},
    {.bitmap_index = 356, .adv_w = 115, .box_w = 5, .box_h = 5, .ofs_x = 1, .ofs_y = 4},
    {.bitmap_index = 360, .adv_w = 115, .box_w = 5, .box_h = 1, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 361, .adv_w = 115, .box_w = 2, .box_h = 2, .ofs_x = 2, .ofs_y = 8},
    {.bitmap_index = 362, .adv_w = 115, .box_w = 5, .box_h = 7, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 367, .adv_w = 115, .box_w = 5, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 373, .adv_w = 115, .box_w = 5, .box_h = 7, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 378, .adv_w = 115, .box_w = 5, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 384, .adv_w = 115, .box_w = 5, .box_h = 7, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 389, .adv_w = 115, .box_w = 6, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 396, .adv_w = 115, .box_w = 5, .box_h = 9, .ofs_x = 1, .ofs_y = -2},
    {.bitmap_index = 402, .adv_w = 115, .box_w = 5, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 408, .adv_w = 115, .box_w = 6, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 416, .adv_w = 115, .box_w = 4, .box_h = 12, .ofs_x = 1, .ofs_y = -2},
    {.bitmap_index = 422, .adv_w = 115, .box_w = 5, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 428, .adv_w = 115, .box_w = 6, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 435, .adv_w = 115, .box_w = 6, .box_h = 7, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 441, .adv_w = 115, .box_w = 5, .box_h = 7, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 446, .adv_w = 115, .box_w = 5, .box_h = 7, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 451, .adv_w = 115, .box_w = 5, .box_h = 9, .ofs_x = 1, .ofs_y = -2},
    {.bitmap_index = 457, .adv_w = 115, .box_w = 5, .box_h = 9, .ofs_x = 1, .ofs_y = -2},
    {.bitmap_index = 463, .adv_w = 115, .box_w = 5, .box_h = 7, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 468, .adv_w = 115, .box_w = 5, .box_h = 7, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 473, .adv_w = 115, .box_w = 6, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 480, .adv_w = 115, .box_w = 5, .box_h = 7, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 485, .adv_w = 115, .box_w = 6, .box_h = 7, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 491, .adv_w = 115, .box_w = 7, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 498, .adv_w = 115, .box_w = 6, .box_h = 7, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 504, .adv_w = 115, .box_w = 6, .box_h = 9, .ofs_x = 1, .ofs_y = -2},
    {.bitmap_index = 511, .adv_w = 115, .box_w = 5, .box_h = 7, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 516, .adv_w = 115, .box_w = 5, .box_h = 12, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 524, .adv_w = 115, .box_w = 1, .box_h = 12, .ofs_x = 3, .ofs_y = -1},
    {.bitmap_index = 526, .adv_w = 115, .box_w = 5, .box_h = 12, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 534, .adv_w = 115, .box_w = 6, .box_h = 3, .ofs_x = 1, .ofs_y = 3}
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

extern const lv_font_t lv_font_montserrat_12;


/*-----------------
 *  PUBLIC FONT
 *----------------*/

/*Initialize a public general font descriptor*/
#if LVGL_VERSION_MAJOR >= 8
const lv_font_t jbm12 = {
#else
lv_font_t jbm12 = {
#endif
    .get_glyph_dsc = lv_font_get_glyph_dsc_fmt_txt,    /*Function pointer to get glyph's data*/
    .get_glyph_bitmap = lv_font_get_bitmap_fmt_txt,    /*Function pointer to get glyph's bitmap*/
    .line_height = 13,          /*The maximum line height required by the font*/
    .base_line = 2,             /*Baseline measured from the bottom of the line*/
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
    .fallback = &lv_font_montserrat_12,
#endif
    .user_data = NULL,
};



#endif /*#if JBM12*/
