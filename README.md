这是一份为你量身定制的**《智能西瓜无损糖度检测系统 (SmartWatermelonSystem) 完整项目白皮书》**。

这份文档不仅是对你整个开发过程的心血总结，更是可以直接作为**毕业设计论文核心章节、GitHub 开源 README、以及交接给其他开发人员（或 AI）的标准技术文档**。

你可以将其保存为 `README.md` 放在项目的根目录下。

---

# 🍉 智能西瓜无损糖度检测系统 (SmartWatermelonSystem)

## 1. 项目概述
本项目是一个集成了 **“物联网边缘计算 + C++ 高性能服务 + 现代 Web 数据可视化”** 的全栈级智慧农业解决方案。
系统通过 ESP32 结合多光谱传感器（AS7341），利用 MLR（多元线性回归）算法实现西瓜糖度的无损检测。同时监测瓜田微型气象（温湿度、光照），提供完整的离线缓存、断点续传、时钟同步及多维度的数据可视化能力。

---

## 2. 核心功能特性

### 🔌 硬件感知端 (Edge Device)
* **双模式无缝切换**：
  * **消费者测试模式 (Local Test)**：无需联网，提供纯净的糖度检测，支持本地 20 次历史记录与均值计算，支持一键清空。
  * **工业监控模式 (Industrial)**：全链路采集，包含左滑实时数据页与右滑历史上传表格页。
* **高可用离线容灾引擎**：内置 `LittleFS` 文件系统。断网时数据无损落盘；网络恢复后，触发 **0~10秒随机错峰延迟** 进行旧数据静默补发，防止服务器雪崩。带有磁盘容量保护（空间不足20%自动清理一半旧数据）。
* **多源绝对时间同步**：采用 `DS3231 RTC` 模块 + `NTP/HTTP` 授时双保险。确保离线断电状态下时间戳依然精准，拒绝 1970 年脏数据。
* **柔性传感器架构**：温湿度、光照传感器支持热拔插。缺失时自动填入 `99.0`，服务器仅存储合法的西瓜光谱数据，不阻断核心业务。
* **多维感官交互**：搭载 2.8 寸 TFT 屏幕（基于 `LVGL 8` 独立触摸总线驱动），配合蜂鸣器提供不同场景的音频反馈。

### ⚙️ 后端服务中心 (C++ Server)
* **极致性能与内存安全**：基于 `cpp-httplib` 构建多线程 HTTP 服务器，彻底解耦静态资源托管与 RESTful API 路由。
* **金融级安全防御**：
  * **防 SQL 注入**：所有网络入参均通过 `mysql_real_escape_string` 严格转义过滤。
  * **抗侧信道攻击**：放弃传统 MD5/bcrypt，采用目前最顶级的 **Argon2id (`libsodium`)** 算法进行管理员密码哈希。
* **智能数据校验与时间中枢**：校验设备 Token，拦截无效环境数据；提供 `/api/device/time` 全局授时接口。

### 💻 监控大屏 (Web Dashboard)
* **现代拟态 UI 设计**：采用 Glassmorphism（毛玻璃）特效、自适应 Grid 网格布局。
* **无人值守大屏巡航**：60 秒无鼠标/键盘操作，自动循环切换监控瓜田；后台每 6 秒静默轮询最新数据，图表无闪烁热更新。
* **设备健康状态监控**：独创超时判定算法，当设备超过 30 分钟未上传数据，西瓜卡片自动变灰并展示红色闪烁警告 `⚠️ 超时未更新`。
* **上帝视角连续时间轴**：彻底重写 ECharts 坐标系，采用 `type: 'value'` 降维打击，支持 `5分钟/2小时/1天/1周` 四种时间尺度的平滑按钮切换，数据精准落点，拒绝时间轴重叠。

---

## 3. 项目架构与技术栈

系统采用标准的 **“端-管-云”** 三层架构：

* **感知层 (硬件端)**: C++ / Arduino 框架 / PlatformIO 构建。
  * 主控：ESP32 DevKit V1
  * 传感器：AS7341 (多光谱), SHT31 (高精度温湿度), BH1750 (光照), DS3231 (RTC)
  * 交互：ILI9341 TFT + XPT2046 独立触摸 + 蜂鸣器 + 3W LED
* **服务层 (后端)**: C++14 / CMake 构建。
  * 核心库：`cpp-httplib` (网络), `nlohmann/json` (JSON处理), `<mysql/mysql.h>` (数据库), `libsodium` (加密)
  * 数据库：MySQL 8.0+ (大量运用 JSON 字段结构化存储)
* **应用层 (前端)**: 纯 Vanilla HTML5 + CSS3 + 原生 ES6 JavaScript。
  * 图表引擎：ECharts v5.5 (支持纯局域网离线渲染)

---

## 4. 核心目录结构

```text
SmartWatermelonSystem/
├── FieldAcquisitionTerminal/      # 硬件设备端 (PlatformIO)
│   ├── platformio.ini             # 硬件编译配置 (库依赖、引脚宏定义)
│   └── src/
│       ├── main.cpp               # 主控状态机、UI 回调分发
│       ├── config.h               # 全局宏定义 (WiFi, ServerIP, 引脚)
│       ├── algorithm/sugar_calc   # 光谱 -> 糖度 MLR 离线算法
│       ├── display/lvgl_ui        # LVGL 界面绘制 (启动页、双模式)
│       ├── network/HttpClient     # 网络重连、HTTP 请求封装
│       ├── sensor_driver/         # I2C 传感器与蜂鸣器驱动
│       └── storage/StorageManager # LittleFS 本地存储与容量管控
│
├── FieldDataProcessingServer/     # C++ 后端服务器 (CMake)
│   ├── CMakeLists.txt             # 编译链接配置 (依赖 Threads, MySQL, Sodium)
│   ├── sql/init_db.sql            # 数据库建表与初始化假数据脚本
│   └── src/
│       ├── main.cpp               # 数据库初始化、启动 Http 服务
│       ├── auth/                  # AdminAuth (Argon2), DeviceAuth
│       ├── data_process/          # 光谱计算、环境校验、成熟度计算
│       ├── db/MySQLDriver         # MySQL 驱动封装 (防注入转义)
│       ├── session/Session        # 内存级 Session 与 UUID 生成
│       └── http_server/Router     # RESTful 接口注册中心
│
└── FieldMonitoringPlatform/       # Web 可视化前端 (原生 JS)
    ├── index.html                 # 门户首页
    ├── admin/
    │   ├── login.html             # 登录视图
    │   ├── dashboard.html         # 大屏监控视图
    │   ├── css/style.css          # 全局样式主题
    │   └── js/
    │       ├── api.js             # Fetch 请求封装
    │       ├── auto_switch.js     # 轮询定时器、时间尺度换算、防抖刷新
    │       └── chart.js           # ECharts 配置构建器、西瓜阵列渲染
    └── assets/libs/echarts.min.js # 离线依赖图表库
```

---

## 5. 核心 API 接口协议

| 接口路径 | 请求方式 | 鉴权要求 | 核心功能 |
| --- | --- | --- | --- |
| `/api/device/time` | `GET` | 无 | 设备开机握手，返回服务器秒级时间戳 |
| `/api/device/upload` | `POST` | Header: `token` | 接收光谱与环境数据，处理容灾与防注入入库，返回校时时间戳 |
| `/api/admin/login` | `POST` | 无 | Argon2 校验密码，成功颁发 Cookie (`session_id`) |
| `/api/admin/field/list` | `GET` | Cookie 校验 | 获取当前数据库中已注册的瓜田列表 |
| `/api/admin/watermelon/list`| `GET` | Cookie 校验 | 根据瓜田号，获取旗下所有西瓜的最新实时糖度与成熟度 |
| `/api/admin/watermelon/history`| `GET` | Cookie 校验 | 获取单个西瓜近 84 天的糖度变化折线数据 |
| `/api/admin/field/environment` | `GET` | Cookie 校验 | 获取瓜田近 84 天的温湿度、光照散点数据 |

---

## 6. 系统部署与启动指南

### 6.1 数据库准备
1. 确保安装 MySQL 8.0+。
2. 创建 `sws_user` 用户并赋予权限。
3. 执行 `FieldDataProcessingServer/sql/init_db.sql` 脚本，完成 5 张核心数据表的创建与基础管理员账号写入（默认账号：`admin` / 密码：`admin123`）。

### 6.2 编译启动 C++ 后端
```bash
# 1. 安装系统级依赖 (CentOS 示例)
sudo yum install mysql-devel libsodium-devel cmake gcc-c++

# 2. 进入后端目录并编译
cd FieldDataProcessingServer
mkdir build && cd build
cmake ..
cmake --build .

# 3. 运行服务器 (默认监听 8080 端口，并自动挂载前端 HTML 目录)
./SmartWatermelonServer
```

### 6.3 访问大屏前端
浏览器直接访问云服务器 IP：
`http://<服务器公网IP>:8080/`

*(注：系统包含严密的 302 拦截机制，未登录直接访问 `/admin/dashboard.html` 将被强行阻断并踢回登录页)*

### 6.4 烧录硬件终端
1. 在 VSCode 中打开 `FieldAcquisitionTerminal` 目录。
2. 编辑 `src/config.h`，填入真实的 `WIFI_SSID`、`WIFI_PASS` 以及修改 `SERVER_URL` 为你的云服务器 IP。
3. 确保底层硬件引脚连线与 `platformio.ini`及 `config.h` 严格对应（特别是双 SPI 隔离总线）。
4. 点击 PlatformIO 底部的 `Upload` 按钮，编译并烧录至 ESP32。

---

## 7. 日常维护与排错 (Troubleshooting)

1. **设备屏幕卡在 "Waiting for Time Sync"**
   - **检查：** DS3231 电池是否耗尽、I2C 连线是否松动；同时检查配置的 WiFi 账号密码是否正确（如果没有 RTC，系统必须依赖 WiFi 授时才能进入工业模式）。
2. **设备上传数据提示 "ERR: Spectrum Failed"**
   - **检查：** AS7341 传感器 I2C 总线脱落，或者遇到了 I2C 地址冲突。
3. **大屏西瓜卡片变成红色虚线并闪烁警告**
   - **说明：** 触发了 30 分钟掉线判定机制。请检查该硬件终端是否断电，或者设备所处位置 WiFi 信号太弱导致离线数据堆积在本地 `LittleFS` 中。
4. **编译后端提示 `sodium.h` 找不到**
   - **检查：** Linux 操作系统缺失 `libsodium-dev` 依赖包，请使用 `apt-get` 或 `yum` 安装。

---
*文档终版生成日期：2026年4月* | *系统设计架构师：Howrun*