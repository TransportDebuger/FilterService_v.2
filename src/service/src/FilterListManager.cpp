/**
@file FilterListManager.cpp
@brief Реализация менеджера централизованного управления списками фильтрации.
@version 2.0.0
@date 2026-07-17
*/
#include "../include/FilterListManager.hpp"

#include <algorithm>
#include <filesystem>

namespace fs = std::filesystem;

namespace stc {

FilterListManager::FilterListManager(std::shared_ptr<stc::logger::ILogger> logger)
    : logger_(std::move(logger)) {}

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
        if (logger_) logger_->Info("FilterListManager initialized with " +
                                   std::to_string(columnData_.size()) + " columns from: " + csvPath_);
        
        for (const auto& pair : columnData_) {
            const std::string& column = pair.first;
            const auto& values = pair.second;
            if (logger_) logger_->Debug("Column '" + column + "' contains " +
                                        std::to_string(values.size()) + " unique values");
        }
    } catch (const std::exception &e) {
        initialized_.store(false);
        if (logger_) logger_->Error("FilterListManager initialization failed: " + std::string(e.what()));
        throw std::runtime_error("Failed to initialize FilterListManager: " + std::string(e.what()));
    }
}

void FilterListManager::reload() {
    if (!initialized_.load()) {
        throw std::runtime_error("FilterListManager not initialized");
    }
    std::unique_lock<std::shared_mutex> lock(mutex_);
    try {
        auto backup = columnData_;
        auto backupHeaders = headers_;
        
        columnData_.clear();
        headers_.clear();
        
        loadCsvData();
        validateData();
        
        if (logger_) logger_->Info("FilterListManager reloaded successfully from: " + csvPath_);
        
        for (const auto &[column, values] : columnData_) {
            auto oldIt = backup.find(column);
            if (oldIt != backup.end()) {
                size_t oldSize = oldIt->second.size();
                size_t newSize = values.size();
                if (oldSize != newSize) {
                    if (logger_) logger_->Info("Column '" + column + "' updated: " + std::to_string(oldSize) +
                                               " -> " + std::to_string(newSize) + " values");
                }
            } else {
                if (logger_) logger_->Info("New column '" + column + "' added with " +
                                           std::to_string(values.size()) + " values");
            }
        }
    } catch (const std::exception &e) {
        if (logger_) logger_->Error("FilterListManager reload failed: " + std::string(e.what()));
        throw std::runtime_error("Failed to reload FilterListManager: " + std::string(e.what()));
    }
}

bool FilterListManager::contains(const std::string &column, const std::string &value) const {
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
        if (logger_) logger_->Debug("Value '" + value + "' found in column '" + column + "'");
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

vsize_t FilterListManager::getTotalRecordsCount() const noexcept {
    return total_records_count_.load(std::memory_order_relaxed);
}

void FilterListManager::loadCsvData() {
    std::ifstream file(csvPath_);
    if (!file.is_open()) {
        if (logger_) logger_->Error("Cannot open CSV: " + csvPath_);
        throw std::runtime_error("Cannot open CSV file: " + csvPath_);
    }

    if (logger_) logger_->Debug("Opening CSV: " + csvPath_);
    
    std::string line;
    bool isFirstLine = true;
    size_t lineNumber = 0;
    size_t dataLinesCount = 0; // Счетчик строк с данными
    
    while (std::getline(file, line)) {
        lineNumber++;
        if (line.empty() || line[0] == '#') {
            continue;
        }
        
        auto values = parseCsvLine(line);
        if (values.empty()) {
            continue;
        }
        
        if (isFirstLine) {
            headers_ = values;
            for (const auto &header : headers_) {
                if (logger_) logger_->Debug("Loaded CSV column: " + header);
                columnData_[header] = std::unordered_set<std::string>();
            }
            isFirstLine = false;
            if (logger_) logger_->Debug("CSV headers loaded: " + std::to_string(headers_.size()) + " columns");
        } else {
            if (values.size() != headers_.size()) {
                if (logger_) logger_->Warning("CSV line " + std::to_string(lineNumber) +
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
            dataLinesCount++; // Инкрементируем счетчик успешных строк данных
        }
    }
    
    file.close();
    
    if (headers_.empty()) {
        throw std::runtime_error("No valid headers found in CSV file");
    }
    
    // Сохраняем подсчитанное количество записей
    total_records_count_.store(dataLinesCount, std::memory_order_relaxed);
    
    if (logger_) logger_->Info("CSV data loaded: " + std::to_string(lineNumber) + 
                               " lines processed, " + std::to_string(dataLinesCount) + " data records");
}

std::vector<std::string> FilterListManager::parseCsvLine(const std::string &line) const {
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
                current += '"';
                ++i;
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
    result.push_back(current);
    return result;
}

std::string FilterListManager::trimAndUnquote(const std::string &value) const {
    std::string result = value;
    size_t start = result.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) {
        return "";
    }
    size_t end = result.find_last_not_of(" \t\r\n");
    result = result.substr(start, end - start + 1);
    
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
    for (const auto &header : headers_) {
        if (columnData_.find(header) == columnData_.end()) {
            throw std::runtime_error("Missing data for column: " + header);
        }
    }
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

} // namespace stc