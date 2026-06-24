#include "ui.h"

#define LVGL_BUF_LINES (320 / 4) // 1/4 屏缓冲
#define LVGL_BUF_SIZE (LVGL_BUF_LINES * 240)


lv_display_t *lv_disp = NULL;
lv_obj_t *page_container = NULL;
lv_obj_t *cur_page = NULL;
lv_obj_t *chat_page = NULL;
lv_obj_t *menu_page = NULL;
lv_obj_t *g_ta = NULL;
lv_obj_t *input_cnt_left = NULL;
uint8_t input_remaining_chars = 0;
lv_obj_t *chat_cursor;

void lvgl_flush_cb(lv_display_t * disp, const lv_area_t * area, uint8_t * px_map)
{
    uint16_t x1 = area->x1, x2 = area->x2;
    uint16_t y1 = area->y1, y2 = area->y2;

    // esp_lcd 自动处理 PSRAM 的 Cache 同步，直接传入 px_map 即可
    esp_lcd_panel_draw_bitmap(panel_handle, x1, y1, x2 + 1, y2 + 1, px_map);
    
    // 必须通知 LVGL 刷新完成
    lv_display_flush_ready(disp);
}

// LVGL Tick 任务（1ms 中断或 RTOS 任务）
void lvgl_tick_task(void *arg)
{
    TickType_t last = xTaskGetTickCount();

    while (1) {
        lv_tick_inc(1);
        vTaskDelayUntil(&last, pdMS_TO_TICKS(1));
    }
}

void ui_init(void) { 
    lv_init();
    size_t buf_bytes = LVGL_BUF_SIZE * sizeof(lv_color_t);
    lv_color_t *buf1 = heap_caps_malloc(buf_bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    lv_color_t *buf2 = heap_caps_malloc(buf_bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    assert(buf1 && buf2 && "PSRAM allocation failed");

    lv_disp = lv_display_create(240, 320);
    lv_display_set_buffers(lv_disp, buf1, buf2, buf_bytes, LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(lv_disp, lvgl_flush_cb);
}

void create_ui(void) {
    lv_obj_t *scr = lv_scr_act();

    lv_coord_t screenWidth = lv_disp_get_hor_res(NULL);
    lv_coord_t screenHeight = lv_disp_get_ver_res(NULL);

    lv_obj_set_style_bg_color(
        scr,
        lv_color_hex(0x000000),
        LV_PART_MAIN
    );

    lv_obj_set_style_bg_opa(
        scr,
        LV_OPA_COVER,
        LV_PART_MAIN
    );

    /* 移除屏幕默认的滚动条和边距，确保内容能填满整个屏幕 */
    lv_obj_set_scrollbar_mode(scr, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_pad_all(scr, 0, LV_STATE_DEFAULT);
    lv_obj_set_style_pad_gap(scr, 0, LV_STATE_DEFAULT);

    /* ========== 1. 顶部状态栏（位于最上层，始终可见） ========== */
    lv_obj_t *status_bar = lv_obj_create(lv_layer_top());
    lv_obj_set_size(status_bar, screenWidth, 15); 
    lv_obj_align(status_bar, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_scrollbar_mode(status_bar, LV_SCROLLBAR_MODE_OFF);
    lv_obj_clear_flag(status_bar, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(status_bar, lv_color_hex(0x000000), LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(status_bar, LV_OPA_COVER, LV_STATE_DEFAULT);
    lv_obj_set_style_radius(status_bar, 0, LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(status_bar, 0, LV_STATE_DEFAULT);

    /* 状态栏文字：时间 */
    lv_obj_t *time_label = lv_label_create(status_bar);
    lv_label_set_text(time_label, "10:30");
    lv_obj_set_style_text_color(time_label, lv_color_white(), LV_STATE_DEFAULT);
    lv_obj_align(time_label, LV_ALIGN_LEFT_MID, 10, 0);
    lv_obj_set_style_text_font(time_label, &lv_font_montserrat_10, LV_PART_MAIN);

    /* 状态栏文字：中间标题 */
    lv_obj_t *title_label = lv_label_create(status_bar);
    lv_label_set_text(title_label, "Lumi-net");
    lv_obj_set_style_text_color(title_label, lv_color_white(), LV_STATE_DEFAULT);
    lv_obj_align(title_label, LV_ALIGN_CENTER, 0, 0);   // 水平垂直居中
    lv_obj_set_style_text_font(title_label, &lv_font_montserrat_10, LV_PART_MAIN);

    /* 状态栏文字：电池图标 */
    lv_obj_t *battery_label = lv_label_create(status_bar);
    lv_label_set_text(battery_label, LV_SYMBOL_BATTERY_FULL);
    lv_obj_set_style_text_color(battery_label, lv_color_white(), LV_STATE_DEFAULT);
    lv_obj_align(battery_label, LV_ALIGN_RIGHT_MID, -10, 0);
    lv_obj_set_style_text_font(battery_label, &lv_font_montserrat_10, LV_PART_MAIN);

    /* ========== 2. 中间列表 ========== */
    page_container = lv_obj_create(scr);

    lv_obj_set_size(page_container,
                    screenWidth,
                    screenHeight - 30 - 50);

    lv_obj_align(page_container,
                LV_ALIGN_TOP_MID,
                0,
                15);

    lv_obj_set_style_bg_opa(page_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(page_container, 0, 0);
    lv_obj_set_style_pad_all(page_container, 0, 0);
    lv_obj_clear_flag(page_container, LV_OBJ_FLAG_SCROLLABLE);


    /* ========== 3. 底部圆角输入框 ========== */
    lv_obj_t *input_area = lv_obj_create(scr);
    lv_obj_set_width(input_area, screenWidth - 20);
    lv_obj_align(input_area, LV_ALIGN_BOTTOM_MID, 0, -5);

    lv_obj_set_style_bg_opa(input_area, LV_OPA_TRANSP, LV_STATE_DEFAULT); // 总输入区域
    lv_obj_set_style_radius(input_area, 10, LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(input_area, 0, LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(input_area, 6, 0);
    lv_obj_set_height(input_area, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(input_area, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(input_area, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER);

    lv_obj_t *input_left_panel = lv_obj_create(input_area); // 左输入面板
    lv_obj_remove_style_all(input_left_panel);
    lv_obj_set_style_bg_opa(input_left_panel, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(input_left_panel, 0, LV_PART_MAIN);
    lv_obj_set_flex_flow(input_left_panel, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(input_left_panel, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_row(input_left_panel, 0, LV_STATE_DEFAULT);
    lv_obj_set_width(input_left_panel, 28);
    lv_obj_set_height(input_left_panel, LV_SIZE_CONTENT);

    lv_obj_t *input_icon_container = lv_obj_create(input_left_panel); // SHIFT和候选&LOCK
    lv_obj_remove_style_all(input_icon_container);
    lv_obj_set_style_bg_opa(input_icon_container, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(input_icon_container, 0, LV_PART_MAIN);
    lv_obj_set_flex_flow(input_icon_container, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(input_icon_container, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(input_icon_container, 6, LV_STATE_DEFAULT);
    lv_obj_set_width(input_icon_container, LV_SIZE_CONTENT); 
    lv_obj_set_height(input_icon_container, 24); 

    input_cnt_left = lv_label_create(input_left_panel); // 剩余字数显示
    lv_label_set_text_fmt(input_cnt_left, "%d", input_remaining_chars);
    lv_obj_set_style_text_color(input_cnt_left, lv_color_hex(0xffffff), LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(input_cnt_left, &lv_font_montserrat_10, LV_PART_MAIN);
    lv_obj_set_width(input_cnt_left, LV_PCT(100));
    lv_obj_set_style_text_align(input_cnt_left, LV_TEXT_ALIGN_RIGHT, LV_STATE_DEFAULT);
    lv_obj_set_height(input_cnt_left, 10); 
    lv_obj_set_height(input_cnt_left, LV_SIZE_CONTENT);

    lv_obj_t *shift_icon = lv_image_create(input_icon_container); // SHIFT
    lv_image_set_src(shift_icon, &SHIFT);
    // lv_obj_align(shift_icon, LV_ALIGN_BOTTOM_MID, 0, 0); 
    lv_obj_t *lock_icon = lv_image_create(input_icon_container); // LOCK
    lv_image_set_src(lock_icon, &LOCK);
    // lv_obj_align(lock_icon, LV_ALIGN_BOTTOM_MID, 0, 0); 


    /* 在容器内创建一个文本区域作为真实的输入框 */
    g_ta = lv_textarea_create(input_area);
    lv_textarea_set_placeholder_text(g_ta, "Please Input...");
    lv_textarea_set_one_line(g_ta, false);
    lv_obj_set_style_bg_color(g_ta, lv_color_hex(0x666666), LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(g_ta, LV_OPA_COVER, LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(g_ta, 0, LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(g_ta, lv_color_white(), LV_STATE_DEFAULT);
    lv_obj_set_style_radius(g_ta, 8, LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(g_ta, &lv_font_montserrat_12, LV_PART_MAIN);
    lv_obj_set_style_bg_color(g_ta, lv_color_white(), LV_PART_CURSOR);
    lv_obj_set_style_bg_opa(g_ta, LV_OPA_COVER, LV_PART_CURSOR);
    lv_obj_set_style_pad_top(g_ta, 4, 0);
    lv_obj_set_style_pad_bottom(g_ta, 4, 0);
    lv_obj_set_style_pad_left(g_ta, 10, LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(g_ta, 8, LV_STATE_DEFAULT);
    lv_obj_set_height(g_ta, LV_SIZE_CONTENT);
    lv_obj_set_style_min_height(g_ta, 40, LV_STATE_DEFAULT);
    lv_obj_set_flex_grow(g_ta, 1);
    lv_obj_set_style_text_align(g_ta, LV_TEXT_ALIGN_LEFT, LV_STATE_DEFAULT);
    lv_obj_add_state(g_ta, LV_STATE_FOCUSED);
}

void chat_add_message(const char *time, const char *text)
{
    lv_obj_t *item = lv_obj_create(chat_page);
    lv_obj_remove_style_all(item);

    /* 整行宽度 */
    lv_obj_set_width(item, lv_pct(100));

    /* 横向布局 */
    lv_obj_set_layout(item, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(item, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(item,
                          LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_START);

    /* ---------- 时间 ---------- */

    lv_obj_t *time_label = lv_label_create(item);

    lv_label_set_text(time_label, time);

    lv_obj_set_width(time_label, 38);      // 固定宽度，根据字体调整

    lv_obj_set_style_text_align(time_label,
                                LV_TEXT_ALIGN_LEFT,
                                0);

    lv_obj_set_style_text_color(time_label,
                                lv_palette_main(LV_PALETTE_GREY),
                                0);

    lv_obj_set_style_text_font(
        time_label,
        &lv_font_montserrat_10,
        0);

    /* ---------- 内容 ---------- */

    lv_obj_t *msg_label = lv_label_create(item);

    lv_label_set_text(msg_label, text);

    lv_label_set_long_mode(msg_label, LV_LABEL_LONG_WRAP);

    /* 剩余宽度全部给内容 */
    lv_obj_set_flex_grow(msg_label, 1);

    lv_obj_set_style_text_color(msg_label,
                                lv_color_white(),
                                0);

    lv_obj_set_style_text_font(
        msg_label,
        &lv_font_montserrat_12,
        0);

    lv_obj_scroll_to_view(item, LV_ANIM_OFF);
}

void ui_show_chat_page(void)
{
    if(cur_page)
        lv_obj_delete(cur_page);

    cur_page = lv_obj_create(page_container);

    lv_obj_remove_style_all(cur_page);

    lv_obj_set_size(cur_page,
                    lv_pct(100),
                    lv_pct(100));

    chat_page = cur_page;

    lv_obj_set_style_pad_all(chat_page, 4, 0);

    lv_obj_set_scrollbar_mode(chat_page,
                              LV_SCROLLBAR_MODE_AUTO);

    lv_obj_set_layout(chat_page,
                      LV_LAYOUT_FLEX);

    lv_obj_set_flex_flow(chat_page,
                         LV_FLEX_FLOW_COLUMN);

    lv_obj_set_flex_align(chat_page,
                          LV_FLEX_ALIGN_END,
                          LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_START);
}

void ui_show_menu_page(void)
{
    if(cur_page)
        lv_obj_delete(cur_page);

    
    cur_page = lv_obj_create(page_container);

    lv_obj_remove_style_all(cur_page);
    lv_obj_set_size(cur_page, lv_pct(100), lv_pct(100));

    menu_page = cur_page;

    chat_cursor = lv_label_create(menu_page);
    lv_label_set_text(chat_cursor, LV_SYMBOL_RIGHT);

    lv_obj_set_style_text_color(chat_cursor,
                                lv_color_white(),
                                0);

    lv_obj_set_style_text_font(chat_cursor,
                               &lv_font_montserrat_12,
                               0);

    item_cnt = sizeof(chat_list) / sizeof(chat_list[0]);

    for(int i = 0; i < item_cnt; i++)
    {
        items[i] = lv_label_create(menu_page);

        lv_label_set_text(items[i], chat_list[i]);

        lv_obj_set_style_text_color(items[i],
                                    lv_color_white(),
                                    0);

        lv_obj_set_style_text_font(items[i],
                                   &lv_font_montserrat_14,
                                   0);

        lv_obj_align(items[i],
                     LV_ALIGN_TOP_LEFT,
                     28,
                     10 + i * 24);
    }

    selected = 0;

    lv_obj_align_to(cursor,
                    items[0],
                    LV_ALIGN_OUT_LEFT_MID,
                    -8,
                    0);
}