#include "stc/logger/formatters/text_formatter.hpp"

#include <ctime>

namespace stc::logger {

TextFormatter::TextFormatter(std::string pattern, std::string time_format)
    : pattern_(std::move(pattern)), time_format_(std::move(time_format)) {}

std::string TextFormatter::Format(const LogRecord& record) const {
  std::string output;
  // Резервируем память для уменьшения количества реаллокаций в куче.
  // +64 байт на случай длинных имен файлов и функций.
  output.reserve(pattern_.size() + record.message.size() + 64);

  for (size_t i = 0; i < pattern_.size(); ++i) {
    if (pattern_[i] == '%' && i + 1 < pattern_.size()) {
      // Проверка плейсхолдеров через std::string::compare (быстрее, чем substr)
      if (pattern_.compare(i, 4, "%msg") == 0) {
        output.append(record.message);
        i += 3;
      } else if (pattern_.compare(i, 6, "%level") == 0) {
        output.append(LevelToString(record.level));
        i += 5;
      } else if (pattern_.compare(i, 5, "%file") == 0) {
        output.append(record.location.file_name());
        i += 4;
      } else if (pattern_.compare(i, 5, "%func") == 0) {
        output.append(record.location.function_name());
        i += 4;
      } else if (pattern_.compare(i, 5, "%line") == 0) {
        output.append(std::to_string(record.location.line()));
        i += 4;
      } else if (pattern_.compare(i, 5, "%time") == 0) {
        output.append(FormatTime(record.timestamp));
        i += 4;
      } else {
        output.push_back(pattern_[i]);
      }
    } else {
      output.push_back(pattern_[i]);
    }
  }
  return output;
}

std::string TextFormatter::FormatTime(
    std::chrono::system_clock::time_point tp) const {
  std::time_t t = std::chrono::system_clock::to_time_t(tp);
  std::tm tm{};

  // Потокобезопасная версия localtime для POSIX (Linux)
  localtime_r(&t, &tm);

  char buffer[64];
  std::strftime(buffer, sizeof(buffer), time_format_.c_str(), &tm);
  return std::string(buffer);
}

std::string_view TextFormatter::LevelToString(LogLevel level) {
  switch (level) {
    case LogLevel::kTrace:
      return "TRACE";
    case LogLevel::kDebug:
      return "DEBUG";
    case LogLevel::kInfo:
      return "INFO";
    case LogLevel::kWarning:
      return "WARNING";
    case LogLevel::kError:
      return "ERROR";
    case LogLevel::kCritical:
      return "CRITICAL";
    default:
      return "UNKNOWN";
  }
}

}  // namespace stc::logger