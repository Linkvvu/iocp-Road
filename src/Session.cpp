#include "Session.h"

Session::Session(SOCKET sock, sockaddr_in* localAddr, sockaddr_in* remoteAddr)
    : sockCtx_(std::make_unique<SockCtx>(sock)) {
  localAddr_ = std::string(::inet_ntoa(localAddr->sin_addr)) + ':' +
               std::to_string(::ntohs(localAddr->sin_port));
  remoteAddr_ = std::string(::inet_ntoa(remoteAddr->sin_addr)) + ':' +
                std::to_string(::ntohs(remoteAddr->sin_port));
}

void Session::send(const void* data, size_t len) {
  if (data == nullptr || len == 0)
    return;

  std::vector<char> buffer(reinterpret_cast<const char*>(data),
                           reinterpret_cast<const char*>(data) + len);

  {
    std::lock_guard<std::mutex> lock(sendMtx_);
    sendQueue_.emplace_back(std::move(buffer));
  }

  trySendNext();
}

void Session::handleRecv(const void* data, size_t len) {
  if (data == nullptr || len == 0)
    return;

  inputBuf_.write(data, len);
  if (onMessage_) {
    onMessage_(shared_from_this(), &inputBuf_);
  }
}

void Session::handleConnected() {
  if (onConnected_) {
    onConnected_(shared_from_this());
  }
}

void Session::handleSendUncompleted(IoCtx* ctx, size_t writtenBytes) {
  assert(writtenBytes < ctx->buffer.size());
  std::vector<char> remained(ctx->buffer.begin() + writtenBytes, ctx->buffer.end());

  // can't set the isSending flag to false, we need ensure the sequence of the content
  // isSending_.store(false, std::memory_order_release);

  {
    std::lock_guard<std::mutex> guard(sendMtx_);
    sendQueue_.emplace_front(std::move(remained));
  }

  doSendNext(ctx);
}

void Session::handleSendCompleted(IoCtx* ctx) {
  isSending_.store(false, std::memory_order_release);

  // sockCtx_->removeIoCtx(ctx);

  trySendNext(ctx);

  if (onSendComp_) {
    onSendComp_(shared_from_this());
  }
}

void Session::doSendNext(IoCtx* ioCtx) {
  std::vector<char> buffer;
  {
    std::lock_guard<std::mutex> guard(sendMtx_);
    if (sendQueue_.empty()) {
      return;
    }

    buffer = std::move(sendQueue_.front());
    sendQueue_.pop_front();
  }

  IoCtx* ctx{};
  if (ioCtx == nullptr) {
    ctx = sockCtx_->newIoCtx();
  }
  ctx->op         = OpType::SEND;
  ctx->sock       = sockCtx_->getSocket();
  ctx->buffer     = std::move(buffer);
  ctx->wsaBuf.buf = ctx->buffer.data();
  ctx->wsaBuf.len = static_cast<ULONG>(ctx->buffer.size());

  DWORD bytesSent = 0;
  DWORD flags     = 0;
  int result = WSASend(ctx->sock, &ctx->wsaBuf, 1, &bytesSent, flags, &ctx->overlapped, nullptr);

  if (result == SOCKET_ERROR && WSAGetLastError() != WSA_IO_PENDING) {
    isSending_.store(false, std::memory_order_release);
    sockCtx_->removeIoCtx(ctx);
    // TODO: handle errors
  }
}

void Session::trySendNext(IoCtx* ioCtx) {
  bool expected = false;
  if (!isSending_.compare_exchange_strong(expected, true)) {
    return;
  }

  doSendNext(ioCtx);
}
