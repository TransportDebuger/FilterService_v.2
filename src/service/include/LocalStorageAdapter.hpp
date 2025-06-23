/**
 * @file LocalStorageAdapter.hpp
 * @brief Адаптер для работы с локальной файловой системой
 *
 * @details Реализует FileStorageInterface для локальных директорий
 *          с интеграцией FileWatcher для событийного мониторинга
 */

#pragma once
#include "../include/filestorageinterface.hpp"
#include "../include/filewatcher.hpp"
#include "../include/sourceconfig.hpp"
#include <atomic>
#include <filesystem>
#include <memory>
#include <mutex>

namespace fs = std::filesystem;

/**
 * @class LocalStorageAdapter
 * @brief Адаптер для работы с локальной файловой системой
 *
 * @note Использует FileWatcher для событийного мониторинга изменений
 *       в директории и поддерживает SMB/CIFS через смонтированные ресурсы
 */
class LocalStorageAdapter : public FileStorageInterface {
public:
  /**
   * @brief Конструктор с конфигурацией источника
   * @param config Конфигурация источника данных
   * @throw std::invalid_argument При невалидной конфигурации
   */
  explicit LocalStorageAdapter(const SourceConfig &config);

  /**
   * @brief Деструктор
   * @note Автоматически останавливает мониторинг и освобождает ресурсы
   */
  ~LocalStorageAdapter() override;

  // Основные операции FileStorageInterface
  std::vector<std::string> listFiles(const std::string &path) override;
  void downloadFile(const std::string &remotePath,
                    const std::string &localPath) override;
  void upload(const std::string &localPath,
              const std::string &remotePath) override;

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
   * @brief Проверяет доступность пути и создает недостающие директории
   * @throw std::runtime_error При недоступности пути
   */
  void ensurePathExists();

  /**
   * @brief Обработчик событий FileWatcher
   * @param event Тип события
   * @param filePath Путь к файлу
   */
  void handleFileEvent(FileWatcher::Event event, const std::string &filePath);

  /**
   * @brief Проверяет соответствие файла маске
   * @param filename Имя файла
   * @return true если файл соответствует маске
   */
  bool matchesFileMask(const std::string &filename) const;

  SourceConfig config_; ///< Конфигурация источника
  std::unique_ptr<FileWatcher> watcher_; ///< Монитор файловой системы
  std::atomic<bool> connected_{false};  ///< Статус соединения
  std::atomic<bool> monitoring_{false}; ///< Статус мониторинга
  mutable std::mutex mutex_; ///< Мьютекс для потокобезопасности
};