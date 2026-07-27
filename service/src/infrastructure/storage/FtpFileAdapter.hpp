/**
 * @file FtpFileAdapter.hpp
 * @brief Адаптер для работы с FTP-хранилищами на базе libcurl.
 * @version 2.0.0
 * @date 2026-07-24
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

#include "../../domain/ports/Ifilestorage.hpp"
#include "../../domain/sourceconfig.hpp"
#include "stc/logger/ilogger.hpp"

namespace fs = std::filesystem;

namespace stc {

/**
 * @class FtpFileAdapter
 * @brief Реализует интерфейс IFileStorage для сетевых FTP-ресурсов.
 */
class FtpFileAdapter : public IFileStorage {
public:
    /**
     * @brief Инициализирует адаптер FTP-соединения.
     * @param[in] config Строгая структура конфигурации источника.
     * @param[in] logger Диспетчер логирования.
     * @throw std::invalid_argument Если URL некорректен или отсутствуют обязательные параметры (username, password).
     */
    explicit FtpFileAdapter(const SourceConfig &config, 
                            std::shared_ptr<stc::logger::ILogger> logger);

    /// @brief Останавливает мониторинг, разрывает соединение и освобождает ресурсы libcurl.
    ~FtpFileAdapter() override;

    /**
     * @brief Возвращает список файлов в удаленной директории, соответствующих маске.
     * @param[in] path Относительный путь к директории на FTP-сервере.
     * @return std::vector<std::string> Вектор имен найденных файлов.
     * @throw std::runtime_error При ошибках инициализации CURL, сетевого взаимодействия или авторизации.
     */
    std::vector<std::string> listFiles(const std::string &path) override;

    /**
     * @brief Скачивает файл с FTP-сервера на локальный диск.
     * @param[in] remotePath Относительный путь к файлу на FTP-сервере.
     * @param[in] localPath Абсолютный или относительный путь для сохранения на локальном диске.
     * @throw std::invalid_argument При некорректных путях (базовая проверка validatePath).
     * @throw std::ios_base::failure При ошибках создания локального файла, записи на диск или разрыва соединения.
     */
    void downloadFile(const std::string &remotePath, const std::string &localPath) override;

    /**
     * @brief Загружает локальный файл на FTP-сервер.
     * @param[in] localPath Путь к существующему локальному файлу.
     * @param[in] remotePath Относительный путь для сохранения на FTP-сервере.
     * @throw std::invalid_argument При некорректных путях или отсутствии локального файла.
     * @throw std::runtime_error При ошибках получения размера файла (stat) или сетевого взаимодействия.
     */
    void upload(const std::string &localPath, const std::string &remotePath) override;
    
    /**
     * @brief Проверяет доступность FTP-сервера и устанавливает базовое соединение.
     * @throw std::runtime_error Если сервер недоступен или неверны учетные данные.
     */
    void connect() override;

    /// @brief Разрывает соединение с FTP-сервером.
    void disconnect() override;
    
    /**
     * @brief Проверяет статус соединения.
     * @return true Если базовое соединение установлено.
     */
    bool isConnected() const noexcept override;
    
    /**
     * @brief Запускает фоновый поток опроса (polling) FTP-директории.
     * @throw std::runtime_error Если соединение не установлено.
     */
    void startMonitoring() override;

    /// @brief Останавливает фоновый поток опроса.
    void stopMonitoring() override;

    /**
     * @brief Проверяет статус мониторинга.
     * @return true Если поток опроса активен.
     */
    bool isMonitoring() const noexcept override;

    /**
     * @brief Устанавливает коллбэк для асинхронных уведомлений о новых файлах.
     * @param[in] callback Функция, которая будет вызвана при обнаружении файла, соответствующего маске конфигурации источника.
     */
    void setCallback(FileDetectedCallback callback) override;

private:
    /**
     * @private
     * @brief Структура для агрегации данных ответа от libcurl.
     */
    struct CurlResponse {
        std::string data; ///< Буфер накопленных данных.
        size_t size;      ///< Текущий размер накопленных данных.
    };

    /**
     * @private
     * @brief Callback записи данных, получаемых от libcurl.
     * @param[in] contents Указатель на полученные данные.
     * @param[in] size Размер одного элемента.
     * @param[in] nmemb Количество элементов.
     * @param[in] response Указатель на структуру-приемник.
     * @return size_t Общее количество обработанных байт.
     */
   static size_t writeCallback(void *contents, size_t size, size_t nmemb, CurlResponse *response);

    /**
     * @private
     * @brief Callback чтения данных для отправки через libcurl.
     * @param[in] ptr Указатель на буфер для чтения.
     * @param[in] size Размер одного элемента.
     * @param[in] nmemb Количество элементов.
     * @param[in] stream Указатель на открытый файловый дескриптор.
     * @return size_t Количество успешно прочитанных байт.
     */
    static size_t readCallback(void *ptr, size_t size, size_t nmemb, FILE *stream);

    /// @private Проверяет доступность FTP-сервера посредством тестового LIST-запроса.
    bool checkServerAvailability() const;

    /// @private Основной цикл фонового потока опроса директории.
    void monitoringLoop();

    /**
     * @private
     * @brief Парсит текстовый ответ FTP LIST в вектор имен файлов.
     * @param[in] listOutput Сырой текстовый вывод команды LIST.
     * @return std::vector<std::string> Вектор имен файлов.
     */
    std::vector<std::string> parseFileList(const std::string &listOutput) const;

    /**
     * @private
     * @brief Проверяет соответствие имени файла заданной маске.
     * @param[in] filename Имя файла для проверки.
     * @return true Если имя соответствует маске.
     */
    bool matchesFileMask(const std::string &filename) const;

    /**
     * @private
     * @brief Формирует полный FTP URL для заданного пути.
     * @param[in] path Относительный путь на сервере.
     * @return std::string Полный URL вида ftp://server:port/path.
     */
    std::string buildFtpUrl(const std::string &path = "") const;

    /// @private Валидирует обязательные поля конфигурации FTP.
    void validateFtpConfig() const;

    /**
     * @private
     * @brief Сравнивает текущий список файлов с предыдущим снимком.
     * @param[in] currentFiles Текущий список файлов, полученный от сервера.
     */
    void compareFilesList(const std::vector<std::string> &currentFiles);

    std::shared_ptr<stc::logger::ILogger> logger_;
    
    /**
     * @private 
     * @brief Конфигурация источника данных.
     */
    SourceConfig config_;
    
    /**
     * @private 
     * @brief Базовый FTP URL (схема, хост, порт).
     */
    std::string ftpUrl_;
    
    /**
     * @private 
     * @brief Имя или IP-адрес FTP-сервера.
     */
    std::string server_;
    
    /**
     * @private 
     * @brief Имя пользователя для аутентификации.
     */
    std::string username_;
    
    /**
     * @private 
     * @brief Пароль для аутентификации.
     */
    std::string password_;
    
    /**
     * @private 
     * @brief Порт FTP-сервера.
     */
    int port_;
    
    /**
     * @private 
     * @brief Атомарный флаг состояния подключения.
     */
    std::atomic<bool> connected_{false};
    
    /**
     * @private 
     * @brief Атомарный флаг состояния мониторинга.
     */
    std::atomic<bool> monitoring_{false};
    
    /**
     * @private 
     * @brief Мьютекс для синхронизации доступа к состоянию и callback.
     */
    mutable std::mutex mutex_;
    
    /**
     * @private 
     * @brief Поток выполнения фонового опроса.
     */
    std::thread monitoringThread_;
    
    /**
     * @private 
     * @brief Предыдущий снимок списка файлов для вычисления дельты.
     */
    std::vector<std::string> lastFilesList_;
    
    /**
     * @private 
     * @brief Интервал опроса.
     */
    std::chrono::seconds pollingInterval_;
};

} // namespace stc