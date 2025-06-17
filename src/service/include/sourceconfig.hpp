/**
 * @file SourceConfig.hpp
 * @brief Конфигурация источника данных для различных типов хранилищ
 * 
 * @details Определяет структуру конфигурации для работы с различными
 *          типами файловых хранилищ (локальные, SMB, FTP) с поддержкой
 *          параметров подключения и валидации
 */

#pragma once
#include <string>
#include <unordered_map>
#include <vector>
#include <nlohmann/json.hpp>
#include <chrono>

/**
 * @struct SourceConfig
 * @brief Структура конфигурации источника данных
 * 
 * @note Поддерживает различные типы хранилищ через параметры подключения[16][17]
 */
struct SourceConfig {
    // ============= Обязательные поля =============
    
    /// Уникальное имя источника данных
    std::string name;
    
    /// Тип хранилища (local, smb, ftp)
    std::string type;
    
    /// Путь к источнику данных
    std::string path;
    
    /// Маска файлов для обработки (поддерживает wildcards *, ?)
    std::string file_mask;
    
    /// Директория для обработанных файлов
    std::string processed_dir;

    // ============= Опциональные поля =============
    
    /// Директория для файлов с ошибками
    std::string bad_dir;
    
    /// Директория для исключенных файлов
    std::string excluded_dir;
    
    /// Шаблон имени для отфильтрованных файлов
    std::string filtered_template = "{filename}_filtered.{ext}";
    
    /// Шаблон имени для исключенных файлов
    std::string excluded_template = "{filename}_excluded.{ext}";
    
    /// Путь к файлу сравнения для фильтрации
    std::string comparison_list = "./comparison_list.csv";
    
    /// Флаг включения фильтрации
    bool filtering_enabled = true;
    
    /// Интервал проверки изменений (в секундах)
    std::chrono::seconds check_interval{5};
    
    /// Флаг активности источника
    bool enabled = true;

    // ============= Параметры подключения =============
    
    /**
     * @brief Параметры подключения для различных типов хранилищ
     * 
     * @details Поддерживаемые параметры:
     * - username: имя пользователя (SMB, FTP)
     * - password: пароль (SMB, FTP)
     * - domain: домен/workgroup (SMB)
     * - port: порт подключения (FTP)
     * - timeout: таймаут соединения (FTP, SMB)
     */
    std::unordered_map<std::string, std::string> params;

    // ============= Методы =============
    
    /**
     * @brief Создает SourceConfig из JSON объекта
     * @param src JSON объект с конфигурацией
     * @return SourceConfig Настроенная структура конфигурации
     * @throw std::runtime_error При отсутствии обязательных полей
     */
    static SourceConfig fromJson(const nlohmann::json& src);
    
    /**
     * @brief Преобразует SourceConfig в JSON
     * @return nlohmann::json JSON представление конфигурации
     */
    nlohmann::json toJson() const;
    
    /**
     * @brief Валидирует конфигурацию источника
     * @throw std::invalid_argument При невалидной конфигурации
     */
    void validate() const;

    /**
     * @brief Получает имя отфильтрованного файла
     * @param original_filename Исходное имя файла
     * @return std::string Имя файла по шаблону filtered_template
     */
    std::string getFilteredFileName(const std::string& original_filename) const;
    
    /**
     * @brief Получает имя исключенного файла
     * @param original_filename Исходное имя файла
     * @return std::string Имя файла по шаблону excluded_template
     */
    std::string getExcludedFileName(const std::string& original_filename) const;
    
    /**
     * @brief Проверяет наличие обязательных параметров для типа хранилища
     * @param required_params Список обязательных параметров
     * @return bool true если все параметры присутствуют
     */
    bool hasRequiredParams(const std::vector<std::string>& required_params) const;

private:
    /**
     * @brief Применяет шаблон к имени файла
     * @param filename Исходное имя файла
     * @param template_str Строка шаблона
     * @return std::string Результирующее имя файла
     */
    std::string applyTemplate(const std::string& filename, const std::string& template_str) const;
};