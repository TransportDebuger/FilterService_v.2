/**
 * @file configuration_service.hpp
 * @brief Сервис управления конфигурацией, реализующий конвейер сборки
 * (Configuration Pipeline).
 * @version 1.0.0
 * @date 2026-07-24
 */
#pragma once

#include <memory>
#include <string>
#include <unordered_map>

#include "../../application/argumentparser.hpp"
#include "../../domain/application_configuration.hpp"
#include "configloader.hpp"
#include "configvalidator.hpp"
#include "enviromentprocessor.hpp"

namespace stc {

/**
 * @class ConfigurationService
 * @brief Инкапсулирует загрузку, валидацию и мердж конфигурации из различных
 * источников.
 */
class ConfigurationService {
 public:
  /// @brief Конструктор сервиса конфигурации.
  ConfigurationService();

  /**
   * @brief Выполняет полный конвейер сборки конфигурации.
   * @param[in] cli_args Аргументы командной строки.
   * @return ApplicationConfiguration Строгий DTO с итоговой конфигурацией.
   * @throw std::runtime_error При ошибках чтения файла, валидации или парсинга.
   */
  ApplicationConfiguration BuildConfiguration(const CliArguments& cli_args);

  /**
   * @brief Перезагружает конфигурацию, используя ранее сохраненные
   * CLI-аргументы.
   * @return ApplicationConfiguration Обновленный строгий DTO.
   * @throw std::runtime_error Если сервис не был инициализирован или произошла
   * ошибка чтения.
   */
  ApplicationConfiguration ReloadConfiguration();

  /**
   * @brief Создает резервную копию текущего состояния сервиса для
   * транзакционной перезагрузки.
   */
  void BackupState();

  /**
   * @brief Восстанавливает состояние сервиса из резервной копии при откате
   * транзакции.
   */
  void RestoreState();

 private:
  /**
   * @private
   * @brief Применяет CLI-переопределения к сырому JSON.
   * @param[in,out] config JSON-объект для модификации.
   * @param[in] overrides Карта переопределений.
   */
  void ApplyCliOverrides(
      nlohmann::json& config,
      const std::unordered_map<std::string, std::string>& overrides);

  /**
   * @private
   * @brief Десериализует сырой JSON в строгий DTO ApplicationConfiguration.
   * @param[in] config Валидный JSON-объект окружения.
   * @param[in] env_name Имя целевого окружения.
   * @return ApplicationConfiguration Заполненный DTO.
   * @throw std::runtime_error При отсутствии обязательных полей.
   */
  ApplicationConfiguration Deserialize(const nlohmann::json& config,
                                       const std::string& env_name);

  /** @private
   * @brief Загрузчик JSON-файлов.*/
  ConfigLoader loader_;
  /** @private
   * @brief Валидатор структуры JSON.*/
  ConfigValidator validator_;
  /** @private
   * @brief Процессор переменных окружения.*/
  EnvironmentProcessor env_processor_;
  /** @private
   * @brief Сырая базовая конфигурация (для поддержки Reload).*/
  nlohmann::json base_config_;
  /** @private
   * @brief Резервная копия сырой конфигурации (для транзакций).*/
  nlohmann::json backup_config_;
  /** @private
   * @brief Сохраненные аргументы CLI (для поддержки Reload). */
  CliArguments current_cli_args_;
  /** @private
   * @brief Резервная копия аргументов CLI.*/
  CliArguments backup_cli_args_;
  /** @private
   * @brief Флаг первичной инициализации.*/
  bool is_initialized_{false};
};

}  // namespace stc