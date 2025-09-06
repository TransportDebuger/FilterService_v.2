/**
 * @file FilterListManager.cpp
 * @brief Реализация менеджера централизованного управления списками фильтрации
 */
#include "../include/FilterListManager.hpp"

#include <codecvt>
#include <filesystem>
#include <locale>
#include <tuple>
#include <algorithm>

#include "stc/SignalRouter.hpp"

namespace fs = std::filesystem;

FilterListManager &FilterListManager::instance() {
  static FilterListManager instance;
  return instance;
}

void FilterListManager::initialize(const std::string &csvPath) {
  std::unique_lock<std::shared_mutex> lock(mutex_);

  if (csvPath.empty()) {
    throw std::invalid_argument("CSV path cannot be empty");
  }

  if (!fs::exists(csvPath)) {
    throw std::runtime_error("CSV file does not exist: " + csvPath);
  }

  csvPath_ = fs::absolute(csvPath).string();

  try {
    loadCsvData();
    validateData();
    initialized_.store(true);

    stc::CompositeLogger::instance().info(
        "FilterListManager initialized with " +
        std::to_string(columnData_.size()) + " columns from: " + csvPath_);

    // Логирование статистики по столбцам
  for (const auto& pair : columnData_) {
    const std::string& column = pair.first;
    const auto& values = pair.second;
    
    stc::CompositeLogger::instance().debug(
        "Column '" + column + "' contains " + 
        std::to_string(values.size()) + " unique values");
  }

  } catch (const std::exception &e) {
    initialized_.store(false);
    stc::CompositeLogger::instance().error(
        "FilterListManager initialization failed: " + std::string(e.what()));
    throw std::runtime_error("Failed to initialize FilterListManager: " +
                             std::string(e.what()));
  }
}

void FilterListManager::reload() {
  if (!initialized_.load()) {
    throw std::runtime_error("FilterListManager not initialized");
  }

  std::unique_lock<std::shared_mutex> lock(mutex_);

  try {
    // Создаем резервную копию текущих данных
    auto backup = columnData_;
    auto backupHeaders = headers_;

    // Очищаем текущие данные
    columnData_.clear();
    headers_.clear();

    // Загружаем новые данные
    loadCsvData();
    validateData();

    stc::CompositeLogger::instance().info(
        "FilterListManager reloaded successfully from: " + csvPath_);

    // Логирование изменений
    for (const auto &[column, values] : columnData_) {
      auto oldIt = backup.find(column);
      if (oldIt != backup.end()) {
        size_t oldSize = oldIt->second.size();
        size_t newSize = values.size();
        if (oldSize != newSize) {
          stc::CompositeLogger::instance().info(
              "Column '" + column + "' updated: " + std::to_string(oldSize) +
              " -> " + std::to_string(newSize) + " values");
        }
      } else {
        stc::CompositeLogger::instance().info(
            "New column '" + column + "' added with " +
            std::to_string(values.size()) + " values");
      }
    }

  } catch (const std::exception &e) {
    stc::CompositeLogger::instance().error("FilterListManager reload failed: " +
                                           std::string(e.what()));
    throw std::runtime_error("Failed to reload FilterListManager: " +
                             std::string(e.what()));
  }
}

bool FilterListManager::contains(const std::string &column,
                                 const std::string &value) const {
  if (!initialized_.load()) {
    throw std::runtime_error("FilterListManager not initialized");
  }

  std::shared_lock<std::shared_mutex> lock(mutex_);

  auto columnIt = columnData_.find(column);
  if (columnIt == columnData_.end()) {
    throw std::invalid_argument("Column not found: " + column);
  }

  bool found = columnIt->second.find(value) != columnIt->second.end();

  if (found) {
    stc::CompositeLogger::instance().debug(
        "Value '" + value + "' found in column '" + column + "'");
  }

  return found;
}

bool FilterListManager::isInitialized() const noexcept {
  return initialized_.load();
}

std::string FilterListManager::getCurrentCsvPath() const {
  std::shared_lock<std::shared_mutex> lock(mutex_);
  return csvPath_;
}

void FilterListManager::loadCsvData() {
  std::ifstream file(csvPath_);
  if (!file.is_open()) {
    stc::CompositeLogger::instance().error("Cannot open CSV: " + csvPath_);
    throw std::runtime_error("Cannot open CSV file: " + csvPath_);
  }

  stc::CompositeLogger::instance().debug("Opening CSV: " + csvPath_);

  std::string line;
  bool isFirstLine = true;
  size_t lineNumber = 0;

  while (std::getline(file, line)) {
    lineNumber++;

    if (line.empty() || line[0] == '#') {
      continue;  // Пропускаем пустые строки и комментарии
    }

    auto values = parseCsvLine(line);
    if (values.empty()) {
      continue;
    }

    if (isFirstLine) {
      // Первая строка содержит заголовки
      headers_ = values;
      for (const auto &header : headers_) {
        stc::CompositeLogger::instance().debug("Loaded CSV column: " + header);
      }
      for (const auto &header : headers_) {
        columnData_[header] = std::unordered_set<std::string>();
      }
      isFirstLine = false;

      stc::CompositeLogger::instance().debug(
          "CSV headers loaded: " + std::to_string(headers_.size()) +
          " columns");
    } else {
      // Обработка строки данных
      if (values.size() != headers_.size()) {
        stc::CompositeLogger::instance().warning(
            "CSV line " + std::to_string(lineNumber) +
            " has incorrect number of columns (expected: " +
            std::to_string(headers_.size()) +
            ", got: " + std::to_string(values.size()) + ")");
        continue;
      }

      for (size_t i = 0; i < headers_.size(); ++i) {
        const std::string &cleanValue = trimAndUnquote(values[i]);
        if (!cleanValue.empty()) {
          columnData_[headers_[i]].insert(cleanValue);
        }
      }
    }
  }

  file.close();

  if (headers_.empty()) {
    throw std::runtime_error("No valid headers found in CSV file");
  }

  stc::CompositeLogger::instance().info(
      "CSV data loaded: " + std::to_string(lineNumber) + " lines processed");
}

std::vector<std::string> FilterListManager::parseCsvLine(
    const std::string &line) const {
  std::vector<std::string> result;
  std::string current;
  bool inQuotes = false;
  bool escapeNext = false;

  for (size_t i = 0; i < line.length(); ++i) {
    char c = line[i];

    if (escapeNext) {
      current += c;
      escapeNext = false;
      continue;
    }

    if (c == '\\') {
      escapeNext = true;
      continue;
    }

    if (c == '"') {
      if (inQuotes && i + 1 < line.length() && line[i + 1] == '"') {
        // Экранированная кавычка ""
        current += '"';
        ++i;  // Пропускаем следующую кавычку
      } else {
        inQuotes = !inQuotes;
      }
    } else if (c == ',' && !inQuotes) {
      result.push_back(current);
      current.clear();
    } else {
      current += c;
    }
  }

  // Добавляем последнее значение
  result.push_back(current);

  return result;
}

std::string FilterListManager::trimAndUnquote(const std::string &value) const {
  std::string result = value;

  // Удаляем пробелы в начале и конце
  size_t start = result.find_first_not_of(" \t\r\n");
  if (start == std::string::npos) {
    return "";
  }

  size_t end = result.find_last_not_of(" \t\r\n");
  result = result.substr(start, end - start + 1);

  // Удаляем внешние кавычки если они есть
  if (result.length() >= 2 && result.front() == '"' && result.back() == '"') {
    result = result.substr(1, result.length() - 2);
  }

  return result;
}

void FilterListManager::validateData() const {
  if (headers_.empty()) {
    throw std::runtime_error("No columns defined in CSV");
  }

  if (columnData_.empty()) {
    throw std::runtime_error("No data loaded from CSV");
  }

  // Проверяем, что все заголовки имеют соответствующие данные
  for (const auto &header : headers_) {
    if (columnData_.find(header) == columnData_.end()) {
      throw std::runtime_error("Missing data for column: " + header);
    }
  }

  // Проверяем, что есть хотя бы один столбец с данными
  bool hasData = std::any_of(
    columnData_.begin(), 
    columnData_.end(),
    [](const auto& pair) {
        return !pair.second.empty();
    });

  if (!hasData) {
    throw std::runtime_error("No valid data found in any column");
  }
}

void registerFilterListReload() {
  stc::SignalRouter::instance().registerHandler(SIGHUP, [](int) {
    try {
      if (FilterListManager::instance().isInitialized()) {
        FilterListManager::instance().reload();
        stc::CompositeLogger::instance().info(
            "FilterListManager reloaded on SIGHUP signal");
      }
    } catch (const std::exception &e) {
      stc::CompositeLogger::instance().error(
          "Failed to reload FilterListManager on SIGHUP: " +
          std::string(e.what()));
    }
  });
}