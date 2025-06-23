/**
 * @file SmbFileAdapter.hpp
 * @brief Адаптер для работы с SMB/CIFS хранилищами
 *
 * @details Реализует FileStorageInterface для SMB/CIFS ресурсов
 *          с автоматическим монтированием и интеграцией FileWatcher
 */

#pragma once
#include "../include/filestorageinterface.hpp"
#include "../include/filewatcher.hpp"
#include "../include/sourceconfig.hpp"
#include <atomic>
#include <chrono>
#include <filesystem>
#include <memory>
#include <mutex>
#include <thread>

namespace fs = std::filesystem;

/**
 * @class SmbFileAdapter
 * @brief Адаптер для работы с SMB/CIFS файловыми хранилищами
 *
 * @note Использует системное монтирование через mount.cifs и FileWatcher
 *       для событийного мониторинга изменений в смонтированной директории
 * @warning Требует установленного пакета cifs-utils и прав для монтирования
 */
class SmbFileAdapter : public FileStorageInterface {
public:
  /**
   * @brief Конструктор с конфигурацией источника
   * @param config Конфигурация SMB-источника данных
   * @throw std::invalid_argument При невалидной конфигурации
   */
  explicit SmbFileAdapter(const SourceConfig &config);

  /**
   * @brief Деструктор
   * @note Автоматически останавливает мониторинг и размонтирует ресурс
   */
  ~SmbFileAdapter() override;

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
   * @brief Монтирует SMB-ресурс в локальную директорию
   * @throw std::runtime_error При ошибках монтирования
   */
  void mountSmbResource();

  /**
   * @brief Размонтирует SMB-ресурс
   * @note Безопасно обрабатывает случаи, когда ресурс уже размонтирован
   */
  void unmountSmbResource();

  /**
   * @brief Проверяет доступность SMB-сервера
   * @return true если сервер доступен
   */
  bool checkServerAvailability() const;

  /**
   * @brief Создает временную точку монтирования
   * @return Путь к созданной точке монтирования
   */
  std::string createMountPoint();

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

  /**
   * @brief Формирует команду монтирования
   * @return Строка с командой mount.cifs
   */
  std::string buildMountCommand() const;

  /**
   * @brief Валидирует параметры SMB-подключения
   * @throw std::invalid_argument При отсутствии обязательных параметров
   */
  void validateSmbConfig() const;

  SourceConfig config_; ///< Конфигурация источника
  std::string mountPoint_; ///< Точка монтирования SMB-ресурса
  std::string smbUrl_; ///< URL SMB-ресурса (//server/share)
  std::unique_ptr<FileWatcher> watcher_; ///< Монитор файловой системы
  std::atomic<bool> connected_{false};  ///< Статус соединения
  std::atomic<bool> monitoring_{false}; ///< Статус мониторинга
  std::atomic<bool> mounted_{false}; ///< Статус монтирования
  mutable std::mutex mutex_; ///< Мьютекс для потокобезопасности

  // SMB-специфичные параметры
  std::string username_; ///< Имя пользователя
  std::string password_; ///< Пароль
  std::string domain_;   ///< Домен (workgroup)
  std::string server_;   ///< Имя или IP сервера
  std::string share_;    ///< Имя общего ресурса
};