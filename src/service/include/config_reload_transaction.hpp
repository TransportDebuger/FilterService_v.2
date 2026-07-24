/**
@file config_reload_transaction.hpp
@brief Транзакционная перезагрузка конфигурации с механизмом отката.
@version 2.0.0
@date 2026-07-17
*/
#pragma once

#include <memory>
#include <nlohmann/json.hpp>

#include "configmanager.hpp"
#include "stc/logger/ilogger.hpp"

namespace stc {

/**
@class ConfigReloadTransaction
@brief Обеспечивает атомарную перезагрузку конфигурации с автоматическим откатом при ошибках.
*/
class ConfigReloadTransaction {
public:
    /**
    @brief Конструктор транзакции.
    @param[in] configMgr Ссылка на менеджер конфигурации.
    @param[in] logger Диспетчер логирования.
    */
    explicit ConfigReloadTransaction(ConfigManager& configMgr,
                                     std::shared_ptr<stc::logger::ILogger> logger);

    /**
    @brief Деструктор. Автоматически выполняет откат, если транзакция не была зафиксирована.
    */
    ~ConfigReloadTransaction();

    ConfigReloadTransaction(const ConfigReloadTransaction&) = delete;
    ConfigReloadTransaction& operator=(const ConfigReloadTransaction&) = delete;

    /**
    @brief Начинает транзакцию: создает резервную копию конфигурации.
    @throw std::runtime_error Если транзакция уже активна.
    */
    void begin();

    /**
    @brief Фиксирует изменения: удаляет резервную копию.
    @throw std::runtime_error Если транзакция не активна.
    */
    void commit();

    /**
    @brief Откатывает изменения: восстанавливает конфигурацию из резервной копии.
    @throw std::runtime_error Если транзакция не активна.
    */
    void rollback();

    /**
    @brief Выполняет полную перезагрузку: begin(), reload(), commit()/rollback().
    @throw std::runtime_error При ошибке перезагрузки.
    */
    void reload();

private:
    /// @private Ссылка на менеджер конфигурации.
    ConfigManager& configMgr_;
    
    /// @private Диспетчер логирования, полученный через DI.
    std::shared_ptr<stc::logger::ILogger> logger_;
    
    /// @private Резервная копия конфигурации для отката.
    nlohmann::json backup_;
    
    /// @private Флаг активности транзакции.
    bool active_ = false;
};

} // namespace stc