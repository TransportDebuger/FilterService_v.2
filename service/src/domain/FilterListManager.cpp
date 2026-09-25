/**
@file FilterListManager.cpp
@brief Реализация менеджера централизованного управления списками фильтрации.
@version 3.0.0
@date 2026-09-06
*/
#include "FilterListManager.hpp"
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <mutex>

namespace fs = std::filesystem;

namespace stc {

FilterListManager::FilterListManager(
    std::shared_ptr<stc::logger::ILogger> logger)
    : logger_(std::move(logger)) {}

void FilterListManager::initialize(const std::string& csvPath) {
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

        if (logger_) {
            logger_->Info("FilterListManager initialized with " +
                          std::to_string(columnData_.size()) +
                          " columns from: " + csvPath_);
        }
    } catch (const std::exception& e) {
        initialized_.store(false);
        if (logger_) {
            logger_->Error("FilterListManager initialization failed: " +
                           std::string(e.what()));
        }
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
        columnData_.clear();
        headers_.clear();
        loadCsvData();
        validateData();

        if (logger_) {
            logger_->Info("FilterListManager reloaded successfully from: " +
                          csvPath_);
        }
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("FilterListManager reload failed: " +
                           std::string(e.what()));
        }
        throw std::runtime_error("Failed to reload FilterListManager: " +
                                 std::string(e.what()));
    }
}

bool FilterListManager::contains(const std::string& column,
                                 const std::string& value) const {
    if (!initialized_.load()) {
        throw std::runtime_error("FilterListManager not initialized");
    }

    std::shared_lock<std::shared_mutex> lock(mutex_);

    auto columnIt = columnData_.find(column);
    if (columnIt == columnData_.end()) {
        throw std::invalid_argument("Column not found: " + column);
    }

    const std::string normalizedValue = normalizeValue(value);
    return columnIt->second.find(normalizedValue) != columnIt->second.end();
}

bool FilterListManager::isInitialized() const noexcept {
    return initialized_.load();
}

std::string FilterListManager::getCurrentCsvPath() const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    return csvPath_;
}

size_t FilterListManager::getTotalRecordsCount() const noexcept {
    return total_records_count_.load(std::memory_order_relaxed);
}

std::string FilterListManager::normalizeValue(const std::string& value) {
    std::string result;
    result.reserve(value.size());

    bool inWhitespace = false;
    bool hasContent = false;

    for (size_t i = 0; i < value.size();) {
        const unsigned char c = static_cast<unsigned char>(value[i]);

        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
            if (hasContent) {
                inWhitespace = true;
            }
            i++;
        } else if (i + 1 < value.size() && c == 0xC2 &&
                   static_cast<unsigned char>(value[i + 1]) == 0xA0) {
            // Неразрывный пробел U+00A0
            if (hasContent) {
                inWhitespace = true;
            }
            i += 2;
        } else {
            if (inWhitespace) {
                result += ' ';
                inWhitespace = false;
            }
            hasContent = true;

            if (c < 0x80) {
                result += static_cast<char>(std::tolower(c));
                i++;
            } else if (i + 1 < value.size() && c == 0xD0) {
                const unsigned char second =
                    static_cast<unsigned char>(value[i + 1]);

                if (second >= 0x90 && second <= 0xAF) {
                    result += static_cast<char>(0xD0);
                    result += static_cast<char>(second + 0x20);
                } else if (second == 0x81) {
                    result += static_cast<char>(0xD1);
                    result += static_cast<char>(0x91);
                } else {
                    result += static_cast<char>(value[i]);
                    result += static_cast<char>(value[i + 1]);
                }
                i += 2;
            } else if (i + 1 < value.size() && c == 0xD1) {
                result += static_cast<char>(value[i]);
                result += static_cast<char>(value[i + 1]);
                i += 2;
            } else {
                result += static_cast<char>(value[i]);
                i++;
            }
        }
    }

    return result;
}

void FilterListManager::loadCsvData() {
    std::ifstream file(csvPath_);
    if (!file.is_open()) {
        if (logger_) logger_->Error("Cannot open CSV: " + csvPath_);
        throw std::runtime_error("Cannot open CSV file: " + csvPath_);
    }

    std::string line;
    bool isFirstLine = true;
    size_t lineNumber = 0;
    size_t dataLinesCount = 0;

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
            for (const auto& header : headers_) {
                const std::string normalizedHeader = normalizeValue(header);
                columnData_[normalizedHeader] =
                    std::unordered_set<std::string>();
            }
            isFirstLine = false;
        } else {
            if (values.size() != headers_.size()) {
                if (logger_) {
                    logger_->Warning("CSV line " + std::to_string(lineNumber) +
                                     " has incorrect number of columns");
                }
                continue;
            }

            for (size_t i = 0; i < headers_.size(); ++i) {
                const std::string normalizedHeader =
                    normalizeValue(headers_[i]);
                const std::string cleanValue = normalizeValue(values[i]);
                // Пустые значения сохраняются как пустая строка
                columnData_[normalizedHeader].insert(cleanValue);
            }
            dataLinesCount++;
        }
    }

    file.close();

    if (headers_.empty()) {
        throw std::runtime_error("No valid headers found in CSV file");
    }

    total_records_count_.store(dataLinesCount, std::memory_order_relaxed);

    if (logger_) {
        logger_->Info("CSV data loaded: " + std::to_string(lineNumber) +
                      " lines processed, " + std::to_string(dataLinesCount) +
                      " data records");
    }
}

std::vector<std::string> FilterListManager::parseCsvLine(
    const std::string& line) const {
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

void FilterListManager::validateData() const {
    if (headers_.empty()) {
        throw std::runtime_error("No columns defined in CSV");
    }

    if (columnData_.empty()) {
        throw std::runtime_error("No data loaded from CSV");
    }

    for (const auto& header : headers_) {
        const std::string normalizedHeader = normalizeValue(header);
        if (columnData_.find(normalizedHeader) == columnData_.end()) {
            throw std::runtime_error("Missing data for column: " + header);
        }
    }
}

}  // namespace stc