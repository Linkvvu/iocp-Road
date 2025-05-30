#pragma once

#include "IOContext.h"
#include "WorkerThread.h"

#include <memory>
#include <mswsock.h>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>
#include <winsock2.h>
#include <ws2tcpip.h>

// IOCP服务器类，实现基于IOCP的Echo服务器
class IOCPServer {
public:
  IOCPServer(const std::string& address, unsigned short port);
  ~IOCPServer();

  // 启动服务器
  bool Start();

  // 停止服务器
  void Stop();

  // 获取服务器状态
  bool IsRunning() const { return running_.load(std::memory_order_acquire); }

  // 处理Accept完成
  void HandleAccept(AcceptCtx* ctx);

  void HandleRecv(IoCtx* ctx);

  // TODO:
  // bool HandleError() const;

  // TODO: as private
  bool RemoveIoCtx(const IoCtx* ctx);

private:
  // 初始化Windows Socket
  bool InitializeWinsock();

  void PreparePostAccept();

  bool InitializeExtraFunc();

  // 创建监听套接字
  bool CreateListenSocket();

  // 创建完成端口
  bool CreateCompletionPort();

  // 关联指定Sock至IOCP
  bool AssociateWithIOCP(SOCKET sock, ULONG_PTR key);

  // 启动工作线程
  void StartWorkerThreads();

  // 投递Accept请求
  bool PostAccept(AcceptCtx* ctx);

  bool PostRecv(IoCtx* ctx);

  // 处理客户端连接
  bool HandleClientConnection(SOCKET clientSocket);

  // 清理资源
  void Cleanup();

  std::string address_;                                      // 服务器地址
  unsigned short port_;                                      // 服务器端口
  SOCKET listenSocket_;                                      // 监听套接字
  HANDLE completionPort_;                                    // 完成端口句柄
  std::vector<std::unique_ptr<WorkerThread>> workerThreads_; // 工作线程池
  std::atomic<bool> running_;                                // 服务器运行标志
  static const size_t MAX_WORKER_THREADS = 4;                // 工作线程数量
  static const size_t MAX_POST_ACCEPT    = 10;               // 最大Accept上下文数量
  // std::unordered_map<SOCKET, std::unique_ptr<SocketCtx>> clients_;
  std::vector<std::unique_ptr<AcceptCtx>> acceptContexts_; // Accept上下文池
  std::vector<std::unique_ptr<IoCtx>> ioContexts_;         // Io上下文池
  std::mutex ioCtxPoolMutex_;                              // Io上下文池
  LPFN_ACCEPTEX lpfnAcceptEx_{};
  LPFN_GETACCEPTEXSOCKADDRS lpfnGetAcceptExSockAddrs_{};
};