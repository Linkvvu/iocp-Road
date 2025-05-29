#include "IOCPServer.h"
#include <iostream>
#include <string>

int main() {
    try {
        // 创建IOCP服务器实例
        IOCPServer server("127.0.0.1", 8888);

        // 启动服务器
        if (!server.Start()) {
            std::cerr << "Failed to start server" << std::endl;
            return 1;
        }

        std::cout << "Echo server is running. Press Enter to stop..." << std::endl;
        std::cin.get();

        // 停止服务器
        server.Stop();
        std::cout << "Server stopped" << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
} 