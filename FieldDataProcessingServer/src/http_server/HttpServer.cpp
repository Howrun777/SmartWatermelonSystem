#include "HttpServer.h"
#include "Router.h"
#include <iostream>

HttpServer::HttpServer() {
    // 构造函数中，把 svr 传给 Router，注册所有 API 接口
    Router::registerRoutes(svr);
}

HttpServer::~HttpServer() {
    stop();
}

void HttpServer::start(const std::string& host, int port) {
    std::cout << "Starting SmartWatermelonServer on http://" << host << ":" << port << "..." << std::endl;
    
    // 让 C++ 托管上级目录的 Web 文件夹
    //svr.set_mount_point("/", "../../FieldMonitoringPlatform");
    
    // 只有样式表、脚本和图片可以直接访问，禁止直接访问任何 HTML 网页！
    svr.set_mount_point("/admin/css", "../../FieldMonitoringPlatform/admin/css");
    svr.set_mount_point("/admin/js", "../../FieldMonitoringPlatform/admin/js");
    svr.set_mount_point("/assets", "../../FieldMonitoringPlatform/assets");

    if (!svr.listen(host.c_str(), port)) {
        std::cerr << "Error: Failed to start server! Port may be in use." << std::endl;
    }
}

void HttpServer::stop() {
    if (svr.is_running()) {
        svr.stop();
        std::cout << "Server stopped." << std::endl;
    }
}