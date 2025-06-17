/**
 * @file FtpFileAdapter.hpp
 * @brief Адаптер для работы с FTP хранилищами
 * 
 * @details Реализует FileStorageInterface для FTP-серверов
 *          с периодическим опросом для мониторинга изменений
 */

#pragma once
#include "../include/filestorageinterface.hpp"
#include "../include/sourceconfig.hpp"
#include <curl/curl.h>
#include <filesystem>
#include <memory>
#include <atomic>
#include <mutex>
#include <thread>
#include <chrono>
#include <sstream>

namespace fs = std::filesystem;

/**
 * @class FtpFileAdapter
 * @brief Адаптер для работы с FTP файловыми хранилищами
 * 
 * @note Использует libcurl для FTP-операций и периодический опрос
 *       для мониторинга изменений в директориях FTP-сервера
 * @warning Требует установленной библиотеки libcurl
 */
class FtpFileAdapter : public FileStorageInterface {
public:
    /**
     * @brief Конструктор с конфигурацией источника
     * @param config Конфигурация FTP-источника данных
     * @throw std::invalid_argument При невалидной конфигурации
     */
    explicit FtpFileAdapter(const SourceConfig& config);
    
    /**
     * @brief Деструктор
     * @note Автоматически останавливает мониторинг и закрывает соединение
     */
    ~FtpFileAdapter() override;

    // Основные операции FileStorageInterface
    std::vector<std::string> listFiles(const std::string& path) override;
    void downloadFile(const std::string& remotePath, 
                     const std::string& localPath) override;
    void upload(const std::string& localPath,
               const std::string& remotePath) override;

    // Управление соединением
    void connect() override;
    void disconnect() override;
    bool isConnected() const noexcept override;

    // Мониторинг изменений
    void startMonitoring() override;
    void stopMonitoring() override;
    bool isMonitoring() const noexcept override;

    // Управление коллбэками
    void setCallback(FileDetectedCallback callback) override;

private:
    /**
     * @brief Структура для хранения данных ответа от сервера
     */
    struct CurlResponse {
        std::string data;
        size_t size;
    };
    
    /**
     * @brief Callback для записи данных от libcurl
     * @param contents Указатель на данные
     * @param size Размер элемента данных
     * @param nmemb Количество элементов
     * @param response Указатель на структуру ответа
     */
    static size_t writeCallback(void* contents, size_t size, size_t nmemb, CurlResponse* response);
    
    /**
     * @brief Callback для чтения данных для libcurl
     * @param ptr Указатель на буфер
     * @param size Размер элемента
     * @param nmemb Количество элементов
     * @param stream Файловый поток
     */
    static size_t readCallback(void* ptr, size_t size, size_t nmemb, FILE* stream);
    
    /**
     * @brief Проверяет доступность FTP-сервера
     * @return true если сервер доступен
     */
    bool checkServerAvailability() const;
    
    /**
     * @brief Основной цикл мониторинга
     */
    void monitoringLoop();
    
    /**
     * @brief Парсит список файлов из ответа FTP LIST
     * @param listOutput Вывод команды LIST
     * @return Вектор имен файлов
     */
    std::vector<std::string> parseFileList(const std::string& listOutput) const;
    
    /**
     * @brief Проверяет соответствие файла маске
     * @param filename Имя файла
     * @return true если файл соответствует маске
     */
    bool matchesFileMask(const std::string& filename) const;
    
    /**
     * @brief Формирует полный FTP URL
     * @param path Путь к файлу или директории
     * @return Полный FTP URL
     */
    std::string buildFtpUrl(const std::string& path = "") const;
    
    /**
     * @brief Валидирует параметры FTP-подключения
     * @throw std::invalid_argument При отсутствии обязательных параметров
     */
    void validateFtpConfig() const;
    
    /**
     * @brief Сравнивает текущий список файлов с предыдущим
     * @param currentFiles Текущий список файлов
     */
    void compareFilesList(const std::vector<std::string>& currentFiles);

    SourceConfig config_;                              ///< Конфигурация источника
    std::string ftpUrl_;                              ///< Базовый FTP URL
    std::string server_;                              ///< Имя или IP сервера
    std::string username_;                            ///< Имя пользователя
    std::string password_;                            ///< Пароль
    int port_;                                        ///< Порт FTP-сервера
    
    std::atomic<bool> connected_{false};              ///< Статус соединения
    std::atomic<bool> monitoring_{false};             ///< Статус мониторинга
    mutable std::mutex mutex_;                        ///< Мьютекс для потокобезопасности
    
    std::thread monitoringThread_;                    ///< Поток мониторинга
    std::vector<std::string> lastFilesList_;         ///< Предыдущий список файлов
    std::chrono::seconds pollingInterval_;           ///< Интервал опроса
};