/**
 * @file json_formatter.cpp
 * @brief Реализация JSON-форматтера логов.
 * @version 3.1.0
 * @author Artem Ulyanov (aka s21::provemet)
 * @date 2026-07-26
 */
#include "stc/logger/formatters/json_formatter.hpp"

#include <cstdio>
#include <ctime>
#include <string_view>

namespace stc::logger {

std::string JsonFormatter::Format(const LogRecord& record) const {
  std::string output;

  output.reserve(256 + record.message.size() +
                 std::string_view(record.location.file_name()).size() +
                 std::string_view(record.location.function_name()).size());

  output.append("{\"timestamp\":\"");
  AppendTimeIso8601(output, record.timestamp);
  output.append("\",\"level\":\"");
  output.append(LevelToString(record.level));
  output.append("\",\"message\":\"");
  AppendEscapedString(output, record.message);
  output.append("\",\"file\":\"");
  AppendEscapedString(output, record.location.file_name());
  output.append("\",\"function\":\"");
  AppendEscapedString(output, record.location.function_name());
  output.append("\",\"line\":");
  output.append(std::to_string(record.location.line()));
  output.append("}\n");
  return output;
}

void JsonFormatter::AppendTimeIso8601(
    std::string& output, std::chrono::system_clock::time_point tp) {
  std::time_t t = std::chrono::system_clock::to_time_t(tp);
  std::tm tm{};
  localtime_r(&t, &tm);

  char buffer[32];
  std::strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%S", &tm);
  output.append(buffer);

  auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                tp.time_since_epoch()) %
            1000;
  output.push_back('.');
  if (ms.count() < 10) {
    output.append("00");
  } else if (ms.count() < 100) {
    output.push_back('0');
  }
  output.append(std::to_string(ms.count()));
}

void JsonFormatter::AppendEscapedString(std::string& output,
                                        std::string_view str) {
  std::size_t start = 0;
  for (std::size_t i = 0; i < str.size(); ++i) {
    const char c = str[i];

    if (static_cast<unsigned char>(c) < 0x20 || c == '"' || c == '\\') {
      if (i > start) {
        output.append(str.data() + start, i - start);
      }

      switch (c) {
        case '"':
          output.append("\\\"");
          break;
        case '\\':
          output.append("\\\\");
          break;
        case '\b':
          output.append("\\b");
          break;
        case '\f':
          output.append("\\f");
          break;
        case '\n':
          output.append("\\n");
          break;
        case '\r':
          output.append("\\r");
          break;
        case '\t':
          output.append("\\t");
          break;
        default: {
          char hex[8];
          std::snprintf(hex, sizeof(hex), "\\u%04x",
                        static_cast<unsigned char>(c));
          output.append(hex);
          break;
        }
      }
      start = i + 1;
    }
  }

  if (start < str.size()) {
    output.append(str.data() + start, str.size() - start);
  }
}

std::string_view JsonFormatter::LevelToString(LogLevel level) {
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