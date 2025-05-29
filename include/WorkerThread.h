#pragma once

#include <WinSock2.h>
#include <Windows.h>
#include <atomic>
#include <functional>
#include <thread>

class IOCPServer;
// 工作线程类，用于处理IOCP的异步I/O操作
class WorkerThread {
public:
  WorkerThread(IOCPServer& srv, HANDLE completionPort);
  ~WorkerThread();

  // 启动工作线程
  void Start();

  // 停止工作线程
  void Stop();

  // 获取线程ID
  DWORD GetThreadId() const { return threadId_; }

private:
  // 线程主函数
  void ThreadProc();

  // 处理完成端口事件
  void HandleCompletion(DWORD bytesTransferred,
                        ULONG_PTR completionKey,
                        LPOVERLAPPED overlapped,
                        BOOL result);

  HANDLE completionPort_;     // 完成端口句柄
  std::thread thread_;        // 工作线程
  DWORD threadId_;            // 线程ID
  std::atomic<bool> running_; // 线程运行标志
  IOCPServer& srv_;
};