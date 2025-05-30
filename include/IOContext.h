#pragma once

#include <WinSock2.h>
#include <Windows.h>
#include <exception>
#include <string>
#include <vector>

#ifndef WSABUF_SIZE
  #define WSABUF_SIZE 1024 * 4 * 2
#endif

#define FMT_ERR_MSG(func, errCode) #func##" failed with error: " + std::to_string(::GetLastError())

enum class OpType {
  Undefine, // placeholader
  ACCEPT,   // 接受连接操作
  RECV,     // 接收数据操作
  SEND,     // 发送数据操作
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
      , op(OpType::Undefine) {}

  explicit IoCtx(SOCKET socket)
      : IoCtx() {
    sock = socket;
  }

  void ResetBuffer() { ZeroMemory(buffer.data(), buffer.size()); }

  ~IoCtx() {
    if (sock != INVALID_SOCKET) {
      ::closesocket(sock);
    }
  }
};

struct AcceptCtx : public IoCtx {
  SOCKET acceptSock;

  AcceptCtx(SOCKET listenSocket)
      : IoCtx(listenSocket) {
    this->op   = OpType::ACCEPT;
    acceptSock = INVALID_SOCKET;
  }

  AcceptCtx(SOCKET listenSocket, SOCKET acceptSocket)
      : IoCtx(listenSocket) {
    this->op         = OpType::ACCEPT;
    this->acceptSock = acceptSocket;
  }

  void ResetAcceptSocket() { acceptSock = INVALID_SOCKET; }

  ~AcceptCtx() {
    if (acceptSock != INVALID_SOCKET) {
      ::closesocket(acceptSock);
    }
  }
};

// struct SocketCtx {
//   SOCKET sock;

//   SocketCtx(SOCKET socket)
//       : sock(socket) {}

//   ~SocketCtx() {
//     if (sock != INVALID_SOCKET) {
//       closesocket(sock);
//     }
//   }
// };
