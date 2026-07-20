#include "stc/logger/formatters/xml_formatter.hpp"

#include <cstdio>
#include <ctime>
#include <string_view>

namespace stc::logger {

std::string XmlFormatter::Format(const LogRecord& record) const {
  // Резервируем память, чтобы избежать множественных реаллокаций в куче.
  std::string output;
  output.reserve(
      256 + record.message.size() +
      std::string_view(record.location.file_name()).size() +  // <-- Исправлено
      std::string_view(record.location.function_name())
          .size());  // <-- Исправлено

  output.append("<log timestamp=\"");
  output.append(FormatTimeIso8601(record.timestamp));

  output.append("\" level=\"");
  output.append(LevelToString(record.level));

  output.append("\" file=\"");
  output.append(EscapeXmlString(record.location.file_name()));

  output.append("\" function=\"");
  output.append(EscapeXmlString(record.location.function_name()));

  output.append("\" line=\"");
  output.append(std::to_string(record.location.line()));

  output.append("\">");
  output.append(EscapeXmlString(record.message));
  output.append("</log>\n");  // NDXML: каждая запись с новой строки

  return output;
}

std::string XmlFormatter::FormatTimeIso8601(
    std::chrono::system_clock::time_point tp) {
  std::time_t t = std::chrono::system_clock::to_time_t(tp);
  std::tm tm{};

  // Потокобезопасная версия localtime для POSIX (Linux)
  localtime_r(&t, &tm);

  auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                tp.time_since_epoch()) %
            1000;

  char buffer[32];
  std::strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%S", &tm);

  std::string result(buffer);
  result.append(".");

  if (ms.count() < 10) {
    result.append("00");
  } else if (ms.count() < 100) {
    result.append("0");
  }
  result.append(std::to_string(ms.count()));

  return result;
}

std::string XmlFormatter::EscapeXmlString(std::string_view str) {
  std::string escaped;
  escaped.reserve(str.size() + 8);

  for (char c : str) {
    switch (c) {
      case '&':
        escaped.append("&amp;");
        break;
      case '<':
        escaped.append("&lt;");
        break;
      case '>':
        escaped.append("&gt;");
        break;
      case '"':
        escaped.append("&quot;");
        break;
      case '\'':
        escaped.append("&apos;");
        break;
      default:
        escaped.push_back(c);
        break;
    }
  }
  return escaped;
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