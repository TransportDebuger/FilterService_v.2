/**
 * @file ilogger.hpp
 * @author Artem Ulyanov
 * @date March 2025
 * @brief Заголовочный файл класса ILogger и вспомогательных компонентов.
 */

#pragma once

#include <atomic>
#include <chrono>
#include <mutex>
#include <string>

namespace stc {

enum class LogLevel {
  LOG_DEBUG,
  LOG_INFO,
  LOG_WARNING,
  LOG_ERROR,
  LOG_CRITICAL
};

enum class RotationType { NONE, SIZE, TIME };

struct RotationConfig {
  bool enabled = false;
  RotationType type = RotationType::NONE;
  size_t maxFileSizeBytes = 0;  //Максимальный размер лог-файла по достижению
                                //которого происходит ротация лога.
  std::chrono::seconds rotationInterval{0};  // например, 24h = 86400s
  std::chrono::system_clock::time_point lastRotationTime;
  std::string filenamePattern;

  RotationConfig() = default;
};

class TimeFormatter {
 public:
  static bool setGlobalFormat(const std::string& fmt);

  static std::string format(const std::chrono::system_clock::time_point& tp);

 private:
  inline static std::string globalFormat_ = "%Y-%m-%d %T";
};

class ILogger {
 public:
  virtual void init(const LogLevel level) = 0;

  virtual void setLogLevel(LogLevel level) = 0;
  virtual LogLevel getLogLevel() const;
  virtual void setRotationConfig(const RotationConfig& config) = 0;
  virtual RotationConfig getRotationConfig() const = 0;

  virtual void debug(const std::string& message);
  virtual void info(const std::string& message);
  virtual void warning(const std::string& message);
  virtual void error(const std::string& message);
  virtual void critical(const std::string& message);

  virtual void flush() = 0;

 protected:
  std::atomic<LogLevel> currentLevel_ = LogLevel::LOG_INFO;
  virtual ~ILogger() = default;
  virtual void log(LogLevel, const std::string&) = 0;
  virtual bool shouldSkipLog(LogLevel level) const = 0;
};

std::string leveltoString(LogLevel level);

}  // namespace stc