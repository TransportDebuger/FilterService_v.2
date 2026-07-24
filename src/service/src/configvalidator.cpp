/**
@file configvalidator.cpp
@brief Реализация валидатора структуры JSON-конфигурации.
@version 2.1.0
@date 2026-07-24
*/
#include "../include/configvalidator.hpp"
#include <algorithm>
#include <stdexcept>
#include <string>
#include <vector>

namespace stc {

bool ConfigValidator::validateRoot(const nlohmann::json &config) const {
    const std::vector<std::string> required_sections = {"defaults", "environments"};
    for (const auto &section : required_sections) {
        if (!config.contains(section) || !config[section].is_object()) {
            throw std::runtime_error("ConfigValidator: Missing required section: " + section);
        }
    }
    if (config["defaults"].empty()) {
        throw std::runtime_error("ConfigValidator: Defaults section cannot be empty");
    }
    return true;
}

bool ConfigValidator::validateSources(const nlohmann::json &sources) const {
    if (!sources.is_array()) {
        throw std::runtime_error("ConfigValidator: Sources must be an array");
    }
    // обязательные параметры
    const std::vector<std::string> required_fields = {"name", "type", "path", "file_mask", "processed_dir"};
    // Допустимые стратегии мониторинга файловой системы
    const std::vector<std::string> valid_strategies = {"auto", "inotify", "polling"};

    for (const auto &source : sources) {
        if (!source.is_object()) {
            throw std::runtime_error("ConfigValidator: Source entry must be an object");
        }
        
        for (const auto &field : required_fields) {
            if (!source.contains(field) || !source[field].is_string()) {
                throw std::runtime_error("ConfigValidator: Missing or invalid required field in source: " + field);
            }
        }
        
        const std::string source_name = source["name"].get<std::string>();
        const std::string type = source["type"].get<std::string>();
        
        if (type == "ftp" || type == "sftp") {
            validateFtpFields(source);
        }
        
        // --- НОВОЕ: Валидация стратегии мониторинга ФС ---
        if (source.contains("monitoring_strategy")) {
            if (!source["monitoring_strategy"].is_string()) {
                throw std::runtime_error(
                    "ConfigValidator: 'monitoring_strategy' must be a string in source: " + source_name);
            }
            const std::string strategy = source["monitoring_strategy"].get<std::string>();
            if (std::find(valid_strategies.begin(), valid_strategies.end(), strategy) == valid_strategies.end()) {
                throw std::runtime_error(
                    "ConfigValidator: Invalid 'monitoring_strategy' value '" + strategy + 
                    "' in source: " + source_name);
            }
        }
    }
    return true;
}

void ConfigValidator::validateFtpFields(const nlohmann::json &source) const {
    const std::vector<std::string> required_ftp_fields = {"username", "password"};
    for (const auto &field : required_ftp_fields) {
        if (!source.contains(field) || !source[field].is_string()) {
            throw std::runtime_error("ConfigValidator: FTP source missing required field: " + field);
        }
    }
    if (source.contains("port") && !source["port"].is_number()) {
        throw std::runtime_error("ConfigValidator: Invalid port type in FTP source");
    }
}

bool ConfigValidator::validateLogging(const nlohmann::json &logging) const {
    if (!logging.is_array()) {
        throw std::runtime_error("ConfigValidator: Logging config must be an array");
    }
    
    const std::vector<std::string> valid_types = {"console", "sync_file", "async_file"};
    const std::vector<std::string> valid_levels = {"trace", "debug", "info", "warning", "error", "critical"};
    const std::vector<std::string> valid_formatters = {"text", "json", "xml"};
    const std::vector<std::string> valid_rotation_types = {"size", "time", "circular"};
    
    for (const auto &logger : logging) {
        if (!logger.is_object()) {
            throw std::runtime_error("ConfigValidator: Logger entry must be an object");
        }
        
        // 1. Валидация типа приемника (Sink)
        if (!logger.contains("type") || !logger["type"].is_string()) {
            throw std::runtime_error("ConfigValidator: Logger missing 'type' field or it is not a string");
        }
        const std::string type = logger["type"].get<std::string>();
        if (std::find(valid_types.begin(), valid_types.end(), type) == valid_types.end()) {
            throw std::runtime_error("ConfigValidator: Invalid logger type: " + type);
        }
        
        // 2. Валидация уровня логирования (опционально)
        if (logger.contains("level")) {
            if (!logger["level"].is_string()) {
                throw std::runtime_error("ConfigValidator: 'level' must be a string");
            }
            const std::string level = logger["level"].get<std::string>();
            if (std::find(valid_levels.begin(), valid_levels.end(), level) == valid_levels.end()) {
                throw std::runtime_error("ConfigValidator: Invalid log level: " + level);
            }
        }
        
        // 3. Валидация форматтера (опционально)
        if (logger.contains("formatter")) {
            if (!logger["formatter"].is_string()) {
                throw std::runtime_error("ConfigValidator: 'formatter' must be a string");
            }
            const std::string formatter = logger["formatter"].get<std::string>();
            if (std::find(valid_formatters.begin(), valid_formatters.end(), formatter) == valid_formatters.end()) {
                throw std::runtime_error("ConfigValidator: Invalid formatter type: " + formatter);
            }
        }
        
        // 4. Валидация пути к файлу (обязательно для файловых синков)
        if (type == "sync_file" || type == "async_file") {
            if (!logger.contains("file") || !logger["file"].is_string()) {
                throw std::runtime_error("ConfigValidator: File logger missing 'file' path or it is not a string");
            }
        }
        
        // 5. Валидация политики ротации (опционально)
        if (logger.contains("rotation")) {
            const auto& rot = logger["rotation"];
            if (!rot.is_object()) {
                throw std::runtime_error("ConfigValidator: 'rotation' must be an object");
            }
            if (!rot.contains("type") || !rot["type"].is_string()) {
                throw std::runtime_error("ConfigValidator: Rotation missing 'type' field");
            }
            const std::string rot_type = rot["type"].get<std::string>();
            if (std::find(valid_rotation_types.begin(), valid_rotation_types.end(), rot_type) == valid_rotation_types.end()) {
                throw std::runtime_error("ConfigValidator: Invalid rotation type: " + rot_type);
            }
            if (rot_type == "size" || rot_type == "circular") {
                if (!rot.contains("max_size_mb") || !rot["max_size_mb"].is_number() || rot["max_size_mb"].get<double>() <= 0) {
                    throw std::runtime_error("ConfigValidator: 'max_size_mb' must be a positive number for " + rot_type + " rotation");
                }
                if (!rot.contains("max_archives") || !rot["max_archives"].is_number_integer() || rot["max_archives"].get<int>() <= 0) {
                    throw std::runtime_error("ConfigValidator: 'max_archives' must be a positive integer for " + rot_type + " rotation");
                }
            } else if (rot_type == "time") {
                if (!rot.contains("interval_sec") || !rot["interval_sec"].is_number() || rot["interval_sec"].get<int>() <= 0) {
                    throw std::runtime_error("ConfigValidator: 'interval_sec' must be a positive number for time rotation");
                }
                if (!rot.contains("time_format") || !rot["time_format"].is_string() || rot["time_format"].get<std::string>().empty()) {
                    throw std::runtime_error("ConfigValidator: 'time_format' must be a non-empty string for time rotation");
                }
            }
        }
    }
    return true;
}

} // namespace stc