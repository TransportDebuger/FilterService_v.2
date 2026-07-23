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
@private Внутренний класс, инкапсулирующий Linux-специфичную логику.
*/
class InotifyMonitor : public IDirectoryMonitor {
 public:
  /**
  @brief Конструктор монитора.
  @param[in] path Абсолютный путь к директории.
  @param[in] callback Функция-обработчик событий.
  @param[in] sys_calls Инъекция системных вызовов (для DI и тестирования).
  @throw std::invalid_argument Если sys_calls равен nullptr.
  */
  InotifyMonitor(const std::string& path, Callback callback,
                 std::shared_ptr<IFileSystemSystemCalls> sys_calls);

  ~InotifyMonitor() override;

  InotifyMonitor(const InotifyMonitor&) = delete;
  InotifyMonitor& operator=(const InotifyMonitor&) = delete;

  void Start() override;
  void Stop() override;

  /**
  @brief Возвращает указатель на исключение, если поток мониторинга завершился с
  ошибкой.
  @return std::exception_ptr Указатель на исключение или nullptr, если ошибок не
  было.
  */
  [[nodiscard]] std::exception_ptr GetException() const noexcept;

 private:
  void Run(std::stop_token stoken);

  std::string path_;
  Callback callback_;
  std::shared_ptr<IFileSystemSystemCalls> sys_calls_;
  int inotify_fd_{-1};
  int watch_descriptor_{-1};
  std::jthread worker_thread_;

  /// @private Мьютекс для защиты указателя на исключение.
  mutable std::mutex exception_mutex_;

  /// @private Указатель на необработанное исключение из фонового потока.
  std::exception_ptr exception_{nullptr};
};

}  // namespace stc::fs