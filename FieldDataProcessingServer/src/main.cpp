#include <iostream>
#include <sodium.h> // 引入 sodium
#include "http_server/HttpServer.h"
#include "db/MySQLDriver.h"
#include "utils/PasswordHasher.h"

int main() {
    std::cout << "=======================================" << std::endl;
    std::cout << "   Smart Watermelon System - V1.0      " << std::endl;
    std::cout << "=======================================" << std::endl;

    // 1. 初始化 libsodium (极其重要)
    if (sodium_init() < 0) {
        std::cerr << "!!! Panic: libsodium initialization failed !!!" << std::endl;
        return 1;
    }

    // 2. 连接数据库
    bool db_ok = MySQLDriver::getInstance().connect("127.0.0.1", "sws_user", "Mhr289839.", "sws_db", 3306);
    if (!db_ok) return 1;

    // 3. 自动迁移密码到 Argon2 (方便你测试)
    std::cout << "[Init] Updating 'admin' password to Argon2 hash for admin123..." << std::endl;
    std::string new_hash = PasswordHasher::hash("admin123");
    MySQLDriver::getInstance().execute("UPDATE admin_users SET password_hash = '" + new_hash + "' WHERE username = 'admin'");

    // 4. 启动服务器
    HttpServer server;
    server.start("0.0.0.0", 8080);

    return 0;
}