/**
 * @file SourceConfig.cpp
 * @brief Реализация конфигурации источника данных
 */

#include "../include/sourceconfig.hpp"
#include <stdexcept>
#include <filesystem>
#include <regex>

namespace fs = std::filesystem;

SourceConfig SourceConfig::fromJson(const nlohmann::json& src) {
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
        throw std::runtime_error("Source config missing required 'file_mask' field");
    }
    config.file_mask = src["file_mask"].get<std::string>();
    
    if (!src.contains("processed_dir") || !src["processed_dir"].is_string()) {
        throw std::runtime_error("Source config missing required 'processed_dir' field");
    }
    config.processed_dir = src["processed_dir"].get<std::string>();
    
    // ============= Парсинг опциональных полей =============
    
    config.bad_dir = src.value("bad_dir", "");
    config.excluded_dir = src.value("excluded_dir", "");
    config.filtered_template = src.value("filtered_template", "{filename}_filtered.{ext}");
    config.excluded_template = src.value("excluded_template", "{filename}_excluded.{ext}");
    config.comparison_list = src.value("comparison_list", "./comparison_list.csv");
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
    
    // ============= Валидация созданной конфигурации =============
    
    try {
        config.validate();
    } catch (const std::exception& e) {
        throw std::runtime_error("Invalid source configuration: " + std::string(e.what()));
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
    if (!bad_dir.empty()) j["bad_dir"] = bad_dir;
    if (!excluded_dir.empty()) j["excluded_dir"] = excluded_dir;
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
    if (std::find(supported_types.begin(), supported_types.end(), type) == supported_types.end()) {
        throw std::invalid_argument("Unsupported source type: " + type);
    }
    
    // Валидация параметров для конкретных типов[17][21]
    if (type == "smb") {
        if (!hasRequiredParams({"username"})) {
            throw std::invalid_argument("SMB source requires 'username' parameter");
        }
    } else if (type == "ftp") {
        if (!hasRequiredParams({"username", "password"})) {
            throw std::invalid_argument("FTP source requires 'username' and 'password' parameters");
        }
    }
    
    // Валидация интервала проверки
    if (check_interval.count() <= 0) {
        throw std::invalid_argument("Check interval must be positive");
    }
}

std::string SourceConfig::getFilteredFileName(const std::string& original_filename) const {
    return applyTemplate(original_filename, filtered_template);
}

std::string SourceConfig::getExcludedFileName(const std::string& original_filename) const {
    return applyTemplate(original_filename, excluded_template);
}

bool SourceConfig::hasRequiredParams(const std::vector<std::string>& required_params) const {
    for (const auto& param : required_params) {
        auto it = params.find(param);
        if (it == params.end() || it->second.empty()) {
            return false;
        }
    }
    return true;
}

std::string SourceConfig::applyTemplate(const std::string& filename, const std::string& template_str) const {
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