/**
@file configmanager.hpp
@brief Фасад для управления загрузкой, обработкой и кэшированием конфигурации сервиса
@version 1.1.2
@date 2026-07-24
*/
#pragma once

#include <mutex>
#include <nlohmann/json.hpp>
#include <string>
#include <unordered_map>
#include <vector>

#include "../include/configcache.hpp"
#include "../include/configloader.hpp"
#include "../include/configvalidator.hpp"
#include "../include/enviromentprocessor.hpp"
#include "../include/logging_config.hpp"
#include "../include/sourceconfig.hpp"

/**
@class ConfigManager
@brief Класс управления жизненным циклом конфигурации
*/
class ConfigManager {
public:
    friend class ConfigReloadTransaction;

    static ConfigManager &instance();

    void initialize(const std::string &filename);
    void reload();

    nlohmann::json getMergedConfig(const std::string &env) const;
    std::string getGlobalComparisonList(const std::string &env) const;

    /**
    @brief Возвращает строгие структуры конфигурации логгирования для заданного окружения.
    @param[in] env Имя окружения (например, "production").
    @return std::vector<stc::LoggerSinkConfig> Вектор DTO конфигураций синков.
    @throw std::runtime_error Если окружение отсутствует в секции environments.
    */
    std::vector<stc::LoggerSinkConfig> getLoggingSinksConfig(const std::string &env) const;

    /**
    @brief Возвращает строгие структуры конфигурации источников данных для заданного окружения.
    @details Выполняет корректный мердж глобальных defaults.sources (object) с каждым 
             элементом массива environments[env].sources, после чего делегирует 
             десериализацию и валидацию методу stc::SourceConfig::fromJson.
    @param[in] env Имя окружения (например, "production").
    @return std::vector<stc::SourceConfig> Вектор DTO конфигураций источников.
    @throw std::runtime_error Если окружение отсутствует или массив sources некорректен.
    @throw std::invalid_argument При провале валидации полей внутри SourceConfig::fromJson.
    */
    std::vector<stc::SourceConfig> getSourcesConfig(const std::string &env) const;

    void applyCliOverrides(const std::unordered_map<std::string, std::string> &overrides);
    const nlohmann::json &getCurrentConfig() const;
    void restoreFromBackup(const nlohmann::json& backup);

private:
    ConfigManager() = default;
    ~ConfigManager() = default;

    void backupCurrentConfig();
    void restoreBackupConfig();
    bool validateConfigSafely(const nlohmann::json &config) const;

    ConfigLoader loader_;
    ConfigValidator validator_;
    EnvironmentProcessor envProcessor_;
    mutable ConfigCache cache_;
    nlohmann::json baseConfig_;
    nlohmann::json backupConfig_;
    std::string configFilePath_;
    mutable std::mutex configMutex_;
};