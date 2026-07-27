/**
 * @file xml_formatter.cpp
 * @brief Реализация XML-форматтера логов.
 * @version 3.1.0
 * @author Artem Ulyanov (aka s21::provemet)
 * @date 2026-07-26
 */
#include "stc/logger/formatters/xml_formatter.hpp"

#include <cstdio>
#include <ctime>
#include <string_view>

namespace stc::logger {

std::string XmlFormatter::Format(const LogRecord& record) const {
  std::string output;
  output.reserve(256 + record.message.size() +
                 std::string_view(record.location.file_name()).size() +
                 std::string_view(record.location.function_name()).size());

  output.append("<log timestamp=\"");
  AppendTimeIso8601(output, record.timestamp);
  output.append("\" level=\"");
  output.append(LevelToString(record.level));
  output.append("\" file=\"");
  AppendEscapedString(output, record.location.file_name());
  output.append("\" function=\"");
  AppendEscapedString(output, record.location.function_name());
  output.append("\" line=\"");
  output.append(std::to_string(record.location.line()));
  output.append("\">");
  AppendEscapedString(output, record.message);
  output.append("</log>\n");
  return output;
}

void XmlFormatter::AppendTimeIso8601(std::string& output,
                                     std::chrono::system_clock::time_point tp) {
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
  if (ms.count() < 10)
    output.append("00");
  else if (ms.count() < 100)
    output.push_back('0');
  output.append(std::to_string(ms.count()));
}

void XmlFormatter::AppendEscapedString(std::string& output,
                                       std::string_view str) {
  std::size_t start = 0;
  for (std::size_t i = 0; i < str.size(); ++i) {
    const char c = str[i];
    // XML требует экранирования: & < > " ' и управляющих символов (кроме \\t \\n
    // \\r)
    bool needs_escape =
        (c == '&' || c == '<' || c == '>' || c == '"' || c == '\'');
    if (!needs_escape && static_cast<unsigned char>(c) < 0x20) {
      if (c != '\t' && c != '\n' && c != '\r') {
        needs_escape = true;
      }
    }

    if (needs_escape) {
      if (i > start) output.append(str.data() + start, i - start);

      switch (c) {
        case '&':
          output.append("&amp;");
          break;
        case '<':
          output.append("&lt;");
          break;
        case '>':
          output.append("&gt;");
          break;
        case '"':
          output.append("&quot;");
          break;
        case '\'':
          output.append("&apos;");
          break;
        default: {
          char hex[16];
          std::snprintf(hex, sizeof(hex), "&#x%02X;",
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

std::string_view XmlFormatter::LevelToString(LogLevel level) {
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