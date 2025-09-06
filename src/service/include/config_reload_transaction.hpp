#pragma once

#include "../include/configmanager.hpp"
#include <nlohmann/json.hpp>

/**
 * @file config_reload_transaction.hpp
 * @brief Транзакционная перезагрузка конфигурации с откатом
 */

class ConfigReloadTransaction {
public:
    /**
     * @brief Конструктор
     * @param configMgr Ссылка на ConfigManager
     */
    explicit ConfigReloadTransaction(ConfigManager& configMgr);

    /**
     * @brief Деструктор.
     * Если транзакция активна, выполняет откат.
     */
    ~ConfigReloadTransaction();

    /**
     * @brief Начало транзакции: сохраняет текущую конфигурацию.
     * @throws std::runtime_error если транзакция уже активна.
     */
    void begin();

    /**
     * @brief Фиксация изменений: удаляет резервную копию.
     * @throws std::runtime_error если транзакция не активна.
     */
    void commit();

    /**
     * @brief Откат: восстанавливает конфигурацию из резервной копии.
     * @throws std::runtime_error если транзакция не активна.
     */
    void rollback();

    /**
     * @brief Полная перезагрузка: begin(), reload(), commit()/rollback().
     * @throws std::runtime_error при ошибке reload().
     */
    void reload();

private:
    ConfigManager&         configMgr_;
    nlohmann::json         backup_;
    bool                   active_ = false;
};