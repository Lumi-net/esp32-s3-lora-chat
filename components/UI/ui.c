#include "ui.h"

#include "esp_log.h"
#include "esp_random.h"
#include "key.h"
#include "lcd.h"
#include "va.h"
#include "wifi.h"
#include "esp_wifi.h"
#include "mynvs.h"
#include "flash.h"
#include "jbm10.h"
#include "jbm12.h"
#include "jbm14.h"
#include <string.h>
#include <errno.h>
#include <inttypes.h>
#include "esp_system.h"

#define LVGL_BUF_LINES (320 / 4) // 1/4 屏缓冲
#define LVGL_BUF_SIZE (LVGL_BUF_LINES * 240)
#define MENU_VISIBLE_ITEMS 5
#define SETTINGS_VISIBLE_ITEMS 11

// 聊天页面配置
#define CHAT_INIT_LOAD_COUNT 30   // 初始加载消息数（预加载超出屏幕）
#define CHAT_LOAD_MORE_COUNT 10   // 每次向上滑动加载的数量
#define CHAT_MAX_UI_ITEMS 50      // 界面上保留的最大消息数（超出则删除旧消息）
#define CHAT_SCROLL_THRESHOLD 50  // 触发加载的滚动阈值

typedef enum {
    DETAIL_STEP_INPUT_ID,
    DETAIL_STEP_INPUT_ALIAS,
    DETAIL_STEP_INPUT_USER_COLOR,
    DETAIL_STEP_INPUT_INTERFACE_COLOR,
    DETAIL_STEP_INPUT_SSID,
    DETAIL_STEP_INPUT_PASSWORD,
    DETAIL_STEP_DEL_CONFIRM,
    DETAIL_STEP_CLEAR_FLASH_CONFIRM,
    DETAIL_STEP_CLEAR_NVS_CONFIRM
} detail_step_t;

// 聊天页面状态管理（静态变量，仅当前聊天页有效）
static chat_cursor_t chat_cursor;
uint8_t g_chat_target_id = 0xFF;
static bool chat_is_loading = false; // 防止滚动时重复触发加载
lv_obj_t *g_chat_scroll_container = NULL; // 提升为全局，方便外部追加消息
char g_chat_last_date[16] = {0};          // 记录当前UI显示的最后一个日期

lv_display_t *lv_disp = NULL;
lv_obj_t *page_container = NULL;
lv_obj_t *cur_page = NULL;
// lv_obj_t *chat_page = NULL;
// lv_obj_t *menu_page = NULL;
lv_obj_t *g_ta = NULL;
lv_obj_t *input_cnt_left = NULL;
uint8_t input_remaining_chars = 120;
lv_obj_t *page_cursor = NULL;
lv_obj_t *lora_status_indicator = NULL;
lv_obj_t *shift_icon_obj = NULL;
lv_obj_t *lock_icon_obj = NULL;
lv_obj_t *soc_label = NULL;
lv_obj_t *time_label = NULL;
lv_obj_t *title_label = NULL;
uint8_t now_page = 0;
static lv_obj_t *g_current_toast = NULL;

lv_obj_t *alias_labels[MENU_VISIBLE_ITEMS];
lv_obj_t *id_labels[MENU_VISIBLE_ITEMS];
lv_obj_t *online_labels[MENU_VISIBLE_ITEMS];
lv_obj_t *time_labels[MENU_VISIBLE_ITEMS];
lv_obj_t *cursor_labels[MENU_VISIBLE_ITEMS];

uint8_t valid_id_map[256]; 
uint8_t valid_cnt = 0; 
uint8_t selected = 0;
uint8_t first_visible = 0;

static detail_step_t current_detail_step = DETAIL_STEP_INPUT_ID;
static uint8_t target_change_id = 0xFF; // 用于暂存步骤 1 中验证通过的 ID
static char target_wifi_ssid[33] = {0};
static char confirm_code[5] = {0}; // 确认操作随机码

// 详情页的 UI 对象指针 (供外部按键函数访问)
lv_obj_t *detail_title_label = NULL;
static lv_obj_t *detail_hint_label = NULL;

static const char *settings_items[] = {
    "Change Alias",
    "Change User Color",
    "Change Interface Color",
    "Wi-Fi (For Time)",
    "Time",
    "Auto Sleep Time",
    "Change Brightness",
    "Customize Title",
    "Add/Remove a contact",
    "Clear Chat History",
    "Clear NVS Data",
    "-- Placeholder C --",
};

#define SETTINGS_COUNT (sizeof(settings_items) / sizeof(settings_items[0]))

// 用于存储每个设置项的 Label 对象指针，方便后续移动光标
static lv_obj_t *settings_labels[SETTINGS_VISIBLE_ITEMS];

static bool parse_uint16(const char *str, void *out, uint8_t base, uint8_t type)
{
    if (str == NULL || *str == '\0')
        return false;

    char *end;
    errno = 0;

    unsigned long value = strtoul(str, &end, base);

    // 溢出 unsigned long
    if (errno == ERANGE)
        return false;

    // 没有读到任何数字
    if (end == str)
        return false;

    // 后面还有其他字符
    if (*end != '\0')
        return false;

    // 超出范围
    if (type == 8) {
        if (value > UINT8_MAX)
            return false;
    } else if (type == 16) {
        if (value > UINT16_MAX)
            return false;
    }

    if (type == 8) {
        *(uint8_t *)out = (uint8_t)value;
    } else if (type == 16) {
        *(uint16_t *)out = (uint16_t)value;
    }
    return true;
}

void lvgl_flush_cb(lv_display_t * disp, const lv_area_t * area, uint8_t * px_map)
{
    uint16_t x1 = area->x1, x2 = area->x2;
    uint16_t y1 = area->y1, y2 = area->y2;

    // esp_lcd 自动处理 PSRAM 的 Cache 同步，直接传入 px_map 即可
    esp_lcd_panel_draw_bitmap(panel_handle, x1, y1, x2 + 1, y2 + 1, px_map);
    
    // 必须通知 LVGL 刷新完成
    lv_display_flush_ready(disp);
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
    lv_obj_t *scr = lv_screen_active();

    lv_obj_clean(scr); 

    lv_coord_t screenWidth = lv_display_get_horizontal_resolution(NULL);
    lv_coord_t screenHeight = lv_display_get_vertical_resolution(NULL);

    lv_obj_set_style_bg_color(
        scr,
        lv_color_hex(color_index[COLOR_BACKGROUND].color),
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
    lv_obj_remove_flag(status_bar, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(status_bar, lv_color_hex(color_index[COLOR_STATUS_BAR].color), LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(status_bar, LV_OPA_COVER, LV_STATE_DEFAULT);
    lv_obj_set_style_radius(status_bar, 0, LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(status_bar, 0, LV_STATE_DEFAULT);

    /* 状态栏文字：时间 */
    time_label = lv_label_create(status_bar);
    lv_label_set_text(time_label, "10:30");
    lv_obj_set_style_text_color(time_label, lv_color_hex(color_index[COLOR_MSG_TEXT].color), LV_STATE_DEFAULT);
    lv_obj_align(time_label, LV_ALIGN_LEFT_MID, 10, 0);
    lv_obj_set_style_text_font(time_label, &jbm10, LV_PART_MAIN);

    /* 状态栏文字：中间标题 */
    title_label = lv_label_create(status_bar);
    lv_label_set_text(title_label, status_title);
    lv_obj_set_style_text_color(title_label, lv_color_hex(color_index[COLOR_MSG_TEXT].color), LV_STATE_DEFAULT);
    lv_obj_align(title_label, LV_ALIGN_CENTER, 0, 0);   // 水平垂直居中
    lv_obj_set_style_text_font(title_label, &jbm10, LV_PART_MAIN);

    /* 状态栏：LoRa 状态指示小方块 */
    lora_status_indicator = lv_obj_create(status_bar);
    lv_obj_remove_style_all(lora_status_indicator);
    lv_obj_set_size(lora_status_indicator, 8, 8);
    lv_obj_set_style_radius(lora_status_indicator, 1, 0);
    lv_obj_set_style_bg_opa(lora_status_indicator, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(lora_status_indicator, lv_palette_main(LV_PALETTE_GREEN), 0);
    lv_obj_align(lora_status_indicator, LV_ALIGN_RIGHT_MID, -48, 0);

    /* 状态栏文字：电池图标 */
    soc_label = lv_label_create(status_bar);
    lv_label_set_text(soc_label, "100%");
    lv_obj_set_style_text_color(soc_label, lv_color_hex(color_index[COLOR_MSG_TEXT].color), LV_STATE_DEFAULT);
    lv_obj_align(soc_label, LV_ALIGN_RIGHT_MID, -10, 0);
    lv_obj_set_style_text_font(soc_label, &jbm10, LV_PART_MAIN);

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
    lv_obj_remove_flag(page_container, LV_OBJ_FLAG_SCROLLABLE);


    /* ========== 3. 底部圆角输入框 ========== */
    lv_obj_t *input_area = lv_obj_create(scr);
    lv_obj_set_width(input_area, screenWidth - 6);
    lv_obj_align(input_area, LV_ALIGN_BOTTOM_MID, 0, -3);

    lv_obj_set_style_bg_opa(input_area, LV_OPA_TRANSP, LV_STATE_DEFAULT); // 总输入区域
    lv_obj_set_style_radius(input_area, 10, LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(input_area, 0, LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(input_area, 2, 0);
    lv_obj_set_height(input_area, LV_SIZE_CONTENT);
    lv_obj_set_style_min_height(input_area, 48, LV_STATE_DEFAULT);
    lv_obj_set_flex_flow(input_area, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(input_area, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_END);

    lv_obj_t *input_left_panel = lv_obj_create(input_area); // 左输入面板
    lv_obj_remove_style_all(input_left_panel);
    lv_obj_set_style_bg_opa(input_left_panel, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(input_left_panel, 0, LV_PART_MAIN);
    lv_obj_set_flex_flow(input_left_panel, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(input_left_panel, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_row(input_left_panel, 0, LV_STATE_DEFAULT);
    lv_obj_set_width(input_left_panel, 48);
    lv_obj_set_height(input_left_panel, 36);

    lv_obj_t *input_icon_container = lv_obj_create(input_left_panel);
    lv_obj_set_style_bg_opa(input_icon_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(input_icon_container, 0, 0);
    lv_obj_set_style_pad_all(input_icon_container, 0, 0);
    lv_obj_set_size(input_icon_container, 48, 22);
    lv_obj_set_scrollbar_mode(input_icon_container, LV_SCROLLBAR_MODE_OFF);

    input_cnt_left = lv_label_create(input_left_panel);
    lv_label_set_text_fmt(input_cnt_left, "%d", input_remaining_chars);
    lv_obj_set_style_text_color(input_cnt_left, lv_color_hex(color_index[COLOR_MSG_TEXT].color), LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(input_cnt_left, &jbm10, LV_PART_MAIN);
    lv_obj_set_width(input_cnt_left, LV_PCT(100));
    lv_obj_set_style_text_align(input_cnt_left, LV_TEXT_ALIGN_RIGHT, LV_STATE_DEFAULT);
    lv_obj_set_height(input_cnt_left, 10); 

    shift_icon_obj = lv_obj_create(input_icon_container);
    lv_obj_remove_style_all(shift_icon_obj);
    lv_obj_set_size(shift_icon_obj, 22, 22);
    lv_obj_set_style_border_width(shift_icon_obj, 1, 0);
    lv_obj_set_style_border_opa(shift_icon_obj, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(shift_icon_obj, lv_color_hex(color_index[COLOR_MSG_TEXT].color), 0);
    lv_obj_set_style_radius(shift_icon_obj, 3, 0);
    lv_obj_set_style_bg_opa(shift_icon_obj, LV_OPA_TRANSP, 0);
    lv_obj_align(shift_icon_obj, LV_ALIGN_LEFT_MID, 0, 0);
    lv_obj_t *shift_l = lv_label_create(shift_icon_obj);
    lv_label_set_text(shift_l, "S");
    lv_obj_set_style_text_font(shift_l, &jbm10, 0);
    lv_obj_set_style_text_color(shift_l, lv_color_hex(color_index[COLOR_MSG_TEXT].color), 0);
    lv_obj_center(shift_l);
    lv_obj_set_style_bg_opa(shift_l, LV_OPA_TRANSP, 0);

    lock_icon_obj = lv_obj_create(input_icon_container);
    lv_obj_remove_style_all(lock_icon_obj);
    lv_obj_set_size(lock_icon_obj, 22, 22);
    lv_obj_set_style_border_width(lock_icon_obj, 1, 0);
    lv_obj_set_style_border_opa(lock_icon_obj, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(lock_icon_obj, lv_color_hex(color_index[COLOR_MSG_TEXT].color), 0);
    lv_obj_set_style_radius(lock_icon_obj, 3, 0);
    lv_obj_set_style_bg_opa(lock_icon_obj, LV_OPA_TRANSP, 0);
    lv_obj_align(lock_icon_obj, LV_ALIGN_RIGHT_MID, 0, 0);
    lv_obj_t *lock_l = lv_label_create(lock_icon_obj);
    lv_label_set_text(lock_l, "?");
    lv_obj_set_style_text_font(lock_l, &jbm10, 0);
    lv_obj_set_style_text_color(lock_l, lv_color_hex(color_index[COLOR_MSG_TEXT].color), 0);
    lv_obj_center(lock_l);
    lv_obj_set_style_bg_opa(lock_l, LV_OPA_TRANSP, 0);

    update_keyboard_icons();


    /* 在容器内创建一个文本区域作为真实的输入框 */
    g_ta = lv_textarea_create(input_area);
    lv_textarea_set_placeholder_text(g_ta, "Please Input...");
    lv_textarea_set_one_line(g_ta, false);
    lv_obj_set_style_bg_color(g_ta, lv_color_hex(color_index[COLOR_TEXTAREA_BACKGROUND].color), LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(g_ta, LV_OPA_COVER, LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(g_ta, 0, LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(g_ta, lv_color_hex(color_index[COLOR_MSG_TEXT].color), LV_STATE_DEFAULT);
    lv_obj_set_style_radius(g_ta, 8, LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(g_ta, &jbm12, LV_PART_MAIN);
    lv_obj_set_style_bg_color(g_ta,lv_color_hex(color_index[COLOR_MSG_TEXT].color), LV_PART_CURSOR);
    lv_obj_set_style_bg_opa(g_ta, LV_OPA_COVER, LV_PART_CURSOR);
    lv_obj_set_style_pad_top(g_ta, 2, 0);
    lv_obj_set_style_pad_bottom(g_ta, 2, 0);
    lv_obj_set_style_pad_left(g_ta, 6, LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(g_ta, 2, LV_STATE_DEFAULT);
    lv_obj_set_height(g_ta, LV_SIZE_CONTENT);
    lv_obj_set_style_min_height(g_ta, 40, LV_STATE_DEFAULT);
    lv_obj_set_style_max_height(g_ta, 100, LV_STATE_DEFAULT);
    lv_obj_set_flex_grow(g_ta, 1);
    lv_obj_set_style_text_align(g_ta, LV_TEXT_ALIGN_LEFT, LV_STATE_DEFAULT);
    lv_obj_add_state(g_ta, LV_STATE_FOCUSED);
}

static void ui_input_clear(void) {
    lv_textarea_set_text(g_ta, "");
    input_remaining_chars = 120;
    lv_label_set_text_fmt(input_cnt_left, "%d", input_remaining_chars);
}

static void deferred_show_settings_page(lv_timer_t * timer) {
    lv_textarea_set_placeholder_text(g_ta, "Please Input...");
    ui_input_clear();
    ui_show_settings_page();
    lv_timer_delete(timer);
}

static void toast_auto_close_cb(lv_timer_t * timer)
{
    if (g_current_toast != NULL) {
        lv_msgbox_close(g_current_toast); // 关闭弹窗及背景
        g_current_toast = NULL;           // 清空指针
    }
    lv_timer_delete(timer); // 销毁定时器
}

// 自动消失弹窗函数
void show_toast_dialog(const char *msg, uint32_t auto_close_ms) 
{
    // 1. 防止重复创建：如果上一个弹窗还没消失，先强制关掉它
    if (g_current_toast != NULL) {
        lv_msgbox_close(g_current_toast);
        g_current_toast = NULL;
    }

    // 2. 创建新的弹窗
    g_current_toast = lv_msgbox_create(lv_screen_active());
    lv_msgbox_add_title(g_current_toast, "提示");
    lv_obj_add_flag(lv_msgbox_get_header(g_current_toast), LV_OBJ_FLAG_HIDDEN);
    lv_msgbox_add_text(g_current_toast, msg);
    lv_obj_center(g_current_toast);

    // 3. 创建定时器（user_data 传 NULL 即可，因为我们用全局变量）
    lv_timer_create(toast_auto_close_cb, auto_close_ms, NULL);
}

void update_keyboard_icons(void)
{
    if (!shift_icon_obj || !lock_icon_obj) return;

    lv_color_t base = lv_color_hex(color_index[COLOR_MSG_TEXT].color);
    lv_color_t invert = lv_color_hex(0xFFFFFF - color_index[COLOR_MSG_TEXT].color);

    lv_obj_t *shift_l = lv_obj_get_child(shift_icon_obj, 0);
    lv_obj_t *lock_l = lv_obj_get_child(lock_icon_obj, 0);

    if (shifted) {
        lv_obj_set_style_bg_color(shift_icon_obj, base, 0);
        lv_obj_set_style_bg_opa(shift_icon_obj, LV_OPA_COVER, 0);
        lv_obj_set_style_text_color(shift_l, invert, 0);
    } else {
        lv_obj_set_style_bg_opa(shift_icon_obj, LV_OPA_TRANSP, 0);
        lv_obj_set_style_text_color(shift_l, base, 0);
    }

    if (locked) {
        lv_obj_set_style_bg_color(lock_icon_obj, base, 0);
        lv_obj_set_style_bg_opa(lock_icon_obj, LV_OPA_COVER, 0);
        lv_obj_set_style_text_color(lock_l, invert, 0);
        lv_label_set_text(lock_l, "L");
    } else if (waited_to_choose) {
        lv_obj_set_style_bg_opa(lock_icon_obj, LV_OPA_TRANSP, 0);
        lv_obj_set_style_text_color(lock_l, base, 0);
        char digit[2] = {(char)('0' + wait_choose), '\0'};
        lv_label_set_text(lock_l, digit);
    } else {
        lv_obj_set_style_bg_opa(lock_icon_obj, LV_OPA_TRANSP, 0);
        lv_obj_set_style_text_color(lock_l, base, 0);
        lv_label_set_text(lock_l, "?");
    }
}

void update_lora_status_indicator(void)
{
    if (!lora_status_indicator) return;
    lv_color_t color;
    switch (lora_status) {
        case LORA_STATUS_IDLE:
            color = lv_palette_main(LV_PALETTE_GREEN);
            break;
        case LORA_STATUS_RECEIVING:
            color = lv_palette_main(LV_PALETTE_BLUE);
            break;
        case LORA_STATUS_SENDING:
            color = lv_palette_main(LV_PALETTE_YELLOW);
            break;
        case LORA_STATUS_WAITING_ACK:
            color = lv_palette_main(LV_PALETTE_PURPLE);
            break;
        case LORA_STATUS_TIMEOUT:
            color = lv_palette_main(LV_PALETTE_RED);
            break;
        default:
            color = lv_palette_main(LV_PALETTE_GREEN);
            break;
    }
    lv_obj_set_style_bg_color(lora_status_indicator, color, 0);
}

// ==========================================
// 核心渲染：添加单条消息到容器（匹配截图布局）（使用 Recolor 实现整体左对齐）
// ==========================================
// 布局: [时间(固定宽)] [ID(颜色): 内容(自动换行)]
static void append_message_ui(lv_obj_t *container, LoRaFrameData *frame, char *last_date, bool insert_at_top)
{
    // 1. 日期处理 (格式: MM/DD)
    char current_date[16];
    snprintf(current_date, sizeof(current_date), "%02d/%02d", frame->month, frame->day);
    
    // 如果日期变了，插入日期标签
    if (strcmp(current_date, last_date) != 0) {
        lv_obj_t *date_label = lv_label_create(container);
        lv_label_set_text(date_label, current_date);
        lv_obj_set_style_text_color(date_label, lv_color_hex(color_index[COLOR_MSG_DATE].color), 0);
        lv_obj_set_style_text_font(date_label, &jbm10, 0);
        lv_obj_set_width(date_label, lv_pct(100));
        lv_obj_set_style_text_align(date_label, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_style_margin_top(date_label, 15, 0);
        lv_obj_set_style_margin_bottom(date_label, 10, 0);
        
        strcpy(last_date, current_date);
    }

    // 2. 消息行容器 (横向布局：左侧时间，右侧内容)
    lv_obj_t *msg_row = lv_obj_create(container);
    lv_obj_remove_style_all(msg_row);
    lv_obj_set_height(msg_row, LV_SIZE_CONTENT);
    lv_obj_set_width(msg_row, lv_pct(100));
    lv_obj_set_layout(msg_row, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(msg_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_row(msg_row, 4, 0);

    if (insert_at_top) {
        lv_obj_move_to_index(msg_row, 0); 
    }

    // 3. 时间标签 (固定宽度，靠左，灰色)
    lv_obj_t *time_label = lv_label_create(msg_row);
    char time_str[16];
    snprintf(time_str, sizeof(time_str), "%02d:%02d", frame->hour, frame->minute);
    lv_label_set_text(time_label, time_str);
    lv_obj_set_width(time_label, 42); // 固定宽度，确保对齐
    lv_obj_set_style_text_color(time_label, lv_color_hex(color_index[COLOR_MSG_TIME].color), 0);
    lv_obj_set_style_text_font(time_label, &jbm10, 0);
    lv_obj_set_style_pad_top(time_label, 2, 0);

    // 4. 消息内容 (Spangroup: alias 带颜色 + text 白色, 同一文本块正确折行)
    const char *alias = "Unknown";
    char unknown_alias[8];
    uint32_t acolor = 0xFFFFFF;
    if (chat_list[frame->self_id].id != 0xFF && chat_list[frame->self_id].alias[0] != '\0') {
        alias = chat_list[frame->self_id].alias;
        acolor = chat_list[frame->self_id].color;
    } else {
        snprintf(unknown_alias, sizeof(unknown_alias), "0x%02X", frame->self_id);
        alias = unknown_alias;
    }
    lv_obj_t *sg = lv_spangroup_create(msg_row);
    lv_spangroup_set_mode(sg, LV_SPAN_MODE_BREAK);
    lv_obj_set_flex_grow(sg, 1);
    lv_obj_set_style_pad_bottom(sg, 8, 0);

    lv_span_t *span_alias = lv_spangroup_add_span(sg);
    lv_span_set_text_fmt(span_alias, "%s: ", alias);
    lv_style_set_text_font(lv_span_get_style(span_alias), &jbm12);
    lv_style_set_text_color(lv_span_get_style(span_alias), lv_color_hex(acolor));

    lv_span_t *span_text = lv_spangroup_add_span(sg);
    lv_span_set_text(span_text, frame->data_str);
    lv_style_set_text_font(lv_span_get_style(span_text), &jbm12);
    lv_style_set_text_color(lv_span_get_style(span_text), lv_color_hex(color_index[COLOR_MSG_TEXT].color));

    lv_spangroup_refresh(sg);
}

// ==========================================
// 聊天页面滚动回调（懒加载与内存管理）
// ==========================================

static void chat_scroll_load_cb(void *arg) {
    lv_obj_t * container = (lv_obj_t *)arg;
    LoRaFrameData frames[CHAT_LOAD_MORE_COUNT];
    uint8_t found = 0;
    char temp_date[16] = {0};

    while (found < CHAT_LOAD_MORE_COUNT) {
        if (chat_storage_read_prev(&chat_cursor, &frames[found]) != ESP_OK) break;
        if (g_chat_target_id == 0x00) {
            if (frames[found].target_id == 0x00) found++;
        } else {
            if (frames[found].self_id == g_chat_target_id) found++;
        }
    }

    if (found > 0) {
        lv_obj_t *first_child = lv_obj_get_child(container, 0);
        lv_coord_t first_y_before = first_child ? lv_obj_get_y(first_child) : 0;

        for (int i = 0; i < found; i++) {
            append_message_ui(container, &frames[i], temp_date, true);
        }

        lv_obj_update_layout(container);
        if (first_child) {
            lv_coord_t delta = lv_obj_get_y(first_child) - first_y_before;
            lv_obj_scroll_by(container, 0, delta, LV_ANIM_OFF);
            lv_obj_scroll_to_y(container, LV_MAX(lv_obj_get_scroll_y(container), 0), LV_ANIM_OFF);
        }
    }
    chat_is_loading = false;
}

static void chat_scroll_cb(lv_event_t * e)
{
    lv_obj_t * container = lv_event_get_target(e);
    lv_coord_t scroll_y = lv_obj_get_scroll_y(container);
    lv_coord_t max_scroll = lv_obj_get_scroll_bottom(container);
    lv_coord_t container_height = lv_obj_get_height(container);

    if (scroll_y < CHAT_SCROLL_THRESHOLD && !chat_is_loading) {
        chat_is_loading = true;
        lv_async_call(chat_scroll_load_cb, container);
    }

    // 内存管理：当消息过多且用户远离顶部时，删除最旧的消息
    uint32_t child_count = lv_obj_get_child_count(container);
    if (child_count > CHAT_MAX_UI_ITEMS) {
        if (scroll_y < container_height / 3) {
            lv_obj_t *oldest = lv_obj_get_child(container, 0);
            if (oldest) lv_obj_delete(oldest);
        } else if (scroll_y > max_scroll - container_height / 3) {
            lv_obj_t *newest = lv_obj_get_child(container, -1);
            if (newest) lv_obj_delete(newest);
        }
    }
}


/**
 * @brief 收到新消息时调用，动态追加到聊天界面底部
 * @param frame 新收到的消息帧
 */
void ui_chat_append_new_message(LoRaFrameData *frame)
{
    // 1. 判断当前是否在聊天界面，且消息匹配当前对话
    bool msg_matches = (g_chat_target_id == 0x00) ? (frame->target_id == 0x00)
                                                   : (g_chat_target_id == frame->self_id);
    if (current_page_id != PAGE_CHAT || !msg_matches) {
        return;
    }

    // 2. 确保容器存在
    if (g_chat_scroll_container == NULL) return;

    // 3. 调用渲染函数，追加到最底部 (insert_at_top = false)
    append_message_ui(g_chat_scroll_container, frame, g_chat_last_date, false);

    // 4. 自动滚动到底部，确保新消息可见
    lv_obj_update_layout(g_chat_scroll_container);
    lv_obj_scroll_to_y(g_chat_scroll_container, LV_COORD_MAX, LV_ANIM_ON);
}

void ui_show_chat_page(uint8_t target_id)
{
    if (cur_page) {
        lv_obj_delete(cur_page);
        cur_page = NULL;
    }
    
    cur_page = lv_obj_create(page_container);
    lv_obj_remove_style_all(cur_page);
    lv_obj_set_size(cur_page, lv_pct(100), lv_pct(100));
    lv_obj_set_style_bg_color(cur_page, lv_color_hex(color_index[COLOR_BACKGROUND].color), 0);
    current_page_id = PAGE_CHAT;
    
    g_chat_target_id = target_id;
    chat_is_loading = false;
    g_chat_last_date[0] = '\0'; // 重置日期状态

    char title_buf[32];
    const char *alias = chat_list[target_id].alias;
    if (alias[0] == '\0') alias = "Unknown";
    snprintf(title_buf, sizeof(title_buf), "Chat | %s", alias);
    lv_obj_t *chat_title = lv_label_create(cur_page);
    lv_label_set_text(chat_title, title_buf);
    lv_obj_set_style_text_color(chat_title, lv_color_hex(color_index[COLOR_MSG_TEXT].color), 0);
    lv_obj_set_style_text_font(chat_title, &jbm14, 0);
    lv_obj_align(chat_title, LV_ALIGN_TOP_LEFT, 10, 10);

    // 创建可滚动容器
    g_chat_scroll_container = lv_obj_create(cur_page);
    lv_obj_remove_style_all(g_chat_scroll_container);
    lv_obj_set_size(g_chat_scroll_container, lv_pct(100), lv_pct(90));
    lv_obj_align(g_chat_scroll_container, LV_ALIGN_TOP_MID, 0, 30);
    lv_obj_set_layout(g_chat_scroll_container, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(g_chat_scroll_container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(g_chat_scroll_container, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_END);
    lv_obj_set_style_pad_all(g_chat_scroll_container, 8, 0);
    lv_obj_set_scrollbar_mode(g_chat_scroll_container, LV_SCROLLBAR_MODE_OFF);
    lv_obj_remove_flag(g_chat_scroll_container, LV_OBJ_FLAG_SCROLL_ELASTIC);
    
    // 绑定滚动事件
    lv_obj_add_event_cb(g_chat_scroll_container, chat_scroll_cb, LV_EVENT_SCROLL, NULL);

    // --- 核心：使用游标进行初始懒加载 ---
    chat_read_backward_init(&chat_cursor);
    
    LoRaFrameData frames[CHAT_INIT_LOAD_COUNT];
    uint8_t found = 0;
    
    while (found < CHAT_INIT_LOAD_COUNT) {
        if (chat_storage_read_prev(&chat_cursor, &frames[found]) != ESP_OK) break;
        if (target_id == 0x00) {
            if (frames[found].target_id == 0x00) found++;
        } else {
            if (frames[found].self_id == target_id) found++;
        }
    }

    if (found > 0) {
        snprintf(g_chat_last_date, sizeof(g_chat_last_date), "%02d/%02d", frames[0].month, frames[0].day);
        char last_date[16] = {0};
        for (int i = found - 1; i >= 0; i--) {
            append_message_ui(g_chat_scroll_container, &frames[i], last_date, false);
        }
        lv_obj_update_layout(g_chat_scroll_container);
        lv_obj_scroll_to_y(g_chat_scroll_container, LV_COORD_MAX, LV_ANIM_OFF);
    } else {
        lv_obj_t *empty_label = lv_label_create(g_chat_scroll_container);
        lv_label_set_text(empty_label, "No messages");
        lv_obj_set_style_text_color(empty_label, lv_color_hex(0x666666), 0);
        lv_obj_center(empty_label);
    }
}


// ==========================================
// 核心辅助函数：构建有效 ID 映射表
// ==========================================
void rebuild_valid_id_map(void) {
    valid_cnt = 0;
    // 从 0 到 255 遍历，天然保证了按 id 从小到大排序
    for (uint16_t i = 0; i < 256; i++) {
        if (chat_list[i].id != 0xFF) {
            valid_id_map[valid_cnt++] = i; // 记录有效的真实下标(即id) 并计数valid_cnt
        }
    }
}

// ==========================================
// 菜单渲染与控制逻辑
// ==========================================

void menu_refresh(void)
{
    uint8_t visible = (valid_cnt < MENU_VISIBLE_ITEMS) ? valid_cnt : MENU_VISIBLE_ITEMS; // 计算一屏显示几个
    char time_str[16];

    for(uint8_t i = 0; i < visible; i++)
    {
        uint8_t map_idx = first_visible + i;
        // 通过映射表，将“菜单连续索引”转换为“chat_list 真实下标”
        uint8_t actual_id = valid_id_map[map_idx]; 

        // 获取 alias，增加防御性处理（防止空字符串）
        const char *alias = chat_list[actual_id].alias;
        if (alias[0] == '\0') {
            alias = "Null"; // 如果没有别名，显示默认文本（事实上必须有 加联系人的时候必须输入一个）
        }
        lv_label_set_text(alias_labels[i], alias);
        
        // 设置 ID
        char id_str[8];
        snprintf(id_str, sizeof(id_str), "0x%02X", actual_id);
        lv_label_set_text(id_labels[i], id_str);

        // 设置在线
        uint32_t last_online = chat_list[actual_id].last_online;
        if (last_online > 0) {
            uint8_t month = last_online / 1000000;
            uint8_t day = (last_online / 10000) % 100;
            uint8_t hour = (last_online / 100) % 100;
            uint8_t minute = last_online % 100;
            snprintf(time_str, sizeof(time_str), "%02d/%02d %02d:%02d", month, day, hour, minute);
        } else {
            snprintf(time_str, sizeof(time_str), "Maybe Offline");
        }
        lv_label_set_text(online_labels[i], time_str);

        // 设置时间
        uint32_t last_time = chat_list[actual_id].last_time;
        if (last_time > 0) {
            uint8_t month = last_time / 1000000;
            uint8_t day = (last_time / 10000) % 100;
            uint8_t hour = (last_time / 100) % 100;
            uint8_t minute = last_time % 100;
            snprintf(time_str, sizeof(time_str), "%02d/%02d %02d:%02d", month, day, hour, minute);
        } else {
            snprintf(time_str, sizeof(time_str), "No messages");
        }
        lv_label_set_text(time_labels[i], time_str);

        lv_label_set_text(cursor_labels[i], (i == (selected - first_visible)) ? ">" : " ");
    }
}

void menu_down(void)
{
    if (selected + 1 >= valid_cnt) return;

    uint8_t old_ui = selected - first_visible;
    selected++;

    if(selected >= first_visible + MENU_VISIBLE_ITEMS)
    {
        first_visible++;
        menu_refresh();
    }
    else
    {
        lv_label_set_text(cursor_labels[old_ui], " ");
        lv_label_set_text(cursor_labels[selected - first_visible], ">");
    }
}

void menu_up(void)
{
    if(selected == 0) return;

    uint8_t old_ui = selected - first_visible;
    selected--;

    if(selected < first_visible)
    {
        first_visible--;
        menu_refresh();
    }
    else
    {
        lv_label_set_text(cursor_labels[old_ui], " ");
        lv_label_set_text(cursor_labels[selected - first_visible], ">");
    }
}

uint8_t menu_get_chat_id(void)
{
    if (valid_cnt == 0) return 0x00;
    // 直接通过映射表获取当前选中项的真实 ID
    return valid_id_map[selected]; 
}

// ==========================================
// UI 页面初始化
// ==========================================
void ui_show_menu_page(void)
{
    if (cur_page) {
        lv_obj_delete(cur_page);
        cur_page = NULL;
    }

    cur_page = lv_obj_create(page_container);

    lv_obj_remove_style_all(cur_page);
    lv_obj_set_size(cur_page, lv_pct(100), lv_pct(100));

    current_page_id = PAGE_MENU;

    // 1. 动态获取数据，构建按 ID 排序的映射表
    rebuild_valid_id_map(); 

    // 2. 处理空列表情况
    if (valid_cnt == 0) {
        lv_obj_t *empty_label = lv_label_create(cur_page);
        lv_label_set_text(empty_label, "No Contacts");
        lv_obj_set_style_text_color(empty_label, lv_color_hex(color_index[COLOR_MSG_TEXT].color), 0);
        lv_obj_center(empty_label);
        return; // 直接返回，不创建列表项
    }

    // 创建光标 (仅在存在联系人时)


    lv_obj_t *menu_title = lv_label_create(cur_page);
    lv_label_set_text(menu_title, "Menu");
    lv_obj_set_style_text_color(menu_title, lv_color_hex(color_index[COLOR_MSG_TEXT].color), 0);
    lv_obj_set_style_text_font(menu_title, &jbm14, 0);
    lv_obj_align(menu_title, LV_ALIGN_TOP_LEFT, 10, 10);

    lv_obj_t *menu_container = lv_obj_create(cur_page);
    lv_obj_remove_style_all(menu_container);
    lv_obj_set_size(menu_container, lv_pct(100), lv_pct(90));
    lv_obj_align(menu_container, LV_ALIGN_TOP_MID, 0, 30);
    lv_obj_set_style_bg_opa(menu_container, LV_OPA_TRANSP, 0);
    lv_obj_set_layout(menu_container, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(menu_container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(menu_container, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_row(menu_container, 0, 0);
    lv_obj_set_style_pad_all(menu_container, 0, 0);

    // 3. 创建列表项 UI
    uint8_t visible = (valid_cnt < MENU_VISIBLE_ITEMS) ? valid_cnt : MENU_VISIBLE_ITEMS;

    const uint16_t ITEM_HEIGHT = 30; // 每行高度
    // const uint16_t ALIAS_WIDTH = 160; // Alias 标签固定宽度
    // const uint16_t ID_WIDTH = 30;     // ID 标签固定宽度

    for(uint8_t i = 0; i < visible; i++)
    {
        // 每个联系人的行容器（垂直 Flex，分上下两排）
        lv_obj_t *row = lv_obj_create(menu_container);
        lv_obj_remove_style_all(row);
        lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
        lv_obj_set_width(row, lv_pct(100));
        lv_obj_set_height(row, ITEM_HEIGHT);
        lv_obj_set_layout(row, LV_LAYOUT_FLEX);
        lv_obj_set_flex_flow(row, LV_FLEX_FLOW_COLUMN); 

        // --- 第一排：Alias 和 ID ---
        lv_obj_t *top_row = lv_obj_create(row);
        lv_obj_remove_style_all(top_row);
        lv_obj_set_style_bg_opa(top_row, LV_OPA_TRANSP, 0);
        lv_obj_set_width(top_row, lv_pct(100));
        lv_obj_set_height(top_row, 16);
        lv_obj_set_layout(top_row, LV_LAYOUT_FLEX);
        lv_obj_set_flex_flow(top_row, LV_FLEX_FLOW_ROW);
        
        cursor_labels[i] = lv_label_create(top_row);
        lv_obj_set_width(cursor_labels[i], 20);
        lv_label_set_text(cursor_labels[i], " ");
        lv_obj_set_style_text_color(cursor_labels[i], lv_color_hex(color_index[COLOR_MSG_TEXT].color), 0);
        lv_obj_set_style_text_font(cursor_labels[i], &jbm12, 0);
        lv_obj_set_style_text_align(cursor_labels[i], LV_TEXT_ALIGN_CENTER, 0);

        alias_labels[i] = lv_label_create(top_row);
        lv_obj_set_flex_grow(alias_labels[i], 1);
        lv_label_set_long_mode(alias_labels[i], LV_LABEL_LONG_MODE_SCROLL);
        lv_label_set_text(alias_labels[i], "");
        lv_obj_set_style_text_color(alias_labels[i], lv_color_hex(color_index[COLOR_MSG_TEXT].color), 0);
        lv_obj_set_style_text_font(alias_labels[i], &jbm14, 0);

        id_labels[i] = lv_label_create(top_row);
        lv_obj_set_width(id_labels[i], 60);
        lv_obj_set_style_text_align(id_labels[i], LV_TEXT_ALIGN_RIGHT, 0);
        lv_label_set_text(id_labels[i], "");
        lv_obj_set_style_text_color(id_labels[i], lv_color_hex(color_index[COLOR_MSG_TEXT].color), 0);
        lv_obj_set_style_text_font(id_labels[i], &jbm14, 0);
        lv_obj_set_style_pad_right(id_labels[i], 4, 0);

        // --- 第二排：Online和Time (左一个右一个) ---
        lv_obj_t *bottom_row = lv_obj_create(row);
        lv_obj_remove_style_all(bottom_row);
        lv_obj_set_style_bg_opa(bottom_row, LV_OPA_TRANSP, 0);
        lv_obj_set_width(bottom_row, lv_pct(100));
        lv_obj_set_height(bottom_row, 14);
        lv_obj_set_layout(bottom_row, LV_LAYOUT_FLEX);
        lv_obj_set_flex_flow(bottom_row, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(bottom_row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER); 

        online_labels[i] = lv_label_create(bottom_row);
        lv_label_set_text(online_labels[i], "Offline"); // 初始文本
        lv_obj_set_style_text_color(online_labels[i], lv_color_hex(color_index[COLOR_MSG_TEXT].color), 0);
        lv_obj_set_style_text_font(online_labels[i], &jbm10, 0);
        lv_obj_set_style_pad_left(online_labels[i], 28, 0); // 与上面的 Alias 保持左对齐

        time_labels[i] = lv_label_create(bottom_row);
        lv_label_set_text(time_labels[i], "");
        lv_obj_set_style_text_color(time_labels[i], lv_color_hex(color_index[COLOR_MSG_TEXT].color), 0);
        lv_obj_set_style_text_font(time_labels[i], &jbm10, 0);
        lv_obj_set_style_pad_right(time_labels[i], 20, 0);
    }

    // 4. 初始化状态并刷新
    selected = 0;
    first_visible = 0;

    menu_refresh();
}

// ==========================================
// 2. 辅助函数：光标移动逻辑 (支持翻页)
// ==========================================

void settings_refresh(void) {
    uint8_t visible = (SETTINGS_COUNT < SETTINGS_VISIBLE_ITEMS) ? SETTINGS_COUNT : SETTINGS_VISIBLE_ITEMS;
    for (uint8_t i = 0; i < visible; i++) {
        uint8_t idx = first_visible + i;
        lv_label_set_text(settings_labels[i], settings_items[idx]);
    }
}

void settings_update_cursor(void) {
    if (SETTINGS_COUNT > 0 && selected < SETTINGS_COUNT) {
        uint8_t ui_idx = selected - first_visible;
        if (ui_idx < SETTINGS_VISIBLE_ITEMS && settings_labels[ui_idx]) {
            lv_obj_align_to(page_cursor, settings_labels[ui_idx], LV_ALIGN_OUT_LEFT_MID, -8, 0);
        }
    }
}

void settings_down(void) {
    if (selected + 1 >= SETTINGS_COUNT) return;
    selected++;
    if (selected >= first_visible + SETTINGS_VISIBLE_ITEMS) {
        first_visible++;
        settings_refresh();
    }
    settings_update_cursor();
}

void settings_up(void) {
    if (selected == 0) return;
    selected--;
    if (selected < first_visible) {
        first_visible--;
        settings_refresh();
    }
    settings_update_cursor();
}

void ui_show_settings_page(void)
{
    if (cur_page) {
        lv_obj_delete(cur_page);
        cur_page = NULL;
        page_cursor = NULL;
        detail_title_label = NULL; 
        detail_hint_label = NULL;
    }

    cur_page = lv_obj_create(page_container);
    lv_obj_remove_style_all(cur_page);
    lv_obj_set_size(cur_page, lv_pct(100), lv_pct(100));
    
    current_page_id = PAGE_SETTINGS;
    key_set_locked(true);
    update_keyboard_icons();

    lv_obj_t *title_label = lv_label_create(cur_page);
    lv_label_set_text(title_label, "Settings");
    lv_obj_set_style_text_color(title_label, lv_color_hex(color_index[COLOR_MSG_TEXT].color), 0);
    lv_obj_set_style_text_font(title_label, &jbm14, 0);
    lv_obj_align(title_label, LV_ALIGN_TOP_LEFT, 10, 10);

    page_cursor = lv_label_create(cur_page);
    lv_label_set_text(page_cursor, ">");
    lv_obj_set_style_text_color(page_cursor, lv_color_hex(color_index[COLOR_MSG_TEXT].color), 0);
    lv_obj_set_style_text_font(page_cursor, &jbm12, 0);

    lv_obj_t *settings_container = lv_obj_create(cur_page);
    lv_obj_remove_style_all(settings_container);
    lv_obj_set_size(settings_container, lv_pct(100), lv_pct(90));
    lv_obj_align(settings_container, LV_ALIGN_TOP_MID, 0, 30);
    lv_obj_set_layout(settings_container, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(settings_container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(settings_container, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_row(settings_container, 4, 0);
    lv_obj_set_style_pad_all(settings_container, 15, 0);

    selected = 0; 
    first_visible = 0;

    uint8_t visible = (SETTINGS_COUNT < SETTINGS_VISIBLE_ITEMS) ? SETTINGS_COUNT : SETTINGS_VISIBLE_ITEMS;
    for (uint8_t i = 0; i < visible; i++) {
        lv_obj_t *item_label = lv_label_create(settings_container);
        lv_label_set_text(item_label, settings_items[i]);
        lv_obj_set_style_text_color(item_label, lv_color_hex(color_index[COLOR_MSG_TEXT].color), 0);
        lv_obj_set_style_text_font(item_label, &jbm12, 0);
        lv_obj_set_width(item_label, lv_pct(100));
        lv_obj_set_style_text_align(item_label, LV_TEXT_ALIGN_LEFT, 0);
        settings_labels[i] = item_label;
    }

    settings_refresh();
    settings_update_cursor();
}
void ui_show_settings_change_alias_or_user_color(void) {
    // 1. 清理旧页面
    if (cur_page) {
        lv_obj_delete(cur_page);
        cur_page = NULL;
    }

    // 2. 创建新页面
    cur_page = lv_obj_create(page_container);
    lv_obj_remove_style_all(cur_page);
    lv_obj_set_size(cur_page, lv_pct(100), lv_pct(100));
    
    current_page_id = PAGE_SETTINGS_DETAIL;
    current_detail_step = DETAIL_STEP_INPUT_ID; // 初始状态为输入 ID
    target_change_id = 0xFF;

    // 3. 创建 UI: 标题
    detail_title_label = lv_label_create(cur_page);
    lv_label_set_text(detail_title_label, "Enter Target ID (00-FF)");
    lv_obj_set_style_text_color(detail_title_label, lv_color_hex(color_index[COLOR_MSG_TEXT].color), 0);
    lv_obj_set_style_text_font(detail_title_label, &jbm14, 0);
    lv_obj_set_width(detail_title_label, 220);
    lv_label_set_long_mode(detail_title_label, LV_LABEL_LONG_MODE_WRAP);
    lv_obj_align(detail_title_label, LV_ALIGN_TOP_MID, 0, 30);
}

void ui_show_settings_add_contact(void) {
    // 1. 清理旧页面
    if (cur_page) {
        lv_obj_delete(cur_page);
        cur_page = NULL;
    }

    // 2. 创建新页面
    cur_page = lv_obj_create(page_container);
    lv_obj_remove_style_all(cur_page);
    lv_obj_set_size(cur_page, lv_pct(100), lv_pct(100));
    
    current_page_id = PAGE_SETTINGS_DETAIL;
    current_detail_step = DETAIL_STEP_INPUT_ID; // 初始状态为输入 ID
    target_change_id = 0xFF;

    // 3. 创建 UI: 标题
    detail_title_label = lv_label_create(cur_page);
    lv_label_set_text(detail_title_label, "Enter Target ID (00-FF)");
    lv_obj_set_style_text_color(detail_title_label, lv_color_hex(color_index[COLOR_MSG_TEXT].color), 0);
    lv_obj_set_style_text_font(detail_title_label, &jbm14, 0);
    lv_obj_set_width(detail_title_label, 220);
    lv_label_set_long_mode(detail_title_label, LV_LABEL_LONG_MODE_WRAP);
    lv_obj_align(detail_title_label, LV_ALIGN_TOP_MID, 0, 30);
}

void ui_show_settings_change_interface_color(void) {
    // 1. 清理旧页面
    if (cur_page) {
        lv_obj_delete(cur_page);
        cur_page = NULL;
    }

    // 2. 创建新页面
    cur_page = lv_obj_create(page_container);
    lv_obj_remove_style_all(cur_page);
    lv_obj_set_size(cur_page, lv_pct(100), lv_pct(100));
    
    current_page_id = PAGE_SETTINGS_DETAIL;
    current_detail_step = DETAIL_STEP_INPUT_ID; // 初始状态为输入 ID
    target_change_id = 0xFF;

    // 3. 创建 UI: 标题
    detail_title_label = lv_label_create(cur_page);
    lv_label_set_text(detail_title_label, "Enter Target ID");
    lv_obj_set_style_text_color(detail_title_label, lv_color_hex(color_index[COLOR_MSG_TEXT].color), 0);
    lv_obj_set_style_text_font(detail_title_label, &jbm14, 0);
    lv_obj_set_width(detail_title_label, 220);
    lv_label_set_long_mode(detail_title_label, LV_LABEL_LONG_MODE_WRAP);
    lv_obj_align(detail_title_label, LV_ALIGN_TOP_MID, 0, 30);

    // 4. 创建 UI: 提示
    detail_hint_label = lv_label_create(cur_page);
    lv_label_set_text(detail_hint_label, "COLOR_BACKGROUND = 0,\n\
    COLOR_MSG_TEXT = 1,\n\
    COLOR_MSG_TIME = 2,\n\
    COLOR_MSG_DATE = 3,\n\
    COLOR_STATUS_BAR= 4,\n\
    COLOR_TEXTAREA_BACKGROUND = 5");
    lv_obj_set_style_text_color(detail_hint_label, lv_color_hex(0x888888), 0);
    lv_obj_align(detail_hint_label, LV_ALIGN_BOTTOM_MID, 0, -40);
}

void ui_show_settings_wifi(void) {
    // 1. 清理旧页面
    if (cur_page) {
        lv_obj_delete(cur_page);
        cur_page = NULL;
    }

    // 2. 创建新页面
    cur_page = lv_obj_create(page_container);
    lv_obj_remove_style_all(cur_page);
    lv_obj_set_size(cur_page, lv_pct(100), lv_pct(100));
    
    current_page_id = PAGE_SETTINGS_DETAIL;
    current_detail_step = DETAIL_STEP_INPUT_ID; // 初始状态为输入 ID
    target_change_id = 0xFF;

    // 3. 创建 UI: 标题
    detail_title_label = lv_label_create(cur_page);
    lv_label_set_text(detail_title_label, "Enter SSID (Max 32 chars) \nNotice: After input will try to connect to WiFi \nOr leave it blank to use soft AP to connect");
    lv_obj_set_style_text_color(detail_title_label, lv_color_hex(color_index[COLOR_MSG_TEXT].color), 0);
    lv_obj_set_style_text_font(detail_title_label, &jbm14, 0);
    lv_obj_set_width(detail_title_label, 220);
    lv_label_set_long_mode(detail_title_label, LV_LABEL_LONG_MODE_WRAP);
    lv_obj_align(detail_title_label, LV_ALIGN_TOP_MID, 0, 30);
}

void ui_show_settings_time(void) {
    // 1. 清理旧页面
    if (cur_page) {
        lv_obj_delete(cur_page);
        cur_page = NULL;
    }

    // 2. 创建新页面
    cur_page = lv_obj_create(page_container);
    lv_obj_remove_style_all(cur_page);
    lv_obj_set_size(cur_page, lv_pct(100), lv_pct(100));
    
    current_page_id = PAGE_SETTINGS_DETAIL;
    current_detail_step = DETAIL_STEP_INPUT_ID; // 初始状态为输入 ID(实则并不是ID 但是为了少写点代码 就这样吧)
    target_change_id = 0xFF;

    // 3. 创建 UI: 标题
    detail_title_label = lv_label_create(cur_page);
    lv_label_set_text(detail_title_label, "Enter Time\nYearMonthDayHourMinuteSecond\nExample: 20260202114514\nOr leave it blank to use WiFi to sync time");
    lv_obj_set_style_text_color(detail_title_label, lv_color_hex(color_index[COLOR_MSG_TEXT].color), 0);
    lv_obj_set_style_text_font(detail_title_label, &jbm14, 0);
    lv_obj_set_width(detail_title_label, 220);
    lv_label_set_long_mode(detail_title_label, LV_LABEL_LONG_MODE_WRAP);
    lv_obj_align(detail_title_label, LV_ALIGN_TOP_MID, 0, 30);
}

void ui_show_settings_sleep_time(void) {
    // 1. 清理旧页面
    if (cur_page) {
        lv_obj_delete(cur_page);
        cur_page = NULL;
    }

    // 2. 创建新页面
    cur_page = lv_obj_create(page_container);
    lv_obj_remove_style_all(cur_page);
    lv_obj_set_size(cur_page, lv_pct(100), lv_pct(100));
    
    current_page_id = PAGE_SETTINGS_DETAIL;
    current_detail_step = DETAIL_STEP_INPUT_ID; // 初始状态为输入 ID(实则并不是ID 但是为了少写点代码 就这样吧)
    target_change_id = 0xFF;

    // 3. 创建 UI: 标题
    detail_title_label = lv_label_create(cur_page);
    lv_label_set_text(detail_title_label, "Enter Auto Sleep Time\nUnit: second\nDefault: 300");
    lv_obj_set_style_text_color(detail_title_label, lv_color_hex(color_index[COLOR_MSG_TEXT].color), 0);
    lv_obj_set_style_text_font(detail_title_label, &jbm14, 0);
    lv_obj_set_width(detail_title_label, 220);
    lv_label_set_long_mode(detail_title_label, LV_LABEL_LONG_MODE_WRAP);
    lv_obj_align(detail_title_label, LV_ALIGN_TOP_MID, 0, 30);
}

void ui_show_settings_brightness(void) {
    // 1. 清理旧页面
    if (cur_page) {
        lv_obj_delete(cur_page);
        cur_page = NULL;
    }

    // 2. 创建新页面
    cur_page = lv_obj_create(page_container);
    lv_obj_remove_style_all(cur_page);
    lv_obj_set_size(cur_page, lv_pct(100), lv_pct(100));
    
    current_page_id = PAGE_SETTINGS_DETAIL;
    current_detail_step = DETAIL_STEP_INPUT_ID; // 初始状态为输入 ID(实则并不是ID 但是为了少写点代码 就这样吧)
    target_change_id = 0xFF;

    // 3. 创建 UI: 标题
    detail_title_label = lv_label_create(cur_page);
    lv_label_set_text(detail_title_label, "Enter Brightness\n10~100\nDefault: 50");
    lv_obj_set_style_text_color(detail_title_label, lv_color_hex(color_index[COLOR_MSG_TEXT].color), 0);
    lv_obj_set_style_text_font(detail_title_label, &jbm14, 0);
    lv_obj_set_width(detail_title_label, 220);
    lv_label_set_long_mode(detail_title_label, LV_LABEL_LONG_MODE_WRAP);
    lv_obj_align(detail_title_label, LV_ALIGN_TOP_MID, 0, 30);
}

void ui_show_settings_customize_title(void) {
    if (cur_page) {
        lv_obj_delete(cur_page);
        cur_page = NULL;
    }

    cur_page = lv_obj_create(page_container);
    lv_obj_remove_style_all(cur_page);
    lv_obj_set_size(cur_page, lv_pct(100), lv_pct(100));

    current_page_id = PAGE_SETTINGS_DETAIL;
    current_detail_step = DETAIL_STEP_INPUT_ID;
    target_change_id = 0xFF;

    detail_title_label = lv_label_create(cur_page);
    lv_label_set_text(detail_title_label, "Enter New Status Bar Title\nMax 16 characters\nDefault: Lumi-net");
    lv_obj_set_style_text_color(detail_title_label, lv_color_hex(color_index[COLOR_MSG_TEXT].color), 0);
    lv_obj_set_style_text_font(detail_title_label, &jbm14, 0);
    lv_obj_set_width(detail_title_label, 220);
    lv_label_set_long_mode(detail_title_label, LV_LABEL_LONG_MODE_WRAP);
    lv_obj_align(detail_title_label, LV_ALIGN_TOP_MID, 0, 30);
}

static void generate_confirm_code(void) {
    uint32_t r = esp_random() & 0xFFFF;
    snprintf(confirm_code, sizeof(confirm_code), "%04X", (unsigned int)(r & 0xFFFF));
}

void ui_show_settings_clear_flash(void) {
    if (cur_page) {
        lv_obj_delete(cur_page);
        cur_page = NULL;
    }

    cur_page = lv_obj_create(page_container);
    lv_obj_remove_style_all(cur_page);
    lv_obj_set_size(cur_page, lv_pct(100), lv_pct(100));

    current_page_id = PAGE_SETTINGS_DETAIL;
    current_detail_step = DETAIL_STEP_CLEAR_FLASH_CONFIRM;
    generate_confirm_code();

    detail_title_label = lv_label_create(cur_page);
    char title_buf[64];
    snprintf(title_buf, sizeof(title_buf), "Enter CLEAR FLASH %s to confirm", confirm_code);
    lv_label_set_text(detail_title_label, title_buf);
    lv_obj_set_style_text_color(detail_title_label, lv_color_hex(color_index[COLOR_MSG_TEXT].color), 0);
    lv_obj_set_style_text_font(detail_title_label, &jbm14, 0);
    lv_obj_set_width(detail_title_label, 220);
    lv_label_set_long_mode(detail_title_label, LV_LABEL_LONG_MODE_WRAP);
    lv_obj_align(detail_title_label, LV_ALIGN_TOP_MID, 0, 30);
}

void ui_show_settings_clear_nvs(void) {
    if (cur_page) {
        lv_obj_delete(cur_page);
        cur_page = NULL;
    }

    cur_page = lv_obj_create(page_container);
    lv_obj_remove_style_all(cur_page);
    lv_obj_set_size(cur_page, lv_pct(100), lv_pct(100));

    current_page_id = PAGE_SETTINGS_DETAIL;
    current_detail_step = DETAIL_STEP_CLEAR_NVS_CONFIRM;
    generate_confirm_code();

    detail_title_label = lv_label_create(cur_page);
    char title_buf[64];
    snprintf(title_buf, sizeof(title_buf), "Enter CLEAR NVS %s to confirm\nWill restart after clearing", confirm_code);
    lv_label_set_text(detail_title_label, title_buf);
    lv_obj_set_style_text_color(detail_title_label, lv_color_hex(color_index[COLOR_MSG_TEXT].color), 0);
    lv_obj_set_style_text_font(detail_title_label, &jbm14, 0);
    lv_obj_set_width(detail_title_label, 220);
    lv_label_set_long_mode(detail_title_label, LV_LABEL_LONG_MODE_WRAP);
    lv_obj_align(detail_title_label, LV_ALIGN_TOP_MID, 0, 30);
}

void handle_settings_detail_enter(void) {
    // 1. 确保我们在正确的页面
    if (current_page_id != PAGE_SETTINGS_DETAIL) return;

    key_set_shifted(false);
    key_set_locked(false);
    update_keyboard_icons();

    const char *input_text = lv_textarea_get_text(g_ta);
    char safe_input[64] = {0}; // AI说这样深拷贝可以防止悬空指针 那我就信一下吧
    strncpy(safe_input, input_text, sizeof(safe_input) - 1);
    safe_input[sizeof(safe_input) - 1] = '\0'; // 确保字符串安全终止

    // ==========================================
    // 状态 1: 用户刚刚输入完 ID，按下了 ENTER
    // ==========================================
    if (current_detail_step == DETAIL_STEP_INPUT_ID) {
        if (selected == 0 || selected == 1) { // "Change Alias" 和 "Change User Color"
            if (strlen(safe_input) == 0) return; // 空输入忽略

            uint8_t id;
            bool ok = parse_uint16(safe_input, &id, 16, 8);

            // 验证 ID 是否有效 (必须在 0-255 范围内，且 chat_list 中该 ID 存在)
            if (!ok || chat_list[id].id == 0xFF) {
                ESP_LOGW("SETTINGS", "Invalid or non-existent ID: %d", id);
                
                // UI 反馈：显示错误并清空，让用户重试
                lv_label_set_text(detail_title_label, "Invalid ID! Try again.");
                ui_input_clear();
                
                show_toast_dialog("Invalid ID!", 3000);
                return;
            }

            // --- ID 验证通过，进入步骤 2 ---
            target_change_id = id;
            if (selected == 0) {
                current_detail_step = DETAIL_STEP_INPUT_ALIAS;

                // 更新 UI，准备接收 Alias
                char title_buf[64];
                snprintf(title_buf, sizeof(title_buf), "New Alias for ID: %02X", id);
                lv_label_set_text(detail_title_label, title_buf);
                
                ui_input_clear(); // 清空输入框
                lv_textarea_set_placeholder_text(g_ta, "Type new alias...");

                ESP_LOGI("SETTINGS", "ID %d validated. Ready for alias input.", id);
            } else if (selected == 1) {
                current_detail_step = DETAIL_STEP_INPUT_USER_COLOR;

                // 更新 UI，准备接收 Color
                char title_buf[64];
                snprintf(title_buf, sizeof(title_buf), "New Color for ID: %02X", id);
                lv_label_set_text(detail_title_label, title_buf);
                
                ui_input_clear(); // 清空输入框
                lv_textarea_set_placeholder_text(g_ta, "Type new color...");

                ESP_LOGI("SETTINGS", "ID %d validated. Ready for color input.", id);
            }
        }
        else if (selected == 2) { // "Change Interface Color"
            if (strlen(safe_input) == 0) return; // 空输入忽略

            uint8_t id;
            bool ok = parse_uint16(safe_input, &id, 16, 8);

            // 验证 ID 是否有效
            if (!ok || id >= COLOR_MAX) {
                ESP_LOGW("SETTINGS", "Invalid or non-existent ID: %d", id);
                
                // UI 反馈：显示错误并清空，让用户重试
                lv_label_set_text(detail_title_label, "Invalid ID! Try again.");
                ui_input_clear();
                
                show_toast_dialog("Invalid ID!", 3000);
                return;
            }

            // --- ID 验证通过，进入步骤 2 ---
            target_change_id = id;
            current_detail_step = DETAIL_STEP_INPUT_INTERFACE_COLOR;

            // 更新 UI，准备接收 Alias
            char title_buf[64];
            snprintf(title_buf, sizeof(title_buf), "New Color for ID: %02X", id);
            lv_label_set_text(detail_title_label, title_buf);
            
            ui_input_clear(); // 清空输入框
            lv_textarea_set_placeholder_text(g_ta, "Type new color...");

            ESP_LOGI("SETTINGS", "ID %d validated. Ready for alias input.", id);
        }
        else if (selected == 3) { // "Wi-Fi"
            if (strlen(safe_input) == 0) { // 空输入转Soft AP
                uint32_t r = esp_random();
                char temp_password[9];
                snprintf(temp_password, sizeof(temp_password), "%08lu", r);
                wifi_ap_start(temp_password);

                char title_buf[64];
                snprintf(title_buf, sizeof(title_buf), "Use %s to connect ESP32S3_Config", temp_password);
                lv_label_set_text(detail_title_label, title_buf);

                // 通过队列通知主任务进入 AP 等待状态机
                UIEvent ev = {.type = EVENT_WIFI, .wifi_state = WIFI_STATE_AP_RUNNING};
                xQueueSend(appQueue, &ev, 0);
                return;
            }   

            // 验证 ID 是否有效
            if (strlen(safe_input) > 32) {
                ESP_LOGW("SETTINGS", "Invalid or non-existent SSID: %s", safe_input);
                
                // UI 反馈：显示错误并清空，让用户重试
                lv_label_set_text(detail_title_label, "Invalid SSID! Try again. Max 32 chars.");
                ui_input_clear();
                
                show_toast_dialog("SSID Too Long! Max 32 chars", 3000);
                return;
            }

            strncpy(target_wifi_ssid, safe_input, sizeof(target_wifi_ssid) - 1);
            target_wifi_ssid[sizeof(target_wifi_ssid) - 1] = '\0';
            
            current_detail_step = DETAIL_STEP_INPUT_PASSWORD;

            // 更新 UI，准备接收 Password
            char title_buf[64];
            snprintf(title_buf, sizeof(title_buf), "Password for SSID: %s", target_wifi_ssid);
            lv_label_set_text(detail_title_label, title_buf);
            
            ui_input_clear(); // 清空输入框
            lv_textarea_set_placeholder_text(g_ta, "Type password...");

            ESP_LOGI("SETTINGS", "SSID %d validated. Ready for password input.", target_wifi_ssid);
        }
        else if (selected == 4) { // "Time"
            if (strlen(safe_input) == 0) { // 空输入转NTP同步
                UIEvent ev = {.type = EVENT_WIFI, .wifi_state = WIFI_STATE_CONNECTING_STA};
                xQueueSend(appQueue, &ev, 0);
                return;
            }
            int y, mo, d, h, mi, s;
            if (sscanf(safe_input, "%4d%2d%2d%2d%2d%2d", &y, &mo, &d, &h, &mi, &s) == 6) {
                if (set_system_time_manual(y, mo, d, h, mi, s)) {
                    lv_label_set_text(detail_title_label, "Time Set OK!");
                    lv_timer_create(deferred_show_settings_page, 2000, NULL);
                } else {
                    lv_label_set_text(detail_title_label, "Invalid Time!");
                    lv_timer_create(deferred_show_settings_page, 2000, NULL);
                }
            }
        }
        else if (selected == 5) { // "Auto Sleep Time"
            if (strlen(safe_input) == 0) return; // 空输入忽略

            uint16_t time;
            bool ok = parse_uint16(safe_input, &time, 10, 16);

            // 验证 ID 是否有效
            if (ok) {
                esp_err_t err = nvs_set_sleep_time(time);
                if (err == ESP_OK) {
                    ESP_LOGI("SETTINGS", "Sleep Time Set OK: %d", time);
                    lv_label_set_text(detail_title_label, "OK!");
                    ui_input_clear();
                    lv_timer_create(deferred_show_settings_page, 2000, NULL);
                } else {
                    ESP_LOGW("SETTINGS", "Sleep Time Set Failed: %d", time);
                    lv_label_set_text(detail_title_label, "Something was wrong...\nPlease try again.");
                    ui_input_clear();
                    lv_timer_create(deferred_show_settings_page, 2000, NULL);
                }
            } else {
                ESP_LOGW("SETTINGS", "Invalid Sleep Time: %d", time);
                // UI 反馈：显示错误并清空，让用户重试
                lv_label_set_text(detail_title_label, "Invalid Time! Try again. Max 65535.");
                ui_input_clear();
                return;
            }
        }
        else if (selected == 6) { // "Change Brightness"
            if (strlen(safe_input) == 0) return; // 空输入忽略

            uint16_t brightness;
            bool ok = parse_uint16(safe_input, &brightness, 10, 16);

            // 验证 ID 是否有效
            if (ok && brightness <= 100 && brightness >= 10) {
                esp_err_t err = nvs_set_brightness(brightness*10);
                if (err == ESP_OK) {
                    ESP_LOGI("SETTINGS", "Brightness Set OK: %u", brightness);
                    lv_label_set_text(detail_title_label, "OK!");
                    ui_input_clear();
                    lv_timer_create(deferred_show_settings_page, 2000, NULL);
                } else {
                    ESP_LOGW("SETTINGS", "Brightness Set Failed: %u", brightness);
                    lv_label_set_text(detail_title_label, "Something was wrong...\nPlease try again.");
                    ui_input_clear();
                    lv_timer_create(deferred_show_settings_page, 2000, NULL);
                }
            } else {
                ESP_LOGW("SETTINGS", "Invalid brightness: %u", brightness);
                // UI 反馈：显示错误并清空，让用户重试
                lv_label_set_text(detail_title_label, "Invalid Brightness! Try again. 10~100.");
                ui_input_clear();
                return;
            }
        }
        else if (selected == 7) { // "Customize Title"
            if (strlen(safe_input) == 0) return;

            if (strlen(safe_input) > 16) {
                ESP_LOGW("SETTINGS", "Title too long: %d", strlen(safe_input));
                lv_label_set_text(detail_title_label, "Too long! Max 16 characters.");
                ui_input_clear();
                return;
            }

            esp_err_t err = nvs_set_status_title(safe_input);
            if (err == ESP_OK) {
                lv_label_set_text(title_label, status_title);
                ESP_LOGI("SETTINGS", "Title Set OK: [%s]", status_title);
                lv_label_set_text(detail_title_label, "OK!");
                ui_input_clear();
                lv_timer_create(deferred_show_settings_page, 2000, NULL);
            } else {
                ESP_LOGW("SETTINGS", "Title Set Failed: %s", esp_err_to_name(err));
                lv_label_set_text(detail_title_label, "Something was wrong...\nPlease try again.");
                ui_input_clear();
                lv_timer_create(deferred_show_settings_page, 2000, NULL);
            }
        }
        if (selected == 8) { // "Add/Remove a contact"
            if (strlen(safe_input) == 0) return;

            uint8_t id;
            bool ok = parse_uint16(safe_input, &id, 16, 8);

            if (!ok || id == 0xFF) {
                ESP_LOGW("SETTINGS", "Invalid ID: %d", id);
                lv_label_set_text(detail_title_label, "Invalid ID! Try again.");
                ui_input_clear();
                show_toast_dialog("Invalid ID!", 3000);
                return;
            }

            target_change_id = id;

            if (chat_list[id].id != 0xFF) {
                // ID 已存在 → 删除确认
                current_detail_step = DETAIL_STEP_DEL_CONFIRM;
                char title_buf[128];
                snprintf(title_buf, sizeof(title_buf), "Delete contact 0x%02X [%s]?\nType \"0x%02X%s\" to confirm delete.",
                         id, chat_list[id].alias, id, chat_list[id].alias);
                lv_label_set_text(detail_title_label, title_buf);
                ui_input_clear();
                lv_textarea_set_placeholder_text(g_ta, "Type id and alias to confirm...");
            } else {
                // ID 不存在 → 添加新联系人
                current_detail_step = DETAIL_STEP_INPUT_ALIAS;
                char title_buf[84];
                snprintf(title_buf, sizeof(title_buf), "New Alias for ID: 0x%02X\nLeave blank to abort.", id);
                lv_label_set_text(detail_title_label, title_buf);
                ui_input_clear();
                lv_textarea_set_placeholder_text(g_ta, "Type new alias...");
            }

            ESP_LOGI("SETTINGS", "ID %d validated.", id);
        }
    }
    // ==========================================
    // 状态 2: 用户刚刚输入完新 Alias，按下了 ENTER
    // ==========================================
    else if (current_detail_step == DETAIL_STEP_INPUT_ALIAS) {
        // safe_input 就是用户输入的新 alias
        
        // 调用你之前写好的 NVS 保存函数 (它会自动同步内存和 NVS)
        esp_err_t err = nvs_set_alias(target_change_id, safe_input);
        
        if (err == ESP_OK) {
            ESP_LOGI("SETTINGS", "Successfully updated alias for ID %d to [%s]", target_change_id, safe_input);
        } else {
            ESP_LOGE("SETTINGS", "Failed to update alias for ID %d", target_change_id);
        }

        // 任务完成，返回到设置主页面
        // (这会销毁当前的 DETAIL 页面，重新渲染 SETTINGS 列表页面)
        lv_timer_create(deferred_show_settings_page, 10, NULL);
    }
    else if (current_detail_step == DETAIL_STEP_DEL_CONFIRM) {
        if (strlen(safe_input) == 0) return;

        char expected[32];
        snprintf(expected, sizeof(expected), "0x%02X%s", target_change_id, chat_list[target_change_id].alias);

        if (strcmp(safe_input, expected) == 0) {
            esp_err_t err = nvs_set_alias(target_change_id, "");
            if (err == ESP_OK) {
                ESP_LOGI("SETTINGS", "Deleted contact 0x%02X", target_change_id);
                lv_label_set_text(detail_title_label, "Contact deleted.");
            } else {
                lv_label_set_text(detail_title_label, "Delete failed! Try again.");
            }
        } else {
            ESP_LOGW("SETTINGS", "Confirm mismatch for delete: [%s] vs [%s]", safe_input, expected);
            lv_label_set_text(detail_title_label, "Mismatch! Deletion cancelled.");
        }
        ui_input_clear();
        lv_timer_create(deferred_show_settings_page, 2000, NULL);
    }
    // ==========================================
    // 状态 3: 用户刚刚输入完新 Color，按下了 CONFIRM
    // ==========================================
    else if (current_detail_step == DETAIL_STEP_INPUT_USER_COLOR) {
        // safe_input 就是用户输入的新 color
        uint32_t color = strtoul(safe_input, NULL, 16);
        
        // 调用你之前写好的 NVS 保存函数 (它会自动同步内存和 NVS)
        esp_err_t err = nvs_set_user_color(target_change_id, color);
        
        if (err == ESP_OK) {
            ESP_LOGI("SETTINGS", "Successfully updated color for ID %d to [%s]", target_change_id, safe_input);
        } else {
            ESP_LOGE("SETTINGS", "Failed to update color for ID %d", target_change_id);
        }

        // 任务完成，返回到设置主页面
        // (这会销毁当前的 DETAIL 页面，重新渲染 SETTINGS 列表页面)
        lv_timer_create(deferred_show_settings_page, 10, NULL);
    }
    else if (current_detail_step == DETAIL_STEP_INPUT_INTERFACE_COLOR) {
        // safe_input 就是用户输入的新 color
        uint32_t color = strtoul(safe_input, NULL, 16);
        
        // 调用你之前写好的 NVS 保存函数 (它会自动同步内存和 NVS)
        esp_err_t err = nvs_set_interface_color(target_change_id, color);
        
        if (err == ESP_OK) {
            ESP_LOGI("SETTINGS", "Successfully updated interface color for ID %d to [%s]", target_change_id, safe_input);
        } else {
            ESP_LOGE("SETTINGS", "Failed to update interface color for ID %d", target_change_id);
        }

        // 任务完成，返回到设置主页面
        // (这会销毁当前的 DETAIL 页面，重新渲染 SETTINGS 列表页面)
        lv_timer_create(deferred_show_settings_page, 10, NULL);
    }
    else if (current_detail_step == DETAIL_STEP_INPUT_PASSWORD) { // 通过本地SSID+pass配网
        // safe_input 就是用户输入的password (密码不能太短 WPA2 至少 8 位)
        if (strlen(safe_input) < 8) {
            lv_label_set_text(detail_title_label, "Password too short!\nMin 8 chars.");
            ui_input_clear(); // 清空让用户重试
            return; // 留在当前步骤，不改变 state
        }

        // 2. UI 提示：正在连接 (不阻塞)
        lv_label_set_text(detail_title_label, "Saving...");
        ui_input_clear(); // 清空密码框，保护隐私

        // 3. 确保 WiFi 驱动已初始化 (esp_wifi_set_config 依赖)
        wifi_time_init();

        // 4. 核心：仅将配置保存到 NVS，不立即阻塞连接
        wifi_config_t cfg = {0};
        strncpy((char *)cfg.sta.ssid, target_wifi_ssid, sizeof(cfg.sta.ssid) - 1);
        strncpy((char *)cfg.sta.password, safe_input, sizeof(cfg.sta.password) - 1);
        
        esp_err_t err = esp_wifi_set_config(WIFI_IF_STA, &cfg);
        if (err != ESP_OK) {
            lv_label_set_text(detail_title_label, "Save Config Failed!");
            return;
        }
        
        UIEvent ev = {
            .type = EVENT_WIFI,
            .wifi_state = WIFI_STATE_CONNECTING_STA
        };
        xQueueSend(appQueue, &ev, 0);

        return; 
    }
    else if (current_detail_step == DETAIL_STEP_CLEAR_FLASH_CONFIRM) {
        if (strlen(safe_input) == 0) return;

        char expected[32];
        snprintf(expected, sizeof(expected), "CLEAR FLASH %s", confirm_code);

        if (strcmp(safe_input, expected) == 0) {
            lv_label_set_text(detail_title_label, "Erasing flash...\n(may take ~1 min)");
            ui_input_clear();
            lv_timer_handler();

            uint32_t flash_sz = ext_flash_size();
            esp_err_t ret = ESP_OK;
            for (uint32_t addr = 0; addr < flash_sz; addr += 4096) {
                ret = ext_flash_erase_sector(addr);
                if (ret != ESP_OK) break;
                if ((addr / 4096) % 64 == 0) {
                    vTaskDelay(1);
                }
            }

            if (ret == ESP_OK) {
                meta_init();
                lv_label_set_text(detail_title_label, "Flash cleared.");
            } else {
                lv_label_set_text(detail_title_label, "Erase failed!");
            }
            lv_timer_handler();
            lv_timer_create(deferred_show_settings_page, 5000, NULL);
        } else {
            ESP_LOGW("SETTINGS", "Confirm mismatch for clear flash: [%s] vs [%s]", safe_input, expected);
            lv_label_set_text(detail_title_label, "Mismatch! Cancelled.");
            ui_input_clear();
            lv_timer_create(deferred_show_settings_page, 2000, NULL);
        }
    }
    else if (current_detail_step == DETAIL_STEP_CLEAR_NVS_CONFIRM) {
        if (strlen(safe_input) == 0) return;

        char expected[32];
        snprintf(expected, sizeof(expected), "CLEAR NVS %s", confirm_code);

        if (strcmp(safe_input, expected) == 0) {
            lv_label_set_text(detail_title_label, "Clearing NVS...");
            ui_input_clear();
            lv_timer_handler();

            nvs_erase_all_partition();
            lv_label_set_text(detail_title_label, "NVS cleared.\nRestarting...");
            lv_timer_handler();
            vTaskDelay(pdMS_TO_TICKS(5000));
            esp_restart();
        } else {
            ESP_LOGW("SETTINGS", "Confirm mismatch for clear NVS: [%s] vs [%s]", safe_input, expected);
            lv_label_set_text(detail_title_label, "Mismatch! Cancelled.");
            ui_input_clear();
            lv_timer_create(deferred_show_settings_page, 2000, NULL);
        }
    }
}

void ui_show_settings_detail_page(void) {
    key_set_shifted(false);
    key_set_locked(false);
    update_keyboard_icons();
    switch (selected)
    {
        case 0:
            ui_show_settings_change_alias_or_user_color();
            break;
        case 1:
            ui_show_settings_change_alias_or_user_color();
            break;
        case 2:
            ui_show_settings_change_interface_color();
            break;
        case 3:
            ui_show_settings_wifi();
            break;
        case 4:
            ui_show_settings_time();
            break;
        case 5:
            ui_show_settings_sleep_time();
            break;
        case 6:
            ui_show_settings_brightness();
            break;
        case 7:
            ui_show_settings_customize_title();
            break;
        case 8:
            ui_show_settings_add_contact();
            break;
        case 9:
            ui_show_settings_clear_flash();
            break;
        case 10:
            ui_show_settings_clear_nvs();
            break;
        default:
            break;
    }
}