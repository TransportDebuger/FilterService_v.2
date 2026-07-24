/**
@file FilterListManager.hpp
@brief Менеджер централизованного управления списками фильтрации XML.
@version 2.0.0
@date 2026-07-17
*/
#pragma once

#include <atomic>
#include <fstream>
#include <memory>
#include <shared_mutex>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "stc/logger/ilogger.hpp"

namespace stc {

/**
@class FilterListManager
@brief Обеспечивает потокобезопасную загрузку, хранение и проверку данных из CSV-файлов.
*/
class FilterListManager {
public:
    /**
    @brief Конструктор менеджера списков фильтрации.
    @param[in] logger Диспетчер логирования.
    */
    explicit FilterListManager(std::shared_ptr<stc::logger::ILogger> logger);

    /**
    @brief Инициализирует менеджер с указанием CSV-файла.
    @param[in] csvPath Путь к CSV-файлу со списками фильтрации.
    @throw std::runtime_error При ошибках загрузки CSV-файла.
    */
    void initialize(const std::string &csvPath);

    /**
    @brief Перезагружает данные из CSV-файла.
    @throw std::runtime_error При ошибках перезагрузки.
    */
    void reload();

    /**
    @brief Проверяет наличие значения в указанном столбце.
    @param[in] column Имя столбца CSV для поиска.
    @param[in] value Искомое значение.
    @return true Если значение найдено.
    @throw std::invalid_argument При отсутствии указанного столбца.
    */
    bool contains(const std::string &column, const std::string &value) const;

    /**
    @brief Проверяет инициализацию менеджера.
    @return true Если менеджер инициализирован.
    */
    bool isInitialized() const noexcept;

    /**
    @brief Получает путь к текущему CSV-файлу.
    @return Путь к файлу или пустая строка, если не инициализирован.
    */
    std::string getCurrentCsvPath() const;

    /**
    @brief Возвращает общее количество загруженных записей (строк данных) из CSV.
    @return size_t Количество записей или 0, если менеджер не инициализирован.
    */
    size_t getTotalRecordsCount() const noexcept;

private:
    /// @private Загружает данные из CSV-файла.
    void loadCsvData();

    /// @private Парсит строку CSV с учетом экранирования.
    std::vector<std::string> parseCsvLine(const std::string &line) const;

    /// @private Очищает значение от пробелов и кавычек.
    std::string trimAndUnquote(const std::string &value) const;

    /// @private Валидирует структуру загруженных данных.
    void validateData() const;

    /// @private Общее количество загруженных строк данных из CSV (без учета заголовка).
    std::atomic<size_t> total_records_count_{0};

    /// @private Диспетчер логирования, полученный через DI.
    std::shared_ptr<stc::logger::ILogger> logger_;

    /// @private Карта столбцов CSV: имя столбца -> множество значений.
    std::unordered_map<std::string, std::unordered_set<std::string>> columnData_;

    /// @private Путь к текущему CSV-файлу.
    std::string csvPath_;

    /// @private Мьютекс для потокобезопасности (поддерживает shared_lock).
    mutable std::shared_mutex mutex_;

    /// @private Флаг инициализации.
    std::atomic<bool> initialized_{false};

    /// @private Заголовки столбцов CSV.
    std::vector<std::string> headers_;
};

} // namespace stc