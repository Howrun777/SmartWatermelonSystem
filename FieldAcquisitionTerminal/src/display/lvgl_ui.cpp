#include "lvgl_ui.h"
#include "../config.h"
#include <Arduino.h> 

// 辅助颜色
#define COLOR_BG lv_color_hex(0xccffcc) // 淡绿色背景
#define COLOR_TEXT lv_color_hex(0x000000) // 黑色文字
#define COLOR_BTN_TEST lv_color_hex(0x3498db) // 蓝色测试按钮
#define COLOR_BTN_IND lv_color_hex(0xe67e22) // 橙色工业按钮
#define COLOR_DANGER lv_color_hex(0xe74c3c) // 红色警告
#define COLOR_GREY lv_color_hex(0x95a5a6) // 灰色

void UIManager::init(MeasureCallback measureCb, ModeSelectCallback modeCb, ClearTestCallback clearCb, ReturnHomeCallback homeCb) {
    this->onMeasure = measureCb;
    this->onModeSelect = modeCb;
    this->onClearTest = clearCb;
    this->onReturnHome = homeCb; // 绑定返回主页事件

    showBootScreen();
}

// ==========================================
// 1. 启动页 
// ==========================================
void UIManager::showBootScreen() {
    lv_obj_clean(lv_scr_act()); // 彻底清空当前屏幕的所有控件，防内存泄漏！
    lv_obj_set_style_bg_color(lv_scr_act(), COLOR_BG, 0);

    lv_obj_t * title = lv_label_create(lv_scr_act());
    lv_obj_set_style_text_color(title, COLOR_TEXT, 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_16, 0);
    lv_label_set_text(title, "Smart Watermelon System");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 30);

    lv_obj_t * info = lv_label_create(lv_scr_act());
    lv_obj_set_style_text_color(info, COLOR_TEXT, 0);
    lv_label_set_text_fmt(info, "ID: %s\nVar: %s", DEVICE_ID, WATERMELON_VARIETY);
    lv_obj_align(info, LV_ALIGN_TOP_MID, 0, 80);

    boot_status_label = lv_label_create(lv_scr_act());
    lv_obj_set_style_text_color(boot_status_label, COLOR_DANGER, 0);
    lv_label_set_text(boot_status_label, "Waiting for Time Sync...");
    lv_obj_align(boot_status_label, LV_ALIGN_CENTER, 0, 10);

    lv_obj_t * author = lv_label_create(lv_scr_act());
    lv_obj_set_style_text_color(author, COLOR_TEXT, 0);
    lv_label_set_text(author, "Designed by: Howrun");
    lv_obj_align(author, LV_ALIGN_BOTTOM_MID, 0, -20);
}

void UIManager::showModeSelection() {
    lv_label_set_text(boot_status_label, "Time Synced! Select Mode:");
    lv_obj_set_style_text_color(boot_status_label, COLOR_TEXT, 0);
    lv_obj_align(boot_status_label, LV_ALIGN_CENTER, 0, -30);

    btn_mode_test = lv_btn_create(lv_scr_act());
    lv_obj_set_size(btn_mode_test, 180, 45);
    lv_obj_align(btn_mode_test, LV_ALIGN_CENTER, 0, 20);
    lv_obj_set_style_bg_color(btn_mode_test, COLOR_BTN_TEST, 0);
    lv_obj_add_event_cb(btn_mode_test, btn_mode_test_cb, LV_EVENT_CLICKED, this);
    lv_obj_t * lbl_test = lv_label_create(btn_mode_test);
    lv_label_set_text(lbl_test, "Consumer Test Mode");
    lv_obj_center(lbl_test);

    btn_mode_ind = lv_btn_create(lv_scr_act());
    lv_obj_set_size(btn_mode_ind, 180, 45);
    lv_obj_align(btn_mode_ind, LV_ALIGN_CENTER, 0, 80);
    lv_obj_set_style_bg_color(btn_mode_ind, COLOR_BTN_IND, 0);
    lv_obj_add_event_cb(btn_mode_ind, btn_mode_ind_cb, LV_EVENT_CLICKED, this);
    lv_obj_t * lbl_ind = lv_label_create(btn_mode_ind);
    lv_label_set_text(lbl_ind, "Industrial Mode");
    lv_obj_center(lbl_ind);
}

// ==========================================
// 2. 测试模式 UI
// ==========================================
void UIManager::buildTestUI() {
    current_mode = MODE_TEST;
    lv_obj_clean(lv_scr_act());
    lv_obj_set_style_bg_color(lv_scr_act(), lv_color_hex(0xfff5e6), 0); 

    lv_obj_t * title = lv_label_create(lv_scr_act());
    lv_label_set_text(title, "Test Mode (Local Only)");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 10);

    // ✅ 双色大字排版 (利用 Flex 容器水平排列)
    lv_obj_t * sugar_cont = lv_obj_create(lv_scr_act());
    lv_obj_set_size(sugar_cont, 220, 40);
    lv_obj_align(sugar_cont, LV_ALIGN_TOP_MID, 0, 30);
    lv_obj_set_style_bg_opa(sugar_cont, 0, 0); 
    lv_obj_set_style_border_width(sugar_cont, 0, 0);
    lv_obj_set_flex_flow(sugar_cont, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(sugar_cont, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(sugar_cont, 0, 0);

    label_test_sugar = lv_label_create(sugar_cont);
    lv_obj_set_style_text_font(label_test_sugar, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(label_test_sugar, COLOR_DANGER, 0);
    lv_label_set_text(label_test_sugar, "--.-");

    label_test_sugar_ref = lv_label_create(sugar_cont);
    lv_obj_set_style_text_font(label_test_sugar_ref, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(label_test_sugar_ref, COLOR_GREY, 0);
    lv_label_set_text(label_test_sugar_ref, " /12.5Brix");

    // ✅ 历史记录表格 (5列，极致压榨空间)
    table_test_history = lv_table_create(lv_scr_act());
    lv_obj_set_size(table_test_history, 240, 160);
    lv_obj_align(table_test_history, LV_ALIGN_TOP_MID, 0, 75);
    lv_obj_set_style_text_font(table_test_history, &lv_font_montserrat_12, LV_PART_ITEMS);
    
    lv_table_set_col_cnt(table_test_history, 5);
    lv_table_set_col_width(table_test_history, 0, 50); // 次数
    lv_table_set_col_width(table_test_history, 1, 50); // 糖度
    lv_table_set_col_width(table_test_history, 2, 50); // 成熟度
    lv_table_set_col_width(table_test_history, 3, 50); // 均糖
    lv_table_set_col_width(table_test_history, 4, 60); // 均熟
    
    lv_table_set_cell_value(table_test_history, 0, 0, "No.");
    lv_table_set_cell_value(table_test_history, 0, 1, "Brix");
    lv_table_set_cell_value(table_test_history, 0, 2, "M%");
    lv_table_set_cell_value(table_test_history, 0, 3, "Avg");
    lv_table_set_cell_value(table_test_history, 0, 4, "AM%");

    // 底部按钮组 (3个)
    lv_obj_t * btn_home = lv_btn_create(lv_scr_act());
    lv_obj_set_size(btn_home, 60, 35);
    lv_obj_align(btn_home, LV_ALIGN_BOTTOM_LEFT, 5, -10);
    lv_obj_set_style_bg_color(btn_home, COLOR_GREY, 0);
    lv_obj_add_event_cb(btn_home, btn_home_cb, LV_EVENT_CLICKED, this);
    lv_obj_t * lbl_home = lv_label_create(btn_home);
    lv_label_set_text(lbl_home, "HOME"); lv_obj_center(lbl_home);

    lv_obj_t * btn_clear = lv_btn_create(lv_scr_act());
    lv_obj_set_size(btn_clear, 65, 35);
    lv_obj_align(btn_clear, LV_ALIGN_BOTTOM_MID, -10, -10);
    lv_obj_set_style_bg_color(btn_clear, COLOR_DANGER, 0);
    lv_obj_add_event_cb(btn_clear, btn_clear_test_cb, LV_EVENT_CLICKED, this);
    lv_obj_t * lbl_clr = lv_label_create(btn_clear);
    lv_label_set_text(lbl_clr, "CLEAR"); lv_obj_center(lbl_clr);

    lv_obj_t * btn_test_measure = lv_btn_create(lv_scr_act());
    lv_obj_set_size(btn_test_measure, 85, 35);
    lv_obj_align(btn_test_measure, LV_ALIGN_BOTTOM_RIGHT, -5, -10);
    lv_obj_set_style_bg_color(btn_test_measure, COLOR_BTN_TEST, 0);
    lv_obj_add_event_cb(btn_test_measure, btn_measure_cb, LV_EVENT_CLICKED, this);
    lv_obj_t * lbl_msr = lv_label_create(btn_test_measure);
    lv_label_set_text(lbl_msr, "MEASURE"); lv_obj_center(lbl_msr);
}

void UIManager::updateTestStats(float brix, int count, float avgBrix) {
    if (current_mode != MODE_TEST) return;
    String bStr = String(brix, 1);
    lv_label_set_text_fmt(label_test_sugar, "%s Brix", bStr.c_str());
}

// ✅ 反转录入表格 (头插法) + 自动计算成熟度百分比
void UIManager::addTestHistory(int index, float brix, float avg) {
    if (current_mode != MODE_TEST) return;
    
    // 以 12.5 为基准计算成熟度
    int mat = round((brix / 12.5) * 100);
    int avg_mat = round((avg / 12.5) * 100);

    uint16_t row_cnt = lv_table_get_row_cnt(table_test_history);
    lv_table_set_row_cnt(table_test_history, row_cnt + 1); // 增加一行

    // 把现有的所有数据全部往下挪一行
    for(int r = row_cnt; r > 1; r--) {
        for(int c = 0; c < 5; c++) {
            lv_table_set_cell_value(table_test_history, r, c, lv_table_get_cell_value(table_test_history, r - 1, c));
        }
    }

    // 把新数据插在第 1 行 (紧贴表头)
    String bStr = String(brix, 1);
    String aStr = String(avg, 1);
    lv_table_set_cell_value_fmt(table_test_history, 1, 0, "%d", index);
    lv_table_set_cell_value_fmt(table_test_history, 1, 1, "%s", bStr.c_str());
    lv_table_set_cell_value_fmt(table_test_history, 1, 2, "%d", mat);
    lv_table_set_cell_value_fmt(table_test_history, 1, 3, "%s", aStr.c_str());
    lv_table_set_cell_value_fmt(table_test_history, 1, 4, "%d", avg_mat);
}

void UIManager::clearTestTable() {
    lv_table_set_row_cnt(table_test_history, 1); // 只保留表头
    lv_label_set_text(label_test_sugar, "--.- Brix");
}

// ==========================================
// 工业模式 UI (双页滑动，带上传表格)
// ==========================================
void UIManager::buildIndustrialUI() {
    current_mode = MODE_INDUSTRIAL;
    lv_obj_clean(lv_scr_act()); // 清空屏幕，防泄漏
    lv_obj_set_style_bg_color(lv_scr_act(), lv_color_hex(0xf4f7f6), 0);

    lv_obj_t * tv = lv_tileview_create(lv_scr_act());
    lv_obj_set_size(tv, 240, 260); 
    lv_obj_align(tv, LV_ALIGN_TOP_MID, 0, 0);

    lv_obj_t * tile1 = lv_tileview_add_tile(tv, 0, 0, LV_DIR_RIGHT); 
    lv_obj_t * tile2 = lv_tileview_add_tile(tv, 1, 0, LV_DIR_LEFT);  

    // --- 工业第一页 (保持不变) ---
    lv_obj_set_flex_flow(tile1, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(tile1, 5, 0);

    label_status = lv_label_create(tile1); 
    lv_label_set_text(label_status, "Initializing...");
    lv_obj_set_style_text_color(label_status, COLOR_BTN_TEST, 0);

    lv_obj_t * panel = lv_obj_create(tile1);
    lv_obj_set_width(panel, 220);
    lv_obj_set_flex_flow(panel, LV_FLEX_FLOW_COLUMN);
    
    label_temp = lv_label_create(panel); lv_label_set_text(label_temp, "Temp: -- C");
    label_hum = lv_label_create(panel); lv_label_set_text(label_hum, "Hum: -- %");
    label_light = lv_label_create(panel); lv_label_set_text(label_light, "Light: -- Lux");
    label_sugar = lv_label_create(panel); 
    lv_obj_set_style_text_color(label_sugar, COLOR_DANGER, 0);
    lv_label_set_text(label_sugar, "Sugar: -- Brix");

    label_spectrum = lv_label_create(tile1);
    lv_label_set_long_mode(label_spectrum, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(label_spectrum, 220);
    lv_obj_set_style_text_font(label_spectrum, &lv_font_montserrat_12, 0);
    lv_label_set_text(label_spectrum, "[Spectrum]\nWaiting...");

    // --- 工业第二页 ---
    lv_obj_set_flex_flow(tile2, LV_FLEX_FLOW_COLUMN);
    lv_obj_t * tbl_title = lv_label_create(tile2);
    lv_label_set_text(tbl_title, "Upload History");

    table_ind_history = lv_table_create(tile2);
    lv_obj_set_width(table_ind_history, 230); 
    lv_obj_set_style_text_font(table_ind_history, &lv_font_montserrat_12, LV_PART_ITEMS);
    
    lv_table_set_col_cnt(table_ind_history, 5); 
    lv_table_set_col_width(table_ind_history, 0, 60); // Time (压榨空间)
    lv_table_set_col_width(table_ind_history, 1, 47); // Sgr 
    lv_table_set_col_width(table_ind_history, 2, 43); // T
    lv_table_set_col_width(table_ind_history, 3, 43); // H
    lv_table_set_col_width(table_ind_history, 4, 45); // Up (最窄)
    
    lv_table_set_cell_value(table_ind_history, 0, 0, "Time");
    lv_table_set_cell_value(table_ind_history, 0, 1, "Su");
    lv_table_set_cell_value(table_ind_history, 0, 2, "T");
    lv_table_set_cell_value(table_ind_history, 0, 3, "H");
    lv_table_set_cell_value(table_ind_history, 0, 4, "Up");

    // --- 工业底部按钮 ---
    lv_obj_t * btn_home = lv_btn_create(lv_scr_act()); // 绑定到主屏幕
    lv_obj_set_size(btn_home, 60, 35);
    lv_obj_align(btn_home, LV_ALIGN_BOTTOM_LEFT, 5, -15);
    lv_obj_set_style_bg_color(btn_home, COLOR_GREY, 0);
    lv_obj_add_event_cb(btn_home, btn_home_cb, LV_EVENT_CLICKED, this);
    lv_obj_t * lbl_home = lv_label_create(btn_home);
    lv_label_set_text(lbl_home, "HOME"); lv_obj_center(lbl_home);

    btn_measure = lv_btn_create(lv_scr_act());
    lv_obj_align(btn_measure, LV_ALIGN_BOTTOM_MID, 15, -15); // 稍微往右挪一点避开Home
    lv_obj_set_size(btn_measure, 120, 35);
    lv_obj_set_style_bg_color(btn_measure, COLOR_BTN_IND, 0);
    lv_obj_add_event_cb(btn_measure, btn_measure_cb, LV_EVENT_CLICKED, this);
    lv_obj_t * btn_label = lv_label_create(btn_measure);
    lv_label_set_text(btn_label, "MEASURE"); lv_obj_center(btn_label);

    label_countdown = lv_label_create(lv_scr_act());
    lv_obj_align(label_countdown, LV_ALIGN_BOTTOM_RIGHT, -70, -48); // 放在右下角之上
    lv_obj_set_style_text_font(label_countdown, &lv_font_montserrat_12, 0);
    lv_label_set_text(label_countdown, "Auto:00:00");
}

void UIManager::updateEnv(float t, float h, int l) {
    if (current_mode != MODE_INDUSTRIAL) return;
    String tStr = String(t, 1); String hStr = String(h, 1);
    lv_label_set_text_fmt(label_temp, "Temp: %s C", tStr.c_str());
    lv_label_set_text_fmt(label_hum, "Hum: %s %%", hStr.c_str());
    lv_label_set_text_fmt(label_light, "Light: %d Lux", l);
}

void UIManager::updateSugar(float brix) {
    if (current_mode != MODE_INDUSTRIAL) return;
    String bStr = String(brix, 1);
    lv_label_set_text_fmt(label_sugar, "Sugar: %s Brix", bStr.c_str());
}

void UIManager::updateSpectrum(uint16_t c415, uint16_t c445, uint16_t c480, uint16_t c515, uint16_t c555, uint16_t c595, uint16_t c640, uint16_t c680, uint16_t clr, uint16_t nir) {
    if (current_mode != MODE_INDUSTRIAL) return;
    lv_label_set_text_fmt(label_spectrum, "415:%d 445:%d 480:%d 515:%d\n555:%d 595:%d 640:%d 680:%d\nCLR:%d NIR:%d",
        c415, c445, c480, c515, c555, c595, c640, c680, clr, nir);
}

void UIManager::updateStatus(const char* msg) {
    if (current_mode == MODE_INDUSTRIAL) {
        lv_label_set_text(label_status, msg);
    } else if (current_mode == MODE_WAITING) {
        lv_label_set_text(boot_status_label, msg);
    }
}

void UIManager::updateCountdown(uint32_t remain_ms, uint32_t total_ms) {
    if (current_mode != MODE_INDUSTRIAL) return;
    uint32_t r_sec = remain_ms / 1000;
    char buf[32];
    sprintf(buf, "Auto: %02d:%02d", (r_sec % 3600) / 60, (r_sec % 60));
    lv_label_set_text(label_countdown, buf);
}

// 追加工业历史表格数据
// ✅ 修复：绕过 LVGL 不支持 %f 的缺陷，使用 String 强转
void UIManager::addIndustrialHistory(const char* timeStr, float brix, float temp, float hum, int light, bool uploaded) {
    if (current_mode != MODE_INDUSTRIAL) return;
    uint16_t row = lv_table_get_row_cnt(table_ind_history);
    
    String ts(timeStr); String shortTime = ts.substring(11, 16); 
    String bStr = String(brix, 1);
    
    lv_table_set_cell_value(table_ind_history, row, 0, shortTime.c_str());
    lv_table_set_cell_value(table_ind_history, row, 1, bStr.c_str());
    
    if (temp == 99.0) lv_table_set_cell_value(table_ind_history, row, 2, "--");
    else { String tStr = String(temp, 0); lv_table_set_cell_value(table_ind_history, row, 2, tStr.c_str()); }
    
    if (hum == 99.0) lv_table_set_cell_value(table_ind_history, row, 3, "--");
    else { String hStr = String(hum, 0); lv_table_set_cell_value(table_ind_history, row, 3, hStr.c_str()); }
    
    lv_table_set_cell_value(table_ind_history, row, 4, uploaded ? "Y" : "N");
}

void UIManager::markIndustrialHistoryUploaded(const char* timeStr) {
    if (current_mode != MODE_INDUSTRIAL || !table_ind_history) return;

    // 只截取时间部分的 hh:mm
    String ts(timeStr);
    String targetTime = ts.substring(11, 16); 
    
    uint16_t row_cnt = lv_table_get_row_cnt(table_ind_history);
    
    // 从第 1 行开始遍历（第 0 行是表头）
    for (uint16_t i = 1; i < row_cnt; i++) {
        const char* cell_time = lv_table_get_cell_value(table_ind_history, i, 0);
        
        // 如果时间匹配上了，就把第 4 列(UP)的值改成 'Y'
        if (cell_time != nullptr && String(cell_time) == targetTime) {
            lv_table_set_cell_value(table_ind_history, i, 4, "Y");
            // 只要找到一个匹配的就退出循环，因为同一分钟可能有多条，我们只标记其中一条
            break; 
        }
    }
}

// --- 事件代理分发 ---
void UIManager::btn_measure_cb(lv_event_t * e) {
    UIManager* ui = (UIManager*)lv_event_get_user_data(e);
    if (ui->onMeasure) ui->onMeasure(); 
}
void UIManager::btn_mode_test_cb(lv_event_t * e) {
    UIManager* ui = (UIManager*)lv_event_get_user_data(e);
    if (ui->onModeSelect) ui->onModeSelect(MODE_TEST); 
}
void UIManager::btn_mode_ind_cb(lv_event_t * e) {
    UIManager* ui = (UIManager*)lv_event_get_user_data(e);
    if (ui->onModeSelect) ui->onModeSelect(MODE_INDUSTRIAL); 
}
void UIManager::btn_clear_test_cb(lv_event_t * e) {
    UIManager* ui = (UIManager*)lv_event_get_user_data(e);
    if (ui->onClearTest) ui->onClearTest(); 
}
void UIManager::btn_home_cb(lv_event_t * e) {
    UIManager* ui = (UIManager*)lv_event_get_user_data(e);
    if (ui->onReturnHome) ui->onReturnHome(); 
}

