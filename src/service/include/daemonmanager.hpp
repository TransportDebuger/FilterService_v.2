/**
 * @file DaemonManager.hpp
 * @brief Управление демонизацией процесса и PID-файлами
 */

#pragma once
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <atomic>
#include <mutex>
#include <stdexcept>
#include <string>

/**
 * @class DaemonManager
 * @brief Класс для управления демонизацией и PID-файлами
 *
 * @note Обеспечивает:
 * - Двойной fork для демонизации
 * - Создание/удаление PID-файла
 * - Проверку существующих процессов
 * - Потокобезопасные операции
 */
class DaemonManager {
 public:
  /**
   * @param pid_file Путь к PID-файлу
   * @param check_existing Проверять существующий процесс при инициализации
   */
  explicit DaemonManager(const std::string& pid_file,
                         bool check_existing = true);

  ~DaemonManager();

  /**
   * @brief Превращает текущий процесс в демона
   * @throw std::runtime_error При ошибках демонизации
   */
  void daemonize();

  /**
   * @brief Записывает текущий PID в файл
   * @throw std::system_error При ошибках записи
   */
  void writePid();

  /**
   * @brief Удаляет PID-файл если был создан
   */
  void cleanup() noexcept;

 private:
  void checkExistingProcess();
  void removeStalePid() noexcept;

  std::string pid_path_;
  std::mutex pid_mutex_;
  std::atomic<bool> pid_written_{false};
  static constexpr mode_t PID_FILE_MODE = 0644;
};