/**
 * @file file_sink.hpp
 * @brief Объявление файлового приемника (Sink) с поддержкой ротации логов.
 * @version 3.0.0
 * @author Artem Ulyanov (aka s21::provemet)
 * @date 2026-07-17
 */

#pragma once

#include <cstdint>
#include <fstream>
#include <memory>
#include <mutex>
#include <string>

#include "stc/logger/core/ilog_filter.hpp"
#include "stc/logger/core/ilog_formatter.hpp"
#include "stc/logger/core/ilog_sink.hpp"
#include "stc/logger/sinks/file/rotation_policy.hpp"

namespace stc::logger {

/**
 * @class FileSink
 * @brief Приемник (Sink) для синхронной записи логов в файл с поддержкой
 * ротации.
 */
class FileSink final : public ILogSink {
 public:
  /**
   * @brief Конструктор файлового приемника.
   * @param file_path Путь к лог-файлу.
   * @param formatter Форматтер для сериализации LogRecord (не может быть
   * nullptr).
   * @param filter Опциональный локальный фильтр для этого приемника.
   * @param rotation_policy Опциональная стратегия ротации. Если nullptr,
   * ротация не выполняется.
   * @throw std::invalid_argument Если formatter равен nullptr.
   */
  FileSink(std::string file_path, std::shared_ptr<ILogFormatter> formatter,
           std::shared_ptr<ILogFilter> filter = nullptr,
           std::shared_ptr<IRotationPolicy> rotation_policy = nullptr);

  ~FileSink() override;

  FileSink(const FileSink&) = delete;
  FileSink& operator=(const FileSink&) = delete;

  /**
   * @brief Записывает отформатированное сообщение в файл.
   * @param record Оригинальная запись (используется для проверки условий
   * ротации).
   * @param formatted_message Уже отформатированная строка для записи.
   */
  void Write(const LogRecord& record,
             std::string_view formatted_message) override;

  /**
   * @brief Принудительно сбрасывает буферы файлового потока на диск.
   */
  void Flush() override;

  /**
   * @brief Возвращает форматтер, связанный с этим приемником.
   * @return Умный указатель на реализацию ILogFormatter.
   */
  std::shared_ptr<ILogFormatter> GetFormatter() const noexcept;

  /**
   * @brief Возвращает локальный фильтр приемника.
   * @return Умный указатель на реализацию ILogFilter или nullptr, если фильтр
   * не задан.
   */
  std::shared_ptr<ILogFilter> GetFilter() const noexcept;

 private:
  /**
   * @private
   * @brief Открывает файл и инициализирует current_size_.
   */
  void OpenFile();

  /**
   * @private
   * @brief Проверяет условия ротации и выполняет её при необходимости.
   * @param now Текущее системное время для передачи в политику ротации.
   */
  void RotateIfNeeded(std::chrono::system_clock::time_point now);

  /// @private
  std::string file_path_;  ///< Путь к лог-файлу.

  /// @private
  std::shared_ptr<ILogFormatter>
      formatter_;  ///< Форматтер для сериализации записей.

  /// @private
  std::shared_ptr<ILogFilter> filter_;  ///< Локальный фильтр приемника.

  /// @private
  std::shared_ptr<IRotationPolicy>
      rotation_policy_;  ///< Стратегия ротации (может быть nullptr).

  /// @private
  std::ofstream file_stream_;  ///< Поток файлового ввода-вывода.

  /// @private
  std::uint64_t current_size_ =
      0;  ///< Текущий размер файла в байтах (отслеживается в памяти).

  /// @private
  std::mutex
      mutex_;  ///< Мьютекс для защиты файлового потока и процесса ротации.
};

}  // namespace stc::logger