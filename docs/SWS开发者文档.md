# 智能西瓜无损糖度检测系统（SmartWatermelonSystem）

## 完整架构设计 + 接口文档

本文档包含**项目分层架构、文件目录架构、功能模块架构、设备-服务端接口、前端-服务端接口**，完全匹配定义的硬件、软件、数据库、业务规则，可直接用于工业落地。

---

## 一、项目整体分层架构

系统采用**三层架构**设计，符合物联网+Web系统标准架构，职责完全解耦：


| 层级  | 名称                                 | 核心职责                                                                 | 技术栈                             |
| --- | ---------------------------------- | -------------------------------------------------------------------- | ------------------------------- |
| 感知层 | 瓜田环境采集终端 FieldAcquisitionTerminal  | 数据采集（光谱/温湿度/光照）、本地计算糖度、本地显示、HTTP上传数据                                 | ESP32、Arduino/ESP-IDF、I2C传感器    |
| 服务层 | 瓜田数据处理中心 FieldDataProcessingServer | 设备认证、数据接收、校验、计算（糖度/成熟度）、MySQL存储、接口提供                                 | C++、HTTP协议、MySQL、Argon2、Session |
| 应用层 | 瓜田监测可视化平台 FieldMonitoringPlatform  | 前台展示、管理员登录、数据可视化（图表/阵列）、掉线感知（30分钟无数据标记红色）、双定时器机制（60s无操作切换瓜田+每6s静默刷新） | HTML/CSS/JS、ECharts、Cookie      |


---

## 二、项目文件目录架构

标准化工业级项目结构，分三大核心模块，目录清晰、可直接落地开发：

```Plain Text

SmartWatermelonSystem/               # 项目根目录（git仓库）
├── README.md                         # 项目说明（概述 + 部署指南）
│
├── docs/                            # 毕业设计文档
│   ├── SWS开发者文档.md              # 本文档（系统全貌 + 各模块说明）
│   ├── SWS需求文档.md               # 需求规格说明
│   ├── 论文框架.md                  # 论文大纲
│   ├── 前端开发者文档.md             # 前端开发指南
│   ├── 设备端开发者文档.md           # ESP32设备端开发指南
│   └── 毕业论文初稿.md              # 论文正文草稿
│
├── FieldAcquisitionTerminal/        # 硬件边缘设备（ESP32，PlatformIO）
│   ├── platformio.ini               # PlatformIO工程配置（开发板、库依赖、构建标志）
│   ├── config.h                    # 全局宏定义（WiFi、服务器地址、I2C/SPI引脚、蜂鸣器）
│   ├── .gitignore                   # Git忽略（构建产物、.pio、.vscode）
│   │
│   ├── .vscode/                    # VSCode调试配置
│   │   ├── c_cpp_properties.json   # C++ IntelliSense路径（PlatformIO自动生成）
│   │   ├── launch.json             # PlatformIO调试器启动配置（3种变体）
│   │   └── extensions.json         # 推荐的VSCode扩展
│   │
│   ├── .pio/                       # PlatformIO构建/缓存目录（已被.gitignore，约2000+文件）
│   │   ├── build/esp32dev/
│   │   │   ├── firmware.map
│   │   │   └── firmware.elf        # （编译产物）
│   │   └── libdeps/esp32dev/      # 下载的第三方库依赖
│   │       ├── lvgl/
│   │       ├── TFT_eSPI/
│   │       ├── Adafruit AS7341/
│   │       ├── Adafruit SHT31 Library/
│   │       ├── BH1750/
│   │       ├── RTClib/
│   │       ├── ArduinoJson/
│   │       └── XPT2046_Touchscreen/
│   │
│   └── src/                        # 应用程序源代码
│       ├── main.cpp                # 入口点，状态机，UI回调，loop主循环
│       │
│       ├── algorithm/               # 离线算法
│       │   └── sugar_calc.h        # MLR糖度/Brix计算（从光谱数据）
│       │
│       ├── display/                # LVGL UI层
│       │   ├── lvgl_ui.h           # UIManager类声明（模式、回调、控件成员）
│       │   └── lvgl_ui.cpp        # UI构建：启动画面、模式选择、检测模式
│       │                             （本地模式）、工业模式（tileview：数据+历史）、
│       │                             光谱/环境/糖度/状态更新
│       │
│       ├── network/                 # WiFi和HTTP网络
│       │   ├── HttpClient.h        # NetworkManager类声明
│       │   └── HttpClient.cpp      # connectWiFi、maintainWiFi、fetchServerTime、uploadData
│       │
│       ├── sensor_driver/           # 硬件外设驱动
│       │   ├── SensorManager.h     # SensorManager：AS7341、SHT31、BH1750封装
│       │   ├── SensorManager.cpp   # readEnvironment、readSpectrum、灵活降级（99.0）
│       │   ├── RTCManager.h        # RTCManager单例：DS3231 + 系统时钟
│       │   ├── RTCManager.cpp      # begin、getTimestamp、syncTime（北京时间+8h）、formatTime
│       │   ├── BuzzerManager.h     # BuzzerManager单例（非阻塞异步蜂鸣）
│       │   └── BuzzerManager.cpp   # beep()、loop() -- 高层触发接口
│       │
│       └── storage/                # 本地文件系统持久化
│           ├── StorageManager.h     # StorageManager单例：LittleFS记录I/O
│           └── StorageManager.cpp   # begin、saveRecord、checkAndCleanCapacity、
│                                     getUnuploadedRecord、markAsUploaded（JSON Lines格式）
│
├── FieldDataProcessingServer/       # C++后端服务器（CMake构建）
│   ├── CMakeLists.txt             # CMake配置：C++17、Threads、MySQL、libsodium
│   │
│   ├── sql/                       # 数据库Schema
│   │   └── init_db.sql           # 5张表：admin_users、device_auth、field_production、
│   │                               watermelon_data、field_environment + 种子数据
│   │
│   ├── third_party/               # 第三方单头文件库
│   │   ├── httplib/
│   │   │   ├── httplib.h         # cpp-httplib v0.x HTTP服务器（单头文件）
│   │   │   └── httplib开发者文档.md  # cpp-httplib使用文档
│   │   └── json/
│   │       └── json.hpp          # nlohmann/json单头文件库
│   │
│   ├── build/                     # CMake构建目录（已被.gitignore）
│   │   ├── CMakeCache.txt
│   │   ├── Makefile
│   │   ├── cmake_install.cmake
│   │   └── CMakeFiles/            # 编译中间产物（.o/.d文件等）
│   │
│   └── src/                       # 应用程序源代码
│       ├── main.cpp              # 入口：libsodium初始化、数据库连接、0.0.0.0:8080启动
│       │
│       ├── http_server/          # HTTP层
│       │   ├── HttpServer.h      # HttpServer类：start/stop（封装httplib::Server）
│       │   ├── HttpServer.cpp    # 构造函数注册路由，设置/mount静态目录/admin/*
│       │   ├── Router.h          # Router静态声明
│       │   └── Router.cpp        # 所有路由注册：/ping、/api/device/*、
│       │                           /api/admin/*、/admin/*.html、CORS OPTIONS处理
│       │
│       ├── auth/                  # 认证
│       │   ├── AdminAuth.h       # AdminAuth::login声明（Argon2 + session）
│       │   ├── AdminAuth.cpp    # 查询admin_users，验证Argon2哈希，颁发session
│       │   ├── DeviceAuth.h     # DeviceAuth::authenticate声明
│       │   └── DeviceAuth.cpp   # 查询device_auth表，检查status=1
│       │
│       ├── session/              # 内存会话管理
│       │   ├── Session.h         # Session单例：createSession、isValid、destroySession
│       │   └── Session.cpp      # UUID会话（32位十六进制），1小时TTL
│       │
│       ├── db/                    # 数据库
│       │   ├── MySQLDriver.h     # MySQLDriver单例：connect、execute、query、escapeString
│       │   ├── MySQLDriver.cpp  # mysql_real_connect/query/escape_string，
│       │   │                       线程安全互斥锁，结果集为vector<map<string,string>>
│       │   ├── ConnectionPool.h  # （CMakeLists中声明，文件为空/未实现）
│       │   └── ConnectionPool.cpp
│       │
│       ├── data_process/          # 业务逻辑/算法
│       │   ├── SugarCalc.h       # SugarCalc::calculate（MLR光谱计算糖度）
│       │   ├── SugarCalc.cpp     # 公式：brix=0.0012*ch415+0.0005*ch445+0.0021*ch480+5.2
│       │   ├── MaturityCalc.h     # MaturityCalc::calculate（brix/瓜田阈值）
│       │   ├── MaturityCalc.cpp  # 查询field_production阈值，返回成熟度比例
│       │   ├── DataCheck.h       # DataCheck::isEnvironmentValid、isFieldExist
│       │   └── DataCheck.cpp    # 校验环境数据（非全99.0）、检查瓜田是否注册
│       │
│       └── utils/                 # 工具类
│           ├── PasswordHasher.h  # PasswordHasher::hash、verify（libsodium Argon2id）
│           ├── PasswordHasher.cpp
│           ├── Logger.h          # （CMakeLists中声明，文件为空/未实现）
│           └── Logger.cpp
│
├── FieldMonitoringPlatform/        # Web前端（原生HTML/CSS/JS）
│   ├── index.html                # 前台首页（产品介绍+系统架构展示）
│   │
│   ├── admin/
│   │   ├── login.html            # 登录页（用户名/密码、显示/隐藏密码、错误提示）
│   │   ├── dashboard.html        # 监控大屏主页面（西瓜阵列 + 4个ECharts图表）
│   │   │
│   │   ├── css/
│   │   │   └── style.css        # 样式表：毛玻璃导航、卡片、西瓜阵列、离线告警动画
│   │   │
│   │   └── js/
│   │       ├── api.js           # BASE_URL，login()封装（POST /api/admin/login）
│   │       ├── chart.js         # ECharts初始化、buildStrictOption、renderWmHistoryChart、
│   │       │                       renderEnvHistoryCharts（value轴时间尺度切换）
│   │       └── auto_switch.js   # 瓜田自动切换（60s空闲）、数据刷新（6s间隔）、
│   │                               changeScale、reloadAllChartsData、idleTimer管理
│   │
│   └── assets/
│       └── libs/
│           └── echarts.min.js   # ECharts v5.5（离线打包，防断网）
│
└── .git/                          # Git仓库元数据
    ├── HEAD
    ├── config
    ├── description
    ├── COMMIT_EDITMSG
    ├── info/exclude
    ├── hooks/                     # Git钩子（模板示例）
    ├── objects/                   # Git对象存储（40+打包对象）
    ├── logs/
    │   ├── refs/heads/master
    │   └── HEAD
    └── refs/heads/master

```

---

## 三、功能模块架构

分三大核心模块，**子模块完全匹配需求**，无冗余、无缺失：

### 模块1：瓜田环境采集终端（FieldAcquisitionTerminal）

```Plain Text

瓜田环境采集终端
├─ 设备配置模块
│  └─ 设置设备编号（瓜田号-瓜组-瓜号）、Device Token、服务器IP
├─ 传感器数据采集模块
│  ├─ 多光谱采集（AS7341 → spectrum_json，12通道）
│  ├─ 温湿度采集（SHT31 → 温度/湿度，高精度±0.2℃）【可选】
│  └─ 光照采集（BH1750 → 光照强度）【可选】
├─ 本地计算模块
│  └─ 光谱数据 → 本地糖度（MLR离线公式，适合无网络场景）
├─ 本地显示模块（TFT+LVGL）
│  ├─ 开机登录页面（淡绿色背景，黑色字体）
│  ├─ 测试模式页面（消费者模式）
│  ├─ 工业模式页面（生产采集模式）
│  └─ 历史数据表格显示
├─ 光源控制模块
│  └─ 3W宽谱LED灯带（GPIO26驱动三极管开关，采集时开启，测完后关闭，排除外界环境光干扰）
│  注：GPIO控制逻辑嵌入主测量流程，无独立驱动文件
├─ 网络通信模块
│  ├─ WiFi连接与重连机制（断线重连，每30秒）
│  ├─ HTTP数据上传（设备编号+光谱+温湿度+光照）
│  └─ 时间同步握手（首次联网获取服务器时间）
├─ 蜂鸣器反馈模块
│  ├─ WiFi连接成功提示（1秒蜂鸣）
│  └─ 光谱检测完成提示（1秒蜂鸣）
│  注：通过 BuzzerManager 单例管理，GPIO26输出，非阻塞beep实现
├─ 时间管理模块
│  ├─ DS3231 RTC时钟读取与写入【可选】
│  ├─ 设备内部时间维护
│  └─ 服务器时间校准
├─ 本地存储模块
│  ├─ 数据持久化存储（SPIFFS/LittleFS）
│  ├─ 存储容量监测（低于20%自动清理）
│  └─ 离线数据队列管理
├─ 离线数据管理模块
│  ├─ 离线数据标记与存储
│  ├─ 网络恢复检测
│  └─ 历史数据补发（随机延迟0~10秒，防止多设备同时上传造成雪崩）
└─ 双模式系统
   ├─ 测试模式（消费者模式）
   │  ├─ 单次糖度检测
   │  ├─ 内存历史数据（最多20条）
   │  ├─ 累计均值计算
   │  ├─ 成熟度百分比显示
   │  └─ 清除按钮
   └─ 工业模式（生产采集模式）
      ├─ 自动定时检测（5分钟间隔）
      ├─ 完整数据上传
      ├─ 本地存储
      └─ 历史记录表格
```

### 模块2：瓜田数据处理中心（FieldDataProcessingServer）

```Plain Text

瓜田数据处理中心
├─ HTTP服务模块
│  ├─ 监听端口、解析请求、封装响应
│  └─ 静态文件托管（前端Web）
├─ 设备认证模块
│  ├─ 校验Header中device_id+token（device_auth表）
│  └─ 设备状态校验（status=1）
├─ 数据接收模块
│  ├─ 接收上传数据：设备编号、光谱、温度、湿度、光照
│  ├─ 从JSON读取数据采集时间戳（优先）
│  └─ 备用：使用服务器收到时刻作为时间戳
├─ 数据校验模块
│  ├─ 设备有效性校验（status=1）
│  ├─ 环境数据校验（三项不能全为99）
│  └─ 瓜田号合法性校验（匹配生产信息表）
├─ 数据计算模块
│  ├─ 光谱→糖度计算（MLR多元线性回归：Sugar = 0.0012*ch415 + 0.0005*ch445 + 0.0021*ch480 + 5.2）
│  └─ 糖度→成熟度计算（maturity_score = sugar_brix / mature_sugar_threshold）
├─ 数据库存储模块
│  ├─ 西瓜信息表写入
│  ├─ 瓜田环境表写入（主键冲突：时间戳+1s）
│  ├─ 设备认证表管理
│  ├─ 瓜田生产信息表管理
│  └─ 管理员信息表管理
├─ 时间同步服务模块
│  ├─ 设备握手接口（GET /api/device/time）
│  └─ 返回服务器当前时间戳
├─ 响应时间戳模块
│  └─ 每次响应加入server_time字段供设备校准
├─ 管理员认证模块
│  ├─ 密码哈希（Argon2，防暴力破解/侧信道攻击）
│  └─ Session+Cookie登录管理（1h过期）
├─ 接口服务模块
│  ├─ 设备数据上传接口（支持JSON时间戳）
│  ├─ 设备时间同步接口
│  ├─ 管理员登录/登出接口
│  ├─ 瓜田列表查询接口
│  ├─ 西瓜数据查询接口
│  ├─ 环境数据查询接口
│  └─ 历史数据查询接口
```

### 模块3：瓜田监测可视化平台（FieldMonitoringPlatform）

```Plain Text

瓜田监测可视化平台
├─ 前台首页模块
│  ├─ 产品介绍（背景/功能/开发者）
│  └─ 后台登录入口
├─ 管理员登录模块
│  ├─ 账号密码验证、Session存储
│  └─ Cookie会话管理
├─ 后台监控模块
│  ├─ 左侧：西瓜数据阵列（编号/成熟度/糖度）
│  ├─ 左侧：单西瓜糖度历史折线图（ECharts）
│  ├─ 右侧：瓜田环境折线图（温度/湿度/光照，ECharts）
│  ├─ 瓜田分页（当前瓜田/总数量）
│  ├─ 60秒无操作自动切换瓜田巡航
│  └─ 每6秒静默刷新图表数据
├─ 历史数据展示模块
│  ├─ 表格格式显示历史记录
│  ├─ 字段：Time/Sugar/Temp/Hum/Light/上传状态
│  └─ 上传状态标识（Y/N）
└─ 数据交互模块
   └─ 与后端接口通信、实时刷新数据
```

---

## 四、数据库设计

### 4.1 数据库信息


| 项目   | 值                    |
| ---- | -------------------- |
| 数据库名 | `sws_db`             |
| 字符集  | `utf8mb4`            |
| 排序规则 | `utf8mb4_unicode_ci` |


---

### 4.2 表结构设计

#### 表1：管理员信息表 `admin_users`

> 用于 Web 后台登录校验


| 字段名           | 类型           | 约束                                                    | 说明                        |
| ------------- | ------------ | ----------------------------------------------------- | ------------------------- |
| id            | INT          | PRIMARY KEY, AUTO_INCREMENT                           | 主键，唯一标识                   |
| username      | VARCHAR(64)  | UNIQUE, NOT NULL                                      | 管理员账号（唯一）                 |
| password_hash | VARCHAR(255) | NOT NULL                                              | libsodium Argon2id 哈希后的密码 |
| role          | TINYINT      | NOT NULL, DEFAULT 1                                   | 角色：0=超级管理员，1=普通管理员        |
| created_at    | TIMESTAMP    | DEFAULT CURRENT_TIMESTAMP                             | 创建时间                      |
| updated_at    | TIMESTAMP    | DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP | 更新时间                      |


**权限说明**：

- `role=0`（超级管理员）：最高权限，可查看数据、创建/删除普通管理员
- `role=1`（普通管理员）：仅可查看西瓜糖度和瓜田环境数据

```sql
CREATE TABLE admin_users (
    id INT PRIMARY KEY AUTO_INCREMENT,
    username VARCHAR(64) NOT NULL UNIQUE,
    password_hash VARCHAR(255) NOT NULL,
    role TINYINT NOT NULL DEFAULT 1 COMMENT '0=超级管理员, 1=普通管理员',
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

-- 初始化超级管理员账号 (密码: admin123)
-- 密码哈希由 libsodium Argon2id (crypto_pwhash_out) 生成
INSERT INTO admin_users (username, password_hash, role) VALUES
('admin', '$2b$12$LQv3c1yqBWVHxkd0LHAkCOYz6TtxMQJqhN8/X4bAq/xxx', 0);
```

---

#### 表2：设备校验表 `device_auth`

> 用于设备身份认证，防止非法设备上传数据


| 字段名        | 类型          | 约束                        | 说明              |
| ---------- | ----------- | ------------------------- | --------------- |
| device_id  | VARCHAR(32) | PRIMARY KEY               | 设备编号（瓜田号-瓜组-瓜号） |
| token      | VARCHAR(64) | NOT NULL, UNIQUE          | 设备唯一认证 Token    |
| status     | TINYINT     | NOT NULL, DEFAULT 1       | 状态：0=禁用，1=启用    |
| created_at | TIMESTAMP   | DEFAULT CURRENT_TIMESTAMP | 创建时间            |


```sql
CREATE TABLE device_auth (
    device_id VARCHAR(32) PRIMARY KEY COMMENT '设备编号：瓜田号-瓜组-瓜号',
    token VARCHAR(64) NOT NULL UNIQUE COMMENT '设备唯一认证Token',
    status TINYINT NOT NULL DEFAULT 1 COMMENT '0=禁用, 1=启用',
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
```

---

#### 表3：西瓜信息表 `watermelon_data`

> 存储每个西瓜的光谱、糖度、成熟度数据


| 字段名            | 类型           | 约束       | 说明                                         |
| -------------- | ------------ | -------- | ------------------------------------------ |
| device_id      | VARCHAR(32)  | NOT NULL | 设备编号（对应西瓜）                                 |
| collected_at   | INT          | NOT NULL | 数据采集时间戳（秒级，服务器生成）                          |
| sugar_brix     | DECIMAL(5,2) | NOT NULL | 当前糖度值（MLR算法计算：a*ch415 + b*ch445 + ... + K） |
| maturity_score | DECIMAL(5,3) | NOT NULL | 成熟度评分（糖度/成熟阈值，可超过1.0）                      |
| spectrum_json  | JSON         | NOT NULL | AS7341 光谱数据                                |



| 联合主键 | (device_id, collected_at) |
| ---- | ------------------------- |


```sql
CREATE TABLE watermelon_data (
    device_id VARCHAR(32) NOT NULL COMMENT '设备编号：瓜田号-瓜组-瓜号',
    collected_at INT NOT NULL COMMENT '数据采集时间戳（服务器生成）',
    sugar_brix DECIMAL(5,2) NOT NULL COMMENT '当前糖度值（MLR算法：a*ch415 + b*ch445 + ... + K）',
    maturity_score DECIMAL(5,3) NOT NULL COMMENT '成熟度评分（可超过1.0）',
    spectrum_json JSON NOT NULL COMMENT 'AS7341光谱数据 {"ch415":xxx, ...}',
    PRIMARY KEY (device_id, collected_at)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
```

**注意**：AS7341 是 8+4 通道传感器，spectrum_json 存储示例：

```json
{"ch415":123,"ch445":456,"ch480":789,"ch515":234,"ch555":567,"ch595":890,"ch640":321,"ch680":654,"chClear":999,"chNIR":111}
```

---

#### 表4：瓜田生产信息表 `field_production`

> 存储瓜田的生产信息，用于成熟度计算


| 字段名                    | 类型           | 约束                                                    | 说明              |
| ---------------------- | ------------ | ----------------------------------------------------- | --------------- |
| field_id               | VARCHAR(16)  | PRIMARY KEY                                           | 瓜田号（由生产者注册）     |
| watermelon_variety     | VARCHAR(64)  | NOT NULL                                              | 西瓜品种            |
| mature_sugar_threshold | DECIMAL(5,2) | NOT NULL                                              | 成熟糖度阈值（用于计算成熟度） |
| created_at             | TIMESTAMP    | DEFAULT CURRENT_TIMESTAMP                             | 创建时间            |
| updated_at             | TIMESTAMP    | DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP | 更新时间            |


```sql
CREATE TABLE field_production (
    field_id VARCHAR(16) PRIMARY KEY COMMENT '瓜田号',
    watermelon_variety VARCHAR(64) NOT NULL COMMENT '西瓜品种',
    mature_sugar_threshold DECIMAL(5,2) NOT NULL COMMENT '成熟糖度阈值（用于计算成熟度）',
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
```

**成熟度计算公式**：`maturity_score = sugar_brix / mature_sugar_threshold`

---

#### 表5：瓜田环境信息表 `field_environment`

> 存储瓜田环境监测数据（温度、湿度、光照）


| 字段名           | 类型           | 约束       | 说明                |
| ------------- | ------------ | -------- | ----------------- |
| field_id      | VARCHAR(16)  | NOT NULL | 瓜田号（从设备编号解析）      |
| collected_at  | INT          | NOT NULL | 数据采集时间戳（服务器生成）    |
| temperature_c | DECIMAL(5,2) | NOT NULL | 温度（℃），无传感器=99     |
| humidity_rh   | DECIMAL(5,2) | NOT NULL | 湿度（%RH），无传感器=99   |
| light_lux     | INT          | NOT NULL | 光照强度（Lux），无传感器=99 |



| 联合主键 | (field_id, collected_at) |
| ---- | ------------------------ |


```sql
CREATE TABLE field_environment (
    field_id VARCHAR(16) NOT NULL COMMENT '瓜田号',
    collected_at INT NOT NULL COMMENT '数据采集时间戳（服务器生成）',
    temperature_c DECIMAL(5,2) NOT NULL COMMENT '温度（℃），无传感器=99',
    humidity_rh DECIMAL(5,2) NOT NULL COMMENT '湿度（%RH），无传感器=99',
    light_lux INT NOT NULL COMMENT '光照强度（Lux），无传感器=99',
    PRIMARY KEY (field_id, collected_at)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
```

---

### 4.3 数据库初始化脚本

```sql
-- ============================================
-- SmartWatermelonSystem 数据库初始化脚本
-- 数据库名：sws_db
-- ============================================

CREATE DATABASE IF NOT EXISTS sws_db
    DEFAULT CHARSET=utf8mb4
    COLLATE=utf8mb4_unicode_ci;

USE sws_db;

-- 1. 管理员信息表
CREATE TABLE IF NOT EXISTS admin_users (
    id INT PRIMARY KEY AUTO_INCREMENT,
    username VARCHAR(64) NOT NULL UNIQUE,
    password_hash VARCHAR(255) NOT NULL,
    role TINYINT NOT NULL DEFAULT 1 COMMENT '0=超级管理员, 1=普通管理员',
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

-- 2. 设备校验表
CREATE TABLE IF NOT EXISTS device_auth (
    device_id VARCHAR(32) PRIMARY KEY,
    token VARCHAR(64) NOT NULL UNIQUE,
    status TINYINT NOT NULL DEFAULT 1 COMMENT '0=禁用, 1=启用',
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

-- 3. 西瓜信息表
CREATE TABLE IF NOT EXISTS watermelon_data (
    device_id VARCHAR(32) NOT NULL,
    collected_at INT NOT NULL,
    sugar_brix DECIMAL(5,2) NOT NULL,
    maturity_score DECIMAL(5,3) NOT NULL,
    spectrum_json JSON NOT NULL,
    PRIMARY KEY (device_id, collected_at)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

-- 4. 瓜田生产信息表
CREATE TABLE IF NOT EXISTS field_production (
    field_id VARCHAR(16) PRIMARY KEY,
    watermelon_variety VARCHAR(64) NOT NULL,
    mature_sugar_threshold DECIMAL(5,2) NOT NULL,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

-- 5. 瓜田环境信息表
CREATE TABLE IF NOT EXISTS field_environment (
    field_id VARCHAR(16) NOT NULL,
    collected_at INT NOT NULL,
    temperature_c DECIMAL(5,2) NOT NULL,
    humidity_rh DECIMAL(5,2) NOT NULL,
    light_lux INT NOT NULL,
    PRIMARY KEY (field_id, collected_at)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

-- ============================================
-- 初始化示例数据
-- ============================================

-- 初始化超级管理员 (用户名: admin, 密码: admin123)
-- libsodium Argon2id hash 由应用程序首次启动时自动生成 (见 PasswordHasher.cpp)
INSERT INTO admin_users (username, password_hash, role) VALUES
('admin', '$2b$12$LQv3c1yqBWVHxkd0LHAkCOYz6TtxMQJqhN8/X4bAq/xxx-placeholder', 0);

-- 初始化瓜田生产信息
INSERT INTO field_production (field_id, watermelon_variety, mature_sugar_threshold) VALUES
('1001', '黑美人', 12.50),
('1002', '麒麟瓜', 13.00),
('1003', '无籽西瓜', 11.50);

-- 初始化设备认证信息
INSERT INTO device_auth (device_id, token, status) VALUES
('1001-01-01', 'device-token-001', 1),
('1001-01-02', 'device-token-002', 1),
('1002-01-01', 'device-token-003', 1);
```

---

### 4.4 数据库连接配置

```ini
# config.ini
[database]
host = localhost
port = 3306
username = sws_user
password = Mhr289839.
database = sws_db
charset = utf8mb4
max_connections = 10
```

---

### 4.5 ER 关系图

```Plain Text
关系说明：

1. device_auth → field_production
   ├── 关系类型：1 : N
   ├── 关联键：device_id 的前缀（如 "1001-01-02" → "1001"）
   └── 含义：1个瓜田可包含多个设备

2. field_production → watermelon_data
   ├── 关系类型：1 : N
   ├── 关联键：field_id ← device_id 前缀
   └── 含义：1个瓜田可包含多个西瓜数据记录

3. field_production → field_environment
   ├── 关系类型：1 : N
   ├── 关联键：field_id
   └── 含义：1个瓜田可记录多条环境数据

4. admin_users、device_auth
   └── 独立实体，不与其他表关联
```

---

### 4.6 关键业务规则


| 规则         | 说明                                                                              |
| ---------- | ------------------------------------------------------------------------------- |
| **环境数据无效** | 温度=99 AND 湿度=99 AND 光照=99 → 环境数据跳过写入瓜田库，西瓜数据不受影响                                |
| **主键冲突解决** | 同一秒同一瓜田重复上传 → 时间戳+1秒后重试                                                         |
| **糖度计算算法** | MLR多元线性回归：Sugar = 0.0012×ch415 + 0.0005×ch445 + 0.0021×ch480 + 5.2（系数由前期实验数据拟合） |
| **成熟度计算**  | `maturity_score = sugar_brix / mature_sugar_threshold`                          |
| **设备禁用**   | `device_auth.status = 0` → HTTP 401 拒绝上传                                        |
| **瓜田号校验**  | 上传数据时必须校验瓜田号存在于 `field_production` 表                                            |


---

## 五、接口文档

分为**设备端 ↔ 服务端**、**前端 ↔ 服务端** 两类接口，**完全遵循你的业务规则**，包含请求头、参数、响应、校验逻辑。

### 基础约定

1. **通信协议**：HTTP/1.1
2. **数据格式**：请求/响应均为 `JSON`
3. **服务端地址**：`http://{服务器IP}:{端口}`（默认8080）
4. **设备编号规则**：`瓜田号-瓜组-瓜号`（例：1001-01-05）
5. **时间戳**：优先使用请求体JSON中的时间戳，其次使用服务器收到时刻（秒级）
6. **状态码**：
  - 200：成功
    - 401：设备/管理员认证失败
    - 400：数据校验失败
    - 500：服务器内部错误

---

### 第一部分：设备端 ↔ 服务端 接口文档

#### 接口1：设备数据上传接口

**核心作用**：设备端上传采集数据，服务端校验、计算、入库

- **接口地址**：`/api/device/upload`
- **请求方式**：`POST`
- **请求头（必传）**：


| 参数名       | 类型     | 说明              |
| --------- | ------ | --------------- |
| device_id | String | 设备编号（瓜田号-瓜组-瓜号） |
| token     | String | 设备唯一认证Token     |


- **请求体（JSON）**：

```JSON

{
  "spectrum_json": {"ch415":123,"ch445":456,"ch480":789}, // 多光谱数据
  "temperature": 25.5,    // 温度（无传感器=99）
  "humidity": 60.2,       // 湿度（无传感器=99）
  "light": 10000,         // 光照（无传感器=99）
  "collected_at": 1712345678  // 【可选】设备端采集时间戳，如不提供则使用服务器时间
}
```

- **服务端业务逻辑**：
  1. 校验`device_id+token` → 匹配`device_auth`表且`status=1`
  2. 解析设备编号提取**瓜田号**
  3. 校验瓜田号是否存在于`瓜田生产信息表`
  4. 校验环境数据：
    - 规则1：温度/湿度/光照不能全为99
    - 规则2：数值范围（温度-4080℃、湿度0100%、光照0~65535Lux）
  5. **获取数据采集时间戳**：
    - 优先：从请求体JSON的`collected_at`字段读取
    - 备用：使用服务器收到消息的当前时刻
  6. 如遇主键冲突，时间戳+1秒
  7. 计算糖度（MLR：Sugar = a×ch415 + b×ch445 + ... + K）→ 成熟度（糖度/成熟阈值）
  8. 入库：
    - 西瓜信息表（设备编号+时间戳+糖度+成熟度+光谱）
    - 瓜田环境表（瓜田号+时间戳+温湿度+光照）
  9. 返回响应
- **成功响应（200）**：

```JSON

{
  "code": 200,
  "msg": "数据上传成功",
  "data": {
    "sugar_brix": 12.5,
    "maturity_score": 1.02,
    "collected_at": 1712345678,
    "server_time": 1712345679  // 服务器当前时间戳，供设备校准时间
  }
}
```

- **失败响应**：
  - 401：`{"code":401,"msg":"设备认证失败"}`
  - 400：`{"code":400,"msg":"数据校验失败"}`

#### 接口2：设备时间同步接口

**核心作用**：设备首次联网时获取服务器当前时间，用于校准设备时间

- **接口地址**：`/api/device/time`
- **请求方式**：`GET`
- **请求头（必传）**：


| 参数名       | 类型     | 说明              |
| --------- | ------ | --------------- |
| device_id | String | 设备编号（瓜田号-瓜组-瓜号） |
| token     | String | 设备唯一认证Token     |


- **服务端业务逻辑**：
  1. 校验`device_id+token` → 匹配`device_auth`表且`status=1`
  2. 返回服务器当前时间戳
- **成功响应（200）**：

```JSON

{
  "code": 200,
  "server_time": 1712345678
}
```

---

### 第二部分：前端 ↔ 服务端 接口文档

#### 接口1：管理员登录接口

- **接口地址**：`/api/admin/login`
- **请求方式**：`POST`
- **请求体**：

```JSON

{
  "username": "admin",
  "password": "123456"
}
```

- **服务端逻辑**：
  1. Argon2验证密码 → 匹配`管理员信息表`（数据库存储 Argon2id hash）
  2. 生成唯一Session ID → 内存存储（1h过期）
  3. 返回Cookie：`session_id=xxx`
- **成功响应（200）**：

```JSON

{"code":200,"msg":"登录成功","role":0} // role=0超级管理员/1普通管理员
```

- **失败响应（401）**：`{"code":401,"msg":"账号或密码错误"}`

#### 接口2：获取所有瓜田列表

- **接口地址**：`/api/admin/field/list`
- **请求方式**：`GET`
- **请求头**：`Cookie: session_id=xxx`
- **成功响应**：

```JSON

{"code":200,"data":{"total":5,"field_list":[1001,1002,1003,1004,1005]}}
```

#### 接口3：获取单瓜田西瓜实时数据

- **接口地址**：`/api/admin/watermelon/list?field_id=1001`
- **请求方式**：`GET`
- **请求头**：`Cookie: session_id=xxx`
- **成功响应**：返回瓜田下所有西瓜的编号、糖度、成熟度、时间戳

#### 接口4：获取单西瓜历史糖度数据

- **接口地址**：`/api/admin/watermelon/history?device_id=1001-01-05`
- **请求方式**：`GET`
- **响应**：时间+糖度折线图数据

#### 接口5：获取单瓜田环境历史数据

- **接口地址**：`/api/admin/field/environment?field_id=1001`
- **请求方式**：`GET`
- **响应**：时间+温度+湿度+光照折线图数据

#### 接口6：管理员登出

- **接口地址**：`/api/admin/logout`
- **请求方式**：`POST`
- **逻辑**：销毁Session

---

## 六、核心交互流程

### 1. 设备端 → 服务端 数据上传流程

1. ESP32采集数据 → 本地显示 → 封装HTTP请求
2. 请求头携带`device_id+token`
3. 服务端认证 → 数据校验 → MLR计算糖度 → 计算成熟度
4. 写入MySQL → 返回结果
5. 设备端接收响应，完成上传

### 2. 前端 → 服务端 监控流程

1. 管理员登录 → 验证密码 → 获取Session
2. 进入后台 → 请求瓜田列表 → 渲染分页
3. 静默刷新（每6秒）→ 请求当前瓜田数据
4. 左侧渲染西瓜阵列，右侧渲染环境图表
5. 点击西瓜 → 请求历史数据 → 渲染折线图

### 3. 关键规则落地

- **环境数据无效**：温湿度光照全为99 → 400拒绝入库
- **主键冲突**：同一秒同一瓜田 → 时间戳+1s
- **成熟度计算**：`成熟度 = 计算糖度 / 瓜田成熟糖度阈值`
- **糖度算法**：`Sugar = 0.0012×ch415 + 0.0005×ch445 + 0.0021×ch480 + 5.2`
- **设备禁用**：`device_auth`表`status=0` → 401拒绝上传

---

## 七、设备端硬件详细说明

### 7.1 硬件清单

本系统设备端采用 **ESP32 主控 + 模块化传感器** 架构，以下是完整硬件清单：

#### 主控板


| 硬件   | 型号/规格           | 数量  | 说明                   |
| ---- | --------------- | --- | -------------------- |
| 主控芯片 | ESP32 DevKit V1 | 1   | 双核处理器，WiFi+蓝牙，240MHz |
| 存储   | 4MB Flash       | 1   | 程序存储                 |
| 工作电压 | 3.3V            | -   | 供电范围 3.0V~3.6V       |


#### 显示屏模块


| 硬件     | 型号/规格        | 数量  | 说明          |
| ------ | ------------ | --- | ----------- |
| TFT显示屏 | 2.8寸 ILI9341 | 1   | 分辨率 240×320 |
| 接口类型   | SPI          | -   | 需12根引脚连接    |
| 触摸功能   | XPT2046触摸芯片  | 1   | 支持触摸操作      |


#### 传感器模块


| 硬件     | 型号/规格        | 数量  | 说明                     |
| ------ | ------------ | --- | ---------------------- |
| 光谱传感器  | GY-AS7341-V1 | 1   | 11通道光谱采集（8个可见光+白光+近红外） |
| 温湿度传感器 | SHT31        | 1   | 精度 ±0.2℃，I2C接口         |
| 光照传感器  | BH1750       | 1   | 量程 0~65535 Lux，I2C接口   |


#### 光源模块


| 硬件    | 型号/规格      | 数量  | 说明             |
| ----- | ---------- | --- | -------------- |
| LED灯带 | 3W 宽谱白光    | 1   | 采集时开启，消除环境光干扰  |
| 驱动方式  | GPIO + 三极管 | -   | 不能直连ESP32 GPIO |


#### 配套材料


| 材料        | 规格         | 数量  | 说明      |
| --------- | ---------- | --- | ------- |
| 面包板       | 830孔       | 1   | 原型搭建    |
| 杜邦线       | 公对母/母对母    | 若干  | 传感器连接   |
| 面包板电源     | 3.3V/5V双电源 | 1   | 为传感器供电  |
| microUSB线 | 数据线        | 1   | 程序烧录与供电 |


---

### 7.2 硬件接线图

#### ESP32 引脚分配


| 功能        | ESP32引脚 | 说明        |
| --------- | ------- | --------- |
| TFT_MISO  | GPIO19  | SPI主机输入   |
| TFT_MOSI  | GPIO23  | SPI主机输出   |
| TFT_SCLK  | GPIO18  | SPI时钟     |
| TFT_CS    | GPIO5   | TFT片选     |
| TFT_DC    | GPIO21  | 数据/命令选择   |
| TFT_RST   | GPIO22  | 复位        |
| TFT_BL    | GPIO4   | 背光控制      |
| SDA       | GPIO27  | I2C数据线    |
| SCL       | GPIO22  | I2C时钟线    |
| LED_POWER | GPIO26  | LED灯带控制   |
| BUZZER    | GPIO27  | 蜂鸣器控制（预留） |


#### I2C 总线接线（所有传感器共用）

```
ESP32          GY-AS7341     SHT31       BH1750
─────────      ─────────     ─────       ───────
GPIO21 (SDA) → SDA引脚      SDA引脚    SDA引脚
GPIO22 (SCL) → SCL引脚      SCL引脚    SCL引脚
3.3V        → VCC引脚      VCC引脚    VCC引脚
GND         → GND引脚      GND引脚    GND引脚
```

#### SPI 总线接线（TFT显示屏）

```
ESP32          ILI9341 TFT
─────────      ───────────
GPIO18      → CLK引脚
GPIO23      → MOSI引脚
GPIO19      → MISO引脚 (可选，用于触摸)
GPIO5       → CS引脚
GPIO21      → DC引脚
GPIO22      → RST引脚
GPIO4       → LED引脚 (背光)
3.3V        → VCC引脚
GND         → GND引脚
```

---

### 7.3 传感器详细参数

#### AS7341 多光谱传感器


| 参数     | 值                                                                       |
| ------ | ----------------------------------------------------------------------- |
| 通信接口   | I2C（地址0x39）                                                             |
| 光谱通道   | 11个（ch415, ch445, ch480, ch515, ch555, ch595, ch640, ch680, Clear, NIR） |
| ADC分辨率 | 16位                                                                     |
| 动态范围   | 可配置增益（GAIN）                                                             |
| 工作电压   | 3.3V                                                                    |
| 数据格式   | 整数（0~65535）                                                             |


**光谱通道波长对照**：


| 通道      | 中心波长  | 半宽  |
| ------- | ----- | --- |
| ch415   | 415nm | 紫色  |
| ch445   | 445nm | 蓝色  |
| ch480   | 480nm | 青蓝色 |
| ch515   | 515nm | 绿色  |
| ch555   | 555nm | 黄绿色 |
| ch595   | 595nm | 黄色  |
| ch640   | 640nm | 橙色  |
| ch680   | 680nm | 红色  |
| chClear | -     | 白光  |
| chNIR   | -     | 近红外 |


#### SHT31 温湿度传感器


| 参数   | 值                |
| ---- | ---------------- |
| 通信接口 | I2C（地址0x44或0x45） |
| 温度范围 | -40℃ ~ +125℃     |
| 温度精度 | ±0.2℃（典型）        |
| 湿度范围 | 0% ~ 100% RH     |
| 湿度精度 | ±2% RH（典型）       |
| 工作电压 | 2.4V ~ 5.5V      |
| 数据格式 | 浮点数              |


#### BH1750 光照传感器


| 参数   | 值                |
| ---- | ---------------- |
| 通信接口 | I2C（地址0x23或0x5C） |
| 光照范围 | 0 ~ 65535 Lux    |
| 测量精度 | 1 Lux            |
| 工作电压 | 3.3V ~ 5V        |
| 数据格式 | 整数               |


---

### 7.4 硬件实物连接注意事项

1. **供电安全**
  - ESP32 工作电压为 3.3V，不要直接连接 5V
  - 所有传感器模块均有 3.3V 稳压芯片，可安全使用 5V 供电
  - 面包板电源模块建议设置为 3.3V 模式
2. **I2C 总线**
  - 所有 I2C 设备共用地线和电源，SDA/SCL 并联
  - 如需加装上拉电阻（4.7kΩ），可增强信号稳定性
  - I2C 地址冲突检查：
    - AS7341：0x39（默认）
    - SHT31：0x44（默认，引脚可切换0x45）
    - BH1750：0x23（默认，引脚可切换0x5C）
3. **LED 驱动**
  - 3W LED 不能直接连接 GPIO，会烧毁芯片
  - 使用 NPN 三极管（如 S8050）作为开关
  - 基极通过 330Ω 电阻连接 ESP32 GPIO
4. **显示屏安装**
  - SPI 连接需注意杜邦线尽量短，减少干扰
  - 背光引脚可连接 PWM，实现亮度调节

---

## 八、系统访问凭证

### 8.1 凭证汇总

> **⚠️ 安全提示**：以下为系统开发/部署所需的所有密码凭证，请妥善保管，切勿泄露。


| 系统/服务    | 用户名      | 密码             | 用途           |
| -------- | -------- | -------------- | ------------ |
| 云服务器     | root     | **Mhr289839.** | SSH远程登录、系统管理 |
| MySQL数据库 | sws_user | **Mhr289839.** | 后端服务连接数据库    |
| MySQL数据库 | root     | **Mhr289839.** | 数据库管理员操作     |
| Web管理后台  | admin    | admin123       | 管理员登录（默认密码）  |


