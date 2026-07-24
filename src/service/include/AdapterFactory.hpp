/**
@file adapterfactory.hpp
@brief Фабрика для создания адаптеров файловых хранилищ.
@version 2.1.0
@date 2026-07-24
*/
#pragma once

#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>
#include "ifilestorage.hpp"
#include "stc/config/sourceconfig.hpp"
#include "stc/logger/ilogger.hpp"

namespace stc {

/**
@class AdapterFactory
@brief Фабрика для создания адаптеров файловых хранилищ.
*/
class AdapterFactory {
public:
    /// @brief Тип функции-производителя адаптеров.
    using CreatorFunction = std::function<std::unique_ptr<IFileStorage>(
        const SourceConfig&, std::shared_ptr<stc::logger::ILogger>)>;

    /// @brief Возвращает единственный экземпляр фабрики.
    static AdapterFactory& instance();

    /**
    @brief Создаёт адаптер указанного типа на основе конфигурации.
    @param[in] config Настройки источника данных.
    @param[in] logger Диспетчер логирования.
    @return std::unique_ptr<IFileStorage> Умный указатель на адаптер.
    @throw std::invalid_argument Если тип не поддерживается.
    */
    std::unique_ptr<IFileStorage> createAdapter(
        const SourceConfig& config, std::shared_ptr<stc::logger::ILogger> logger);

    /// @brief Регистрирует новый тип адаптера в фабрике.
    void registerAdapter(const std::string& type, CreatorFunction creator);

    /// @brief Проверяет поддержку типа хранилища.
    bool isSupported(const std::string& type) const noexcept;

    /// @brief Возвращает список всех поддерживаемых типов адаптеров.
    std::vector<std::string> getSupportedTypes() const;

private:
    AdapterFactory();
    ~AdapterFactory() = default;
    AdapterFactory(const AdapterFactory&) = delete;
    AdapterFactory& operator=(const AdapterFactory&) = delete;

    /// @private Регистрирует встроенные адаптеры: local, ftp.
    void registerBuiltinAdapters();

    /**
    @private
    @brief Валидирует обязательные поля SourceConfig.params.
    @param[in] config Конфигурация источника.
    @param[in] required_fields Список ключей, обязательных в params.
    */
    void validateRequiredFields(const SourceConfig& config,
                                const std::vector<std::string>& required_fields) const;

    /// @private Реестр функций-производителей адаптеров.
    std::unordered_map<std::string, CreatorFunction> creators_;

    /// @private Мьютекс для синхронизации доступа к реестру.
    mutable std::mutex mutex_;
};

} // namespace stc