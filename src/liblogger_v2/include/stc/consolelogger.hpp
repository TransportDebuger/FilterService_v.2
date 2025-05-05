#pragma once

#include "stc/ilogger.hpp"

#ifdef _WIN32
#include <windows.h>
#else
#define ANSI_COLOR_RESET "\033[0m"
#define ANSI_COLOR_RED "\033[31m"
#define ANSI_COLOR_GREEN "\033[32m"
#define ANSI_COLOR_YELLOW "\033[33m"
#define ANSI_COLOR_BLUE "\033[34m"
#define ANSI_COLOR_MAGENTA "\033[35m"
#endif

namespace stc {

class ConsoleLogger : public ILogger {
 public:
  static ConsoleLogger& instance();

  void init(const LogLevel level) override;
  void setLogLevel(LogLevel level) override;
  void setRotationConfig(const RotationConfig& config) override;
  RotationConfig getRotationConfig() const override;
  void flush() override;

 protected:
  ConsoleLogger() = default;
  ~ConsoleLogger() override = default;
  void log(LogLevel level, const std::string& message) override;
  bool shouldSkipLog(LogLevel level) const;

 private:
  void setConsoleColor(LogLevel level);
  void resetConsoleColor();
  mutable std::mutex mutex_;
  RotationConfig rotationConfig_;  // Не используется
};
}  // namespace stc