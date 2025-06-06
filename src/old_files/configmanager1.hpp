#pragma once

#include <string>
#include <vector>
#include <set>
#include <unordered_map>
#include <nlohmann/json.hpp>
#include <filesystem>
#include <chrono>
#include <ctime>
#include <memory>
#include <mutex>

#include "../includes/logger.hpp"
#include "../includes/AdapterFabric.hpp"

struct SourceConfig {
    std::string name;
    SourceType type;
    bool enabled = true;
    bool filtering_enabled = true;
    std::string path;
    std::string file_mask = "*.xml";
    std::string username;
    std::string password;
    int port = 0;
    std::string processed_dir;
    std::string filtered_template = "{filename}_filtered.{ext}";
    std::string excluded_template = "{filename}_excluded.{ext}";

    std::string getFilteredFileName(const std::string& original) const;
    std::string getExcludedFileName(const std::string& original) const;
};

class ConfigManager {
    public:
        static ConfigManager& getInstance();
    
        ConfigManager(const ConfigManager&) = delete;
        ConfigManager& operator=(const ConfigManager&) = delete;
    
        bool loadConfig(const std::string& path);
        bool validate() const;
    
        const std::vector<SourceConfig>& getSources() const { return sources_; }
        const std::string& getGlobalExcludedFile() const { return globalExcludedFile_; }
        const std::set<std::string>& getComparisonList() const { return comparisonList_; }
        const std::string& getLogFile() const { return logFile_; }
        bool getColorOutput() const { return colorOutput_; }
        LogLevel getMinLogLevel() const { return minLogLevel_; }
        bool getUseSyslog() const { return useSyslog_; }
    
        const SourceConfig* getSourceByName(const std::string& name) const;
        std::string generateTimestamp() const;
    
    private:
        ConfigManager() = default;
        ~ConfigManager() = default;
    
        SourceType parseSourceType(const std::string& typeStr) const;
        LogLevel stringToLogLevel(const std::string& levelStr) const;
        std::string resolveEnvVars(const std::string& value);
    
        std::vector<SourceConfig> sources_;
        std::unordered_map<std::string, size_t> sourceIndex_;
        std::string globalExcludedFile_ = "excluded.xml";
        std::set<std::string> comparisonList_;
        std::string logFile_ = "/var/log/xml_filter_service.log";
        bool colorOutput_ = false;
        LogLevel minLogLevel_ = LogLevel::INFO;
        bool useSyslog_ = false;
        mutable std::mutex configMutex_;
    
        // Static constexpr maps for source types and log levels
        static constexpr std::array<std::pair<const char*, SourceType>, 4> SOURCE_TYPES = {{
            {"local", SourceType::LOCAL},
            {"ftp", SourceType::FTP},
            {"sftp", SourceType::SFTP},
            {"smb", SourceType::SMB}
        }};
    
};