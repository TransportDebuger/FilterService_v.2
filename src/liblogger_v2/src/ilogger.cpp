#include "stc/ilogger.hpp"

#include <iomanip>
#include <iostream>
#include <sstream>

bool stc::TimeFormatter::setGlobalFormat(const std::string& fmt) {
  try {
    auto now = std::chrono::system_clock::now();
    format(now);
    globalFormat_ = fmt;
    return true;
  } catch (const std::exception& e) {
    std::cerr << "Ошибка формата времени: " << e.what() << std::endl;
    return false;
  }
}

std::string stc::TimeFormatter::format(
    const std::chrono::system_clock::time_point& tp) {
  try {
    auto now_time = std::chrono::system_clock::to_time_t(tp);
    std::tm now_tm;

#ifdef _WIN32
    localtime_s(&now_tm, &now_time);
#else
    localtime_r(&now_time, &now_tm);
#endif

    std::ostringstream oss;
    oss << std::put_time(&now_tm, globalFormat_.c_str());
    return oss.str();
  } catch (...) {
    return "[INVALID_TIME]";
  }
}

stc::LogLevel stc::ILogger::getLogLevel() const {
  return currentLevel_.load(std::memory_order_acquire);
}

void stc::ILogger::debug(const std::string& message) {
  log(stc::LogLevel::LOG_DEBUG, message);
}

void stc::ILogger::info(const std::string& message) {
  log(stc::LogLevel::LOG_INFO, message);
}

void stc::ILogger::warning(const std::string& message) {
  log(stc::LogLevel::LOG_WARNING, message);
}

void stc::ILogger::error(const std::string& message) {
  log(stc::LogLevel::LOG_ERROR, message);
}

void stc::ILogger::critical(const std::string& message) {
  log(stc::LogLevel::LOG_CRITICAL, message);
}

std::string stc::leveltoString(LogLevel level) {
  std::string level_;
  switch (level) {
    case LogLevel::LOG_DEBUG:
      level_ = "DEBUG";
      break;
    case LogLevel::LOG_INFO:
      level_ = "INFO";
      break;
    case LogLevel::LOG_WARNING:
      level_ = "WARNING";
      break;
    case LogLevel::LOG_ERROR:
      level_ = "ERROR";
      break;
    case LogLevel::LOG_CRITICAL:
      level_ = "CRITICAL";
      break;
    default:
      break;
  }
  return level_;
}