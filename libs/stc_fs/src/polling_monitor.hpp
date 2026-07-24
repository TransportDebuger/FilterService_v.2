/**
@file polling_monitor.hpp
@brief Универсальный монитор на базе опроса директории (polling).
@version 1.1.0
@date 2026-07-22
*/
#pragma once

#include <chrono>
#include <filesystem>
#include <string>
#include <thread>
#include <unordered_map>

#include "stc/fs/i_directory_monitor.hpp"

namespace stc::fs {

/**
@class PollingMonitor
@brief Обеспечивает обнаружение событий путем периодического опроса директории.
@internal Внутренний класс, не экспортируемый в публичный API.
*/
class PollingMonitor : public IDirectoryMonitor {
 public:
  /**
    @brief Конструктор монитора.
    @param[in] path Абсолютный путь к директории.
    @param[in] callback Функция-обработчик событий.
    @param[in] polling_interval Интервал между опросами.
    @throw std::runtime_error Если путь не существует или не является
    директорией.
    */
  PollingMonitor(const std::string& path, Callback callback,
                 std::chrono::seconds polling_interval);

  /// @brief Деструктор. Гарантирует освобождение системных ресурсов и остановку
  /// фонового потока.
  ~PollingMonitor() override;

  PollingMonitor(const PollingMonitor&) = delete;
  PollingMonitor& operator=(const PollingMonitor&) = delete;

  /**
    @brief Запускает фоновый поток периодического опроса директории.
    @throw std::system_error При ошибках создания или запуска фонового потока.
    */
  void Start() override;

  /// @brief Останавливает фоновый поток периодического опроса.
  void Stop() override;

  /**
    @brief Извлекает состояние фонового потока на наличие фатальных ошибок.
    @return std::exception_ptr Указатель на перехваченное исключение или
    nullptr, если поток работает штатно.
    */
  [[nodiscard]] std::exception_ptr GetException() const noexcept;

 private:
  /**
    @brief Основной цикл периодического опроса и сравнения состояния директории.
    @param[in] stoken Токен кооперативной остановки потока.
    @private Внутренний метод, вызываемый из фонового std::jthread.
    */
  void Run(std::stop_token stoken);

  /// @private Абсолютный путь к наблюдаемой директории. Нормализуется в
  /// конструкторе для исключения зависимости от текущего рабочего каталога
  /// (CWD).
  std::string path_;

  /// @private Пользовательская функция обратного вызова для асинхронной
  /// обработки событий файловой системы.
  Callback callback_;

  /// @private Интервал времени между последовательными итерациями опроса
  /// состояния директории.
  std::chrono::seconds polling_interval_;

  /// @private Фоновый поток периодического опроса на базе std::jthread.
  /// Гарантирует поддержку кооперативной остановки (std::stop_token) и
  /// автоматический join в деструкторе.
  std::jthread worker_thread_;

  /// @private Снимок (snapshot) состояния директории, хранящий пути и время
  /// последней модификации файлов. Используется для вычисления дельты событий
  /// (Created, Modified, Deleted) между итерациями опроса.
  std::unordered_map<std::string, std::filesystem::file_time_type> known_files_;

  /// @private Мьютекс для обеспечения потокобезопасного доступа (чтения и
  /// записи) к указателю на исключение из разных потоков.
  mutable std::mutex exception_mutex_;

  /// @private Указатель на исключение, перехваченное в фоновом потоке
  /// (например, std::filesystem::filesystem_error). Предотвращает вызов
  /// std::terminate и позволяет основному потоку извлечь ошибку через
  /// GetException().
  std::exception_ptr exception_{nullptr};
};

}  // namespace stc::fs