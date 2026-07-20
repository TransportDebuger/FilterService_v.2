/**
 * @file console_sink.hpp
 * @brief Объявление приемника (Sink) для вывода логов в консоль
 * (stdout/stderr).
 * @version 3.0.0
 * @author Artem Ulyanov (aka s21::provemet)
 * @date 2026-07-17
 */

#pragma once

#include <memory>
#include <mutex>

#include "stc/logger/core/ilog_filter.hpp"
#include "stc/logger/core/ilog_formatter.hpp"
#include "stc/logger/core/ilog_sink.hpp"

namespace stc::logger {

/**
 * @class ConsoleSink
 * @brief Приемник (Sink) для вывода логов в консоль (stdout/stderr).
 */
class ConsoleSink final : public ILogSink {
 public:
  /**
   * @brief Конструктор приемника консоли.
   * @param formatter Форматтер для сериализации LogRecord (не может быть
   * nullptr).
   * @param filter Опциональный локальный фильтр для этого приемника.
   * @param use_colors Флаг включения цветного вывода (ANSI escape codes).
   * @throw std::invalid_argument Если передан nullptr в formatter.
   */
  explicit ConsoleSink(std::shared_ptr<ILogFormatter> formatter,
                       std::shared_ptr<ILogFilter> filter = nullptr,
                       bool use_colors = true);

  ~ConsoleSink() override = default;

  ConsoleSink(const ConsoleSink&) = delete;
  ConsoleSink& operator=(const ConsoleSink&) = delete;

  /**
   * @brief Записывает отформатированное сообщение в консоль.
   * @param record Оригинальная запись (используется для определения уровня и
   * маршрутизации в stdout/stderr).
   * @param formatted_message Уже отформатированная строка.
   */
  void Write(const LogRecord& record,
             std::string_view formatted_message) override;

  /// @brief Принудительно сбрасывает буферы stdout и stderr.
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
  /// @private
  std::shared_ptr<ILogFormatter>
      formatter_;  ///< Форматтер для сериализации записей.

  /// @private
  std::shared_ptr<ILogFilter> filter_;  ///< Локальный фильтр приемника.

  /// @private
  bool use_colors_;  ///< Флаг включения цветного вывода (ANSI).

  /**
   * @private
   * @brief Мьютекс для защиты от интерливинга (перемешивания) вывода из разных
   * потоков.
   */
  std::mutex mutex_;
};

}  // namespace stc::logger