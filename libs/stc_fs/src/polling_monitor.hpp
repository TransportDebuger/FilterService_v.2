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
@private Внутренний класс, используемый для файловых систем, не поддерживающих
inotify.
*/
class PollingMonitor : public IDirectoryMonitor {
 public:
  /**
  @brief Конструктор монитора.
  @param[in] path Абсолютный путь к директории.
  @param[in] callback Функция-обработчик событий.
  @param[in] polling_interval Интервал между опросами.
  @throw std::runtime_error Если путь не существует или не является директорией.
  */
  PollingMonitor(const std::string& path, Callback callback,
                 std::chrono::seconds polling_interval);

  ~PollingMonitor() override;

  PollingMonitor(const PollingMonitor&) = delete;
  PollingMonitor& operator=(const PollingMonitor&) = delete;

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
  std::chrono::seconds polling_interval_;
  std::jthread worker_thread_;
  std::unordered_map<std::string, std::filesystem::file_time_type> known_files_;

  /// @private Мьютекс для защиты указателя на исключение.
  mutable std::mutex exception_mutex_;

  /// @private Указатель на необработанное исключение из фонового потока.
  std::exception_ptr exception_{nullptr};
};

}  // namespace stc::fs