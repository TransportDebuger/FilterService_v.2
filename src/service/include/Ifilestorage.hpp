/**
@file ifilestorage.hpp
@brief Абстрактный интерфейс для работы с файловыми хранилищами.
@version 2.1.0
@date 2026-07-24
*/
#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>
#include "stc/logger/ilogger.hpp"

namespace stc {

/**
@class IFileStorage
@brief Интерфейс операций с файловым хранилищем.
*/
class IFileStorage {
public:
    /// @brief Тип callback-функции для обнаруженных файлов.
    using FileDetectedCallback = std::function<void(const std::string&)>;

    /// @brief Виртуальный деструктор.
    virtual ~IFileStorage() = default;

    /// @brief Возвращает список файлов в заданной директории.
    virtual std::vector<std::string> listFiles(const std::string& path) = 0;

    /// @brief Скачивает файл из хранилища на локальный диск.
    virtual void downloadFile(const std::string& remotePath, const std::string& localPath) = 0;

    /// @brief Загружает локальный файл в хранилище.
    virtual void upload(const std::string& localPath, const std::string& remotePath) = 0;

    /// @brief Устанавливает соединение с файловым хранилищем.
    virtual void connect() = 0;

    /// @brief Разрывает соединение с хранилищем.
    virtual void disconnect() = 0;

    /// @brief Проверяет текущее состояние соединения.
    virtual bool isConnected() const noexcept = 0;

    /// @brief Запускает фоновый мониторинг изменений в хранилище.
    virtual void startMonitoring() = 0;

    /// @brief Останавливает фоновый мониторинг изменений.
    virtual void stopMonitoring() = 0;

    /// @brief Проверяет, активен ли мониторинг.
    virtual bool isMonitoring() const noexcept = 0;

    /// @brief Устанавливает коллбэк для уведомлений о новых файлах.
    virtual void setCallback(FileDetectedCallback callback) = 0;

protected:
    /**
    @brief Базовая проверка корректности пути.
    @param[in] path Строка с путём к файлу или директории.
    @throw std::invalid_argument При пустом или некорректном пути.
    */
    virtual void validatePath(const std::string& path) {
        if (path.empty() || path.find("..") != std::string::npos) {
            throw std::invalid_argument("Invalid path: " + path);
        }
    }

    /// @private Диспетчер логирования, полученный через DI.
    std::shared_ptr<stc::logger::ILogger> logger_;

    /// @private Коллбэк для уведомления о новых файлах.
    FileDetectedCallback onFileDetected_;
};

} // namespace stc