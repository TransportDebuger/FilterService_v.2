/**
 * @file ilog_filter.hpp
 * @brief Определение абстрактного интерфейса фильтра логирования.
 * @version 3.0.0
 * @author Artem Ulyanov (aka s21::provemet)
 * @date 2026-07-17
 */

#pragma once

#include "stc/logger/core/log_record.hpp"

namespace stc::logger {

/**
 * @class ILogFilter
 * @brief Абстрактный базовый класс, определяющий контракт для всех
 *        реализаций фильтров.
 */
class ILogFilter {
 public:
  /// @brief Виртуальный деструктор по умолчанию.
  virtual ~ILogFilter() = default;

  /**
   * @brief Проверяет, должна ли запись быть пропущена через фильтр.
   * @param record Константная ссылка на анализируемую запись лога.
   * @return true если запись проходит фильтр, иначе false.
   */
  virtual bool ShouldPass(const LogRecord& record) const = 0;
};

}  // namespace stc::logger