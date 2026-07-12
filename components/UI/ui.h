#ifndef __UI_H__
#define __UI_H__

#include "lvgl.h"

extern lv_obj_t *g_ta;
extern lv_obj_t *input_cnt_left;
extern uint8_t input_remaining_chars;

void ui_init(void);
void lvgl_flush_cb(lv_display_t * disp, const lv_area_t * area, uint8_t * px_map);
void lvgl_tick_task(void *arg);
void create_ui(void);
void chat_add_message(const char *time, const char *text);
void ui_show_chat_page(void);
void ui_show_menu_page(void);
void ui_show_settings_page(void);
void show_toast_dialog(const char *msg, uint32_t auto_close_ms);

#endif