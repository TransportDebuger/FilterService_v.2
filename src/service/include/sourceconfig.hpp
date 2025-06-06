#pragma once
#include <nlohmann/json.hpp>
#include <string>

struct SourceConfig {
  // Обязательные поля
  std::string name;
  std::string type;
  std::string path;
  std::string file_mask;
  std::string processed_dir;

  // Опциональные поля со значениями по умолчанию
  std::string bad_dir;
  std::string excluded_dir;
  std::string filtered_template;
  std::string excluded_template;
  std::string comparison_list;  // Для интеграции с Worker
  bool filtering_enabled = true;
  int check_interval = 5;
  bool enabled = true;

  // Метод преобразования JSON -> SourceConfig
  static SourceConfig fromJson(const nlohmann::json& src);
};