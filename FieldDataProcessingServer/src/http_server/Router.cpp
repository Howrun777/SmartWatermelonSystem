#include "Router.h"
#include "../db/MySQLDriver.h"
#include "../auth/DeviceAuth.h"
#include "../data_process/DataCheck.h"
#include "../data_process/SugarCalc.h"
#include "../data_process/MaturityCalc.h"
#include <json.hpp>
#include <iostream>
#include <chrono>
#include "../auth/AdminAuth.h"
#include "../session/Session.h"
#include <fstream>
#include <sstream>

using json = nlohmann::json;

// 辅助函数：读取本地 HTML 文件内容
std::string readFile(const std::string& path) {
    std::ifstream file(path);
    if (!file) return "<h1>404 Not Found - 找不到文件</h1>";
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

void Router::registerRoutes(httplib::Server& svr) {
    
    svr.Get("/ping", [](const httplib::Request& req, httplib::Response& res) {
        res.set_content(R"({"msg": "pong", "status": "success"})", "application/json");
    });

    // 设备上传接口 (重构后)
    svr.Post("/api/device/upload", [](const httplib::Request& req, httplib::Response& res) {
        json response;
        try {
            std::string device_id = req.has_header("device_id") ? req.get_header_value("device_id") : "";
            std::string token = req.has_header("token") ? req.get_header_value("token") : "";

            // [1] 设备认证
            if (!DeviceAuth::authenticate(device_id, token)) {
                response["code"] = 401;
                response["msg"] = "认证失败：Token错误、设备不存在或已被禁用";
                res.set_content(response.dump(), "application/json");
                return;
            }

            auto body = json::parse(req.body);
            double temp = body.value("temperature", 99.0);
            double hum = body.value("humidity", 99.0);
            int light = body.value("light", 99);
            auto spectrum = body["spectrum_json"]; 

            // [2] 瓜田号合法性校验 (必须存在，否则西瓜也存不了成熟度)
            std::string field_id = device_id.substr(0, 4);
            if (!DataCheck::isFieldExist(field_id)) {
                response["code"] = 400;
                response["msg"] = "数据拒绝：该设备所属瓜田未在生产信息表中注册";
                res.set_content(response.dump(), "application/json");
                return;
            }

            // [3] 核心算法计算 (西瓜数据永远计算)
            double sugar_brix = SugarCalc::calculate(spectrum);
            double maturity_score = MaturityCalc::calculate(field_id, sugar_brix);
            
            auto now = std::chrono::system_clock::now();
            long long collected_at = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
            
            // [4] 组装西瓜写入 SQL (永远写入)
            std::string sql_w = "INSERT INTO watermelon_data (device_id, collected_at, sugar_brix, maturity_score, spectrum_json) VALUES ('" 
                + device_id + "', " + std::to_string(collected_at) + ", " + std::to_string(sugar_brix) + ", " 
                + std::to_string(maturity_score) + ", '" + spectrum.dump() + "')";

            bool w_ok = MySQLDriver::getInstance().execute(sql_w);
            bool e_ok = true; // 环境数据默认成功（如果没传环境，就不算失败）
            std::string extra_msg = "";

            // [5] 环境数据的柔性写入逻辑
            // 只有当环境数据有效时（不全为99），才执行环境库插入
            if (DataCheck::isEnvironmentValid(temp, hum, light)) {
                std::string sql_e = "INSERT INTO field_environment (field_id, collected_at, temperature_c, humidity_rh, light_lux) VALUES ('" 
                    + field_id + "', " + std::to_string(collected_at) + ", " + std::to_string(temp) + ", " 
                    + std::to_string(hum) + ", " + std::to_string(light) + ")";
                e_ok = MySQLDriver::getInstance().execute(sql_e);
            } else {
                extra_msg = " (⚠️检测到无环境传感器，已跳过环境数据保存)";
            }

            // [6] 返回响应
            if (w_ok && e_ok) {
                response["code"] = 200;
                response["msg"] = "数据上传成功" + extra_msg;
                response["data"]["sugar_brix"] = sugar_brix;
                response["data"]["maturity_score"] = maturity_score;
            } else {
                response["code"] = 500;
                response["msg"] = "部分数据库写入失败，可能因为时间戳冲突";
            }
        } catch (const std::exception& e) {
            response["code"] = 400;
            response["msg"] = std::string("系统异常或JSON格式错误: ") + e.what();
        }

        res.set_content(response.dump(), "application/json");
    });
// ========== Web 端接口 ==========

    // 1. 管理员登录接口
    svr.Post("/api/admin/login", [](const httplib::Request& req, httplib::Response& res) {
        json response;
        try {
            auto body = json::parse(req.body);
            std::string user = body.value("username", "");
            std::string pass = body.value("password", "");

            std::string session_id = AdminAuth::login(user, pass);
            if (!session_id.empty()) {
                response["code"] = 200;
                response["msg"] = "登录成功";
                response["role"] = 0;
                // 设置 Cookie 返回给浏览器
                res.set_header("Set-Cookie", "session_id=" + session_id + "; Path=/; HttpOnly; Max-Age=3600");
            } else {
                response["code"] = 401;
                response["msg"] = "账号或密码错误";
            }
        } catch (...) {
            response["code"] = 400; response["msg"] = "请求格式错误";
        }
        res.set_content(response.dump(), "application/json");
    });

    // 2. 瓜田列表查询接口 (需要鉴权)
    svr.Get("/api/admin/field/list", [](const httplib::Request& req, httplib::Response& res) {
        json response;
        // 简易拦截器：从 Cookie 中提取 session_id 并校验
        std::string cookie = req.has_header("Cookie") ? req.get_header_value("Cookie") : "";
        std::string sid = "";
        size_t pos = cookie.find("session_id=");
        if (pos != std::string::npos) sid = cookie.substr(pos + 11, 32);

        if (!Session::getInstance().isValid(sid)) {
            response["code"] = 401; response["msg"] = "未登录或会话已过期";
            res.set_content(response.dump(), "application/json"); return;
        }

        // 校验通过，查询数据库
        auto db_res = MySQLDriver::getInstance().query("SELECT field_id, watermelon_variety FROM field_production");
        json field_list = json::array();
        for (const auto& row : db_res) {
            field_list.push_back({{"field_id", row.at("field_id")}, {"variety", row.at("watermelon_variety")}});
        }
        
        response["code"] = 200;
        response["data"]["total"] = field_list.size();
        response["data"]["list"] = field_list;
        res.set_content(response.dump(), "application/json");
    });

    svr.Options(R"(.*)", [](const httplib::Request& req, httplib::Response& res) {
        // ... (跨域代码保持不变) ...
    });
     // 1. 官网首页 (完全公开)
    svr.Get("/", [](const httplib::Request& req, httplib::Response& res) {
        res.set_content(readFile("../../FieldMonitoringPlatform/index.html"), "text/html; charset=utf-8");
    });
    svr.Get("/index.html", [](const httplib::Request& req, httplib::Response& res) {
        res.set_redirect("/"); // 重定向到根目录
    });

    // 2. 登录页面 (完全公开)
    svr.Get("/admin/login.html", [](const httplib::Request& req, httplib::Response& res) {
        res.set_content(readFile("../../FieldMonitoringPlatform/admin/login.html"), "text/html; charset=utf-8");
    });

    // 3. 核心大屏监控页 (【严格安全校验】)
    svr.Get("/admin/dashboard.html", [](const httplib::Request& req, httplib::Response& res) {
        // 提取 Cookie 中的 session_id
        std::string cookie = req.has_header("Cookie") ? req.get_header_value("Cookie") : "";
        std::string sid = "";
        size_t pos = cookie.find("session_id=");
        if (pos != std::string::npos) sid = cookie.substr(pos + 11, 32);

        // 核心拦截逻辑
        if (!Session::getInstance().isValid(sid)) {
            // 如果没登录，或者 Session 过期了
            std::cout << "[Security] Unauthorized access to dashboard. Redirecting to login." << std::endl;
            // 触发 HTTP 302 重定向，把非法的访客一脚踢回登录页
            res.set_redirect("/admin/login.html");
            return;
        }

        // 校验通过，读取后台网页并返回给管理员
        res.set_content(readFile("../../FieldMonitoringPlatform/admin/dashboard.html"), "text/html; charset=utf-8");
    });
    
    // 3. 获取单瓜田西瓜实时数据
    svr.Get("/api/admin/watermelon/list", [](const httplib::Request& req, httplib::Response& res) {
        json response;
        std::string field_id = req.has_param("field_id") ? req.get_param_value("field_id") : "";
        if (field_id.empty()) {
            response["code"] = 400; response["msg"] = "缺少 field_id 参数";
            res.set_content(response.dump(), "application/json"); return;
        }

        // 查询该瓜田下每个西瓜最新的一条数据 (利用 MAX 时间戳)
        std::string sql = "SELECT a.device_id, a.sugar_brix, a.maturity_score FROM watermelon_data a "
                          "INNER JOIN (SELECT device_id, MAX(collected_at) as max_time FROM watermelon_data GROUP BY device_id) b "
                          "ON a.device_id = b.device_id AND a.collected_at = b.max_time "
                          "WHERE a.device_id LIKE '" + field_id + "-%'";
        
        auto db_res = MySQLDriver::getInstance().query(sql);
        json wm_list = json::array();
        for (const auto& row : db_res) {
            wm_list.push_back({
                {"device_id", row.at("device_id")},
                {"sugar_brix", std::stod(row.at("sugar_brix"))},
                {"maturity_score", std::stod(row.at("maturity_score"))}
            });
        }
        
        response["code"] = 200; response["data"] = wm_list;
        res.set_content(response.dump(), "application/json");
    });

    // 4. 获取单果历史糖度数据 (最近 84 天)
    svr.Get("/api/admin/watermelon/history", [](const httplib::Request& req, httplib::Response& res) {
        json response;
        std::string device_id = req.has_param("device_id") ? req.get_param_value("device_id") : "";
        
        auto now = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
        long long seven_days_ago = now - 84 * 24 * 3600; // 84天前的时间戳
        
        std::string sql = "SELECT collected_at, sugar_brix FROM watermelon_data "
                          "WHERE device_id = '" + device_id + "' AND collected_at > " + std::to_string(seven_days_ago) + " "
                          "ORDER BY collected_at ASC";
                          
        auto db_res = MySQLDriver::getInstance().query(sql);
        json history = json::array();
        for (const auto& row : db_res) {
            history.push_back({
                {"timestamp", std::stoll(row.at("collected_at"))},
                {"sugar_brix", std::stod(row.at("sugar_brix"))}
            });
        }
        
        response["code"] = 200; response["data"] = history;
        res.set_content(response.dump(), "application/json");
    });

    // 5. 获取单瓜田环境历史数据 (最近 84 天)
    svr.Get("/api/admin/field/environment", [](const httplib::Request& req, httplib::Response& res) {
        json response;
        std::string field_id = req.has_param("field_id") ? req.get_param_value("field_id") : "";
        
        auto now = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
        long long seven_days_ago = now - 84 * 24 * 3600;
        
        std::string sql = "SELECT collected_at, temperature_c, humidity_rh, light_lux FROM field_environment "
                          "WHERE field_id = '" + field_id + "' AND collected_at > " + std::to_string(seven_days_ago) + " "
                          "ORDER BY collected_at ASC";
                          
        auto db_res = MySQLDriver::getInstance().query(sql);
        json env_data = json::array();
        for (const auto& row : db_res) {
            env_data.push_back({
                {"timestamp", std::stoll(row.at("collected_at"))},
                {"temperature", std::stod(row.at("temperature_c"))},
                {"humidity", std::stod(row.at("humidity_rh"))},
                {"light", std::stoi(row.at("light_lux"))}
            });
        }
        
        response["code"] = 200; response["data"] = env_data;
        res.set_content(response.dump(), "application/json");
    });
}


