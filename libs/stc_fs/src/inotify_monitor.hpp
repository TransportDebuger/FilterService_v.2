/**
@file inotify_monitor.hpp
@brief Событийный монитор на базе inotify.
@version 1.1.2
@date 2026-07-22
*/
#pragma once

#include <exception>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

#include "stc/fs/i_directory_monitor.hpp"
#include "stc/fs/i_file_system_system_calls.hpp"

namespace stc::fs {

/**
@class InotifyMonitor
@brief Обеспечивает мгновенное обнаружение событий через inotify.
@internal Внутренний класс, инкапсулирующий Linux-специфичную логику.
*/
class InotifyMonitor : public IDirectoryMonitor {
 public:
  /**
    @brief Конструктор монитора.
    @param[in] path Путь к наблюдаемой директории.
    @param[in] callback Функция-обработчик событий.
    @param[in] sys_calls Инъекция системных вызовов (для DI и тестирования).
    @throw std::invalid_argument Если sys_calls равен nullptr.
    */
  InotifyMonitor(const std::string& path, Callback callback,
                 std::shared_ptr<IFileSystemSystemCalls> sys_calls);

  /// @brief Деструктор. Гарантирует остановку потока и освобождение ресурсов.
  ~InotifyMonitor() override;

  InotifyMonitor(const InotifyMonitor&) = delete;
  InotifyMonitor& operator=(const InotifyMonitor&) = delete;

  /**
    @brief Запускает фоновый поток событийного мониторинга.
    @throw std::system_error При ошибках инициализации механизма мониторинга или
    регистрации наблюдения.
    */
  void Start() override;

  /// @brief Останавливает фоновый поток мониторинга и освобождает системные
  /// ресурсы.
  void Stop() override;

  /**
    @brief Извлекает состояние фонового потока на наличие фатальных ошибок.
    @return std::exception_ptr Указатель на перехваченное исключение или
    nullptr, если поток работает штатно.
    */
  [[nodiscard]] std::exception_ptr GetException() const noexcept;

 private:
  /**
    @brief Основной цикл обработки событий inotify.
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

  /// @private Интерфейс системных вызовов, внедряемый для реализации паттерна
  /// инверсии зависимостей (DI) и обеспечения 100% тестируемости.
  std::shared_ptr<IFileSystemSystemCalls> sys_calls_;

  /// @private Файловый дескриптор экземпляра inotify. Значение -1 является
  /// инвариантом, указывающим на неинициализированное или корректно
  /// остановленное состояние.
  int inotify_fd_{-1};

  /// @private Дескриптор наблюдения (watch descriptor), возвращаемый системным
  /// вызовом регистрации пути. Используется для точечного удаления наблюдения.
  int watch_descriptor_{-1};

  /// @private Фоновый поток обработки событий на базе std::jthread. Гарантирует
  /// поддержку кооперативной остановки (std::stop_token) и автоматический join
  /// в деструкторе.
  std::jthread worker_thread_;

  /// @private Мьютекс для обеспечения потокобезопасного доступа (чтения и
  /// записи) к указателю на исключение из разных потоков.
  mutable std::mutex exception_mutex_;

  /// @private Указатель на исключение, перехваченное в фоновом потоке.
  /// Предотвращает вызов std::terminate и позволяет основному потоку извлечь
  /// ошибку через GetException().
  std::exception_ptr exception_{nullptr};
};

}  // namespace stc::fs