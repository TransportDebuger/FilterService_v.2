/**
 * @file SourceConfig.cpp
 * @brief Реализация конфигурации источника данных
 */

#include "../include/sourceconfig.hpp"
#include <filesystem>
#include <regex>
#include <stdexcept>

namespace fs = std::filesystem;

SourceConfig SourceConfig::fromJson(const nlohmann::json &src) {
  SourceConfig config;

  // ============= Валидация и парсинг обязательных полей =============

  if (!src.contains("name") || !src["name"].is_string()) {
    throw std::runtime_error("Source config missing required 'name' field");
  }
  config.name = src["name"].get<std::string>();

  if (!src.contains("type") || !src["type"].is_string()) {
    throw std::runtime_error("Source config missing required 'type' field");
  }
  config.type = src["type"].get<std::string>();

  if (!src.contains("path") || !src["path"].is_string()) {
    throw std::runtime_error("Source config missing required 'path' field");
  }
  config.path = src["path"].get<std::string>();

  if (!src.contains("file_mask") || !src["file_mask"].is_string()) {
    throw std::runtime_error(
        "Source config missing required 'file_mask' field");
  }
  config.file_mask = src["file_mask"].get<std::string>();

  if (!src.contains("processed_dir") || !src["processed_dir"].is_string()) {
    throw std::runtime_error(
        "Source config missing required 'processed_dir' field");
  }
  config.processed_dir = src["processed_dir"].get<std::string>();

  // ============= Парсинг опциональных полей =============

  config.bad_dir = src.value("bad_dir", "");
  config.excluded_dir = src.value("excluded_dir", "");
  config.filtered_template =
      src.value("filtered_template", "{filename}_filtered.{ext}");
  config.excluded_template =
      src.value("excluded_template", "{filename}_excluded.{ext}");
  config.comparison_list =
      src.value("comparison_list", "./comparison_list.csv");
  config.filtering_enabled = src.value("filtering_enabled", true);
  config.enabled = src.value("enabled", true);

  // Парсинг интервала проверки[20][22]
  if (src.contains("check_interval")) {
    if (src["check_interval"].is_number_integer()) {
      int interval = src["check_interval"].get<int>();
      config.check_interval = std::chrono::seconds(interval);
    } else {
      config.check_interval = std::chrono::seconds(5);
    }
  }

  // ============= Парсинг параметров подключения =============

  if (src.contains("params") && src["params"].is_object()) {
    for (auto it = src["params"].begin(); it != src["params"].end(); ++it) {
      if (it.value().is_string()) {
        config.params[it.key()] = it.value().get<std::string>();
      } else if (it.value().is_number()) {
        // Преобразование чисел в строки для унификации[22]
        config.params[it.key()] = std::to_string(it.value().get<double>());
      }
    }
  }

  // ============= Парсинг секции xml_filter =============
  if (src.contains("xml_filter") && src["xml_filter"].is_object()) {
    const auto &xf = src["xml_filter"];

    // Парсинг оператора логики
    config.xml_filter.logic_operator = xf.value("logic_operator", "AND");

    // Парсинг порога для MAJORITY и WEIGHTED
    config.xml_filter.threshold = xf.value("threshold", 0.5);

    if (xf.contains("comparison_list")) {
      config.xml_filter.comparison_list =
          xf["comparison_list"].get<std::string>();
    } else {
      // Используем значение из корневого уровня или environments
      config.xml_filter.comparison_list = config.comparison_list;
    }

    // Парсинг пространств имён
    if (xf.contains("namespaces") && xf["namespaces"].is_array()) {
      for (const auto &ns : xf["namespaces"]) {
        XmlNamespace xmlNs;
        xmlNs.prefix = ns.value("prefix", "");
        xmlNs.uri = ns.value("uri", "");
        if (!xmlNs.prefix.empty() && !xmlNs.uri.empty()) {
          config.xml_filter.namespaces.push_back(xmlNs);
        }
      }
    }

    // Парсинг критериев
    if (xf.contains("criteria") && xf["criteria"].is_array()) {
      for (const auto &crit : xf["criteria"]) {
        XmlFilterCriterion criterion;
        criterion.xpath = crit.value("xpath", "");
        criterion.attribute = crit.value("attribute", "");
        criterion.csv_column = crit.value("csv_column", "");
        criterion.required = crit.value("required", true);
        criterion.weight = crit.value("weight", 1.0);
        config.xml_filter.criteria.push_back(criterion);
      }
    } else if (xf.contains("xpath")) {
      // Обратная совместимость с одиночным критерием
      XmlFilterCriterion criterion;
      criterion.xpath = xf.value("xpath", "");
      criterion.attribute = xf.value("attribute", "");
      criterion.csv_column = xf.value("csv_column", "");
      config.xml_filter.criteria.push_back(criterion);
    }
  }

  // ============= Валидация созданной конфигурации =============

  try {
    config.validate();
  } catch (const std::exception &e) {
    throw std::runtime_error("Invalid source configuration: " +
                             std::string(e.what()));
  }

  return config;
}

nlohmann::json SourceConfig::toJson() const {
  nlohmann::json j;

  // Обязательные поля
  j["name"] = name;
  j["type"] = type;
  j["path"] = path;
  j["file_mask"] = file_mask;
  j["processed_dir"] = processed_dir;

  // Опциональные поля
  if (!bad_dir.empty())
    j["bad_dir"] = bad_dir;
  if (!excluded_dir.empty())
    j["excluded_dir"] = excluded_dir;
  j["filtered_template"] = filtered_template;
  j["excluded_template"] = excluded_template;
  j["comparison_list"] = comparison_list;
  j["filtering_enabled"] = filtering_enabled;
  j["check_interval"] = check_interval.count();
  j["enabled"] = enabled;

  // Параметры подключения
  if (!params.empty()) {
    j["params"] = params;
  }

  // Конфигурация XML-фильтрации
  if (!xml_filter.criteria.empty()) {
    nlohmann::json xf;
    xf["logic_operator"] = xml_filter.logic_operator;
    xf["threshold"] = xml_filter.threshold;

    if (!xml_filter.comparison_list.empty() &&
        xml_filter.comparison_list != comparison_list) {
      xf["comparison_list"] = xml_filter.comparison_list;
    }

    if (!xml_filter.namespaces.empty()) {
      nlohmann::json namespaces = nlohmann::json::array();
      for (const auto &ns : xml_filter.namespaces) {
        nlohmann::json namespace_obj;
        namespace_obj["prefix"] = ns.prefix;
        namespace_obj["uri"] = ns.uri;
        namespaces.push_back(namespace_obj);
      }
      xf["namespaces"] = namespaces;
    }

    nlohmann::json criteria = nlohmann::json::array();
    for (const auto &crit : xml_filter.criteria) {
      nlohmann::json criterion;
      criterion["xpath"] = crit.xpath;
      if (!crit.attribute.empty())
        criterion["attribute"] = crit.attribute;
      criterion["csv_column"] = crit.csv_column;
      criterion["required"] = crit.required;
      criterion["weight"] = crit.weight;
      criteria.push_back(criterion);
    }
    xf["criteria"] = criteria;

    j["xml_filter"] = xf;
  }

  return j;
}

void SourceConfig::validate() const {
  // Валидация обязательных полей
  if (name.empty()) {
    throw std::invalid_argument("Source name cannot be empty");
  }

  if (type.empty()) {
    throw std::invalid_argument("Source type cannot be empty");
  }

  if (path.empty()) {
    throw std::invalid_argument("Source path cannot be empty");
  }

  if (file_mask.empty()) {
    throw std::invalid_argument("File mask cannot be empty");
  }

  if (processed_dir.empty()) {
    throw std::invalid_argument("Processed directory cannot be empty");
  }

  // Валидация типа источника
  std::vector<std::string> supported_types = {"local", "smb", "ftp"};
  if (std::find(supported_types.begin(), supported_types.end(), type) ==
      supported_types.end()) {
    throw std::invalid_argument("Unsupported source type: " + type);
  }

  // Валидация параметров для конкретных типов
  if (type == "smb") {
    if (!hasRequiredParams({"username"})) {
      throw std::invalid_argument("SMB source requires 'username' parameter");
    }
  } else if (type == "ftp") {
    if (!hasRequiredParams({"username", "password"})) {
      throw std::invalid_argument(
          "FTP source requires 'username' and 'password' parameters");
    }
  }

  // Валидация интервала проверки
  if (check_interval.count() <= 0) {
    throw std::invalid_argument("Check interval must be positive");
  }

  // Валидация параметров XML-фильтрации
  if (filtering_enabled) {
    if (xml_filter.criteria.empty()) {
      throw std::invalid_argument("XML filter requires at least one criterion");
    }

    for (const auto &crit : xml_filter.criteria) {
      if (crit.xpath.empty()) {
        throw std::invalid_argument("Criterion xpath cannot be empty");
      }
      if (crit.csv_column.empty()) {
        throw std::invalid_argument("Criterion csv_column cannot be empty");
      }
    }

    // Проверка операторов
    const std::vector<std::string> valid_operators = {"AND", "OR", "MAJORITY",
                                                      "WEIGHTED"};
    if (std::find(valid_operators.begin(), valid_operators.end(),
                  xml_filter.logic_operator) == valid_operators.end()) {
      throw std::invalid_argument("Invalid logic operator: " +
                                  xml_filter.logic_operator);
    }

    // Проверка весов для WEIGHTED
    if (xml_filter.logic_operator == "WEIGHTED") {
      double total_weight = 0.0;
      for (const auto &crit : xml_filter.criteria) {
        if (crit.weight <= 0) {
          throw std::invalid_argument("Criterion weight must be positive");
        }
        total_weight += crit.weight;
      }
      if (total_weight <= 0) {
        throw std::invalid_argument("Total criteria weight must be positive");
      }
    }

    // Проверка порога
    if (xml_filter.threshold <= 0.0 || xml_filter.threshold > 1.0) {
      throw std::invalid_argument("Threshold must be in range (0.0, 1.0]");
    }
  }
}

std::string
SourceConfig::getFilteredFileName(const std::string &original_filename) const {
  return applyTemplate(original_filename, filtered_template);
}

std::string
SourceConfig::getExcludedFileName(const std::string &original_filename) const {
  return applyTemplate(original_filename, excluded_template);
}

bool SourceConfig::hasRequiredParams(
    const std::vector<std::string> &required_params) const {
  for (const auto &param : required_params) {
    auto it = params.find(param);
    if (it == params.end() || it->second.empty()) {
      return false;
    }
  }
  return true;
}

std::string SourceConfig::applyTemplate(const std::string &filename,
                                        const std::string &template_str) const {
  fs::path file_path(filename);
  std::string name = file_path.stem().string();
  std::string ext = file_path.extension().string();

  // Убираем точку из расширения если она есть
  if (!ext.empty() && ext[0] == '.') {
    ext = ext.substr(1);
  }

  std::string result = template_str;

  // Замена плейсхолдеров
  size_t pos = 0;
  while ((pos = result.find("{filename}", pos)) != std::string::npos) {
    result.replace(pos, 10, name);
    pos += name.length();
  }

  pos = 0;
  while ((pos = result.find("{ext}", pos)) != std::string::npos) {
    result.replace(pos, 5, ext);
    pos += ext.length();
  }

  return result;
}