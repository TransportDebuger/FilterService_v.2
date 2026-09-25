/**
@file configuration_service.cpp
@brief Реализация сервиса управления конфигурацией.
@version 1.0.0
@date 2026-07-24
*/
#include "configuration_service.hpp"

#include <nlohmann/json.hpp>
#include <stdexcept>

#include "../../domain/logging_config.hpp"
#include "../../domain/sourceconfig.hpp"

namespace stc {

ConfigurationService::ConfigurationService() = default;

ApplicationConfiguration ConfigurationService::BuildConfiguration(
    const CliArguments& cli_args) {
  // 1. Загрузка и обработка ENV (работаем с локальной копией для безопасности)
  nlohmann::json new_config = loader_.loadFromFile(cli_args.config_path);
  env_processor_.process(new_config);

  // 2. Применение CLI-переопределений
  if (!cli_args.overrides.empty()) {
    ApplyCliOverrides(new_config, cli_args.overrides);
  }

  // 3. Валидация структуры
  if (!validator_.validateRoot(new_config)) {
    throw std::runtime_error("ConfigurationService: Root validation failed");
  }

  // 4. Десериализация в строгий DTO
  ApplicationConfiguration app_config =
      Deserialize(new_config, cli_args.environment);

  // 5. Фиксация состояния (только после успешного прохождения всех этапов)
  base_config_ = std::move(new_config);
  current_cli_args_ = cli_args;
  is_initialized_ = true;

  return app_config;
}

ApplicationConfiguration ConfigurationService::ReloadConfiguration() {
  if (!is_initialized_) {
    throw std::runtime_error(
        "ConfigurationService: Cannot reload, service not initialized");
  }
  return BuildConfiguration(current_cli_args_);
}

void ConfigurationService::BackupState() {
  backup_config_ = base_config_;
  backup_cli_args_ = current_cli_args_;
}

void ConfigurationService::RestoreState() {
  if (!backup_config_.is_null()) {
    base_config_ = std::move(backup_config_);
    current_cli_args_ = backup_cli_args_;
    backup_config_.clear();
  }
}

void ConfigurationService::ApplyCliOverrides(
    nlohmann::json& config,
    const std::unordered_map<std::string, std::string>& overrides) {
  nlohmann::json override_json;
  for (const auto& [key, value] : overrides) {
    override_json[key] = value;
  }
  config.merge_patch(override_json);
}

ApplicationConfiguration ConfigurationService::Deserialize(
    const nlohmann::json& config, const std::string& env_name) {
  if (!config.contains("environments") ||
      !config["environments"].contains(env_name)) {
    throw std::runtime_error("Environment '" + env_name + "' not found");
  }

  const auto& env_config = config["environments"][env_name];
  ApplicationConfiguration app_config;
  app_config.environment_name = env_name;

  // Десериализация источников (Sources)
  if (!env_config.contains("sources") || !env_config["sources"].is_array()) {
    throw std::runtime_error("Sources array not found for environment: " +
                             env_name);
  }

  nlohmann::json default_sources =
      config.value("defaults", nlohmann::json::object())
          .value("sources", nlohmann::json::object());

  for (const auto& src_json : env_config["sources"]) {
    nlohmann::json merged_src = default_sources;
    merged_src.merge_patch(src_json);
    app_config.sources.push_back(SourceConfig::fromJson(merged_src));
  }

  // Десериализация логгеров (Loggers)
  if (env_config.contains("logging") && env_config["logging"].is_array()) {
    for (const auto& entry : env_config["logging"]) {
      LoggerSinkConfig cfg;
      cfg.type = entry.value("type", "console");
      cfg.level = entry.value("level", "info");
      cfg.formatter = entry.value("formatter", "text");
      cfg.file_path = entry.value("file", "");

      if (entry.contains("rotation") && entry["rotation"].is_object()) {
        const auto& rot = entry["rotation"];
        RotationConfig rc;
        rc.type = rot.value("type", "size");
        double max_size_mb = rot.value("max_size_mb", 10.0);
        rc.max_size_bytes =
            static_cast<std::uint64_t>(max_size_mb * 1024 * 1024);
        rc.max_archives = rot.value("max_archives", 5);
        rc.interval_sec =
            std::chrono::seconds(rot.value("interval_sec", 86400));
        rc.time_format = rot.value("time_format", "%Y%m%d_%H%M%S");
        cfg.rotation = rc;
      }
      app_config.loggers.push_back(std::move(cfg));
    }
  }

  // Глобальный CSV
  app_config.global_comparison_list =
      env_config.value("comparison_list", "./comparison_list.csv");

  return app_config;
}

}  // namespace stc