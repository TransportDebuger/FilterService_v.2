/**
 * @file configcache.cpp
 * @author Artem Ulyanov
 * @company STC Ltd.
 * @date May 2025
 * @brief Реализация валидатора структуры JSON-конфигурации
 *
 * @version 1.0
 */
#include "../include/configvalidator.hpp"

#include <vector>

using namespace std;

bool ConfigValidator::validateRoot(const nlohmann::json &config) const {
  const vector<string> required_sections = {"defaults", "environments"};

  for (const auto &section : required_sections) {
    if (!config.contains(section) || !config[section].is_object()) {
      throw runtime_error("ConfigValidator: Missing required section: " +
                          section);
    }
  }

  if (config["defaults"].empty()) {
    throw runtime_error("ConfigValidator: Defaults section cannot be empty");
  }

  return true;
}

bool ConfigValidator::validateSources(const nlohmann::json &sources) const {
  if (!sources.is_array()) {
    throw runtime_error("ConfigValidator: Sources must be an array");
  }

  const vector<string> required_fields = {"name", "type", "path", "file_mask",
                                          "processed_dir"};

  for (const auto &source : sources) {
    if (!source.is_object()) {
      throw runtime_error("ConfigValidator: Source entry must be an object");
    }

    // Проверка обязательных полей
    for (const auto &field : required_fields) {
      if (!source.contains(field)) {
        throw runtime_error(
            "ConfigValidator: Missing required field in source: " + field);
      }
    }

    // Проверка типов
    if (!source["name"].is_string() || !source["type"].is_string() ||
        !source["path"].is_string() || !source["file_mask"].is_string() ||
        !source["processed_dir"].is_string()) {
      throw runtime_error(
          "ConfigValidator: Invalid type in source configuration");
    }

    // Специфичная проверка для сетевых источников
    const string type = source["type"].get<string>();
    if (type == "ftp" || type == "sftp") {
      validateFtpFields(source);
    }
  }
  return true;
}

void ConfigValidator::validateFtpFields(const nlohmann::json &source) const {
  const vector<string> required_ftp_fields = {"username", "password"};

  for (const auto &field : required_ftp_fields) {
    if (!source.contains(field) || !source[field].is_string()) {
      throw runtime_error(
          "ConfigValidator: FTP source missing required field: " + field);
    }
  }

  if (source.contains("port") && !source["port"].is_number()) {
    throw runtime_error("ConfigValidator: Invalid port type in FTP source");
  }
}

bool ConfigValidator::validateLogging(const nlohmann::json &logging) const {
  if (!logging.is_array()) {
    throw runtime_error("ConfigValidator: Logging config must be an array");
  }

  const vector<string> valid_types = {"console", "sync_file", "async_file"};

  for (const auto &logger : logging) {
    if (!logger.is_object()) {
      throw runtime_error("ConfigValidator: Logger entry must be an object");
    }

    if (!logger.contains("type") || !logger["type"].is_string()) {
      throw runtime_error("ConfigValidator: Logger missing type field");
    }

    const string type = logger["type"].get<string>();
    if (find(valid_types.begin(), valid_types.end(), type) ==
        valid_types.end()) {
      throw runtime_error("ConfigValidator: Invalid logger type: " + type);
    }

    if (logger.contains("level") && !logger["level"].is_string()) {
      throw runtime_error("ConfigValidator: Invalid log level type");
    }

    if ((type == "sync_file" || type == "async_file") &&
        (!logger.contains("file") || !logger["file"].is_string())) {
      throw runtime_error("ConfigValidator: File logger missing file path");
    }
  }
  return true;
}