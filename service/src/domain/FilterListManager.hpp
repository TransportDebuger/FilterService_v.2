/**
@file FilterListManager.hpp
@brief Менеджер централизованного управления списками фильтрации (SQLite + JSON).
@version 4.0.0
@date 2026-10-24
*/
#pragma once

#include <atomic>
#include <memory>
#include <shared_mutex>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>

#include "stc/logger/ilogger.hpp"

namespace stc {

/**
@class FilterListManager
@brief Обеспечивает потокобезопасную загрузку из SQLite, хранение и проверку данных.
*/
class FilterListManager {
public:
    explicit FilterListManager(std::shared_ptr<stc::logger::ILogger> logger);
    ~FilterListManager() = default;

    /**
    @brief Инициализирует менеджер с указанием пути к SQLite и ID списка.
    @param[in] db_path Абсолютный путь к файлу базы данных SQLite.
    @param[in] list_id Идентификатор списка сравнения в таблице comparison_lists.
    @throw std::runtime_error При ошибках открытия БД или чтения данных.
    */
    void initialize(const std::string& db_path, int list_id);
    
    /**
    @brief Выполняет горячую перезагрузку данных из SQLite.
    @details Использует паттерн Read-Copy-Update (RCU). Воркеры не блокируются,
             за исключением микросекунд на атомарную подмену указателя на хеш-таблицу.
    */
    void hotReload();
    
    /**
    @brief Проверяет наличие значения в указанном столбце.
    @param[in] column Имя столбца (ключ JSON).
    @param[in] value Искомое значение.
    @return true Если значение найдено.
    */
    bool contains(const std::string& column, const std::string& value) const;
    
    bool isInitialized() const noexcept;
    size_t getTotalRecordsCount() const noexcept;
    
    static std::string normalizeValue(const std::string& value);

private:
    /// @private Загружает данные из SQLite во временный контейнер и атомарно подменяет основные.
    void loadFromSqlite();

    std::shared_ptr<stc::logger::ILogger> logger_;
    
    std::string db_path_;
    int list_id_{-1};
    
    /// @private Основная структура данных: column_name -> set of values.
    std::unordered_map<std::string, std::unordered_set<std::string>> columnData_;
    
    /// @private Мьютекс для потокобезопасности (поддерживает shared_lock для чтения).
    mutable std::shared_mutex mutex_;
    
    std::atomic<size_t> total_records_count_{0};
    std::atomic<bool> initialized_{false};
};

} // namespace stc