#pragma once

#include <vector>
#include <string>

// Базовый интерфейс адаптера
class FileStorageInterface {
    public:
        virtual ~FileStorageInterface() = default;
        // Получить список файлов
        virtual std::vector<std::string> listFiles(const std::string& path) = 0;
        // Скачать файл
        virtual void downloadFile(const std::string& remotePath, const std::string& localPath) = 0;
        // Проверить доступность хранилища
        virtual bool isAvailable() = 0;
        virtual void connect() = 0;
        virtual void upload(const std::string& localPath) = 0;
};