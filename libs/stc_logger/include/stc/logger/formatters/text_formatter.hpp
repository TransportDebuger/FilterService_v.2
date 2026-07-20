/**
 * @file text_formatter.hpp
 * @brief Объявление классического текстового форматтера логов на основе
 * шаблонов.
 * @version 3.0.0
 * @author Artem Ulyanov (aka s21::provemet)
 * @date 2026-07-17
 */

#pragma once

#include <string>

#include "stc/logger/core/ilog_formatter.hpp"

namespace stc::logger {

/**
 * @class TextFormatter
 * @brief Форматтер, преобразующий LogRecord в строку на основе
 * пользовательского шаблона.
 */
class TextFormatter final : public ILogFormatter {
 public:
  /**
   * @brief Конструктор текстового форматтера.
   * @param pattern Шаблон строки с плейсхолдерами.
   * @param time_format Формат временной метки (совместим с std::strftime).
   */
  explicit TextFormatter(
      std::string pattern = "[%Y-%m-%d %H:%M:%S] [%level] %msg\n",
      std::string time_format = "%Y-%m-%d %H:%M:%S");

  ~TextFormatter() override = default;

  TextFormatter(const TextFormatter&) = delete;
  TextFormatter& operator=(const TextFormatter&) = delete;

  /**
   * @brief Форматирует запись лога в строку согласно шаблону.
   * @param record Константная ссылка на анализируемую запись лога.
   * @return Отформатированная строка, готовая к записи.
   */
  std::string Format(const LogRecord& record) const override;

 private:
  /** @private
   * @brief Форматирует временную метку с использованием std::strftime.
   * @param tp Точка времени для форматирования.
   * @return Строка с отформатированным временем согласно time_format_.
   */
  std::string FormatTime(std::chrono::system_clock::time_point tp) const;

  /** @private
   * @brief Преобразует уровень логирования в строковое представление.
   * @param level Уровень логирования из перечисления LogLevel.
   * @return Строковое представление уровня (например, "INFO", "ERROR").
   */
  static std::string_view LevelToString(LogLevel level);

  std::string pattern_;
  std::string time_format_;
};

}  // namespace stc::logger