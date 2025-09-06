/**
 * @file FilterListManager.hpp
 * @brief Менеджер централизованного управления списками фильтрации XML
 *
 * @details Реализует синглтон для потокобезопасного управления данными
 * из CSV-файлов, используемыми для фильтрации XML-файлов. Поддерживает
 * динамическое обновление данных без перезапуска сервиса.
 *
 * @author FilterService Development Team
 * @version 1.0
 * @date 2024
 */
#pragma once

#include <algorithm>
#include <fstream>
#include <shared_mutex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>

#include "stc/compositelogger.hpp"

/**
 * @class FilterListManager
 * @brief Синглтон для централизованного управления списками фильтрации
 *
 * @details Обеспечивает потокобезопасную загрузку, хранение и проверку
 * данных из CSV-файлов для многокритериальной фильтрации XML-файлов.
 * Поддерживает множественные столбцы CSV и динамическое обновление
 * данных по сигналу SIGHUP.
 *
 * @note Использует std::shared_mutex для оптимизации чтения данных
 * несколькими потоками одновременно при редких операциях записи
 *
 * @warning Класс не копируемый и не перемещаемый (Singleton pattern)
 */
class FilterListManager {
 public:
  /**
   * @brief Получить единственный экземпляр менеджера (Singleton)
   * @return FilterListManager& Ссылка на экземпляр синглтона
   *
   * @note Реализует thread-safe lazy initialization через C++11 magic static
   */
  static FilterListManager &instance();

  /**
   * @brief Инициализирует менеджер с указанием CSV-файла
   * @param csvPath Путь к CSV-файлу со списками фильтрации
   * @throw std::runtime_error При ошибках загрузки CSV-файла
   *
   * @details Загружает данные из CSV-файла в память и строит
   * индексированные структуры данных для быстрого поиска.
   * Автоматически определяет столбцы CSV по заголовкам.
   *
   * @note Должен вызываться один раз при запуске сервиса
   */
  void initialize(const std::string &csvPath);

  /**
   * @brief Перезагружает данные из CSV-файла
   * @throw std::runtime_error При ошибках перезагрузки
   *
   * @details Атомарно обновляет данные фильтрации без блокировки
   * читающих потоков. Вызывается автоматически при получении
   * сигнала SIGHUP для обновления списков без перезапуска сервиса.
   *
   * @note Потокобезопасен, использует exclusive lock для записи
   */
  void reload();

  /**
   * @brief Проверяет наличие значения в указанном столбце
   * @param column Имя столбца CSV для поиска
   * @param value Искомое значение
   * @return bool true если значение найдено в указанном столбце
   * @throw std::invalid_argument При отсутствии указанного столбца
   *
   * @details Выполняет поиск за время O(1) благодаря использованию
   * std::unordered_set для хранения значений каждого столбца.
   * Использует shared_lock для неблокирующего чтения.
   *
   * @note Потокобезопасен для множественного чтения
   */
  bool contains(const std::string &column, const std::string &value) const;

  /**
   * @brief Проверяет инициализацию менеджера
   * @return bool true если менеджер инициализирован
   *
   * @note Потокобезопасен, использует атомарную переменную
   */
  bool isInitialized() const noexcept;

  /**
   * @brief Получает путь к текущему CSV-файлу
   * @return std::string Путь к файлу или пустая строка если не инициализирован
   */
  std::string getCurrentCsvPath() const;

 private:
  /**
   * @brief Приватный конструктор (Singleton pattern)
   */
  FilterListManager() = default;

  /**
   * @brief Деструктор
   */
  ~FilterListManager() = default;

  // Запрещаем копирование и перемещение
  FilterListManager(const FilterListManager &) = delete;
  FilterListManager &operator=(const FilterListManager &) = delete;
  FilterListManager(FilterListManager &&) = delete;
  FilterListManager &operator=(FilterListManager &&) = delete;

  /**
   * @brief Загружает данные из CSV-файла
   * @throw std::runtime_error При ошибках чтения файла или парсинга
   *
   * @details Внутренний метод для загрузки и парсинга CSV.
   * Поддерживает экранирование запятых в кавычках и различные
   * кодировки (UTF-8, Windows-1251).
   */
  void loadCsvData();

  /**
   * @brief Парсит строку CSV с учетом экранирования
   * @param line Строка для парсинга
   * @return std::vector<std::string> Вектор значений столбцов
   *
   * @details Корректно обрабатывает:
   * - Запятые внутри кавычек
   * - Экранированные кавычки ("")
   * - Пустые значения
   * - Пробелы в начале и конце значений
   */
  std::vector<std::string> parseCsvLine(const std::string &line) const;

  /**
   * @brief Очищает значение от пробелов и кавычек
   * @param value Исходное значение
   * @return std::string Очищенное значение
   */
  std::string trimAndUnquote(const std::string &value) const;

  /**
   * @brief Валидирует структуру загруженных данных
   * @throw std::runtime_error При некорректной структуре CSV
   */
  void validateData() const;

  /// Карта столбцов CSV: имя столбца -> множество значений
  std::unordered_map<std::string, std::unordered_set<std::string>> columnData_;

  /// Путь к текущему CSV-файлу
  std::string csvPath_;

  /// Мьютекс для потокобезопасности (поддерживает shared_lock)
  mutable std::shared_mutex mutex_;

  /// Флаг инициализации
  std::atomic<bool> initialized_{false};

  /// Заголовки столбцов CSV
  std::vector<std::string> headers_;
};

/**
 * @brief Вспомогательная функция для интеграции с SignalRouter
 * @details Регистрирует обработчик SIGHUP для автоматической перезагрузки
 * FilterListManager при изменении конфигурации
 */
void registerFilterListReload();