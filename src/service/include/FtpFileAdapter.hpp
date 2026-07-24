/**
@file FtpFileAdapter.hpp
@brief Адаптер для работы с FTP хранилищами.
@version 2.0.0
@date 2026-07-17
*/
#pragma once

#include <curl/curl.h>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <memory>
#include <mutex>
#include <sstream>
#include <thread>
#include <vector>

#include "filestorageinterface.hpp"
#include "sourceconfig.hpp"
#include "stc/logger/ilogger.hpp"

namespace fs = std::filesystem;

namespace stc {

/**
@class FtpFileAdapter
@brief Адаптер для работы с FTP файловыми хранилищами.
*/
class FtpFileAdapter : public IFileStorage {
public:
    /**
    @brief Конструктор адаптера FTP.
    @param[in] config Конфигурация FTP-источника данных.
    @param[in] logger Диспетчер логирования.
    @throw std::invalid_argument При невалидной конфигурации или URL.
    */
    explicit FtpFileAdapter(const SourceConfig &config, 
                            std::shared_ptr<stc::logger::ILogger> logger);

    ~FtpFileAdapter() override;

    std::vector<std::string> listFiles(const std::string &path) override;
    void downloadFile(const std::string &remotePath, const std::string &localPath) override;
    void upload(const std::string &localPath, const std::string &remotePath) override;
    void connect() override;
    void disconnect() override;
    bool isConnected() const noexcept override;
    void startMonitoring() override;
    void stopMonitoring() override;
    bool isMonitoring() const noexcept override;
    void setCallback(FileDetectedCallback callback) override;

private:
    /// @private Структура для хранения данных ответа от сервера.
    struct CurlResponse {
        std::string data;
        size_t size;
    };

    /// @private Callback для записи данных от libcurl.
    static size_t writeCallback(void *contents, size_t size, size_t nmemb, CurlResponse *response);

    /// @private Callback для чтения данных для libcurl.
    static size_t readCallback(void *ptr, size_t size, size_t nmemb, FILE *stream);

    /// @private Проверяет доступность FTP-сервера.
    bool checkServerAvailability() const;

    /// @private Основной цикл мониторинга.
    void monitoringLoop();

    /// @private Парсит список файлов из ответа FTP LIST.
    std::vector<std::string> parseFileList(const std::string &listOutput) const;

    /// @private Проверяет соответствие файла маске.
    bool matchesFileMask(const std::string &filename) const;

    /// @private Формирует полный FTP URL.
    std::string buildFtpUrl(const std::string &path = "") const;

    /// @private Валидирует параметры FTP-подключения.
    void validateFtpConfig() const;

    /// @private Сравнивает текущий список файлов с предыдущим.
    void compareFilesList(const std::vector<std::string> &currentFiles);

    /// @private Конфигурация источника.
    SourceConfig config_;
    
    /// @private Базовый FTP URL.
    std::string ftpUrl_;
    
    /// @private Имя или IP сервера.
    std::string server_;
    
    /// @private Имя пользователя.
    std::string username_;
    
    /// @private Пароль.
    std::string password_;
    
    /// @private Порт FTP-сервера.
    int port_;
    
    /// @private Статус соединения.
    std::atomic<bool> connected_{false};
    
    /// @private Статус мониторинга.
    std::atomic<bool> monitoring_{false};
    
    /// @private Мьютекс для потокобезопасности.
    mutable std::mutex mutex_;
    
    /// @private Поток мониторинга.
    std::thread monitoringThread_;
    
    /// @private Предыдущий список файлов.
    std::vector<std::string> lastFilesList_;
    
    /// @private Интервал опроса.
    std::chrono::seconds pollingInterval_;
};

} // namespace stc