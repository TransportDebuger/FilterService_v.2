#include "stc/logger/logger.hpp"

#include <chrono>
#include <mutex>
#include <shared_mutex>
#include <stdexcept>
#include <utility>

namespace stc::logger {

Logger::Logger(std::string name) : name_(std::move(name)) {}

Logger::~Logger() = default;

void Logger::AddSink(std::shared_ptr<ILogSink> sink) {
  if (!sink) {
    throw std::invalid_argument("Logger::AddSink: sink cannot be null");
  }
  std::unique_lock lock(mutex_);
  sinks_.push_back(std::move(sink));
}

void Logger::AddGlobalFilter(std::shared_ptr<ILogFilter> filter) {
  if (!filter) {
    throw std::invalid_argument(
        "Logger::AddGlobalFilter: filter cannot be null");
  }
  std::unique_lock lock(mutex_);
  global_filters_.push_back(std::move(filter));
}

void Logger::Log(LogLevel level, std::string_view message,
                 std::source_location location) {
  // 1. Формируем запись лога.
  LogRecord record{std::chrono::system_clock::now(), level,
                   std::string(message), location};

  // 2. Захватываем мьютекс в режиме чтения (shared_lock).
  std::shared_lock lock(mutex_);

  // 3. Проверка глобальных фильтров (Short-circuit evaluation).
  for (const auto& filter : global_filters_) {
    if (!filter->ShouldPass(record)) {
      return;
    }
  }

  // 4. Диспетчеризация по Sink'ам
  for (const auto& sink : sinks_) {
    // 4.1. Проверка локального фильтра Sink'а
    auto sink_filter = sink->GetFilter();
    if (sink_filter && !sink_filter->ShouldPass(record)) {
      continue;  // Фильтр Sink'а отклонил сообщение
    }

    // 4.2. Форматирование сообщения специфичным для Sink'а форматтером
    auto formatter = sink->GetFormatter();
    if (!formatter) {
      continue;  // Нарушение контракта, но мы не должны падать
    }

    std::string formatted_msg = formatter->Format(record);

    // 4.3. Физическая запись
    sink->Write(record, formatted_msg);
  }
}

// === Методы-обертки ===

void Logger::Trace(std::string_view message, std::source_location location) {
  Log(LogLevel::kTrace, message, location);
}

void Logger::Debug(std::string_view message, std::source_location location) {
  Log(LogLevel::kDebug, message, location);
}

void Logger::Info(std::string_view message, std::source_location location) {
  Log(LogLevel::kInfo, message, location);
}

void Logger::Warning(std::string_view message, std::source_location location) {
  Log(LogLevel::kWarning, message, location);
}

void Logger::Error(std::string_view message, std::source_location location) {
  Log(LogLevel::kError, message, location);
}

void Logger::Critical(std::string_view message, std::source_location location) {
  Log(LogLevel::kCritical, message, location);
}

void Logger::Flush() {
  std::shared_lock lock(mutex_);
  for (const auto& sink : sinks_) {
    sink->Flush();
  }
}

}  // namespace stc::logger