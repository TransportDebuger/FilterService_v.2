/**
@file config_reload_transaction.hpp
@brief Транзакционная перезагрузка конфигурации с механизмом отката.
@version 3.0.0
@date 2026-07-24
*/
#pragma once

#include <memory>

#include "../../domain/application_configuration.hpp"
#include "configuration_service.hpp"
#include "stc/logger/ilogger.hpp"

namespace stc {

/**
@class ConfigReloadTransaction
@brief Обеспечивает атомарную перезагрузку конфигурации через
ConfigurationService.
*/
class ConfigReloadTransaction {
 public:
  /**
  @brief Конструктор транзакции.
  @param[in] service Сервис управления конфигурацией.
  @param[in] logger Диспетчер логирования.
  */
  explicit ConfigReloadTransaction(
      ConfigurationService& service,
      std::shared_ptr<stc::logger::ILogger> logger);

  /// @brief Деструктор. Автоматически откатывает состояние, если транзакция не
  /// завершена.
  ~ConfigReloadTransaction();

  ConfigReloadTransaction(const ConfigReloadTransaction&) = delete;
  ConfigReloadTransaction& operator=(const ConfigReloadTransaction&) = delete;

  /**
  @brief Выполняет полную перезагрузку конфигурации.
  @return ApplicationConfiguration Новая конфигурация.
  @throw std::runtime_error При ошибке чтения или валидации.
  */
  ApplicationConfiguration reload();

 private:
  /// @private Сервис управления конфигурацией.
  ConfigurationService& service_;
  /// @private Диспетчер логирования.
  std::shared_ptr<stc::logger::ILogger> logger_;
  /// @private Флаг активности транзакции.
  bool active_{false};
};

}  // namespace stc