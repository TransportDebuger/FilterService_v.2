/**
 * @file ifilestorage.hpp
 * @brief Абстрактный интерфейс (порт) для работы с файловыми хранилищами.
 * @version 2.2.0
 * @date 2026-07-24
 */
#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>
#include <stdexcept>

namespace stc {

/**
 * @class IFileStorage
 * @brief Абстрактный интерфейс операций с файловым хранилищем.
 */
class IFileStorage {
public:
    /// @brief Тип callback-функции для уведомлений о новых файлах.
    using FileDetectedCallback = std::function<void(const std::string&)>;

    /// @brief Виртуальный деструктор.
    virtual ~IFileStorage() = default;

    /**
     * @brief Возвращает список файлов в заданной директории.
     * @param[in] path Путь к директории.
     * @return std::vector<std::string> Вектор полных путей к найденным файлам.
     * @throw std::invalid_argument При некорректном пути.
     * @throw std::runtime_error При ошибках доступа к файловой системе.
     */
    virtual std::vector<std::string> listFiles(const std::string& path) = 0;

    /**
     * @brief Скачивает файл из хранилища на локальный диск.
     * @param[in] remotePath Путь к файлу в хранилище.
     * @param[in] localPath Локальный путь для сохранения.
     * @throw std::invalid_argument При некорректных путях.
     * @throw std::runtime_error При ошибках сети или записи на диск.
     */
    virtual void downloadFile(const std::string& remotePath, const std::string& localPath) = 0;

    /**
     * @brief Загружает локальный файл в хранилище.
     * @param[in] localPath Локальный путь к файлу.
     * @param[in] remotePath Путь для сохранения в хранилище.
     * @throw std::invalid_argument При некорректных путях.
     * @throw std::runtime_error При ошибках сети или чтения с диска.
     */
    virtual void upload(const std::string& localPath, const std::string& remotePath) = 0;

    /**
     * @brief Устанавливает соединение с файловым хранилищем.
     * @throw std::runtime_error При невозможности установить соединение.
     */
    virtual void connect() = 0;

    /**
     * @brief Разрывает соединение с хранилищем.
     * @throw std::runtime_error При критических ошибках завершения соединения.
     */
    virtual void disconnect() = 0;

    /** 
     * @brief Проверяет текущее состояние соединения.
     * @return true Если соединение активно, иначе false.
     */
    virtual bool isConnected() const noexcept = 0;

    /**
     * @brief Запускает фоновый мониторинг изменений в хранилище.
     * @throw std::runtime_error Если соединение не установлено или мониторинг уже запущен.
     */
    virtual void startMonitoring() = 0;

    /// @brief Останавливает фоновый мониторинг изменений.
    virtual void stopMonitoring() = 0;

    /** 
     * @brief Проверяет, активен ли мониторинг.
     * @return true Если мониторинг активен, иначе false.
     */ 
    virtual bool isMonitoring() const noexcept = 0;

    /**
     * @brief Устанавливает коллбэк для уведомлений о новых файлах.
     * @param[in] callback Функция, вызываемая при обнаружении файла.
     */
    virtual void setCallback(FileDetectedCallback callback) = 0;

protected:
    /** 
     * @protected
     * @brief Базовая проверка корректности пути.
     * @param[in] path Путь к файлу или директории (строка).
     * @throw std::invalid_argument При пустом пути или наличии последовательности "..".
     */
    virtual void validatePath(const std::string& path) {
        if (path.empty() || path.find("..") != std::string::npos) {
            throw std::invalid_argument("Invalid path: " + path);
        }
    }

    /** 
     * @protected
     * @brief Коллбэк для уведомления о новых файлах (общее состояние для наследников).
     */
    FileDetectedCallback onFileDetected_;
};

} // namespace stc