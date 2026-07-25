/**
 * @file xml_formatter.hpp
 * @brief Объявление форматтера логов в формате XML (NDXML).
 * @version 3.0.0
 * @author Artem Ulyanov (aka s21::provemet)
 * @date 2026-07-17
 */

#pragma once

#include <string>
#include <string_view>

#include "stc/logger/core/ilog_formatter.hpp"

namespace stc::logger {

/**
 * @class XmlFormatter
 * @brief Форматтер, преобразующий LogRecord в однострочный XML-элемент (NDXML).
 */
class XmlFormatter final : public ILogFormatter {
 public:
  XmlFormatter() = default;
  ~XmlFormatter() override = default;

  XmlFormatter(const XmlFormatter&) = delete;
  XmlFormatter& operator=(const XmlFormatter&) = delete;

  /**
   * @brief Форматирует запись лога в однострочный XML.
   * @param record Константная ссылка на анализируемую запись лога.
   * @return Строка, содержащая валидный XML-элемент <log>, заканчивающаяся
   * символом новой строки.
   */
  std::string Format(const LogRecord& record) const override;

 private:
  static void AppendEscapedString(std::string& output, std::string_view str);
  static void AppendTimeIso8601(std::string& output, std::chrono::system_clock::time_point tp);

  /**
   * @private
   * @brief Преобразует уровень логирования в строковое представление.
   * @param level Уровень логирования из перечисления LogLevel.
   * @return Строковое представление уровня (например, "INFO").
   */
  static std::string_view LevelToString(LogLevel level);
};

}  // namespace stc::logger