-- 生成西瓜测试数据（带随机波动）
-- 糖度从10到12单调递增（总体趋势），但每条数据有随机小波动
-- 每5分钟一条，共1000条
-- 时间范围：2026-05-06 14:10:00 到 2026-05-10 01:25:00

USE sws_db;

-- 清空该设备的历史数据
DELETE FROM watermelon_data WHERE device_id = '1001-01-01';
DELETE FROM field_environment WHERE field_id = '1001';

-- 使用存储过程生成西瓜数据
DELIMITER //

DROP PROCEDURE IF EXISTS generate_watermelon_data//

CREATE PROCEDURE generate_watermelon_data()
BEGIN
    DECLARE i INT DEFAULT 0;
    DECLARE progress DECIMAL(5,4);
    DECLARE base_sugar DECIMAL(5,2);
    DECLARE final_sugar DECIMAL(5,2);
    DECLARE maturity_val DECIMAL(5,3);
    DECLARE ts INT;
    DECLARE start_ts INT;
    DECLARE random_offset DECIMAL(5,2);
    
    -- 开始时间戳：2026-05-06 14:10:00
    SET start_ts = UNIX_TIMESTAMP('2026-05-06 14:10:00');
    
    -- 循环插入1000条数据
    WHILE i < 1000 DO
        -- 进度：0到1
        SET progress = i / 999.0;
        
        -- 基础糖度：使用二次多项式，越往后增长越快（加速增长曲线）
        -- 从10.00到12.00，初期缓慢，后期加速
        SET base_sugar = ROUND(10.00 + 2.00 * POW(progress, 1.8), 2);
        
        -- 随机波动：-0.2 到 +0.2 之间（保持小波动）
        SET random_offset = ROUND((RAND() - 0.5) * 0.4, 2);
        
        -- 最终糖度 = 基础值 + 随机波动
        SET final_sugar = ROUND(base_sugar + random_offset, 2);
        
        -- 确保糖度在有效范围内
        IF final_sugar < 9.5 THEN SET final_sugar = 9.5; END IF;
        IF final_sugar > 12.5 THEN SET final_sugar = 12.5; END IF;
        
        -- 成熟度 = 糖度 / 12.5
        SET maturity_val = ROUND(final_sugar / 12.5, 3);
        
        -- 当前时间戳
        SET ts = start_ts + (i * 300);
        
        -- 插入西瓜数据
        INSERT INTO watermelon_data (device_id, collected_at, sugar_brix, maturity_score, spectrum_json)
        VALUES (
            '1001-01-01',
            ts,
            final_sugar,
            maturity_val,
            CONCAT('{\"c415\":', 1000 + i*5, 
                   ',\"c445\":', 1100 + i*5,
                   ',\"c480\":', 1200 + i*5,
                   ',\"c515\":', 1300 + i*5,
                   ',\"c555\":', 1400 + i*5,
                   ',\"c595\":', 1500 + i*5,
                   ',\"c640\":', 1600 + i*5,
                   ',\"c680\":', 1700 + i*5,
                   ',\"clr\":', 2000 + i*5,
                   ',\"nir\":', 5000 + i*10, '}')
        );
        
        SET i = i + 1;
    END WHILE;
    
    SELECT CONCAT('西瓜数据已插入 ', i, ' 条') AS result;
END//

-- 使用存储过程生成环境数据
DROP PROCEDURE IF EXISTS generate_environment_data//

CREATE PROCEDURE generate_environment_data()
BEGIN
    DECLARE i INT DEFAULT 0;
    DECLARE ts INT;
    DECLARE start_ts INT;
    DECLARE hour_of_day INT;
    
    -- 温度参数
    DECLARE base_temp DECIMAL(5,2);
    DECLARE temp_offset DECIMAL(5,2);
    DECLARE final_temp DECIMAL(5,2);
    
    -- 湿度参数（50-70%之间，缓慢漂移变化）
    DECLARE humidity DECIMAL(5,2);
    DECLARE humidity_trend DECIMAL(5,3);
    
    -- 光照参数（60000 Lux 左右，缓慢漂移变化）
    DECLARE light INT;
    DECLARE light_trend DECIMAL(5,2);
    
    -- 开始时间戳：2026-05-06 14:10:00
    SET start_ts = UNIX_TIMESTAMP('2026-05-06 14:10:00');
    
    -- 初始化湿度和趋势
    SET humidity = 60;
    SET humidity_trend = 0;
    SET light = 60000;
    SET light_trend = 0;
    
    -- 循环插入1000条数据
    WHILE i < 1000 DO
        SET ts = start_ts + (i * 300);
        
        -- 获取当前小时 (0-23)
        SET hour_of_day = HOUR(FROM_UNIXTIME(ts));
        
        -- 温度：白天(6-18点)最高30，晚上(其他时间)最低20
        IF hour_of_day >= 6 AND hour_of_day <= 18 THEN
            -- 白天：20到30之间波动，以14点为最高点
            SET base_temp = 25 + 5 * SIN((hour_of_day - 6) * 3.14159 / 12);
        ELSE
            -- 晚上：20到25之间波动
            IF hour_of_day < 6 THEN
                SET base_temp = 22.5 - 2.5 * COS((hour_of_day + 6) * 3.14159 / 12);
            ELSE
                SET base_temp = 22.5 - 2.5 * COS((hour_of_day - 18) * 3.14159 / 12);
            END IF;
        END IF;
        
        -- 温度随机波动 ±1
        SET temp_offset = ROUND((RAND() - 0.5) * 2, 2);
        SET final_temp = ROUND(base_temp + temp_offset, 2);
        
        -- 确保温度在合理范围
        IF final_temp < 18 THEN SET final_temp = 18; END IF;
        IF final_temp > 32 THEN SET final_temp = 32; END IF;
        
        -- 湿度：50-70% 之间，缓慢漂移变化（每步最多变化 ±0.5%）
        -- 使用随机趋势方向
        IF RAND() < 0.02 THEN
            -- 2% 概率改变方向
            SET humidity_trend = -humidity_trend + (RAND() - 0.5) * 0.4;
        ELSE
            -- 继续当前方向，缓慢漂移
            SET humidity_trend = humidity_trend + (RAND() - 0.5) * 0.2;
        END IF;
        
        -- 限制趋势范围，确保变化平缓
        SET humidity_trend = ROUND(humidity_trend, 3);
        IF humidity_trend < -0.5 THEN SET humidity_trend = -0.5; END IF;
        IF humidity_trend > 0.5 THEN SET humidity_trend = 0.5; END IF;
        
        -- 应用趋势变化
        SET humidity = ROUND(humidity + humidity_trend, 2);
        
        -- 确保在范围内，并保持一定波动
        IF humidity < 50 THEN 
            SET humidity = 50 + RAND() * 2; 
            SET humidity_trend = ABS(humidity_trend) * 0.5;
        END IF;
        IF humidity > 70 THEN 
            SET humidity = 70 - RAND() * 2; 
            SET humidity_trend = -ABS(humidity_trend) * 0.5;
        END IF;
        
        -- 光照：55000-65000 Lux 之间，缓慢漂移变化（每步最多变化 ±200 Lux）
        IF RAND() < 0.02 THEN
            SET light_trend = -light_trend + (RAND() - 0.5) * 100;
        ELSE
            SET light_trend = light_trend + (RAND() - 0.5) * 80;
        END IF;
        
        SET light_trend = ROUND(light_trend, 2);
        IF light_trend < -200 THEN SET light_trend = -200; END IF;
        IF light_trend > 200 THEN SET light_trend = 200; END IF;
        
        SET light = light + light_trend;
        
        IF light < 55000 THEN 
            SET light = 55000 + RAND() * 500; 
            SET light_trend = ABS(light_trend) * 0.3;
        END IF;
        IF light > 65000 THEN 
            SET light = 65000 - RAND() * 500; 
            SET light_trend = -ABS(light_trend) * 0.3;
        END IF;
        
        SET light = FLOOR(light);
        
        -- 插入环境数据
        INSERT INTO field_environment (field_id, collected_at, temperature_c, humidity_rh, light_lux)
        VALUES ('1001', ts, final_temp, humidity, light);
        
        SET i = i + 1;
    END WHILE;
    
    SELECT CONCAT('环境数据已插入 ', i, ' 条') AS result;
END//

DELIMITER ;

-- 执行存储过程
CALL generate_watermelon_data();
CALL generate_environment_data();

-- 删除存储过程
DROP PROCEDURE IF EXISTS generate_watermelon_data;
DROP PROCEDURE IF EXISTS generate_environment_data;

-- 验证西瓜数据
SELECT '=== 西瓜数据统计 ===' AS info;
SELECT COUNT(*) AS total,
       MIN(sugar_brix) AS min_sugar,
       MAX(sugar_brix) AS max_sugar,
       MIN(FROM_UNIXTIME(collected_at)) AS first_time,
       MAX(FROM_UNIXTIME(collected_at)) AS last_time
FROM watermelon_data WHERE device_id = '1001-01-01';

-- 验证环境数据
SELECT '=== 环境数据统计 ===' AS info;
SELECT COUNT(*) AS total,
       MIN(temperature_c) AS min_temp,
       MAX(temperature_c) AS max_temp,
       MIN(humidity_rh) AS min_humidity,
       MAX(humidity_rh) AS max_humidity,
       MIN(light_lux) AS min_light,
       MAX(light_lux) AS max_light
FROM field_environment WHERE field_id = '1001';
