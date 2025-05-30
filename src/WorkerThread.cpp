#include "WorkerThread.h"

#include "IOCPServer.h"
#include "IOContext.h"
#include "log.h"
#include <cassert>
#include <iostream>

WorkerThread::WorkerThread(IOCPServer& srv, HANDLE completionPort)
    : srv_(srv)
    , completionPort_(completionPort)
    , running_(false) {}

WorkerThread::~WorkerThread() {
  this->Stop();
  if (thread_.joinable()) {
    thread_.join();
  }
}

void WorkerThread::Start() {
  bool expected = false;

  if (running_.compare_exchange_strong(expected, true)) {
    thread_ = std::thread(&WorkerThread::ThreadProc, this);
  }
}

void WorkerThread::Stop() { running_.store(false, std::memory_order_release); }

void WorkerThread::ThreadProc() {
  while (running_.load(std::memory_order_acquire)) {
    DWORD bytesTransferred  = 0;
    ULONG_PTR completionKey = 0;
    LPOVERLAPPED overlapped = nullptr;

    // 等待完成端口事件
    BOOL result = GetQueuedCompletionStatus(completionPort_,
                                            &bytesTransferred,
                                            &completionKey,
                                            &overlapped,
                                            INFINITE);

    if (!running_.load(std::memory_order_acquire)) {
      break;
    }

    // 处理完成事件
    HandleCompletion(bytesTransferred, completionKey, overlapped, result);
  }
}

void WorkerThread::HandleCompletion(DWORD bytesTransferred,
                                    ULONG_PTR completionKey,
                                    LPOVERLAPPED overlapped,
                                    BOOL result) {
  // 获取重叠上下文
  IoCtx* ctx = CONTAINING_RECORD(overlapped, IoCtx, overlapped);

  if (!result) {
    DWORD dwError = GetLastError();

    switch (dwError) {
    case WAIT_TIMEOUT:
      return;

    case ERROR_NETNAME_DELETED:
      srv_.RemoveIoCtx(ctx);
      return;

    default:
      // 其他未知错误
      // 记录日志并决定是否继续
      LOG("GetQueuedCompletionStatus failed: %d, close the socket %d", dwError, ctx->sock);
      srv_.RemoveIoCtx(ctx);
      return;
    }
  }

  // 接收到客户端发送的FIN包
  if ((bytesTransferred == 0) && (ctx->op == OpType::RECV || ctx->op == OpType::SEND)) {
    LOG("socket %d 断开连接", ctx->sock);
    bool ok = srv_.RemoveIoCtx(ctx);
    assert(ok);
    return;
  }

  // 根据操作类型处理完成事件
  switch (ctx->op) {
  case OpType::ACCEPT: {
    // 处理Accept完成
    AcceptCtx* acceptContext = static_cast<AcceptCtx*>(ctx);
    // IOCPServer* server       = reinterpret_cast<IOCPServer*>(completionKey);
    srv_.HandleAccept(acceptContext);
    break;
  }
  case OpType::RECV: {
    srv_.HandleRecv(ctx);
    break;
  }
  case OpType::SEND: {
    break;
  }
  default:
    LOG("uninitialized operation flag!");
    break;
  }
}