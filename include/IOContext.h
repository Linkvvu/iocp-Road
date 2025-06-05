#pragma once
#include "Buffer.h"
#include "callback.h"

#include <WinSock2.h>
#include <Windows.h>
#include <atomic>
#include <cassert>
#include <deque>
#include <exception>
#include <functional>
#include <mutex>
#include <string>
#include <vector>

#ifndef WSABUF_SIZE
  #define WSABUF_SIZE 1024 * 4 * 2
#endif

#define FMT_ERR_MSG(func, errCode) #func##" failed with error: " + std::to_string(errCode)

enum class OpType {
  UNDEFINED, // placeholader
  ACCEPT,    // 接受连接操作
  RECV,      // 接收数据操作
  SEND,      // 发送数据操作
};

struct IoCtx {
  WSAOVERLAPPED overlapped; // Windows重叠I/O结构
  SOCKET sock;              // 关联的套接字
  std::vector<char> buffer; // 数据缓冲区 TODO: 在子类定义
  WSABUF wsaBuf;            // Windows Socket缓冲区
  OpType op;

  IoCtx()
      : overlapped{}
      , sock(INVALID_SOCKET)
      , buffer(WSABUF_SIZE)
      , wsaBuf{static_cast<ULONG>(buffer.size()), buffer.data()}
      , op(OpType::UNDEFINED) {}

  explicit IoCtx(SOCKET socket)
      : IoCtx() {
    sock = socket;
  }

  void ResetBuffer() { buffer.clear(); }

  ~IoCtx() {
    if (sock != INVALID_SOCKET) {
      ::closesocket(sock);
    }
  }
};

class SockCtx {
public:
  SockCtx(SOCKET sock)
      : sock_(sock) {}

  ~SockCtx() {
    sock_ = INVALID_SOCKET;
    std::lock_guard<std::mutex> guard(mtx_);
    {
      for (auto ctx : ioCtxs_) {
        delete ctx;
      }
    }
  }

  IoCtx* newIoCtx() {
    IoCtx* newIoCtx = new IoCtx(sock_);
    {
      std::lock_guard<std::mutex> guard(mtx_);
      ioCtxs_.push_back(newIoCtx);
    }
    return newIoCtx;
  }

  void removeIoCtx(IoCtx* target) {
    {
      std::lock_guard<std::mutex> guard(mtx_);
      auto it = std::find(ioCtxs_.begin(), ioCtxs_.end(), target);
      if (it != ioCtxs_.end()) {
        ioCtxs_.erase(it);
      }
    }
    delete target;
  }

  SOCKET getSocket() const { return sock_; }

private:
  SOCKET sock_;
  std::vector<IoCtx*> ioCtxs_;
  std::mutex mtx_;
};
