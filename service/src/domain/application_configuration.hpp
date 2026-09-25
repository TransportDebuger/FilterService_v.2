/**
@file application_configuration.hpp
@brief DTO, агрегирующий полностью собранную и провалидированную конфигурацию
запуска.
@version 1.0.0
@date 2026-07-28
*/
#pragma once

#include <string>
#include <vector>

#include "logging_config.hpp"
#include "sourceconfig.hpp"

namespace stc {

/**
 * @struct ApplicationConfiguration
 * @brief Инкапсулирует результат работы конвейера конфигурации (Configuration
 * Pipeline).
 */
struct ApplicationConfiguration {
  /// @brief Имя активного окружения (например, "production").
  std::string environment_name;

  /// @brief Вектор строгих DTO конфигураций источников данных.
  std::vector<SourceConfig> sources;

  /// @brief Вектор строгих DTO конфигураций приемников логирования.
  std::vector<LoggerSinkConfig> loggers;

  /// @brief Путь к глобальному CSV-файлу списков фильтрации.
  std::string global_comparison_list;
};

}  // namespace stc