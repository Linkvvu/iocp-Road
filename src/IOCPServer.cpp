#include "IOCPServer.h"
#include "WorkerThread.h"

#include <Mswsock.h> // 添加Mswsock.h头文件
#include <algorithm>
#include <exception>
#include <iostream>
#include <log.h>

// 定义SIO_KEEPALIVE_VALS
#ifndef SIO_KEEPALIVE_VALS
  #define SIO_KEEPALIVE_VALS _WSAIOW(IOC_VENDOR, 4)
#endif

IOCPServer::IOCPServer(const std::string& address, unsigned short port)
    : address_(address)
    , port_(port)
    , listenSocket_(INVALID_SOCKET)
    , completionPort_(NULL)
    , running_(false) {}

IOCPServer::~IOCPServer() { Stop(); }

bool IOCPServer::Start() {
  try {
    bool expected = false;
    if (running_.compare_exchange_strong(expected, true) == false) {
      return true; // 服务器已启动
    }

    // 初始化Windows Socket
    if (!InitializeWinsock()) {
      throw std::runtime_error("failed to InitializeWinsock");
    }

    // 创建完成端口
    if (!CreateCompletionPort()) {
      throw std::runtime_error("failed to CreateCompletionPort");
    }

    // 启动工作线程
    StartWorkerThreads();

    // 创建监听套接字
    if (!CreateListenSocket()) {
      throw std::runtime_error("failed to CreateListenSocket");
    }

    if (!InitializeExtraFunc()) {
      throw std::runtime_error("failed to InitializeExtraFunc");
    }

    // 投递初始Accept请求
    PreparePostAccept();

  } catch (const std::exception& e) {
    LOG("failed to start IOCP server, detail: %s", e.what());
    running_.store(false, std::memory_order_release);
    return false;
  }

  std::cout << "Server started on " << address_ << ":" << port_ << std::endl;
  return true;
}

void IOCPServer::Stop() {
  bool expected = true;
  if (running_.compare_exchange_strong(expected, false) == false) {
    return; // 服务器已停止
  }

  // 停止所有工作线程
  for (auto& thread : workerThreads_) {
    thread->Stop();
  }

  for (size_t i = 0; i < workerThreads_.size(); ++i) {
    ::PostQueuedCompletionStatus(completionPort_, 0, NULL, NULL);
  }

  workerThreads_.clear();

  // 关闭监听套接字
  if (listenSocket_ != INVALID_SOCKET) {
    closesocket(listenSocket_);
    listenSocket_ = INVALID_SOCKET;
  }

  // 关闭完成端口
  if (completionPort_) {
    CloseHandle(completionPort_);
    completionPort_ = NULL;
  }

  // 清理Windows Socket
  WSACleanup();
}

void IOCPServer::RemoveSession(SOCKET target) {
  std::lock_guard<std::mutex> guard(sessionsMtx_);
  sessions_.erase(target);
}

bool IOCPServer::InitializeWinsock() {
  WSADATA wsaData;
  int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
  if (result != 0) {
    std::cerr << "WSAStartup failed with error: " << result << std::endl;
    return false;
  }
  return true;
}

void IOCPServer::PreparePostAccept() {
  for (size_t i = 0; i < MAX_POST_ACCEPT; ++i) {
    auto ctx = listenerCxt_->newIoCtx();
    auto ok  = this->PostAccept(ctx);
    if (!ok) {
      listenerCxt_->removeIoCtx(ctx);
    }
  }
}

bool IOCPServer::InitializeExtraFunc() { // 获取AcceptEx函数指针
  GUID GuidAcceptEx             = WSAID_ACCEPTEX;
  GUID GuidGetAcceptExSockAddrs = WSAID_GETACCEPTEXSOCKADDRS;
  DWORD dwBytes                 = 0;

  if (SOCKET_ERROR == WSAIoctl(listenSocket_,
                               SIO_GET_EXTENSION_FUNCTION_POINTER,
                               &GuidAcceptEx,
                               sizeof(GuidAcceptEx),
                               &lpfnAcceptEx_,
                               sizeof(lpfnAcceptEx_),
                               &dwBytes,
                               NULL,
                               NULL)) {
    LOG("failed to get the pointer to AcceptEx, error: %d", WSAGetLastError());
    return false;
  }

  // 获取GetAcceptExSockAddrs函数指针，也是同理
  if (SOCKET_ERROR == WSAIoctl(listenSocket_,
                               SIO_GET_EXTENSION_FUNCTION_POINTER,
                               &GuidGetAcceptExSockAddrs,
                               sizeof(GuidGetAcceptExSockAddrs),
                               &lpfnGetAcceptExSockAddrs_,
                               sizeof(lpfnGetAcceptExSockAddrs_),
                               &dwBytes,
                               NULL,
                               NULL)) {
    LOG("failed to get the pointer to GuidGetAcceptExSockAddrs, error: %d", WSAGetLastError());
    return false;
  }

  return true;
}

bool IOCPServer::CreateListenSocket() {
  // 创建TCP套接字
  listenSocket_ = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
  if (listenSocket_ == INVALID_SOCKET) {
    std::cerr << "WSASocket failed with error: " << WSAGetLastError() << std::endl;
    return false;
  }

  // 设置地址重用选项
  BOOL reuseAddr = TRUE;
  if (setsockopt(listenSocket_,
                 SOL_SOCKET,
                 SO_REUSEADDR,
                 reinterpret_cast<char*>(&reuseAddr),
                 sizeof(reuseAddr)) == SOCKET_ERROR) {
    std::cerr << "setsockopt failed with error: " << WSAGetLastError() << std::endl;
    return false;
  }

  // u_long mode = 1;
  // if (ioctlsocket(listenSocket_, FIONBIO, &mode) == SOCKET_ERROR) {
  //   std::cerr << "ioctlsocket failed with error: " << WSAGetLastError() << std::endl;
  //   return false;
  // }

  if (!AssociateWithIOCP(this->listenSocket_, NULL)) {
    LOG("AssociateWithIOCP failed with error: %d", GetLastError());
    return false;
  }

  // 绑定地址和端口
  sockaddr_in serverAddr;
  serverAddr.sin_family      = AF_INET;
  serverAddr.sin_addr.s_addr = inet_addr(address_.c_str());
  serverAddr.sin_port        = htons(port_);

  if (bind(listenSocket_, reinterpret_cast<sockaddr*>(&serverAddr), sizeof(serverAddr)) ==
      SOCKET_ERROR) {
    LOG("bind failed with error: %d", GetLastError());
    return false;
  }

  // 开始监听
  if (listen(listenSocket_, SOMAXCONN) == SOCKET_ERROR) {
    LOG("listen failed with error: %d", GetLastError());
    return false;
  }

  listenerCxt_ = std::make_unique<SockCtx>(listenSocket_);

  return true;
}

bool IOCPServer::CreateCompletionPort() {
  // 创建完成端口
  completionPort_ = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
  if (completionPort_ == NULL) {
    std::cerr << "CreateIoCompletionPort failed with error: " << GetLastError() << std::endl;
    return false;
  }

  return true;
}

bool IOCPServer::AssociateWithIOCP(SOCKET sock, ULONG_PTR key) {
  HANDLE hTemp = ::CreateIoCompletionPort((HANDLE)sock, this->completionPort_, key, 0);
  if (hTemp == NULL) {
    return false;
  }
  return true;
}

void IOCPServer::StartWorkerThreads() {
  // 创建工作线程
  for (size_t i = 0; i < MAX_WORKER_THREADS; ++i) {
    auto thread = std::make_unique<WorkerThread>(*this, completionPort_);
    thread->Start();
    workerThreads_.push_back(std::move(thread));
  }
}

bool IOCPServer::PostAccept(IoCtx* ctx) {
  // 为以后新连入的客户端先准备好Socket
  ctx->sock = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, WSA_FLAG_OVERLAPPED);
  if (INVALID_SOCKET == ctx->sock) {
    LOG("WSASocket failed with error: %d", GetLastError());
    return false;
  }

  DWORD bytes;
  WSABUF* pWsaBuf = &ctx->wsaBuf;
  OVERLAPPED* pOl = &ctx->overlapped;
  ctx->op         = OpType::ACCEPT;
  BOOL ret        = this->lpfnAcceptEx_(listenerCxt_->getSocket(),
                                 ctx->sock,
                                 pWsaBuf->buf,
                                 0,
                                 sizeof(sockaddr_in) + 16,
                                 sizeof(sockaddr_in) + 16,
                                 &bytes,
                                 pOl);
  if (ret == FALSE) {
    if (WSA_IO_PENDING != WSAGetLastError()) {
      LOG("AcceptEx failed with error: %d", WSAGetLastError());
      return false;
    }
  }
  return true;
}

bool IOCPServer::PostRecv(IoCtx* ctx) {
  DWORD flags = 0, bytes = 0;
  WSABUF* pWsaBuf = &ctx->wsaBuf;
  OVERLAPPED* pOl = &ctx->overlapped;

  ctx->ResetBuffer();
  ctx->op        = OpType::RECV;
  int nbytesRecv = ::WSARecv(ctx->sock, pWsaBuf, 1, &bytes, &flags, pOl, NULL);

  if ((nbytesRecv == SOCKET_ERROR) && (WSAGetLastError() != WSA_IO_PENDING)) {
    LOG("failed to post recv on socket %d", ctx->sock);
    return false;
  }
  return true;
}

void IOCPServer::HandleAccept(IoCtx* ctx) {
  sockaddr_in* LocalAddr  = NULL;
  sockaddr_in* ClientAddr = NULL;
  int remoteLen = sizeof(sockaddr_in), localLen = sizeof(sockaddr_in);
  this->lpfnGetAcceptExSockAddrs_(ctx->wsaBuf.buf,
                                  0,
                                  sizeof(sockaddr_in) + 16,
                                  sizeof(sockaddr_in) + 16,
                                  (LPSOCKADDR*)&LocalAddr,
                                  &localLen,
                                  (LPSOCKADDR*)&ClientAddr,
                                  &remoteLen);
  auto session = std::make_shared<Session>(ctx->sock, LocalAddr, ClientAddr);
  session->setConnectedCallback(onConnected_);
  session->setMessageCallback(onMessage_);
  session->setSendCompletedCallback(onSendComp_);

  bool ok = this->AssociateWithIOCP(ctx->sock, 0);
  if (!ok) {
    LOG("AssociateWithIOCP failed with error: %d", GetLastError());
    return;
  }

  session->handleConnected();

  auto newIoCtx = session->getSockCtx()->newIoCtx();
  ok            = this->PostRecv(newIoCtx);
  if (!ok) {
    LOG("PostRecv failed with error: %d", GetLastError());
    session->getSockCtx()->removeIoCtx(newIoCtx);
    return;
  }

  {
    std::lock_guard<std::mutex> guard(this->sessionsMtx_);
    sessions_.insert({ctx->sock, std::move(session)});
  }

  // post accept again
  ctx->ResetBuffer();
  ok = this->PostAccept(ctx);
  if (!ok) {
    LOG("PostAccept failed");
    listenerCxt_->removeIoCtx(ctx);
    return;
  }
}

void IOCPServer::HandleRecv(std::shared_ptr<Session> session, IoCtx* ctx, size_t recvBytes) {
  session->handleRecv(ctx->buffer.data(), recvBytes);
  PostRecv(ctx);
}

void IOCPServer::HandleSend(std::shared_ptr<Session> session, IoCtx* ctx, size_t writtenBytes) {
  size_t needBytes = ctx->buffer.size();
  if (writtenBytes < needBytes) {
    session->handleSendUncompleted(ctx, writtenBytes);
    return;
  }
  session->handleSendCompleted(ctx);
}

std::shared_ptr<Session> IOCPServer::getSession(SOCKET sock) const {
  std::lock_guard<std::mutex> guard(sessionsMtx_);
  return sessions_.at(sock);
}
