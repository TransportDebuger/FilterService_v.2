/**
@file adapterfactory.hpp
@brief Фабрика для создания адаптеров файловых хранилищ.
@version 2.2.0
@date 2026-07-24
*/
#pragma once

#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "../../domain/ports/Ifilestorage.hpp"
#include "../../domain/sourceconfig.hpp"
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

    /// @brief Конструктор фабрики. Регистрирует встроенные адаптеры.
    AdapterFactory();

    /**
     * @brief Создаёт адаптер указанного типа на основе конфигурации.
     * @param[in] config Настройки источника данных.
     * @param[in] logger Диспетчер логирования.
     * @return std::unique_ptr<IFileStorage> Умный указатель на созданный адаптер.
     * @throw std::invalid_argument Если тип хранилища не поддерживается или конфигурация невалидна.
     */
    std::unique_ptr<IFileStorage> createAdapter(
        const SourceConfig& config, std::shared_ptr<stc::logger::ILogger> logger) const;

    /**
     * @brief Регистрирует новый тип адаптера в фабрике.
     * @param[in] type Строковый идентификатор типа хранилища.
     * @param[in] creator Функция-производитель адаптера.
     * @throw std::invalid_argument Если тип пуст или функция-производитель равна nullptr.
     */
    void registerAdapter(const std::string& type, CreatorFunction creator);

    /**
     * @brief Проверяет поддержку типа хранилища.
     * @param[in] type Строковый идентификатор типа хранилища.
     * @return true Если тип поддерживается, иначе false.
     */
    bool isSupported(const std::string& type) const noexcept;

    /**
     * @brief Возвращает список всех поддерживаемых типов адаптеров.
     * @return std::vector<std::string> Вектор поддерживаемых типов.
     */
    std::vector<std::string> getSupportedTypes() const;

private:
    /** @private 
     * @brief Регистрирует встроенные адаптеры: local, ftp. */
    void registerBuiltinAdapters();

    /** @private 
     * @brief Реестр функций-производителей адаптеров. */
    std::unordered_map<std::string, CreatorFunction> creators_;

    /** @private 
     * @brief Мьютекс для синхронизации доступа к реестру. */
    mutable std::mutex mutex_;
};

} // namespace stc