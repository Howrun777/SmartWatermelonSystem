#include "MySQLDriver.h"
#include <iostream>

MySQLDriver::MySQLDriver() {
    conn = mysql_init(nullptr);
}

MySQLDriver::~MySQLDriver() {
    if (conn) {
        mysql_close(conn);
    }
}

MySQLDriver& MySQLDriver::getInstance() {
    static MySQLDriver instance;
    return instance;
}

bool MySQLDriver::connect(const std::string& host, const std::string& user, const std::string& pwd, const std::string& db, int port) {
    if (mysql_real_connect(conn, host.c_str(), user.c_str(), pwd.c_str(), db.c_str(), port, nullptr, 0) == nullptr) {
        std::cerr << "[MySQL Error] Failed to connect: " << mysql_error(conn) << std::endl;
        return false;
    }
    // 设置字符集，防止中文乱码
    mysql_set_character_set(conn, "utf8mb4");
    std::cout << "[MySQL] Connected to database: " << db << " successfully!" << std::endl;
    return true;
}

bool MySQLDriver::execute(const std::string& sql) {
    std::lock_guard<std::mutex> lock(mtx); // 加锁防并发冲突
    if (mysql_query(conn, sql.c_str())) {
        std::cerr << "[MySQL Error] Execute failed: " << mysql_error(conn) << "\nSQL: " << sql << std::endl;
        return false;
    }
    return true;
}

std::vector<std::map<std::string, std::string>> MySQLDriver::query(const std::string& sql) {
    std::lock_guard<std::mutex> lock(mtx);
    std::vector<std::map<std::string, std::string>> result_list;

    if (mysql_query(conn, sql.c_str())) {
        std::cerr << "[MySQL Error] Query failed: " << mysql_error(conn) << "\nSQL: " << sql << std::endl;
        return result_list;
    }

    MYSQL_RES* res = mysql_store_result(conn);
    if (!res) return result_list;

    int num_fields = mysql_num_fields(res);
    MYSQL_FIELD* fields = mysql_fetch_fields(res);
    MYSQL_ROW row;

    // 遍历结果集，组装成 map
    while ((row = mysql_fetch_row(res))) {
        std::map<std::string, std::string> row_map;
        for (int i = 0; i < num_fields; i++) {
            row_map[fields[i].name] = row[i] ? row[i] : ""; // 处理 NULL 值
        }
        result_list.push_back(row_map);
    }
    
    mysql_free_result(res);
    return result_list;
}

// 过滤外部字符串，防止 SQL 注入
std::string MySQLDriver::escapeString(const std::string& str) {
    std::lock_guard<std::mutex> lock(mtx);
    if (!conn) return str;
    
    // MySQL 转义后的字符串长度最多是原字符串的 2 倍 + 1
    char* buffer = new char[str.length() * 2 + 1];
    mysql_real_escape_string(
        conn,           // [输入] MySQL 连接句柄
        buffer,         // [输出] 转义后的结果缓冲区
        str.c_str(),    // [输入] 原始字符串
        str.length()    // [输入] 原始字符串的长度
    );
    std::string escaped(buffer);
    delete[] buffer;
    
    return escaped;
}