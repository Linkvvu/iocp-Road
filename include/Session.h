#pragma once

#include "IOContext.h"

class Session : public std::enable_shared_from_this<Session> {
  friend class IOCPServer;

public:
  Session(SOCKET sock, sockaddr_in* localAddr, sockaddr_in* remoteAddr);

  std::string getLocalAddr() const { return localAddr_; }

  std::string getRemoteAddr() const { return remoteAddr_; }

  void send(const void* data, size_t len);

  std::unique_ptr<SockCtx>& getSockCtx() { return sockCtx_; }

  const std::unique_ptr<SockCtx>& getSockCtx() const { return sockCtx_; }

  void setConnectedCallback(onConnectedCallback cb) { onConnected_ = cb; }
  void setMessageCallback(onMessageCallback cb) { onMessage_ = cb; }
  void setSendCompletedCallback(onSendCompletedCallback cb) { onSendComp_ = cb; }

private:
  Session(const Session&) = delete;

  Session operator=(const Session&) = delete;

  void handleRecv(const void* data, size_t len /*, timestamp */);

  void handleConnected();

  void handleSendUncompleted(IoCtx* ctx, size_t writtenBytes);

  void handleSendCompleted(IoCtx* ctx);

  void doSendNext(IoCtx* ioCtx);

  void trySendNext(IoCtx* ioCtx = nullptr);

private:
  std::unique_ptr<SockCtx> sockCtx_;
  std::string localAddr_;
  std::string remoteAddr_;

  Buffer inputBuf_;
  // std::mutex inputMtx_; 链式post read，无需mtx
  std::deque<std::vector<char>> sendQueue_;
  std::mutex sendMtx_;
  std::atomic<bool> isSending_ = {false};

  onConnectedCallback onConnected_;
  onMessageCallback onMessage_;
  onSendCompletedCallback onSendComp_;
};