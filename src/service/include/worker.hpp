/**
 * @file Worker.hpp
 * @brief Класс для обработки файлов из различных типов хранилищ
 *
 * @details Реализует автономную единицу обработки одного источника данных
 *          с поддержкой локальных, SMB и FTP хранилищ через адаптеры
 */

#pragma once
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <filesystem>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

#include "../include/AdapterFactory.hpp"
#include "../include/filestorageinterface.hpp"
#include "../include/sourceconfig.hpp"
#include "stc/MetricsCollector.hpp"
#include "stc/compositelogger.hpp"

namespace fs = std::filesystem;

/**
 * @class Worker
 * @brief Автономная единица обработки файлов из источника данных
 *
 * @note Использует адаптеры для работы с различными типами хранилищ
 *       и обеспечивает потокобезопасное управление жизненным циклом [25][28]
 */
class Worker {
 public:
  /**
   * @brief Конструктор с конфигурацией источника
   * @param config Конфигурация источника данных
   * @throw std::runtime_error При ошибках создания адаптера
   */
  explicit Worker(const SourceConfig &config);

  /**
   * @brief Деструктор
   * @note Автоматически останавливает Worker и освобождает ресурсы
   */
  ~Worker();

  // Основные методы управления жизненным циклом
  void start();
  void stop();
  void pause();
  void resume();
  void restart();
  void stopGracefully();

  // Методы проверки состояния
  bool isAlive() const noexcept;
  bool isRunning() const noexcept;
  bool isPaused() const noexcept;

  // Доступ к конфигурации
  const SourceConfig &getConfig() const noexcept { return config_; }

 private:
  /**
   * @brief Основной цикл обработки файлов
   */
  void run();

  /**
   * @brief Обработка одного файла
   * @param filePath Путь к файлу для обработки
   */
  void processFile(const std::string &filePath);

  /**
   * @brief Валидация путей директорий
   * @throw std::runtime_error При недоступности путей
   */
  void validatePaths() const;

  /**
   * @brief Вычисление хеша файла
   * @param filePath Путь к файлу
   * @return std::string SHA256 хеш файла
   */
  std::string getFileHash(const std::string &filePath) const;

  /**
   * @brief Получение имени отфильтрованного файла
   * @param originalPath Исходный путь к файлу
   * @return std::string Путь к отфильтрованному файлу
   */
  std::string getFilteredFilePath(const std::string &originalPath) const;

  /**
   * @brief Перемещение файла в директорию обработанных
   * @param filePath Путь к обработанному файлу
   * @param processedPath Путь назначения
   */
  void moveToProcessed(const std::string &filePath,
                       const std::string &processedPath);

  /**
   * @brief Обработка ошибок файла
   * @param filePath Путь к файлу с ошибкой
   * @param error Описание ошибки
   */
  void handleFileError(const std::string &filePath, const std::string &error);

  // Конфигурация и идентификация
  SourceConfig config_;  ///< Конфигурация источника
  pid_t pid_;  ///< PID процесса для идентификации
  std::string workerTag_;  ///< Тег воркера для логирования
  static std::atomic<int> instanceCounter_;
  // Адаптер хранилища
  std::unique_ptr<FileStorageInterface>
      adapter_;  ///< Адаптер файлового хранилища

  // Состояние воркера [28]
  std::atomic<bool> running_{false};  ///< Флаг активности
  std::atomic<bool> paused_{false};   ///< Флаг паузы
  std::atomic<bool> processing_{false};  ///< Флаг обработки файла

  // Многопоточность [24][25]
  std::thread worker_thread_;       ///< Рабочий поток
  mutable std::mutex state_mutex_;  ///< Мьютекс состояния
  std::condition_variable cv_;  ///< Условная переменная для паузы

  // Метрики
  std::chrono::steady_clock::time_point start_time_;  ///< Время запуска
  std::atomic<size_t> files_processed_{0};  ///< Количество обработанных файлов
  std::atomic<size_t> files_failed_{0};  ///< Количество файлов с ошибками
};