#pragma once

#include "Session.h"

#include <memory>
#include <mswsock.h>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>
#include <winsock2.h>
#include <ws2tcpip.h>

class WorkerThread;

// IOCP服务器类，实现基于IOCP的Echo服务器
class IOCPServer {
public:
  IOCPServer(const std::string& address, unsigned short port);

  ~IOCPServer();

  void setConnectedCallback(onConnectedCallback cb) { onConnected_ = cb; }
  void setMessageCallback(onMessageCallback cb) { onMessage_ = cb; }
  void setSendCompletedCallback(onSendCompletedCallback cb) { onSendComp_ = cb; }

  // 启动服务器
  bool Start();

  // 停止服务器
  void Stop();

  // 获取服务器状态
  bool IsRunning() const { return running_.load(std::memory_order_acquire); }

  // 处理Accept完成
  void HandleAccept(IoCtx* ctx);

  void HandleRecv(std::shared_ptr<Session> session, IoCtx* ctx, size_t len);

  void HandleSend(std::shared_ptr<Session> session, IoCtx* ctx, size_t writenBytes);

  std::shared_ptr<Session> getSession(SOCKET sock) const;

  // TODO:
  // bool HandleError() const;

  // TODO: as private
  void RemoveSession(SOCKET target);

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
  bool PostAccept(IoCtx* ctx);

  bool PostRecv(IoCtx* ctx);

  // 清理资源
  void Cleanup();

  std::string address_;                                           // 服务器地址
  unsigned short port_;                                           // 服务器端口
  SOCKET listenSocket_;                                           // 监听套接字
  HANDLE completionPort_;                                         // 完成端口句柄
  std::vector<std::unique_ptr<WorkerThread>> workerThreads_;      // 工作线程池
  std::atomic<bool> running_;                                     // 服务器运行标志
  static const size_t MAX_WORKER_THREADS = 4;                     // 工作线程数量
  static const size_t MAX_POST_ACCEPT    = 10;                    // 最大Accept上下文数量
  std::unique_ptr<SockCtx> listenerCxt_;                          // Accept上下文池
  std::unordered_map<SOCKET, std::shared_ptr<Session>> sessions_; // Client session pool
  mutable std::mutex sessionsMtx_;                                // mutex for sessions
  LPFN_ACCEPTEX lpfnAcceptEx_{};
  LPFN_GETACCEPTEXSOCKADDRS lpfnGetAcceptExSockAddrs_{};

  onConnectedCallback onConnected_{};
  onMessageCallback onMessage_{};
  onSendCompletedCallback onSendComp_{};
};