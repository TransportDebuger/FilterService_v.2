#include "stc/logger/sinks/console/console_sink.hpp"

#include <iostream>
#include <stdexcept>

namespace stc::logger {

namespace {
// ANSI escape codes for Linux terminals
constexpr std::string_view kColorReset = "\033[0m";
constexpr std::string_view kColorTrace = "\033[36m";       // Cyan
constexpr std::string_view kColorDebug = "\033[32m";       // Green
constexpr std::string_view kColorWarning = "\033[33m";     // Yellow
constexpr std::string_view kColorError = "\033[31m";       // Red
constexpr std::string_view kColorCritical = "\033[1;31m";  // Bold Red
}  // namespace

ConsoleSink::ConsoleSink(std::shared_ptr<ILogFormatter> formatter,
                         std::shared_ptr<ILogFilter> filter, bool use_colors)
    : formatter_(std::move(formatter)),
      filter_(std::move(filter)),
      use_colors_(use_colors) {
  if (!formatter_) {
    throw std::invalid_argument("ConsoleSink: formatter cannot be null");
  }
}

void ConsoleSink::Write(const LogRecord& record,
                        std::string_view formatted_message) {
  std::lock_guard<std::mutex> lock(mutex_);

  // Маршрутизация: WARNING и выше идут в stderr, остальные в stdout
  std::ostream& out =
      (record.level >= LogLevel::kWarning) ? std::cerr : std::cout;

  // Применение цвета к конкретному потоку
  if (use_colors_) {
    switch (record.level) {
      case LogLevel::kTrace:
        out << kColorTrace;
        break;
      case LogLevel::kDebug:
        out << kColorDebug;
        break;
      case LogLevel::kWarning:
        out << kColorWarning;
        break;
      case LogLevel::kError:
        out << kColorError;
        break;
      case LogLevel::kCritical:
        out << kColorCritical;
        break;
      default:
        break;  // kInfo не имеет цвета (используется стандартный)
    }
  }

  out << formatted_message;

  // Сброс цвета в том же потоке
  if (use_colors_) {
    out << kColorReset;
  }

  // Критические сообщения должны быть немедленно видны, даже если буфер не
  // полон
  if (record.level == LogLevel::kCritical) {
    out.flush();
  }
}

void ConsoleSink::Flush() {
  std::lock_guard<std::mutex> lock(mutex_);
  std::cout.flush();
  std::cerr.flush();
}

std::shared_ptr<ILogFormatter> ConsoleSink::GetFormatter() const noexcept {
  return formatter_;
}

std::shared_ptr<ILogFilter> ConsoleSink::GetFilter() const noexcept {
  return filter_;
}

}  // namespace stc::logger