-- ============================================
-- SmartWatermelonSystem 数据库初始化脚本
-- 数据库名：sws_db
-- 版本：v1.0
-- ============================================

-- 创建数据库
CREATE DATABASE IF NOT EXISTS sws_db
    DEFAULT CHARSET=utf8mb4
    COLLATE=utf8mb4_unicode_ci;

USE sws_db;

-- ============================================
-- 1. 管理员信息表
-- 用于 Web 后台登录校验
-- ============================================
CREATE TABLE IF NOT EXISTS admin_users (
    id INT PRIMARY KEY AUTO_INCREMENT,
    username VARCHAR(64) NOT NULL UNIQUE COMMENT '管理员账号（唯一）',
    password_hash VARCHAR(255) NOT NULL COMMENT 'bcrypt 哈希后的密码',
    role TINYINT NOT NULL DEFAULT 1 COMMENT '角色：0=超级管理员，1=普通管理员',
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP COMMENT '创建时间',
    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP COMMENT '更新时间'
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

-- ============================================
-- 2. 设备校验表
-- 用于设备身份认证，防止非法设备上传数据
-- ============================================
CREATE TABLE IF NOT EXISTS device_auth (
    device_id VARCHAR(32) PRIMARY KEY COMMENT '设备编号：瓜田号-瓜组-瓜号',
    token VARCHAR(64) NOT NULL UNIQUE COMMENT '设备唯一认证Token',
    status TINYINT NOT NULL DEFAULT 1 COMMENT '状态：0=禁用，1=启用',
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP COMMENT '创建时间'
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

-- ============================================
-- 3. 瓜田生产信息表
-- 存储瓜田的生产信息，用于成熟度计算
-- ============================================
CREATE TABLE IF NOT EXISTS field_production (
    field_id VARCHAR(16) PRIMARY KEY COMMENT '瓜田号',
    watermelon_variety VARCHAR(64) NOT NULL COMMENT '西瓜品种',
    mature_sugar_threshold DECIMAL(5,2) NOT NULL COMMENT '成熟糖度阈值（用于计算成熟度）',
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP COMMENT '创建时间',
    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP COMMENT '更新时间'
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

-- ============================================
-- 4. 西瓜信息表
-- 存储每个西瓜的光谱、糖度、成熟度数据
-- ============================================
CREATE TABLE IF NOT EXISTS watermelon_data (
    device_id VARCHAR(32) NOT NULL COMMENT '设备编号：瓜田号-瓜组-瓜号',
    collected_at INT NOT NULL COMMENT '数据采集时间戳（服务器生成）',
    sugar_brix DECIMAL(5,2) NOT NULL COMMENT '当前糖度值（MLR算法）',
    maturity_score DECIMAL(5,3) NOT NULL COMMENT '成熟度评分（可超过1.0）',
    spectrum_json JSON NOT NULL COMMENT 'AS7341光谱数据',
    PRIMARY KEY (device_id, collected_at)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

-- ============================================
-- 5. 瓜田环境信息表
-- 存储瓜田环境监测数据（温度、湿度、光照）
-- ============================================
CREATE TABLE IF NOT EXISTS field_environment (
    field_id VARCHAR(16) NOT NULL COMMENT '瓜田号',
    collected_at INT NOT NULL COMMENT '数据采集时间戳（服务器生成）',
    temperature_c DECIMAL(5,2) NOT NULL COMMENT '温度（℃），无传感器=99',
    humidity_rh DECIMAL(5,2) NOT NULL COMMENT '湿度（%RH），无传感器=99',
    light_lux INT NOT NULL COMMENT '光照强度（Lux），无传感器=99',
    PRIMARY KEY (field_id, collected_at)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

-- ============================================
-- 初始化示例数据
-- ============================================

-- 初始化超级管理员 (用户名: admin, 密码: admin123)
-- bcrypt hash 由应用程序首次启动时生成
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

-- ============================================
-- 验证脚本
-- ============================================
-- SHOW TABLES;
-- SELECT * FROM admin_users;
-- SELECT * FROM device_auth;
-- SELECT * FROM field_production;
