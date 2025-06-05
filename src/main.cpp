#include "IOCPServer.h"
#include <iostream>
#include <string>

void onConnected(shared_session_ptr session) {
  std::printf("new clien: localaddr[%s] - reomteaddr[%s]\n",
              session->getLocalAddr().c_str(),
              session->getRemoteAddr().c_str());
}

void onMessage(shared_session_ptr session, Buffer* buffer) {
  std::string msg(buffer->peek(), buffer->readableBytes());
  buffer->retrieve(buffer->readableBytes());
  std::printf("recv clien[%s] msg: %s\n", session->getRemoteAddr().c_str(), msg.c_str());

  // do echo
  session->send(msg.data(), msg.size());
}

int main() {
  try {
    // 创建IOCP服务器实例
    IOCPServer server("127.0.0.1", 8888);
    server.setConnectedCallback(std::bind(onConnected, std::placeholders::_1));
    server.setMessageCallback(std::bind(onMessage, std::placeholders::_1, std::placeholders::_2));

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