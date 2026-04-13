

#pragma once

// ======= 设备与网络配置 =======
#define DEVICE_ID "1001-01-01"
#define WATERMELON_VARIETY "Kirin Watermelon" // 西瓜品种 (麒麟瓜)
#define DEVICE_TOKEN "device-token-001"

#define WIFI_SSID "Howrun777"
#define WIFI_PASS "28983904"
#define SERVER_URL "http://47.107.41.102:8080/api/device/upload"

// ======= I2C 传感器总线引脚 =======
#define I2C_SDA_PIN 27
#define I2C_SCL_PIN 22

// ======= 独立触摸屏 SPI 总线引脚 =======
#define TOUCH_MOSI_PIN 32
#define TOUCH_MISO_PIN 39
#define TOUCH_CLK_PIN  25
#define TOUCH_CS_PIN   33
#define TOUCH_IRQ_PIN  36

// ======= 宽谱 LED 控制引脚 =======
#define LED_PIN 26 

// ======= 业务定时配置 =======
#define ENV_UPDATE_INTERVAL 2000    // UI环境数据刷新频率: 2秒
#define AUTO_UPLOAD_INTERVAL 300000 // 自动检测上传频率: 5分钟 (300000毫秒)