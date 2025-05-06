#include "stc/consolelogger.hpp"

#include <iomanip>
#include <iostream>
#include <sstream>

stc::ConsoleLogger& stc::ConsoleLogger::instance() {
  static stc::ConsoleLogger instance;
  return instance;
}

void stc::ConsoleLogger::init(const LogLevel level = LogLevel::LOG_INFO) {
  setLogLevel(level);
}

void stc::ConsoleLogger::setLogLevel(stc::LogLevel level) {
  currentLevel_.store(level, std::memory_order_release);
}

void stc::ConsoleLogger::flush() {
  std::lock_guard<std::mutex> lock(mutex_);
  std::cout.flush();
  std::cerr.flush();
}

void stc::ConsoleLogger::log(LogLevel level, const std::string& message) {
  if (shouldSkipLog(level)) return;

    std::string formattedMsg;
    try {
        auto now = std::chrono::system_clock::now();
        std::string timestamp = TimeFormatter::format(now);
        std::ostringstream formatted;
        formatted << timestamp << " [" << leveltoString(level) << "] " << message;
        formattedMsg = formatted.str();
    } catch (const std::exception& e) {
        formattedMsg = "[LOGGER ERROR: " + (std::string)e.what() + "]";
    } catch (...) {
        formattedMsg = "[LOGGER ERROR: unknown exception]";
    }

    try {
        std::lock_guard lock(mutex_);
        setConsoleColor(level);
        std::cout << formattedMsg << std::endl;
        resetConsoleColor();
        std::cout.flush();
    } catch (const std::exception& e) {
        std::lock_guard lock(mutex_);
        resetConsoleColor();
        std::cerr << "[LOGGER ERROR: " << e.what() << "] " << formattedMsg << std::endl;
    } catch (...) {
        std::lock_guard lock(mutex_);
        resetConsoleColor();
        std::cerr << "[LOGGER ERROR: unknown exception] " << formattedMsg << std::endl;
    }
}

bool stc::ConsoleLogger::shouldSkipLog(LogLevel level) const {
  return static_cast<int>(level) <
         static_cast<int>(currentLevel_.load(std::memory_order_acquire));
}

void stc::ConsoleLogger::setConsoleColor(LogLevel level) {
#ifdef _WIN32
  HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
  WORD color = 0;

  switch (level) {
    case LogLevel::LOG_DEBUG:
      color = FOREGROUND_BLUE | FOREGROUND_GREEN;
      break;  // Cyan
    case LogLevel::LOG_INFO:
      color = FOREGROUND_GREEN;
      break;  // Green
    case LogLevel::LOG_WARNING:
      color = FOREGROUND_RED | FOREGROUND_GREEN;
      break;  // Yellow
    case LogLevel::LOG_ERROR:
      color = FOREGROUND_RED | FOREGROUND_INTENSITY;
      break;  // Bright Red
    case LogLevel::LOG_CRITICAL:
      color =
          BACKGROUND_RED | FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE;
      break;  // White on Red
    default:
      color = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE;  // White
  }

  SetConsoleTextAttribute(hConsole, color);
#else
  const char* colorCode = ANSI_COLOR_RESET;

  switch (level) {
    case LogLevel::LOG_DEBUG:
      colorCode = "\033[36m";
      break;  // Cyan
    case LogLevel::LOG_INFO:
      colorCode = "\033[32m";
      break;  // Green
    case LogLevel::LOG_WARNING:
      colorCode = "\033[33m";
      break;  // Yellow
    case LogLevel::LOG_ERROR:
      colorCode = "\033[31m";
      break;  // Red
    case LogLevel::LOG_CRITICAL:
      colorCode = "\033[41m\033[37m";
      break;  // White on Red
    default:
      colorCode = ANSI_COLOR_RESET;
  }

  std::cout << colorCode;
#endif
}

void stc::ConsoleLogger::resetConsoleColor() {
#ifdef _WIN32
  HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
  SetConsoleTextAttribute(hConsole,
                          FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
#else
  std::cout << ANSI_COLOR_RESET;
#endif
}
