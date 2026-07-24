/**
@file localstorageadapter.hpp
@brief Адаптер для работы с локальной файловой системой (включая точки монтирования SMB).
@version 2.1.0
@date 2026-07-24
*/
#pragma once

#include <atomic>
#include <filesystem>
#include <memory>
#include <mutex>
#include <string>
#include "ifilestorage.hpp"
#include "stc/config/sourceconfig.hpp"
#include "stc/fs/directory_monitor.hpp"
#include "stc/logger/ilogger.hpp"

namespace stc {

/**
@class LocalStorageAdapter
@brief Адаптер для локальной файловой системы с поддержкой событийного или опросного мониторинга.
*/
class LocalStorageAdapter : public IFileStorage {
public:
    /**
    @brief Конструктор адаптера локальной директории.
    @param[in] config Конфигурация источника.
    @param[in] logger Диспетчер логирования.
    */
    explicit LocalStorageAdapter(const SourceConfig& config,
                                 std::shared_ptr<stc::logger::ILogger> logger);
    ~LocalStorageAdapter() override;

    std::vector<std::string> listFiles(const std::string& path) override;
    void downloadFile(const std::string& remotePath, const std::string& localPath) override;
    void upload(const std::string& localPath, const std::string& remotePath) override;
    void connect() override;
    void disconnect() override;
    bool isConnected() const noexcept override;
    void startMonitoring() override;
    void stopMonitoring() override;
    bool isMonitoring() const noexcept override;
    void setCallback(FileDetectedCallback callback) override;

private:
    /// @private Проверяет доступность config_.path и создает директории.
    void ensurePathExists();
    
    /// @private Обрабатывает события stc::fs::IDirectoryMonitor.
    void handleFileEvent(stc::fs::IDirectoryMonitor::Event event, const std::string& filePath);
    
    /// @private Проверяет соответствие имени файла маске.
    bool matchesFileMask(const std::string& filename) const;
    
    /// @private Преобразует строковую стратегию из конфига в enum.
    stc::fs::DirectoryMonitor::MonitoringStrategy resolveStrategy() const;

    /// @private Конфигурация источника данных.
    SourceConfig config_;
    
    /// @private Объект мониторинга файловой системы из stc::fs.
    std::unique_ptr<stc::fs::IDirectoryMonitor> monitor_;
    
    /// @private Атомарный флаг состояния подключения.
    std::atomic<bool> connected_{false};
    
    /// @private Атомарный флаг состояния мониторинга.
    std::atomic<bool> monitoring_{false};
    
    /// @private Мьютекс для синхронизации многопоточного доступа.
    mutable std::mutex mutex_;
};

} // namespace stc