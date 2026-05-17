#include "HttpClient.h"
#include "../config.h"
#include <WiFi.h>
#include <HTTPClient.h>

unsigned long NetworkManager::last_reconnect_attempt = 0;

bool NetworkManager::connectWiFi() {
    Serial.printf("Connecting to %s ", WIFI_SSID);
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    
    int retries = 0;
    while (WiFi.status() != WL_CONNECTED && retries < 20) { // 最多等 10 秒
        delay(500); 
        Serial.print(".");
        retries++;
    }
    
    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("\nWiFi Connected! IP: " + WiFi.localIP().toString());
        return true;
    } else {
        Serial.println("\nWiFi Failed. Entering Offline Mode.");
        return false;
    }
}

void NetworkManager::maintainWiFi() {
    // 只有在断网的情况下，才尝试重连
    if (WiFi.status() != WL_CONNECTED) {
        unsigned long now = millis();
        // 每隔 30 秒尝试重连一次
        if (now - last_reconnect_attempt > 30000) {
            last_reconnect_attempt = now;
            Serial.println("WiFi disconnected. Attempting to reconnect...");
            WiFi.disconnect();
            WiFi.begin(WIFI_SSID, WIFI_PASS);
            // 这里不使用 while 阻塞，让底层的 WiFi 栈自己去连，不卡死 UI
        }
    }
}

uint32_t NetworkManager::fetchServerTime() {
    if (WiFi.status() != WL_CONNECTED) return 0;
    
    HTTPClient http;
    String url = String(SERVER_URL);
    url.replace("/device/upload", "/device/time"); // 替换为授时接口
    http.begin(url);
    
    int httpResponseCode = http.GET();
    uint32_t s_time = 0;
    
    if (httpResponseCode > 0) {
        String responseStr = http.getString();
        StaticJsonDocument<512> resDoc;
        if (!deserializeJson(resDoc, responseStr)) {
            s_time = resDoc["data"]["server_time"].as<uint32_t>();
        }
    }
    http.end();
    return s_time;
}

bool NetworkManager::uploadData(const JsonDocument& payload, String& outMsg, uint32_t* outServerTime) {
    if (WiFi.status() != WL_CONNECTED) {
        outMsg = "WiFi Offline (Saved locally)"; 
        return false;
    }
    
    HTTPClient http;
    http.begin(SERVER_URL);
    http.addHeader("Content-Type", "application/json");
    http.addHeader("device_id", DEVICE_ID);
    http.addHeader("token", DEVICE_TOKEN);

    String requestBody;
    serializeJson(payload, requestBody);

    int httpResponseCode = http.POST(requestBody);
    
    if (httpResponseCode > 0) {
        String responseStr = http.getString();
        StaticJsonDocument<512> resDoc;
        DeserializationError err = deserializeJson(resDoc, responseStr);
        if (err) {
            outMsg = "Invalid server response";
            http.end();
            return false;
        }

        outMsg = resDoc["msg"].as<String>();
        int businessCode = resDoc["code"].as<int>();

        // 如果外部传入了指针，且服务器返回了时间戳，则赋值回去用于校时
        if (outServerTime != nullptr && resDoc["data"].containsKey("server_time")) {
            *outServerTime = resDoc["data"]["server_time"].as<uint32_t>();
        }

        http.end();
        return httpResponseCode == 200 && businessCode == 200;
    } else {
        outMsg = "HTTP Error: " + String(httpResponseCode);
        http.end();
        return false;
    }
}
