/**
 * @file composite_filter.hpp
 * @brief Объявление композитного фильтра, объединяющего несколько фильтров
 * логическим оператором.
 * @version 3.0.0
 * @author Artem Ulyanov (aka s21::provemet)
 * @date 2026-07-17
 */

#pragma once

#include <memory>
#include <vector>

#include "stc/logger/core/ilog_filter.hpp"

namespace stc::logger {

/**
 * @enum LogicOperator
 * @brief Логические операторы для композиции фильтров.
 */
enum class LogicOperator {
  kAnd,  ///< Все фильтры должны вернуть true.
  kOr  ///< Хотя бы один фильтр должен вернуть true.
};

/**
 * @class CompositeFilter
 * @brief Композитный фильтр, объединяющий несколько фильтров логическим
 * оператором.
 */
class CompositeFilter final : public ILogFilter {
 public:
  /**
   * @brief Конструктор композитного фильтра.
   * @param filters Вектор умных указателей на дочерние фильтры.
   * @param op Логический оператор (kAnd или kOr).
   * @throw std::invalid_argument Если вектор filters пуст.
   */
  CompositeFilter(std::vector<std::shared_ptr<ILogFilter>> filters,
                  LogicOperator op);

  ~CompositeFilter() override = default;

  CompositeFilter(const CompositeFilter&) = delete;
  CompositeFilter& operator=(const CompositeFilter&) = delete;

  /**
   * @brief Проверяет запись, применяя логику композиции к дочерним фильтрам.
   * @param record Константная ссылка на анализируемую запись лога.
   * @return true если запись проходит композицию, иначе false.
   */
  bool ShouldPass(const LogRecord& record) const override;

 private:
  /// @private
  std::vector<std::shared_ptr<ILogFilter>> filters_;  ///< Дочерние фильтры.

  /// @private
  LogicOperator op_;  ///< Логический оператор композиции.
};

}  // namespace stc::logger