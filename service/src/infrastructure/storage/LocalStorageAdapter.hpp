/**
 * @file localstorageadapter.hpp
 * @brief Адаптер для работы с локальной файловой системой и точками монтирования SMB.
 * @version 2.2.0
 * @date 2026-07-24
 */
#pragma once

#include <atomic>
#include <filesystem>
#include <memory>
#include <mutex>
#include <string>

#include "../../domain/ports/Ifilestorage.hpp"
#include "../../domain/sourceconfig.hpp"
#include "stc/fs/directory_monitor.hpp"
#include "stc/logger/ilogger.hpp"

namespace stc {

/**
 * @class LocalStorageAdapter
 * @brief Реализует интерфейс IFileStorage для локальных POSIX-путей и SMB-шар.
 */
class LocalStorageAdapter : public IFileStorage {
public:
    /**
     * @brief Конструктор адаптера локальной файловой системы.
     * @param[in] config Строгая структура конфигурации источника.
     * @param[in] logger Диспетчер логирования.
     * @throw std::invalid_argument Если путь пуст или маска файлов не задана.
     */
    explicit LocalStorageAdapter(const SourceConfig& config,
                                 std::shared_ptr<stc::logger::ILogger> logger);
    
    /// @brief Виртуальный деструктор. Останавливает мониторинг и освобождает ресурсы.
    ~LocalStorageAdapter() override;

    /**
     * @brief Возвращает список файлов, соответствующих маске из конфигурации.
     * @param[in] path Абсолютный путь к директории.
     * @return std::vector<std::string> Вектор полных путей к найденным файлам.
     * @throw std::runtime_error При ошибках доступа к файловой системе.
     */
    std::vector<std::string> listFiles(const std::string& path) override;
    
    /**
     * @brief Копирует файл из remotePath в localPath.
     * @param[in] remotePath Путь к исходному файлу.
     * @param[in] localPath Путь для сохранения копии.
     * @throw std::invalid_argument Если исходный файл не существует.
     * @throw std::ios_base::failure При ошибках копирования или создания директорий.
     */
    void downloadFile(const std::string& remotePath, const std::string& localPath) override;
    
    /**
     * @brief Копирует файл из localPath в remotePath.
     * @param[in] localPath Путь к исходному файлу.
     * @param[in] remotePath Путь для сохранения копии.
     * @throw std::invalid_argument Если локальный файл не существует.
     * @throw std::runtime_error При ошибках копирования или создания директорий.
     */
    void upload(const std::string& localPath, const std::string& remotePath) override;
    
    /**
     * @brief Проверяет существование директории и рекурсивно создает её при необходимости.
     * @throw std::runtime_error Если путь не является директорией или недоступен.
     */
    void connect() override;
    
    /// @brief Разрывает соединение и останавливает мониторинг.
    void disconnect() override;

    /**
     * @brief Проверяет статус соединения.
     * @return true Если директория доступна и соединение установлено.
     */
    bool isConnected() const noexcept override;

    /**
     * @brief Запускает мониторинг директории через stc::fs::DirectoryMonitor.
     * @throw std::runtime_error Если соединение не установлено, мониторинг уже запущен или стратегия не поддерживается.
     */
    void startMonitoring() override;

    /// @brief Останавливает мониторинг и освобождает дескрипторы stc::fs.
    void stopMonitoring() override;

    /**
     * @brief Проверяет статус мониторинга.
     * @return true Если поток мониторинга активен.
     */
    bool isMonitoring() const noexcept override;

    /**
     * @brief Устанавливает callback для уведомлений о новых файлах.
     * @param[in] callback Функция, вызываемая при обнаружении файла.
     */
    void setCallback(FileDetectedCallback callback) override;

private:
    /**
     * @private
     * @brief Проверяет доступность config_.path и создает директории.
     * @throw std::runtime_error Если создание директории невозможно или путь указывает на файл.
     */
    void ensurePathExists();
    
    /**
     * @private
     * @brief Обрабатывает события от stc::fs::IDirectoryMonitor.
     * @param[in] event Тип произошедшего события.
     * @param[in] filePath Абсолютный путь к файлу.
     */
    void handleFileEvent(stc::fs::IDirectoryMonitor::Event event, const std::string& filePath);
    
    /**
     * @private
     * @brief Проверяет соответствие имени файла заданной маске (glob-синтаксис).
     * @param[in] filename Имя файла без пути.
     * @return true Если имя соответствует маске.
     */
    bool matchesFileMask(const std::string& filename) const;
    
    /**
     * @private
     * @brief Транслирует строковую стратегию мониторинга из конфигурации в перечисление.
     * @return stc::fs::DirectoryMonitor::MonitoringStrategy Выбранная стратегия.
     */
    stc::fs::DirectoryMonitor::MonitoringStrategy resolveStrategy() const;

    /**
     * @private 
     * @brief Конфигурация источника данных.
     */
    SourceConfig config_;
    
    /**
     * @private 
     * @brief Инкапсулированный объект мониторинга файловой системы stc::fs.
     */
    std::unique_ptr<stc::fs::IDirectoryMonitor> monitor_;

    std::shared_ptr<stc::logger::ILogger> logger_;
    
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
     * @brief Мьютекс для синхронизации многопоточного доступа.
     */
    mutable std::mutex mutex_;
};

} // namespace stc