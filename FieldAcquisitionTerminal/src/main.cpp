#include <Arduino.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h> // 独立的触摸库
#include <lvgl.h>
#include <WiFi.h>  // ✅ 必须添加这一行！

#include "../config.h"

#include "sensor_driver/SensorManager.h"
#include "sensor_driver/RTCManager.h"
#include "sensor_driver/BuzzerManager.h" // ✅ 引入蜂鸣器库
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

// ==== 回调：返回主页 ====
void onReturnHomeClicked() {
    sysMode = MODE_WAITING;
    test_count = 0;      
    test_sugar_sum = 0;
    
    ui.showBootScreen();
    lv_timer_handler(); // 强制刷出界面

    // 检查是否已经拥有合法的时间戳 (大于2020年即可认为合法)
    // 这样用户从测试模式退回主页时，不需要重新等待联网
    if (RTCManager::getInstance().getTimestamp() > 1600000000) {
        ui.showModeSelection(); // 时间合法，直接放出工业按钮
    }
}

// ==== 回调：统一测量引擎 ====
void onMeasureBtnClicked() {
    if (sysMode == MODE_WAITING) return;

    ui.updateStatus("Measuring..."); 
    lv_timer_handler();

    //sensorMgr.controlLight(true); delay(500); 

    float t, h; int l;
    sensorMgr.readEnvironment(t, h, l);
    
    // 如果是测试模式，强行忽略环境数据 (解耦环境传感器)
    if (sysMode == MODE_TEST) { t = 99.0; h = 99.0; l = 99; }

    StaticJsonDocument<512> payload;
    JsonObject specObj = payload.createNestedObject("spectrum_json");
    bool specOk = sensorMgr.readSpectrum(specObj);
    //sensorMgr.controlLight(false);

    if (!specOk) { 
        if (sysMode == MODE_INDUSTRIAL) ui.updateStatus("ERR: Spectrum Failed!"); 
        return; 
    }

    // ✅ 数据采集成功！无论是何种模式，只要拿到光谱就响 1 秒！
    BuzzerManager::getInstance().beep(1000);

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
void setup() {
    Serial.begin(115200);
    // ✅ 初始化蜂鸣器
    BuzzerManager::getInstance().begin();
    sensorMgr.begin();
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

    // 1. 加载启动页 (默认显示 Syncing time...)
    ui.init(onMeasureBtnClicked, onModeSelected, onClearTestClicked, onReturnHomeClicked);
    lv_timer_handler();

    bool timeIsSynced = false;
    bool hasWiFi = false;
    bool hasServer = false;

    // 2. 尝试从 DS3231 硬件获取绝对时间
    RTCManager::getInstance().begin(); 
    if (RTCManager::getInstance().getTimestamp() > 1600000000) {
        timeIsSynced = true;
        // ✅ 硬件 RTC 自带合法时间，一开机就同步成功，响 2 秒！
        BuzzerManager::getInstance().beep(2000);
    } 

    // 3. 如果 RTC 没时间，尝试连 WiFi 兜底授时
    if (!timeIsSynced) {
        if (NetworkManager::connectWiFi()) {
            hasWiFi = true; // 标记 WiFi 是通的
            ui.updateStatus("WIFI OK! Syncing Server..."); 
            lv_timer_handler();
            
            uint32_t st = NetworkManager::fetchServerTime();
            if (st > 1600000000) {
                hasServer = true; // 标记服务器是通的
                RTCManager::getInstance().syncTime(st);
                timeIsSynced = true;
                // ✅ 从服务器拿到合法时间，同步成功，响 2 秒！
                BuzzerManager::getInstance().beep(2000);
            }
        }
    }
    
    // 4. 严苛的审判时刻 (动态矩阵输出)
    if (timeIsSynced) {
        ui.showModeSelection();
    } else {
        // 根据状态决定输出的故障代码
        if (!hasWiFi) {
            ui.updateStatus("Sync Failed!\nRTC: X | WIFI: X | SVR: ?");
        } else if (!hasServer) {
            ui.updateStatus("Sync Failed!\nRTC: X | WIFI: OK | SVR: X");
        }
    }
    
    lastAutoMeasure = millis();
}

void loop() {
    unsigned long now = millis();

    // ==========================================
    // 1. 网络维护与热解锁状态机
    // ==========================================
    // ✅ 每帧必须运行蜂鸣器引擎，才能实现非阻塞响铃！
    BuzzerManager::getInstance().loop();
    NetworkManager::maintainWiFi(); 
    bool currentWiFiState = (WiFi.status() == WL_CONNECTED);
    
    // 【场景 A：网络刚刚恢复 (断开 -> 连接)】
    if (currentWiFiState && !wasWiFiConnected) {
        // 1. 激活延迟补发机制
        isRecoveringData = true;
        recoverStartTime = now;
        recoverDelayDelay = random(0, 10000); 

        // 2. 【热解锁逻辑】
        if (sysMode == MODE_WAITING && !ui.isIndustrialModeUnlocked()) {
            ui.updateStatus("WIFI Restored! Syncing SVR...");
            lv_timer_handler(); 

            uint32_t st = NetworkManager::fetchServerTime();
            if (st > 1600000000) {
                RTCManager::getInstance().syncTime(st);
                ui.showModeSelection(); 
                // ✅ 热连网同步时间成功，响 2 秒！
                BuzzerManager::getInstance().beep(2000);
            } else {
                // 热解锁时，WiFi 肯定连上了，但服务器没给时间
                ui.updateStatus("Sync Failed!\nRTC: X | WIFI: OK | SVR: X");
            }
        }
    }
    wasWiFiConnected = currentWiFiState;

    // ==========================================
    // 2. 工业级防雪崩与抗云端宕机补发机制
    // ==========================================
    // 【场景 B：网络一直连着，周期性检查本地是否有因为服务器宕机遗留的数据】
    if (currentWiFiState && !isRecoveringData) {
        static unsigned long lastCheckTime = 0;
        if (now - lastCheckTime > 15000) {
            lastCheckTime = now;
            
            StaticJsonDocument<512> tempDoc;
            JsonObject recordObj = tempDoc.to<JsonObject>();
            uint32_t target_ts = 0;
            
            // 查到了未上传的老数据，激活补发！
            if (StorageManager::getInstance().getUnuploadedRecord(recordObj, target_ts)) {
                isRecoveringData = true;
                recoverStartTime = now;
                recoverDelayDelay = random(0, 10000); 
                Serial.printf("[Cloud Check] Found unsent data! Recovery starts in %lu sec...\n", recoverDelayDelay / 1000);
            }
        }
    }

    // 【场景 C：执行补发 (时间到了就开始发)】
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
            Serial.printf("Recovering old data (TS: %u)...\n", target_ts);
            
            if (NetworkManager::uploadData(payload, serverMsg)) {
                // 补发成功：磁盘标记为已传，且 UI 表格 N 变 Y
                StorageManager::getInstance().markAsUploaded(target_ts);
                String timeStr = RTCManager::getInstance().formatTime(target_ts);
                ui.markIndustrialHistoryUploaded(timeStr.c_str());
            } else {
                // 服务器还是宕机状态，暂停补发，等 15 秒后的周期检查再试
                Serial.println("Server still down. Pause recovery.");
                isRecoveringData = false; 
            }
        } else {
            isRecoveringData = false; 
            Serial.println("All old data recovered successfully!");
        }
    }

    // ==========================================
    // 3. 工业模式特有逻辑 (UI 刷新与定时上传)
    // ==========================================
    if (sysMode == MODE_INDUSTRIAL) {
        if (now - lastEnvUpdate > ENV_UPDATE_INTERVAL) {
            lastEnvUpdate = now;
            float t, h; int l;
            sensorMgr.readEnvironment(t, h, l);
            ui.updateEnv(t, h, l);
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