/**
 * @file level_filter.hpp
 * @brief Объявление класса фильтрации логов по уровню важности.
 * @version 3.0.0
 * @author Artem Ulyanov (aka s21::provemet)
 * @date 2026-07-17
 */

#pragma once

#include "stc/logger/core/ilog_filter.hpp"
#include "stc/logger/core/log_level.hpp"

namespace stc::logger {

/**
 * @class LevelFilter
 * @brief Фильтр, пропускающий сообщения с уровнем важности не ниже заданного.
 */
class LevelFilter final : public ILogFilter {
 public:
  /**
   * @brief Конструирует фильтр с заданным минимальным порогом.
   * @param min_level Минимальный уровень логирования, который будет пропущен.
   */
  explicit constexpr LevelFilter(LogLevel min_level) noexcept
      : min_level_(min_level) {}

  ~LevelFilter() override = default;

  LevelFilter(const LevelFilter&) = delete;
  LevelFilter& operator=(const LevelFilter&) = delete;

  /**
   * @brief Проверяет, проходит ли запись фильтр по уровню.
   * @param record Константная ссылка на анализируемую запись лога.
   * @return true если уровень записи >= min_level_, иначе false.
   */
  bool ShouldPass(const LogRecord& record) const override;

 private:
  LogLevel min_level_;  ///< Минимальный порог уровня логирования.
};

}  // namespace stc::logger