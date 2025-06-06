#include "../includes/configmanager.hpp"
#include <fstream>
#include <stdexcept>
#include <algorithm>
#include <iomanip>

namespace fs = std::filesystem;

    const std::unordered_map<std::string, SourceType> SOURCE_TYPES = {
        {"local", SourceType::LOCAL},
        {"ftp", SourceType::FTP},
        {"sftp", SourceType::SFTP},
        {"smb", SourceType::SMB}
    };

    std::string replacePlaceholders(const std::string& templateStr,
                                 const std::string& filename,
                                 const std::string& extension,
                                 const std::string& timestamp = "") {
        std::string result = templateStr;
        
        auto replaceAll = [&](const std::string& from, const std::string& to) {
            size_t pos = 0;
            while ((pos = result.find(from, pos)) != std::string::npos) {
                result.replace(pos, from.length(), to);
                pos += to.length();
            }
        };

        replaceAll("{filename}", filename);
        replaceAll("{ext}", extension);
        replaceAll("{timestamp}", timestamp);
        
        return result;
    }

ConfigManager& ConfigManager::getInstance() {
    static ConfigManager instance;
    return instance;
}

std::string SourceConfig::getFilteredFileName(const std::string& original) const {
    fs::path p(original);
    std::string filename = p.stem().string();
    std::string ext = p.extension().string();
    if (!ext.empty()) ext.erase(0, 1); // Remove leading dot

    if (filtered_template.find("{timestamp}") != std::string::npos) {
        return replacePlaceholders(excluded_template, filename, ext, ConfigManager::getInstance().generateTimestamp());
    }

    return replacePlaceholders(filtered_template, filename, ext);
}

std::string SourceConfig::getExcludedFileName(const std::string& original) const {
    fs::path p(original);
    std::string filename = p.stem().string();
    std::string ext = p.extension().string();
    if (!ext.empty()) ext.erase(0, 1);
    
    if (excluded_template.find("{timestamp}") != std::string::npos) {
        return replacePlaceholders(excluded_template, filename, ext, ConfigManager::getInstance().generateTimestamp());
    }
    
    return replacePlaceholders(excluded_template, filename, ext);
}

std::string ConfigManager::generateTimestamp() const {
    auto now = std::chrono::system_clock::now();
    auto in_time_t = std::chrono::system_clock::to_time_t(now);
    std::tm tm_buf;
    localtime_r(&in_time_t, &tm_buf);
    
    std::stringstream ss;
    ss << std::put_time(&tm_buf, "%Y%m%d_%H%M%S");
    return ss.str();
}

bool ConfigManager::loadConfig(const std::string& path) {
    std::lock_guard<std::mutex> lock(configMutex_);
    
    try {
        std::ifstream file(path);
        if (!file.is_open()) {
            Logger::error("Cannot open config file: " + path);
            return false;
        }

        nlohmann::json config;
        try {
            file >> config;
        } catch (const nlohmann::json::parse_error& e) {
            Logger::error("JSON parse error: " + std::string(e.what()));
            return false;
        }

        // Clear previous config
        sources_.clear();
        sourceIndex_.clear();

        // Parse sources
        if (config.contains("sources") && config["sources"].is_array()) {
            for (const auto& source : config["sources"]) {
                SourceConfig cfg;
                
                // Required parameters
                cfg.name = source.value("name", "");
                if (cfg.name.empty()) {
                  //Logger::error("Source name cannot be empty");
                    throw std::runtime_error("Source name cannot be empty");
                }
                
                cfg.type = parseSourceType(source.value("type", ""));
                cfg.path = source.value("path", "");
                
                // Optional parameters
                cfg.enabled = source.value("enabled", true);
                cfg.filtering_enabled = source.value("filtering_enabled", true);
                cfg.file_mask = source.value("file_mask", "*.xml");
                cfg.username = resolveEnvVars(source.value("username", ""));
                cfg.password = resolveEnvVars(source.value("password", ""));
                cfg.port = source.value("port", 0);
                cfg.processed_dir = source.value("processed_dir", "");
                cfg.filtered_template = source.value("filtered_template", "{filename}_filtered.{ext}");
                cfg.excluded_template = source.value("excluded_template", "{filename}_excluded.{ext}");

                sources_.push_back(cfg);
                sourceIndex_[cfg.name] = sources_.size() - 1;
            }
        }

        // Global settings
        globalExcludedFile_ = config.value("global_excluded_file", "excluded.xml");
        
        if (config.contains("comparison_list")) {
            comparisonList_.clear();
            for (const auto& item : config["comparison_list"]) {
                comparisonList_.insert(item.get<std::string>());
            }
        }

        // Logging settings
        if (config.contains("logging")) {
            logFile_ = config["logging"].value("file", "/var/log/xml_filter_service.log");
            colorOutput_ = config["logging"].value("color", false);
            minLogLevel_ = stringToLogLevel(config["logging"].value("level", "info"));
            useSyslog_ = config["logging"].value("syslog", false);
        }

        if (!validate()) {
            Logger::error("Configuration validation failed");
            throw std::runtime_error("Configuration validation failed");
        }

        Logger::info("Configuration loaded successfully from " + path);
        return true;
    } catch (const std::exception& e) {
        Logger::error("Failed to load configuration: " + std::string(e.what()));
        sources_.clear();
        sourceIndex_.clear();
        return false;
    }
}

bool ConfigManager::validateSource(const SourceConfig& source, std::set<std::string>& names) const {
    // Check for duplicate names
    if (names.count(source.name)) {
        Logger::error("Duplicate source name: " + source.name);
        return false;
    }
    names.insert(source.name);

    // Validate source path
    if (source.path.empty()) {
        Logger::error("Empty path for source: " + source.name);
        return false;
    }

    // Check if path exists for local sources
    if (source.type == SourceType::LOCAL && !fs::exists(source.path)) {
        Logger::error("Local path does not exist: " + source.path);
        return false;
    }

    // Validate templates
    if (!validateTemplates(source)) {
        return false;
    }

    // Validate remote sources
    if (source.type != SourceType::LOCAL) {
        if (source.username.empty() || source.password.empty()) {
            Logger::error("Missing credentials for remote source: " + source.name);
            return false;
        }
        
        // Дополнительная валидация URL
        // TODO: Добавить валидацию формата URL
    }

    return true;
}

bool ConfigManager::validate() const {
    std::lock_guard<std::mutex> lock(configMutex_);
    return validateSources();
}

bool ConfigManager::validateSources() const {
    if (sources_.empty()) {
        Logger::error("No sources configured");
        return false;
    }

    std::set<std::string> names;
    for (const auto& source : sources_) {
        if (!validateSource(source, names)) {
            return false;
        }
    }

    return true;
}

bool ConfigManager::validateSource(const SourceConfig& source, std::set<std::string>& names) const {
    // Check for duplicate names
    if (names.count(source.name)) {
        Logger::error("Duplicate source name: " + source.name);
        return false;
    }
    names.insert(source.name);

    // Validate source path
    if (source.path.empty()) {
        Logger::error("Empty path for source: " + source.name);
        return false;
    }

    // Check if path exists for local sources
    if (source.type == SourceType::LOCAL && !fs::exists(source.path)) {
        Logger::error("Local path does not exist: " + source.path);
        return false;
    }

    // Validate templates
    if (!validateTemplates(source)) {
        return false;
    }

    // Validate remote sources
    if (source.type != SourceType::LOCAL) {
        if (source.username.empty() || source.password.empty()) {
            Logger::error("Missing credentials for remote source: " + source.name);
            return false;
        }
    }

    return true;
}

bool ConfigManager::validateTemplates(const SourceConfig& source) const {
    if (source.filtered_template.find("{filename}") == std::string::npos ||
        source.filtered_template.find("{ext}") == std::string::npos) {
        Logger::error("Invalid filtered_template for source: " + source.name);
        return false;
    }

    if (source.excluded_template.find("{filename}") == std::string::npos ||
        source.excluded_template.find("{ext}") == std::string::npos) {
        Logger::error("Invalid excluded_template for source: " + source.name);
        return false;
    }

    return true;
}

SourceType ConfigManager::parseSourceType(const std::string& typeStr) const {
    auto it = SOURCE_TYPES.find(typeStr);
    if (it == SOURCE_TYPES.end()) {
        throw std::runtime_error("Unknown source type: " + typeStr);
    }
    return it->second;
}

LogLevel ConfigManager::stringToLogLevel(const std::string& levelStr) const {
    std::string lowerLevel;
    std::transform(levelStr.begin(), levelStr.end(), std::back_inserter(lowerLevel),
                  [](unsigned char c){ return std::tolower(c); });

    auto it = LOG_LEVELS.find(lowerLevel);
    if (it == LOG_LEVELS.end()) {
        // Logger::warn("Unknown log level: " + levelStr + ". Defaulting to INFO");
        return LogLevel::INFO;
    }
    return it->second;
}

const SourceConfig* ConfigManager::getSourceByName(const std::string& name) const {
    std::lock_guard<std::mutex> lock(configMutex_);
    auto it = sourceIndex_.find(name);
    if (it == sourceIndex_.end()) {
        return nullptr;
    }
    return &sources_[it->second];
}

std::string resolveEnvVars(const std::string& value) {
    static std::regex env_regex("@env\\('([^']+)'\\)");
    std::smatch match;
    
    if (std::regex_match(value, match, env_regex)) {
        const char* env_value = std::getenv(match[1].str().c_str());
        return env_value ? env_value : "";
    }
    return value;
}