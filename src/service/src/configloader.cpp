/**
 * @file configloader.cpp
 * @author Artem Ulyanov
 * @company STC Ltd.
 * @date May 2025
 * @brief Реализация загрузчика конфигураций из JSON-файлов
 *
 * @details
 * Содержит полную реализацию методов класса ConfigLoader для загрузки
 * и перезагрузки конфигураций из JSON-файлов. Обеспечивает надежное
 * чтение файлов с детальной обработкой ошибок и валидацией данных.
 *
 * Ключевые особенности реализации:
 * - Использование RAII для автоматического управления ресурсами
 * - Детальная обработка исключений с информативными сообщениями
 * - Оптимизированное чтение файлов через std::ifstream
 * - Интеграция с nlohmann/json для высокопроизводительного парсинга
 *
 * @version 1.0
 */

#include "../include/configloader.hpp"

#include <fstream>
#include <sstream>

nlohmann::json ConfigLoader::loadFromFile(const std::string &filename) {
  lastLoadedFile = filename;
  return readFileContents(filename);
}

nlohmann::json ConfigLoader::reload(const std::string &currentFile) {
  if (currentFile.empty()) {
    throw std::runtime_error("ConfigLoader: no file specified for reload");
  }
  return readFileContents(currentFile);
}

std::string ConfigLoader::getLastLoadedFile() const { return lastLoadedFile; }

bool ConfigLoader::hasLoadedFile() const { return !lastLoadedFile.empty(); }

nlohmann::json ConfigLoader::readFileContents(
    const std::string &filename) const {
  std::ifstream file(filename);

  if (!file.is_open()) {
    throw std::runtime_error("ConfigLoader: Failed to open file " + filename);
  }

  try {
    nlohmann::json config;
    file >> config;
    return config;
  } catch (const nlohmann::json::parse_error &e) {
    std::stringstream ss;
    ss << "ConfigLoader: JSON parse error: " << e.what() << " at byte "
       << e.byte;
    throw std::runtime_error(ss.str());
  }
}