/**
@file FilterListManager.cpp
@brief Реализация менеджера списков фильтрации на базе SQLite и nlohmann::json.
@version 4.0.0
@date 2026-10-24
*/
#include "FilterListManager.hpp"

#include <sqlite3.h>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <mutex> 

namespace fs = std::filesystem;

namespace stc {

FilterListManager::FilterListManager(std::shared_ptr<stc::logger::ILogger> logger)
    : logger_(std::move(logger)) {}

void FilterListManager::initialize(const std::string& db_path, int list_id) {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    
    if (db_path.empty()) {
        throw std::invalid_argument("SQLite DB path cannot be empty");
    }
    if (!fs::exists(db_path)) {
        throw std::runtime_error("SQLite DB file does not exist: " + db_path);
    }
    
    db_path_ = db_path;
    list_id_ = list_id;
    
    try {
        loadFromSqlite();
        initialized_.store(true);
        if (logger_) {
            logger_->Info("FilterListManager initialized from SQLite. DB: " + db_path_ + 
                          ", List ID: " + std::to_string(list_id_) + 
                          ", Records: " + std::to_string(total_records_count_.load()));
        }
    } catch (const std::exception& e) {
        initialized_.store(false);
        if (logger_) {
            logger_->Error("FilterListManager initialization failed: " + std::string(e.what()));
        }
        throw std::runtime_error("Failed to initialize FilterListManager: " + std::string(e.what()));
    }
}

void FilterListManager::hotReload() {
    if (!initialized_.load()) {
        if (logger_) logger_->Warning("hotReload called but manager is not initialized.");
        return;
    }
    
    if (logger_) logger_->Info("Starting hot-reload of comparison lists from SQLite...");
    
    try {
        // loadFromSqlite инкапсулирует логику RCU и сам захватывает unique_lock для подмены
        loadFromSqlite();
        if (logger_) {
            logger_->Info("Hot-reload completed successfully. Total records: " + 
                          std::to_string(total_records_count_.load()));
        }
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("Hot-reload failed: " + std::string(e.what()) + ". Keeping old data in memory.");
        }
    }
}

void FilterListManager::loadFromSqlite() {
    sqlite3* db = nullptr;
    // Открываем строго в режиме READONLY. Это позволяет Flask-приложению (Writer) 
    // продолжать записи в WAL-файл без блокировок.
    int rc = sqlite3_open_v2(db_path_.c_str(), &db, SQLITE_OPEN_READONLY, nullptr);
    if (rc != SQLITE_OK) {
        std::string err = sqlite3_errmsg(db);
        sqlite3_close(db);
        throw std::runtime_error("Failed to open SQLite DB: " + err);
    }

    const char* sql = "SELECT data FROM comparison_records WHERE list_id = ? AND is_deleted = 0";
    sqlite3_stmt* stmt = nullptr;
    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        std::string err = sqlite3_errmsg(db);
        sqlite3_close(db);
        throw std::runtime_error("Failed to prepare SQL statement: " + err);
    }

    sqlite3_bind_int(stmt, 1, list_id_);

    // Временный контейнер для новых данных (Фаза Copy в паттерне RCU)
    std::unordered_map<std::string, std::unordered_set<std::string>> tempData;
    size_t record_count = 0;

    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        const unsigned char* text = sqlite3_column_text(stmt, 0);
        if (!text) continue;

        try {
            std::string json_str(reinterpret_cast<const char*>(text));
            auto json_obj = nlohmann::json::parse(json_str);

            for (auto& [key, value] : json_obj.items()) {
                std::string norm_key = normalizeValue(key);
                // Приводим значение к строке (поддержка строк, чисел и т.д. из JSON)
                std::string val_str = value.is_string() ? value.get<std::string>() : value.dump();
                std::string norm_val = normalizeValue(val_str);
                
                tempData[norm_key].insert(norm_val);
            }
            record_count++;
        } catch (const nlohmann::json::exception& e) {
            if (logger_) {
                logger_->Warning("Failed to parse JSON record in SQLite: " + std::string(e.what()));
            }
            // Пропускаем битую запись, не прерываем весь reload (Fail-Safe)
        }
    }

    sqlite3_finalize(stmt);
    sqlite3_close(db);

    if (rc != SQLITE_DONE) {
        throw std::runtime_error("Error during SQL execution");
    }

    // Фаза Update в паттерне RCU: атомарная подмена данных под эксклюзивной блокировкой
    {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        columnData_ = std::move(tempData);
    }
    
    total_records_count_.store(record_count, std::memory_order_relaxed);
}

bool FilterListManager::contains(const std::string& column, const std::string& value) const {
    if (!initialized_.load()) {
        throw std::runtime_error("FilterListManager not initialized");
    }

    // Захватываем shared_lock. Множество воркеров могут читать данные параллельно.
    std::shared_lock<std::shared_mutex> lock(mutex_);
    
    std::string norm_col = normalizeValue(column);
    auto columnIt = columnData_.find(norm_col);
    if (columnIt == columnData_.end()) {
        return false; // В текущем списке нет такой колонки
    }

    std::string norm_val = normalizeValue(value);
    return columnIt->second.find(norm_val) != columnIt->second.end();
}

bool FilterListManager::isInitialized() const noexcept {
    return initialized_.load();
}

size_t FilterListManager::getTotalRecordsCount() const noexcept {
    return total_records_count_.load(std::memory_order_relaxed);
}

std::string FilterListManager::normalizeValue(const std::string& value) {
    // Логика нормализации (trim, lowercase, UTF-8 Cyrillic handling) 
    // остается идентичной предыдущей версии для сохранения обратной совместимости
    // с логикой XMLProcessor.
    std::string result;
    result.reserve(value.size());
    bool inWhitespace = false;
    bool hasContent = false;

    for (size_t i = 0; i < value.size();) {
        const unsigned char c = static_cast<unsigned char>(value[i]);
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
            if (hasContent) inWhitespace = true;
            i++;
        } else if (i + 1 < value.size() && c == 0xC2 && static_cast<unsigned char>(value[i + 1]) == 0xA0) {
            if (hasContent) inWhitespace = true;
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
                const unsigned char second = static_cast<unsigned char>(value[i + 1]);
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

} // namespace stc