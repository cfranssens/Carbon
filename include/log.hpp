#pragma once

#include <chrono>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <ostream>
#include <sstream>
#include <string>
#include <thread>
enum class LogLevel { Debug, Info, Warn, Error };

class Logger {
public:
  // Static instance of the logger
  static Logger &instance() {
    static Logger instance;
    return instance;
  }

  void setLogFile(const std::string &fp) {
    std::lock_guard lock(m_mtx);
    m_logFile.open(fp, std::ios::in | std::ios::out);
  }

  void log(LogLevel level, const std::string &msg) {
    auto now = std::chrono::system_clock::now();
    auto t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                  now.time_since_epoch()) %
              1000;

    std::ostringstream oss;
    oss << std::put_time(std::localtime(&t), "%H:%M:%S") << "."
        << std::setfill('0') << std::setw(3) << ms.count() << " ["
        << levelToString(level) << "]" << " [T" << std::this_thread::get_id()
        << "] " << msg << std::endl;

    std::lock_guard lock(m_mtx);
    std::cerr << oss.str();
    if (m_logFile.is_open())
      m_logFile << oss.str();
  }

private:
  Logger() = default;
  std::ofstream m_logFile;
  std::mutex m_mtx;
  std::string levelToString(LogLevel level) {
    switch (level) {
    case LogLevel::Debug:
      return "DEBUG";
    case LogLevel::Info:
      return "INFO";
    case LogLevel::Warn:
      return "WARN";
    case LogLevel::Error:
      return "ERROR";
    }

    return "UNKNOWN";
  };
};

#define LOG_DEBUG(msg) Logger::instance().log(LogLevel::Debug, msg)
#define LOG_INFO(msg) Logger::instance().log(LogLevel::Info, msg)
#define LOG_WARN(msg) Logger::instance().log(LogLevel::Warn, msg)
#define LOG_ERROR(msg) Logger::instance().log(LogLevel::Error, msg)
