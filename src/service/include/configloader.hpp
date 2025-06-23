/**
 * @file configloader.hpp
 * @author Artem Ulyanov
 * @version 1
 * @date May, 2025
 * @brief Заголовочный файл класса ConfigLoader
 * @details Заголовочный файл класса CofnfigLoader. Содержит объявление скласса,
 * его свойств и методов.
 */

#pragma once
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <string>

/**
 * @class ConfigLoader
 * @brief Класс для загрузки конфигурации из JSON-файлов
 *
 * @note Для работы требует наличия библиотеки nlohmann/json
 * @warning Не потокобезопасен. Для многопоточного использования требуется
 * внешняя синхронизация
 */
class ConfigLoader {
 public:
  /**
   * @brief Загружает конфигурацию из указанного файла
   * @param filename Путь к JSON-файлу конфигурации
   * @return nlohmann::json Объект с распарсенной конфигурацией
   * @throw std::runtime_error При ошибках открытия файла или парсинга JSON
   * @throw std::invalid_argument Если filename пустой
   *
   * @note Сохраняет путь к файлу для последующих вызовов reload()
   * @code
   * ConfigLoader loader;
   * auto config = loader.loadFromFile("config.json");
   * @endcode
   *
   * @see ConfigManager::loadFromFile()
   */
  nlohmann::json loadFromFile(const std::string &filename);

  /**
   * @brief Перезагружает конфигурацию из последнего использованного файла
   * @return nlohmann::json Обновленный объект конфигурации
   * @throw std::runtime_error Если файл не был загружен ранее или недоступен
   *
   * @warning Перед вызовом должен быть выполнен loadFromFile()
   * @code
   * loader.reload(); // Перезагружает config.json
   * @endcode
   */
  nlohmann::json reload(const std::string &currentFile);

 private:
  /**
   * @brief Внутренний метод чтения содержимого файла
   * @param filename Путь к файлу для чтения
   * @return nlohmann::json Распарсенный JSON объект
   * @throw std::runtime_error При ошибках ввода-вывода
   * @throw nlohmann::json::parse_error При синтаксических ошибках JSON
   *
   * @note Использует стандартный парсер nlohmann/json
   * @warning Не обрабатывает кодировки, отличные от UTF-8
   */
  nlohmann::json readFileContents(const std::string &filename) const;

  std::string lastLoadedFile;  ///< Путь к последнему загруженному файлу
};