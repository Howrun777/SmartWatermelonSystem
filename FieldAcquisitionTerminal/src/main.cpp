#include <Arduino.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h> // 独立的触摸库
#include <lvgl.h>
#include <WiFi.h>  // ✅ 必须添加这一行！

#include "../config.h"

#include "sensor_driver/SensorManager.h"
#include "sensor_driver/RTCManager.h"
#include "storage/StorageManager.h"
#include "network/HttpClient.h"
#include "display/lvgl_ui.h"
#include "algorithm/sugar_calc.h"

TFT_eSPI tft = TFT_eSPI();
SensorManager sensorMgr;
UIManager ui;

SPIClass touchSPI(HSPI); 
XPT2046_Touchscreen ts(TOUCH_CS_PIN, TOUCH_IRQ_PIN);

static const uint16_t screenWidth  = 240;
static const uint16_t screenHeight = 320;
static lv_disp_draw_buf_t draw_buf;
static lv_color_t buf[screenWidth * 10];

unsigned long lastEnvUpdate = 0;
unsigned long lastAutoMeasure = 0; 
SystemMode sysMode = MODE_WAITING;

// 测试模式本地内存数据
float test_sugar_sum = 0;
int test_count = 0;
#define MAX_TEST_COUNT 20

// 错峰补发
bool wasWiFiConnected = false;
bool isRecoveringData = false;
unsigned long recoverStartTime = 0;
unsigned long recoverDelayDelay = 0; 

void my_disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p) {
    uint32_t w = (area->x2 - area->x1 + 1); uint32_t h = (area->y2 - area->y1 + 1);
    tft.startWrite(); tft.setAddrWindow(area->x1, area->y1, w, h);
    tft.pushColors((uint16_t *)&color_p->full, w * h, true); tft.endWrite();
    lv_disp_flush_ready(disp);
}

void my_touch_read(lv_indev_drv_t * indrv, lv_indev_data_t * data) {
    if (ts.tirqTouched() && ts.touched()) {
        TS_Point p = ts.getPoint(); 
        data->state = LV_INDEV_STATE_PR;
        data->point.x = map(p.x, 250, 3800, 0, screenWidth);
        data->point.y = map(p.y, 250, 3800, 0, screenHeight);
    } else {
        data->state = LV_INDEV_STATE_REL;
    }
}

// ==== 回调：清除测试数据 ====
void onClearTestClicked() {
    test_sugar_sum = 0;
    test_count = 0;
    ui.clearTestTable();
}

// ==== 回调：用户选择模式 ====
void onModeSelected(SystemMode mode) {
    sysMode = mode;
    if (mode == MODE_TEST) {
        ui.buildTestUI();
    } else if (mode == MODE_INDUSTRIAL) {
        ui.buildIndustrialUI();
        lastAutoMeasure = millis(); // 工业模式启动倒计时
    }
}

// ✅ 新增回调：返回主页 ====
void onReturnHomeClicked() {
    sysMode = MODE_WAITING;
    test_count = 0;      // 清空测试数据
    test_sugar_sum = 0;
    
    // 因为此时肯定已经同步过时间了，我们重新绘制启动页后，立刻开放模式选择按钮
    ui.showBootScreen();
    ui.showModeSelection();
}

// ==== 回调：统一测量引擎 ====
void onMeasureBtnClicked() {
    if (sysMode == MODE_WAITING) return;

    sensorMgr.controlLight(true); delay(500); 

    float t, h; int l;
    sensorMgr.readEnvironment(t, h, l);
    
    // 如果是测试模式，强行忽略环境数据 (解耦环境传感器)
    if (sysMode == MODE_TEST) { t = 99.0; h = 99.0; l = 99; }

    StaticJsonDocument<512> payload;
    JsonObject specObj = payload.createNestedObject("spectrum_json");
    bool specOk = sensorMgr.readSpectrum(specObj);
    sensorMgr.controlLight(false);

    if (!specOk) { 
        if (sysMode == MODE_INDUSTRIAL) ui.updateStatus("ERR: Spectrum Failed!"); 
        return; 
    }

    float brix = SugarCalc::calculate(specObj);

    // ==========================================
    // 逻辑分流：测试模式 vs 工业模式
    // ==========================================
    if (sysMode == MODE_TEST) {
        // [测试模式] 仅在本地内存计算和显示
        if (test_count >= MAX_TEST_COUNT) {
            onClearTestClicked(); // 超过20次自动清空
        }
        test_count++;
        test_sugar_sum += brix;
        float avg = test_sugar_sum / test_count;
        
        ui.updateTestStats(brix, test_count, avg);
        ui.addTestHistory(test_count, brix, avg);

    } else if (sysMode == MODE_INDUSTRIAL) {
        // [工业模式] 全链路上传与持久化
        ui.updateStatus("Uploading..."); lv_timer_handler();

        ui.updateSpectrum(specObj["ch415"], specObj["ch445"], specObj["ch480"], specObj["ch515"],
                          specObj["ch555"], specObj["ch595"], specObj["ch640"], specObj["ch680"],
                          specObj["chClear"], specObj["chNIR"]);
        ui.updateSugar(brix);

        uint32_t current_ts = RTCManager::getInstance().getTimestamp();
        payload["collected_at"] = current_ts;
        payload["temperature"] = t; 
        payload["humidity"] = h; 
        payload["light"] = l;

        String serverMsg;
        uint32_t serverTime = 0;
        bool uploadOk = NetworkManager::uploadData(payload, serverMsg, &serverTime);
        
        StorageManager::getInstance().saveRecord(current_ts, brix, t, h, l, specObj, uploadOk);

        if (uploadOk) {
            ui.updateStatus("Success & Saved!");
            if (serverTime > 0) RTCManager::getInstance().syncTime(serverTime);
        } else {
            ui.updateStatus(serverMsg.c_str());
        }

        // 把这条记录插入到工业模式的 UI 表格中
        String timeStr = RTCManager::getInstance().formatTime(current_ts);
        ui.addIndustrialHistory(timeStr.c_str(), brix, t, h, l, uploadOk);
        
        lastAutoMeasure = millis();
    }
}

// ==== 系统初始化 ====
// ==== 系统初始化 ====
void setup() {
    Serial.begin(115200);
    sensorMgr.begin();
    RTCManager::getInstance().begin();
    StorageManager::getInstance().begin();
    
    pinMode(TFT_BL, OUTPUT); digitalWrite(TFT_BL, HIGH); 
    tft.begin(); tft.setRotation(0); 

    // 初始化 LVGL
    touchSPI.begin(TOUCH_CLK_PIN, TOUCH_MISO_PIN, TOUCH_MOSI_PIN, TOUCH_CS_PIN);
    ts.begin(touchSPI); ts.setRotation(0); 
    lv_init(); lv_disp_draw_buf_init(&draw_buf, buf, NULL, screenWidth * 10);
    static lv_disp_drv_t disp_drv; lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = screenWidth; disp_drv.ver_res = screenHeight;
    disp_drv.flush_cb = my_disp_flush; disp_drv.draw_buf = &draw_buf;
    lv_disp_drv_register(&disp_drv);
    static lv_indev_drv_t indev_drv; lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER; indev_drv.read_cb = my_touch_read;
    lv_indev_drv_register(&indev_drv);

    // ✅ 修改：多传一个 onReturnHomeClicked 回调给 UI
    ui.init(onMeasureBtnClicked, onModeSelected, onClearTestClicked, onReturnHomeClicked);
    lv_timer_handler();

    if (NetworkManager::connectWiFi()) {
        ui.updateStatus("WIFI Connected! Syncing..."); lv_timer_handler();
        uint32_t st = NetworkManager::fetchServerTime();
        if (st > 0) RTCManager::getInstance().syncTime(st);
    } else {
        ui.updateStatus("WIFI Failed! Using RTC Time."); lv_timer_handler();
    }
    
    delay(1000); 

    ui.showModeSelection();
    lastAutoMeasure = millis();
}

void loop() {
    unsigned long now = millis();

    // ==========================================
    // 后台网络重连与防雪崩补发 (不论哪个模式都允许后台补发老数据)
    // ==========================================
    NetworkManager::maintainWiFi(); 
    bool currentWiFiState = (WiFi.status() == WL_CONNECTED);
    
    if (currentWiFiState && !wasWiFiConnected) {
        isRecoveringData = true;
        recoverStartTime = now;
        recoverDelayDelay = random(0, 10000); // 测试期改成了 0~10 秒，正式演示可改回 200 秒
    }
    wasWiFiConnected = currentWiFiState;

    if (isRecoveringData && currentWiFiState && (now - recoverStartTime > recoverDelayDelay)) {
        StaticJsonDocument<512> tempDoc;
        JsonObject recordObj = tempDoc.to<JsonObject>();
        uint32_t target_ts = 0;
        
        if (StorageManager::getInstance().getUnuploadedRecord(recordObj, target_ts)) {
            StaticJsonDocument<512> payload;
            payload["collected_at"] = target_ts;
            payload["temperature"] = recordObj["t"];
            payload["humidity"] = recordObj["h"];
            payload["light"] = recordObj["l"];
            payload["spectrum_json"] = recordObj["spec"];

            String serverMsg;
            // 发送给服务器进行补发
            if (NetworkManager::uploadData(payload, serverMsg)) {
                // 1. 磁盘持久化更新：将这条数据的状态由 0 改为 1
                StorageManager::getInstance().markAsUploaded(target_ts);
                
                // 2. ✅ UI 更新：把时间戳转为文本，告诉屏幕把这行数据的 'N' 刷成 'Y'
                String timeStr = RTCManager::getInstance().formatTime(target_ts);
                ui.markIndustrialHistoryUploaded(timeStr.c_str());
            }
        } else {
            isRecoveringData = false; // 没有未上传的数据了，结束补发状态机
        }
    }

    // ==========================================
    // 工业模式特有逻辑 (UI 刷新与定时上传)
    // ==========================================
    if (sysMode == MODE_INDUSTRIAL) {
        if (now - lastEnvUpdate > ENV_UPDATE_INTERVAL) {
            lastEnvUpdate = now;
            float t, h; int l;
            sensorMgr.readEnvironment(t, h, l);
            ui.updateEnv(t, h, l);
            
            // 工业模式第一页顶端显示实时时钟
            ui.updateStatus(RTCManager::getInstance().formatTime(RTCManager::getInstance().getTimestamp()).c_str());
        }

        unsigned long elapsed = now - lastAutoMeasure;
        if (elapsed > AUTO_UPLOAD_INTERVAL) elapsed = AUTO_UPLOAD_INTERVAL;
        ui.updateCountdown(AUTO_UPLOAD_INTERVAL - elapsed, AUTO_UPLOAD_INTERVAL);

        if (elapsed >= AUTO_UPLOAD_INTERVAL) {
            onMeasureBtnClicked();
        }
    }

    lv_timer_handler();
    delay(5);
}