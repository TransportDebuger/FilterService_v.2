#include "stc/logger/formatters/json_formatter.hpp"

#include <cstdio>
#include <ctime>
#include <string_view>

namespace stc::logger {

std::string JsonFormatter::Format(const LogRecord& record) const {
  // Резервируем память, чтобы избежать множественных реаллокаций в куче.
  // +256 байт на служебные символы JSON, ключи и время.
  std::string output;
  output.reserve(
      256 + record.message.size() +
      std::string_view(record.location.file_name()).size() +  // <-- Исправлено
      std::string_view(record.location.function_name())
          .size());  // <-- Исправлено

  output.append("{\"timestamp\":\"");
  output.append(FormatTimeIso8601(record.timestamp));

  output.append("\",\"level\":\"");
  output.append(LevelToString(record.level));

  output.append("\",\"message\":\"");
  output.append(EscapeJsonString(record.message));

  output.append("\",\"file\":\"");
  output.append(EscapeJsonString(record.location.file_name()));

  output.append("\",\"function\":\"");
  output.append(EscapeJsonString(record.location.function_name()));

  output.append("\",\"line\":");
  output.append(std::to_string(record.location.line()));

  output.append("}\n");  // NDJSON: каждая запись с новой строки

  return output;
}

std::string JsonFormatter::FormatTimeIso8601(
    std::chrono::system_clock::time_point tp) {
  std::time_t t = std::chrono::system_clock::to_time_t(tp);
  std::tm tm{};

  // Потокобезопасная версия localtime для POSIX (Linux)
  localtime_r(&t, &tm);

  // Извлекаем миллисекунды
  auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                tp.time_since_epoch()) %
            1000;

  char buffer[32];
  // Формируем базовую часть: YYYY-MM-DDThh:mm:ss
  std::strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%S", &tm);

  std::string result(buffer);
  result.append(".");

  // Добавляем миллисекунды с добивкой нулями до 3 знаков
  if (ms.count() < 10) {
    result.append("00");
  } else if (ms.count() < 100) {
    result.append("0");
  }
  result.append(std::to_string(ms.count()));

  return result;
}

std::string JsonFormatter::EscapeJsonString(std::string_view str) {
  std::string escaped;
  // Резервируем память с небольшим запасом на возможные
  // escape-последовательности
  escaped.reserve(str.size() + 8);

  for (char c : str) {
    switch (c) {
      case '"':
        escaped.append("\\\"");
        break;
      case '\\':
        escaped.append("\\\\");
        break;
      case '\b':
        escaped.append("\\b");
        break;
      case '\f':
        escaped.append("\\f");
        break;
      case '\n':
        escaped.append("\\n");
        break;
      case '\r':
        escaped.append("\\r");
        break;
      case '\t':
        escaped.append("\\t");
        break;
      default:
        // Экранируем остальные управляющие символы (ASCII < 0x20)
        if (static_cast<unsigned char>(c) < 0x20) {
          char hex[8];
          std::snprintf(hex, sizeof(hex), "\\u%04x",
                        static_cast<unsigned char>(c));
          escaped.append(hex);
        } else {
          escaped.push_back(c);
        }
        break;
    }
  }
  return escaped;
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