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

using json = nlohmann::json;

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

            // [2] 环境数据校验
            if (!DataCheck::isEnvironmentValid(temp, hum, light)) {
                response["code"] = 400;
                response["msg"] = "环境数据无效";
                res.set_content(response.dump(), "application/json");
                return;
            }

            // [3] 瓜田号校验
            std::string field_id = device_id.substr(0, 4);
            if (!DataCheck::isFieldExist(field_id)) {
                response["code"] = 400;
                response["msg"] = "数据拒绝：该设备所属瓜田未在生产信息表中注册";
                res.set_content(response.dump(), "application/json");
                return;
            }

            // [4] 核心算法计算
            double sugar_brix = SugarCalc::calculate(spectrum);
            double maturity_score = MaturityCalc::calculate(field_id, sugar_brix);
            
            // [5] 延顺入库机制处理
            auto now = std::chrono::system_clock::now();
            long long collected_at = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
            
            std::string sql_w = "INSERT INTO watermelon_data (device_id, collected_at, sugar_brix, maturity_score, spectrum_json) VALUES ('" 
                + device_id + "', " + std::to_string(collected_at) + ", " + std::to_string(sugar_brix) + ", " 
                + std::to_string(maturity_score) + ", '" + spectrum.dump() + "')";

            std::string sql_e = "INSERT INTO field_environment (field_id, collected_at, temperature_c, humidity_rh, light_lux) VALUES ('" 
                + field_id + "', " + std::to_string(collected_at) + ", " + std::to_string(temp) + ", " 
                + std::to_string(hum) + ", " + std::to_string(light) + ")";

            // 注意：如果并发插入报错主键冲突，数据库操作直接 return false。这里简化处理，毕设阶段直接执行即可。
            if (MySQLDriver::getInstance().execute(sql_w) && MySQLDriver::getInstance().execute(sql_e)) {
                response["code"] = 200;
                response["msg"] = "数据上传成功";
                response["data"]["sugar_brix"] = sugar_brix;
                response["data"]["maturity_score"] = maturity_score;
            } else {
                response["code"] = 500;
                response["msg"] = "数据库写入失败";
            }
        } catch (const std::exception& e) {
            response["code"] = 400;
            response["msg"] = std::string("系统异常: ") + e.what();
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


}