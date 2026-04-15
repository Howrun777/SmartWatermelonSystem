# SmartWatermelonSystem 需求文档

> 毕业设计项目：智能西瓜无损糖度检测系统
>
> 本文档严格按照实际代码实现编写，完整覆盖设备端、后端服务器、前端三个模块的所有功能。

---

## 一、项目概述

本系统旨在解决传统西瓜成熟度检测中破坏性强、抽检局限的问题。通过多光谱传感器与物联网技术，实现瓜田环境的实时监控与西瓜糖度的精准无损检测，并将数据上传至云端服务器，最终在 Web 端以可视化大屏呈现。

系统分为三大模块：

| 模块 | 名称 | 职责 | 技术栈 |
|---|---|---|---|
| 感知层 | 瓜田环境采集终端 FieldAcquisitionTerminal | 数据采集（光谱/温湿度/光照）、本地显示、HTTP上传 | ESP32、LVGL、I2C传感器 |
| 服务层 | 瓜田数据处理中心 FieldDataProcessingServer | 设备认证、数据接收、校验、计算（糖度/成熟度）、MySQL存储 | C++、cpp-httplib、MySQL、libsodium |
| 应用层 | 瓜田监测可视化平台 FieldMonitoringPlatform | 前台展示、登录鉴权、数据可视化、自动轮询监控大屏 | HTML/CSS/JS、ECharts、Cookie |

---

## 二、模块一：设备端（FieldAcquisitionTerminal）

### 2.1 硬件配置

| 硬件 | 型号/规格 | 说明 |
|---|---|---|
| 主控芯片 | ESP32 DevKit V1 | 双核240MHz，WiFi+蓝牙 |
| 多光谱传感器 | GY-AS7341-V1 | 11通道（8可见光+白光+近红外），I2C地址0x39 |
| 温湿度传感器 | SHT31 | 精度±0.2℃，I2C地址0x44/0x45（可选） |
| 光照传感器 | BH1750 | 量程0~65535 Lux，I2C地址0x23/0x5C（可选） |
| 光源 | 3W宽谱白光LED | GPIO26驱动（三极管开关），采集时开启 |
| 实时时钟 | DS3231 | I2C，断电保持（可选） |
| 蜂鸣器 | 有源蜂鸣器 | GPIO26，低电平触发 |
| 显示屏 | 2.8寸TFT ILI9341 + XPT2046触摸 | SPI接口，240×320分辨率 |

### 2.2 设备配置（config.h）

| 配置项 | 默认值 | 说明 |
|---|---|---|
| DEVICE_ID | "1001-01-01" | 瓜田号-瓜组-瓜号 |
| DEVICE_TOKEN | "device-token-001" | 设备认证令牌 |
| SERVER_URL | "http://47.107.41.102:8080/api/device/upload" | 后端地址 |
| ENV_UPDATE_INTERVAL | 2000 ms | 环境数据采集间隔 |
| AUTO_UPLOAD_INTERVAL | 300000 ms（5分钟） | 自动上传间隔 |

### 2.3 传感器管理模块（SensorManager）

- **begin()**：初始化I2C总线、检测AS7341/SHT31/BH1750是否在线、配置AS7341（ATIME=50/ASTEP=999/GAIN=256X）
- **readEnvironment(float &temp, float &hum, int &light)**：读取SHT31温湿度+BH1750光照；传感器缺失时返回99.0/99占位
- **readSpectrum(JsonObject &doc)**：调用AS7341读取10通道数据（ch415/ch445/ch480/ch515/ch555/ch595/ch640/ch680/Clear/NIR），写入JSON对象

### 2.4 本地糖度计算（sugar_calc.h）

使用MLR多元线性回归模型（系数由前期标定实验拟合）：

```
Sugar = 0.0012 × ch415 + 0.0005 × ch445 + 0.0021 × ch480 + 5.2
```

结果限制在0.0~20.0 Brix范围。

### 2.5 双模式系统

#### 2.5.1 测试模式（消费者模式）

- 单次手动触发糖度检测
- 内存存储最多20条历史记录（不掉盘）
- 实时计算并显示：糖度、成熟度百分比、累计均值
- 不上传数据到服务器
- 环境数据固定为99（不采集）
- 提供清除历史按钮

#### 2.5.2 工业模式（生产采集模式）

- 5分钟自动定时检测
- 每次测量完成后立即上传到服务器
- 完整采集环境数据（温度/湿度/光照）
- 数据持久化到LittleFS
- 实时倒计时显示距下次自动采集的时间
- 历史记录表格显示（Time/Brix/Temp/Hum/Light/Upload Status）

### 2.6 时间管理模块（RTCManager，单例）

- **getTimestamp()**：优先从DS3231读取；降级使用ESP32内部`gettimeofday()`
- **syncTime(server_time)**：同时更新ESP32系统时钟和DS3231硬件时钟
- **formatTime(timestamp)**：UTC+8小时偏移，格式`YYYY/MM/DD HH:mm:ss`
- 设备端发起HTTP上传时在请求体中附加`collected_at`字段（优先采用），若无则由服务器生成时间戳

### 2.7 网络通信模块（NetworkManager，静态工具类）

- **connectWiFi()**：非阻塞连接，最多等待10秒；超时时设备进入离线模式
- **maintainWiFi()**：断线后每30秒自动重连（非阻塞）
- **fetchServerTime()**：GET `/api/device/time`，返回服务器Unix时间戳（秒）
- **uploadData(payload)**：POST JSON到服务器，响应中提取`server_time`供校时

### 2.8 本地存储模块（StorageManager，单例）

基于LittleFS，JSON Lines格式存储：

- **saveRecord(ts, brix, t, h, l, spec, is_uploaded)**：追加写入`/records.jsonl`
- **checkAndCleanCapacity()**：已用容量>80%时删除最早的50%记录
- **getUnuploadedRecord(doc, ts)**：顺序返回第一条`up=0`的记录
- **markAsUploaded(target_ts)**：将指定时间戳记录的`up`字段从0改为1

### 2.9 离线数据补发机制

- 每次网络连接成功后（WiFi边沿检测），自动触发补发状态机
- 按文件顺序查找所有`up=0`记录，依次补发
- 补发前随机延迟0~10秒，防止多设备同时上传造成服务器压力
- 补发成功后立即标记`up=1`
- 新数据上传优先级高于老数据（先发新，再补旧）

### 2.10 蜂鸣器反馈模块（BuzzerManager，单例）

- WiFi连接成功 → 响1秒
- 糖度检测完成 → 响1秒
- `loop()`必须在主循环中调用（非阻塞）

### 2.11 LVGL显示界面（UIManager）

4个页面：启动页、模式选择页、测试模式页、工业模式页

**启动页**：淡绿色背景，设备编号，西瓜种类，时间同步状态（"正在等待时间更新..."）

**模式选择页**：时间同步成功后动态显示"测试模式"和"工业模式"按钮

**测试模式**：单次检测按钮、糖度/成熟度实时显示、历史记录表格（No/Brix/M%/Avg/AM%）、清除按钮

**工业模式**：双TileView设计

- 第一页（实时数据）：设备编号、糖度大数字、成熟度百分比、温湿度光照四格数据、下次自动检测倒计时、立即检测按钮
- 第二页（历史记录）：Time/Brix/Temp/Hum/Light列表格，Upload列Y/N标识

---

## 三、模块二：后端服务器（FieldDataProcessingServer）

### 3.1 技术架构

- **编程语言**：C++17
- **HTTP库**：cpp-httplib
- **JSON库**：nlohmann/json
- **密码哈希**：libsodium（Argon2id）
- **数据库**：MySQL（sws_db，utf8mb4）
- **服务器配置**：config.ini

### 3.2 认证与安全

#### 3.2.1 设备认证（DeviceAuth）

- 验证HTTP Header中的`device_id`+`token`
- 查询`device_auth`表，校验token匹配且`status=1`（启用）
- 认证失败返回HTTP 401 Unauthorized

#### 3.2.2 管理员认证（AdminAuth + Session）

- 密码存储：Argon2id哈希（防暴力破解、GPU加速攻击、侧信道攻击）
- 登录成功后生成32字符UUID Session ID，1小时过期，存储在内存`unordered_map`中
- HTTP响应通过`Set-Cookie: session_id=xxx`返回
- 后续请求验证Cookie中的Session ID有效性

### 3.3 数据接收与处理

#### 3.3.1 设备数据上传接口 POST /api/device/upload

**请求头**：`device_id`、`token`、`Content-Type: application/json`

**请求体**：

```json
{
  "spectrum_json": {"ch415":123,"ch445":456,"ch480":789,"ch515":234,"ch555":567,"ch595":890,"ch640":321,"ch680":654,"chClear":999,"chNIR":111},
  "temperature": 25.5,
  "humidity": 60.2,
  "light": 10000,
  "collected_at": 1712345678
}
```

**服务端处理流程**：

1. 设备Token认证
2. 从设备编号解析出瓜田号（取前缀，如"1001-01-01"→"1001"）
3. 校验瓜田号存在于`field_production`表
4. 数据合法性校验：
   - 环境三值不全为99
   - 温度范围 -40~80℃、湿度 0~100%、光照 0~65535 Lux
5. 获取数据采集时间戳：优先从JSON读取，备用服务器当前时间
6. 若`field_environment`主键冲突（同一秒同一瓜田），时间戳+1秒重试
7. MLR计算糖度 + 成熟度计算
8. 同时写入`watermelon_data`和`field_environment`表
9. 响应中携带`server_time`供设备校时

**成功响应**：

```json
{
  "code": 200,
  "msg": "数据上传成功",
  "data": {
    "sugar_brix": 12.5,
    "maturity_score": 1.02,
    "collected_at": 1712345678,
    "server_time": 1712345679
  }
}
```

### 3.4 数据计算模块

#### 3.4.1 糖度计算（SugarCalc）

```
Sugar = 0.0012 × ch415 + 0.0005 × ch445 + 0.0021 × ch480 + 5.2
```

#### 3.4.2 成熟度计算（MaturityCalc）

```
maturity_score = sugar_brix / mature_sugar_threshold
```

- 从`field_production`表读取该瓜田的`mature_sugar_threshold`
- 可超过1.0（如糖度13.0 / 阈值12.0 = 1.083）

### 3.5 数据库设计

#### admin_users（管理员信息表）

| 字段 | 类型 | 约束 | 说明 |
|---|---|---|---|
| id | INT | PK, AUTO_INCREMENT | 主键 |
| username | VARCHAR(64) | UNIQUE, NOT NULL | 账号 |
| password_hash | VARCHAR(255) | NOT NULL | Argon2id哈希 |
| role | TINYINT | DEFAULT 1 | 0=超级管理员，1=普通管理员 |
| created_at | TIMESTAMP | DEFAULT CURRENT_TIMESTAMP | 创建时间 |
| updated_at | TIMESTAMP | AUTO UPDATE | 更新时间 |

#### device_auth（设备认证表）

| 字段 | 类型 | 约束 | 说明 |
|---|---|---|---|
| device_id | VARCHAR(32) | PK | 设备编号（瓜田号-瓜组-瓜号） |
| token | VARCHAR(64) | UNIQUE, NOT NULL | 认证令牌 |
| status | TINYINT | DEFAULT 1 | 0=禁用，1=启用 |
| created_at | TIMESTAMP | DEFAULT CURRENT_TIMESTAMP | 创建时间 |

#### watermelon_data（西瓜信息表）

| 字段 | 类型 | 约束 | 说明 |
|---|---|---|---|
| device_id | VARCHAR(32) | NOT NULL | 设备编号 |
| collected_at | INT | NOT NULL | 数据采集时间戳（秒） |
| sugar_brix | DECIMAL(5,2) | NOT NULL | 糖度 |
| maturity_score | DECIMAL(5,3) | NOT NULL | 成熟度评分 |
| spectrum_json | JSON | NOT NULL | AS7341光谱数据 |

**联合主键**：(device_id, collected_at)

#### field_production（瓜田生产信息表）

| 字段 | 类型 | 约束 | 说明 |
|---|---|---|---|
| field_id | VARCHAR(16) | PK | 瓜田号 |
| watermelon_variety | VARCHAR(64) | NOT NULL | 品种 |
| mature_sugar_threshold | DECIMAL(5,2) | NOT NULL | 成熟糖度阈值 |
| created_at | TIMESTAMP | DEFAULT CURRENT_TIMESTAMP | 创建时间 |
| updated_at | TIMESTAMP | AUTO UPDATE | 更新时间 |

#### field_environment（瓜田环境信息表）

| 字段 | 类型 | 约束 | 说明 |
|---|---|---|---|
| field_id | VARCHAR(16) | NOT NULL | 瓜田号 |
| collected_at | INT | NOT NULL | 时间戳（秒） |
| temperature_c | DECIMAL(5,2) | NOT NULL | 温度（℃） |
| humidity_rh | DECIMAL(5,2) | NOT NULL | 湿度（%RH） |
| light_lux | INT | NOT NULL | 光照（Lux） |

**联合主键**：(field_id, collected_at)

### 3.6 API接口总表

| 接口 | 方法 | 路径 | 认证 | 功能 |
|---|---|---|---|---|
| 心跳检测 | GET | `/ping` | 无 | 返回pong |
| 设备时间同步 | GET | `/api/device/time` | 设备Token | 返回服务器Unix时间戳 |
| 设备数据上传 | POST | `/api/device/upload` | 设备Token | 接收数据→校验→计算→入库→返回糖度/成熟度/server_time |
| 管理员登录 | POST | `/api/admin/login` | 无 | Argon2验证→Set-Cookie |
| 管理员登出 | POST | `/api/admin/logout` | Session | 销毁Session |
| 瓜田列表 | GET | `/api/admin/field/list` | Session | 返回所有瓜田及品种 |
| 西瓜实时数据 | GET | `/api/admin/watermelon/list` | Session | 返回瓜田下每个设备最新一条数据（含collected_at） |
| 西瓜历史糖度 | GET | `/api/admin/watermelon/history` | Session | 返回单设备最近84天糖度数据 |
| 瓜田环境数据 | GET | `/api/admin/field/environment` | Session | 返回瓜田最近84天温湿度光照数据 |
| 静态文件托管 | GET | `/*` | 无/重定向 | HTML/CSS/JS/ECharts库 |

### 3.7 关键业务规则

| 规则 | 说明 |
|---|---|
| 环境数据无效 | 温度=99 AND 湿度=99 AND 光照=99 → 跳过field_environment写入，西瓜数据不受影响 |
| 主键冲突解决 | 同一秒同一瓜田重复上传 → 时间戳+1秒后重试 |
| 设备禁用 | device_auth.status=0 → HTTP 401 |
| 瓜田号校验 | 上传时必须校验瓜田号存在field_production表 |
| 时间戳优先级 | 优先采用请求体JSON中的collected_at，否则用服务器时间 |

---

## 四、模块三：前端（FieldMonitoringPlatform）

### 4.1 技术架构

- **视图层**：HTML5 + 原生CSS3（CSS Variables、Flexbox、Grid）
- **逻辑层**：原生JavaScript（ES6+、Async/Await、Fetch API）
- **可视化**：ECharts v5.5.0（离线部署，防断网）
- **部署方式**：由后端C++服务器（cpp-httplib）挂载静态目录直接托管

### 4.2 前台首页（index.html）

- 产品介绍区域：系统背景、功能说明
- 技术特性展示卡片：无损糖度检测、环境实时监控、智能监控大屏
- 后台登录入口按钮（→ admin/login.html）
- 底部：开发团队联系方式

### 4.3 管理员登录模块（admin/login.html + api.js）

- 账号密码表单提交
- POST `/api/admin/login`
- 登录成功：服务器在响应头写入`Set-Cookie: session_id=xxx`，跳转`dashboard.html`
- 登录失败：显示错误提示

### 4.4 监控大屏模块（admin/dashboard.html + chart.js + auto_switch.js）

#### 4.4.1 页面布局

- **顶部导航栏**：瓜田切换按钮（◀/▶）、当前瓜田信息（品种/总数量）、最后同步时间、强制刷新按钮、系统时间、注销按钮
- **左侧面板（flex:4）**：西瓜状态阵列（CSS Grid）+ 糖度历史折线图
- **右侧面板（flex:5）**：温度折线图 + 湿度折线图 + 光照折线图（纵向排列）

#### 4.4.2 西瓜阵列卡片（renderWatermelonGrid）

- 显示设备编号、糖度（Brix）、成熟度百分比标签
- 成熟标签样式：糖度≥1.0→红色"成熟"；<1.0→黄色"生长"
- **掉线感知**：计算`当前时间戳 - collected_at > 1800秒（30分钟）`→ 标记离线
- 离线卡片样式：红色虚线边框 + 浅红背景（opacity:0.8）+ 底部闪烁"⚠️ 超时未更新"文字
- 点击卡片：选中西瓜并刷新历史糖度图表

#### 4.4.3 ECharts图表引擎（chart.js）

**X轴严格控制**：强制使用`type: 'value'`（纯数值轴），传入精确毫秒时间戳作为`min/max/interval`，通过`formatter`还原为可读时间格式（时:分 或 月.日）。

**Y轴色彩映射**：通过颜色数组给`splitLine.lineStyle.color`，实现温度[-10,0℃]和[40,60℃]为红色告警线、[10,30℃]为黑色正常线的工业级UI还原。

**Tooltip自定义**：重写`tooltip.formatter`，将数值时间戳翻译为`YYYY年MM月DD日 HH:mm:ss`。

**糖度阈值线**：注入黄色`markLine`作为成熟度阈值基准线。

#### 4.4.4 时间刻度控制

支持4档切换（5分钟/2小时/1天/1周），每个图表独立控制，通过`getTimeRange(scaleLevel)`计算X轴边界。

### 4.5 双定时器机制（auto_switch.js）

| 定时器 | 类型 | 周期 | 功能 |
|---|---|---|---|
| idleTimer | setTimeout | 60秒无操作触发 | 切换到下一个瓜田巡航 |
| dataRefreshTimer | setInterval | 每6秒（永久运行） | 静默拉取最新数据重绘图表，不打断用户交互 |

- 任何鼠标/键盘操作调用`resetIdleTimer()`重置无操作计时器
- 强制刷新按钮触发`loadFieldData()`并重置idleTimer

### 4.6 前端API请求

| 接口 | 方法 | 路径 | 参数 | 返回数据 |
|---|---|---|---|---|
| 管理员登录 | POST | `/api/admin/login` | username, password | code+role+Set-Cookie |
| 瓜田列表 | GET | `/api/admin/field/list` | — | [{field_id, watermelon_variety}] |
| 西瓜实时数据 | GET | `/api/admin/watermelon/list` | field_id | 每个设备最新一条，含collected_at |
| 西瓜历史糖度 | GET | `/api/admin/watermelon/history` | device_id | [[timestamp, sugar_brix], ...] |
| 瓜田环境数据 | GET | `/api/admin/field/environment` | field_id | [[timestamp, temp, hum, light], ...] |

### 4.7 掉线检测参数

| 参数 | 值 | 说明 |
|---|---|---|
| OFFLINE_THRESHOLD | 30 * 60（1800秒） | 30分钟无数据判定为离线 |
| 判断字段 | collected_at | 服务器返回的数据采集时间戳（Unix秒） |
| 兜底逻辑 | 若collected_at为空 | 默认为在线，不触发警告 |

---

## 五、三端数据流总图

```
┌─────────────────────────────────────────────────────────────────────┐
│                     FieldAcquisitionTerminal (ESP32)                  │
│                                                                      │
│  AS7341光谱 ──┐                                                     │
│  SHT31温湿 ──┼──→ SensorManager ──→ MLR糖度计算 ──→ 本地显示(LVGL) │
│  BH1750光照 ──┘                                                     │
│                                         │                           │
│                          ┌──────────────┴──────────────┐            │
│                     测试模式（不上传）              工业模式（每5分钟）   │
│                          │                            │              │
│                   LittleFS本地存储              HTTP POST上传         │
│                   （最多20条内存）                  │               │
└────────────────────────────────────┼──────────────────────────────┘
                                     │
                    HTTP POST /api/device/upload
                    Header: device_id + token
                    Body: spectrum_json, temperature, humidity,
                          light, collected_at
                                     │
                                     ▼
┌──────────────────────────────────────────────────────────────────────┐
│               FieldDataProcessingServer (C++ MySQL)                   │
│                                                                       │
│  ① DeviceAuth: token校验 ──────────────────────────────────────────┐  │
│  ② DataCheck: 瓜田号存在性校验                                      │  │
│  ③ DataCheck: 环境数据合法性（不全为99 + 范围校验）                 │  │
│  ④ SugarCalc: MLR糖度计算                                         │  │
│  ⑤ MaturityCalc: 成熟度 = 糖度 / 品种阈值                          │  │
│  ⑥ MySQLDriver: 同时写入watermelon_data + field_environment        │  │
│  ⑦ 响应: sugar_brix + maturity_score + server_time                │  │
│                                                                      │  │
│  存储表:                                                            │  │
│   admin_users ──→ 管理员登录 (Argon2 + Session)                   │  │
│   device_auth ──→ 设备Token认证                                    │  │
│   watermelon_data ──→ device_id + collected_at + sugar_brix +     │  │
│                       maturity_score + spectrum_json               │  │
│   field_production ──→ field_id + 品种 + 成熟阈值                   │  │
│   field_environment ──→ field_id + collected_at +                │  │
│                         temperature_c + humidity_rh + light_lux    │  │
│                                                                      │  │
│  对外接口:                                                           │  │
│   GET  /api/device/time       ── 设备校时                          │  │
│   POST /api/device/upload      ── 设备数据上传                       │  │
│   POST /api/admin/login        ── 管理员登录                         │  │
│   GET  /api/admin/field/list  ── 瓜田列表                          │  │
│   GET  /api/admin/watermelon/list     ── 西瓜实时数据              │  │
│   GET  /api/admin/watermelon/history ── 单瓜历史糖度               │  │
│   GET  /api/admin/field/environment  ── 瓜田环境历史              │  │
│   GET  /* (静态文件)           ── 前端页面托管                       │  │
└──────────────────────────────────────┼──────────────────────────────┘
                                       │
                       GET /api/admin/watermelon/list
                       GET /api/admin/watermelon/history
                       GET /api/admin/field/environment
                       Cookie: session_id=xxx
                                       │
                                       ▼
┌──────────────────────────────────────────────────────────────────────┐
│              FieldMonitoringPlatform (HTML/CSS/JS + ECharts)           │
│                                                                       │
│  index.html ── 产品介绍 + 登录入口                                    │
│   │                                                                  │
│   └── admin/login.html ── 管理员登录 → dashboard.html                 │
│                                                                       │
│  dashboard.html ── 监控大屏                                           │
│   │                                                                  │
│   ├── chart.js ── renderWatermelonGrid（离线感知，30分钟判定）        │
│   │                 buildStrictOption（严格坐标轴配置）               │
│   │                 renderWmHistoryChart / renderEnvHistoryCharts     │
│   │                                                                  │
│   └── auto_switch.js ── 双定时器：60s切换瓜田 + 每6s静默刷新图表      │
│                         fetchFieldList / loadFieldData               │
│                         getTimeRange（4档时间刻度）                   │
│                         reloadAllChartsData（环境+糖度数据）           │
└──────────────────────────────────────────────────────────────────────┘
```

---

## 六、核心算法汇总

### 6.1 糖度MLR计算公式

```
Sugar (Brix) = 0.0012 × ch415 + 0.0005 × ch445 + 0.0021 × ch480 + 5.2
```

- ch415/ch445/ch480：AS7341的415nm/445nm/480nm通道光谱值
- 结果限制在0.0~20.0 Brix范围
- 系数由标定实验数据拟合，适用于宽谱白光LED照射条件

### 6.2 成熟度计算公式

```
maturity_score = sugar_brix / mature_sugar_threshold
```

- `mature_sugar_threshold`：该瓜田品种的成熟糖度阈值（由生产者注册到`field_production`表）
- 等于1.0表示刚好成熟，超过1.0表示过熟

---

## 七、数据库ER关系

```
┌─────────────────┐       1:N        ┌─────────────────┐
│  device_auth    │◄────────────────│  watermelon_data │
│  (设备Token表)    │  device_id      │  (西瓜信息表)     │
└────────┬────────┘   前缀解析       └────────┬────────┘
         │                                    │
         │      1:N (field_id前缀)           │ maturity_score
         │◄──────────────────────────────────┘
         │    查询 mature_sugar_threshold
         ▼
┌─────────────────┐       1:N        ┌─────────────────┐
│field_production │─────────────────►│field_environment │
│ (瓜田生产信息表)   │  field_id       │ (瓜田环境信息表)  │
└─────────────────┘                  └─────────────────┘
```

---

## 八、访问凭证

| 系统 | 用户名 | 密码 | 说明 |
|---|---|---|---|
| 云服务器SSH | root | Mhr289839. | 远程登录 |
| MySQL | root / sws_user | Mhr289839. | 数据库管理 |
| Web管理后台 | admin | admin123 | 默认超级管理员 |
