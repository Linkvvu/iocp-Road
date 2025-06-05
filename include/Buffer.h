#pragma once

#include <algorithm>
#include <cstddef>
#include <cstring>
#include <vector>

class Buffer {
  using size_t = std::size_t;

public:
  // 构造函数，默认大小
  explicit Buffer(size_t initialSize = 1024 * 4)
      : buffer_(initialSize)
      , readPos_(0)
      , writePos_(0) {}

  // 写入数据
  void write(const void* data, size_t length) {
    ensureWriteSpace(length);
    std::memcpy(buffer_.data() + writePos_, data, length);
    writePos_ += length;
  }

  // 读取数据
  size_t read(void* output, size_t length) {
    length = std::min<size_t>(length, readableBytes());
    if (length == 0)
      return 0;

    std::memcpy(output, buffer_.data() + readPos_, length);
    readPos_ += length;

    // 如果所有数据都已读取，重置位置
    if (readPos_ == writePos_) {
      readPos_ = writePos_ = 0;
    }

    return length;
  }

  // 获取当前可读数据大小
  size_t readableBytes() const { return writePos_ - readPos_; }

  // 获取当前可写空间大小（连续空间）
  size_t writableBytes() const { return buffer_.size() - writePos_; }

  // 获取缓冲区容量
  size_t capacity() const { return buffer_.size(); }

  // 清空缓冲区
  void clear() {
    readPos_ = writePos_ = 0;
    buffer_.clear();
  }

  // 获取可读数据的指针
  const char* peek() const { return buffer_.data() + readPos_; }

  // 丢弃已读取的数据
  void retrieve(size_t len) {
    if (len > readableBytes()) {
      throw std::out_of_range("Buffer::retrieve");
    }
    readPos_ += len;

    if (readPos_ == writePos_) {
      readPos_ = writePos_ = 0;
    }
  }

private:
  // 获取总可用空间（前面空闲+后面空闲）
  size_t totalWritableBytes() const { return buffer_.size() - (writePos_ - readPos_); }

  // 确保有足够的写入空间
  void ensureWriteSpace(size_t length) {
    // 如果后面空间足够，直接返回
    if (length <= writableBytes()) {
      return;
    }

    // 计算总可用空间（前面空闲+后面空闲）
    size_t totalAvailable = totalWritableBytes();

    if (length <= totalAvailable) {
      // 移动可读数据到头部
      size_t readable = readableBytes();
      if (readable > 0) {
        std::memmove(buffer_.data(), buffer_.data() + readPos_, readable);
      }
      readPos_  = 0;
      writePos_ = readable;
    } else {
      // 需要扩容 - 至少扩容到当前内容+新数据的两倍大小
      size_t newSize = std::max<size_t>(buffer_.size() * 2, writePos_ + length);
      buffer_.resize(newSize);
    }
  }

  std::vector<char> buffer_;
  size_t readPos_  = 0;
  size_t writePos_ = 0;
};