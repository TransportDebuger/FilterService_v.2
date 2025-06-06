#include "../include/sourceconfig.hpp"

#include <stdexcept>

SourceConfig SourceConfig::fromJson(const nlohmann::json& src) {
  SourceConfig config;

  // Обязательные поля
  if (!src.contains("name") || !src["name"].is_string()) {
    throw std::runtime_error("Source config missing 'name' field");
  }
  config.name = src["name"].get<std::string>();

  if (!src.contains("type") || !src["type"].is_string()) {
    throw std::runtime_error("Source config missing 'type' field");
  }
  config.type = src["type"].get<std::string>();

  if (!src.contains("path") || !src["path"].is_string()) {
    throw std::runtime_error("Source config missing 'path' field");
  }
  config.path = src["path"].get<std::string>();

  if (!src.contains("file_mask") || !src["file_mask"].is_string()) {
    throw std::runtime_error("Source config missing 'file_mask' field");
  }
  config.file_mask = src["file_mask"].get<std::string>();

  if (!src.contains("processed_dir") || !src["processed_dir"].is_string()) {
    throw std::runtime_error("Source config missing 'processed_dir' field");
  }
  config.processed_dir = src["processed_dir"].get<std::string>();

  // Опциональные поля
  config.bad_dir = src.value("bad_dir", "");
  config.excluded_dir = src.value("excluded_dir", "");
  config.filtered_template =
      src.value("filtered_template", "{filename}_filtered.{ext}");
  config.excluded_template =
      src.value("excluded_template", "{filename}_excluded.{ext}");
  config.comparison_list =
      src.value("comparison_list", "./comparison_list.csv");
  config.filtering_enabled = src.value("filtering_enabled", true);
  config.check_interval = src.value("check_interval", 5);
  config.enabled = src.value("enabled", true);

  return config;
}
