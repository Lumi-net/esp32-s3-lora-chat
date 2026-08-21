#ifndef __UI_H__
#define __UI_H__

#include "lvgl.h"
#include "types.h"

extern lv_obj_t *g_ta;
extern lv_obj_t *input_cnt_left;
extern uint8_t input_remaining_chars;
extern lv_obj_t *g_chat_scroll_container; // 提升为全局，方便外部追加消息
extern char g_chat_last_date[16];
extern uint8_t g_chat_target_id;
extern lv_obj_t *detail_title_label;
extern lv_obj_t *soc_label;
extern lv_obj_t *time_label;
extern lv_obj_t *title_label;
extern lv_obj_t *shift_icon_obj;
extern lv_obj_t *lock_icon_obj;
extern lv_obj_t *lora_status_indicator;
extern lv_obj_t *heartbeat_status_indicator;

void update_keyboard_icons(void);
void update_lora_status_indicator(void);
void update_heartbeat_status_indicator(void);

void ui_init(void);
void lvgl_flush_cb(lv_display_t * disp, const lv_area_t * area, uint8_t * px_map);
void lvgl_tick_task(void *arg);
void create_ui(void);
void chat_add_message(const char *time, const char *text);
void ui_chat_append_new_message(LoRaFrameData *frame);
void ui_show_chat_page(uint8_t target_id);
uint8_t menu_get_chat_id(void);
void ui_show_menu_page(void);
void ui_show_settings_page(void);
void show_toast_dialog(const char *msg, uint32_t auto_close_ms);
void ui_show_settings_detail_page(void);
void handle_settings_detail_enter(void);
void menu_refresh(void);
void menu_up(void);
void menu_down(void);
void settings_up(void);
void settings_down(void);

#endif