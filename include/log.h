#include <chrono>
#include <codecvt>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <locale>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>

class SimpleLogger {
public:
  // 获取单例
  static SimpleLogger& getInstance() {
    static SimpleLogger instance;
    return instance;
  }

  // 记录窄字符日志（std::string 或 const char*）
  template <typename... Args>
  void log(const std::string& format, Args&&... args) {
    std::lock_guard<std::mutex> lock(logMutex);
    ensureLogFileOpen();

    std::string message = formatMessage(format.c_str(), std::forward<Args>(args)...);
    logFile << "[" << currentTime() << "]" << message << std::endl;
    logFile.flush();
  }

  // 记录宽字符日志（std::wstring 或 const wchar_t*）
  template <typename... Args>
  void log(const std::wstring& format, Args&&... args) {
    std::lock_guard<std::mutex> lock(logMutex);
    ensureLogFileOpen();

    std::wstring message    = formatMessage(format.c_str(), std::forward<Args>(args)...);
    std::string utf8Message = wstringToUtf8(message);
    logFile << "[" << currentTime() << "]" << utf8Message << std::endl;
    logFile.flush();
  }

private:
  std::ofstream logFile;
  std::mutex logMutex;

  SimpleLogger() { ensureLogFileOpen(); }

  void ensureLogFileOpen() {
    if (!logFile.is_open()) {
      logFile.open("appLog.log", std::ios::out | std::ios::app);
      if (!logFile.is_open()) {
        std::cerr << "无法打开日志文件 appLog.log" << std::endl;
      }
    }
  }

  // 时间戳
  std::string currentTime() {
    auto now            = std::chrono::system_clock::now();
    std::time_t timeNow = std::chrono::system_clock::to_time_t(now);
    std::tm tmNow{};
#ifdef _WIN32
    localtime_s(&tmNow, &timeNow);
#else
    localtime_r(&timeNow, &tmNow);
#endif
    std::ostringstream oss;
    oss << std::put_time(&tmNow, "%Y-%m-%d %H:%M:%S");
    return oss.str();
  }

  // 格式化多字节字符串
  template <typename... Args>
  std::string formatMessage(const char* format, Args&&... args) {
    int size = std::snprintf(nullptr, 0, format, std::forward<Args>(args)...) + 1;
    if (size <= 0)
      return "格式化失败";
    std::unique_ptr<char[]> buf(new char[size]);
    std::snprintf(buf.get(), size, format, std::forward<Args>(args)...);
    return std::string(buf.get());
  }

  // 格式化宽字符字符串
  template <typename... Args>
  std::wstring formatMessage(const wchar_t* format, Args&&... args) {
    int size = std::swprintf(nullptr, 0, format, std::forward<Args>(args)...) + 1;
    if (size <= 0)
      return L"格式化失败";
    std::unique_ptr<wchar_t[]> buf(new wchar_t[size]);
    std::swprintf(buf.get(), size, format, std::forward<Args>(args)...);
    return std::wstring(buf.get());
  }

  // 宽字符转 UTF-8
  std::string wstringToUtf8(const std::wstring& wstr) {
    int size_needed = WideCharToMultiByte(CP_UTF8,
                                          0,
                                          wstr.c_str(),
                                          (int)wstr.size(),
                                          nullptr,
                                          0,
                                          nullptr,
                                          nullptr);
    std::string strTo(size_needed, 0);
    WideCharToMultiByte(CP_UTF8,
                        0,
                        wstr.c_str(),
                        (int)wstr.size(),
                        &strTo[0],
                        size_needed,
                        nullptr,
                        nullptr);
    return strTo;
  }

  // 禁用拷贝与赋值
  SimpleLogger(const SimpleLogger&)            = delete;
  SimpleLogger& operator=(const SimpleLogger&) = delete;
};

// 简化宏
#define LOG(format, ...) SimpleLogger::getInstance().log(format, ##__VA_ARGS__)
