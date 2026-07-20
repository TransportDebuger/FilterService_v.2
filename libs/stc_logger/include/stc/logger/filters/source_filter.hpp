/**
 * @file source_filter.hpp
 * @brief Объявление фильтрации логов по источнику вызова (имя файла или
 * функции).
 * @version 3.0.0
 * @author Artem Ulyanov (aka s21::provemet)
 * @date 2026-07-17
 */

#pragma once

#include <string>
#include <string_view>

#include "stc/logger/core/ilog_filter.hpp"

namespace stc::logger {

/**
 * @enum SourceMatchTarget
 * @brief Определяет, какую часть std::source_location использовать для
 * фильтрации.
 */
enum class SourceMatchTarget {
  kFileName,  ///< Имя файла (например, "xml_processor.cpp").
  kFunctionName  ///< Имя функции (например, "void XMLProcessor::Process()").
};

/**
 * @enum SourceMatchMode
 * @brief Определяет тип сравнения строки источника с шаблоном.
 */
enum class SourceMatchMode {
  kExact,      ///< Полное совпадение строки
  kContains,   ///< Наличие подстроки
  kStartsWith  ///< Строка начинается с шаблона
};

/**
 * @class SourceFilter
 * @brief Фильтр, отсекающий или пропускающий логи на основе имени файла или
 * функции.
 */
class SourceFilter final : public ILogFilter {
 public:
  /**
   * @brief Конструирует фильтр по источнику вызова.
   * @param pattern Строка для поиска или сравнения.
   * @param target Что сравниваем (имя файла или имя функции).
   * @param mode Режим сравнения (точное, подстрока, начало).
   * @param invert Если true, логика инвертируется (Blacklist). По умолчанию
   * false (Whitelist).
   */
  SourceFilter(std::string pattern,
               SourceMatchTarget target = SourceMatchTarget::kFileName,
               SourceMatchMode mode = SourceMatchMode::kContains,
               bool invert = false);

  ~SourceFilter() override = default;

  SourceFilter(const SourceFilter&) = delete;
  SourceFilter& operator=(const SourceFilter&) = delete;

  /**
   * @brief Проверяет, проходит ли запись фильтр по источнику вызова.
   * @param record Константная ссылка на анализируемую запись лога.
   * @return true если запись проходит фильтр, иначе false.
   */
  bool ShouldPass(const LogRecord& record) const override;

 private:
  /** @private
   * @brief Выполняет сравнение строки источника с шаблоном согласно режиму.
   */
  bool Match(std::string_view source_value) const;

  /// @private
  std::string pattern_;  ///< Строка для поиска/сравнения.

  /// @private
  SourceMatchTarget target_;  ///< Цель сравнения (файл или функция).

  /// @private
  SourceMatchMode mode_;  ///< Режим сравнения (точное, подстрока, начало).

  /// @private
  bool invert_;  ///< Флаг инверсии логики (Blacklist).
};

}  // namespace stc::logger