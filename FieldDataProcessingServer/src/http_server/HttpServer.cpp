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
    
    // listen 会阻塞当前线程，直到服务器停止
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