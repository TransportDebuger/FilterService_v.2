/**
@file directory_monitor.hpp
@brief Фабрика для создания мониторов файловой системы с автоматическим выбором
стратегии.
@version 1.1.0
@date 2026-07-23
*/
#pragma once

#include <chrono>
#include <memory>
#include <string>

#include "i_directory_monitor.hpp"
#include "i_file_system_system_calls.hpp"

namespace stc::fs {

/**
@class DirectoryMonitor
@brief Предоставляет статические методы для инстанцирования стратегий
мониторинга.
*/
class DirectoryMonitor {
 public:
  /**
  @brief Создает монитор, автоматически выбирая стратегию на основе типа
  файловой системы.
  @param[in] path Абсолютный путь к директории.
  @param[in] callback Функция-обработчик событий.
  @param[in] polling_interval Интервал опроса (используется только для
  PollingMonitor).
  @param[in] sys_calls Опциональная инъекция системных вызовов (для DI и
  тестирования).
  @return std::unique_ptr<IDirectoryMonitor> Умный указатель на созданный
  монитор.
  @throw std::runtime_error При невозможности определить тип ФС или
  инициализировать стратегию.
  @throw std::system_error При ошибках системных вызовов (statfs, inotify).
  */
  [[nodiscard]] static std::unique_ptr<IDirectoryMonitor> Create(
      const std::string& path, IDirectoryMonitor::Callback callback,
      std::chrono::seconds polling_interval = std::chrono::seconds(5),
      std::shared_ptr<IFileSystemSystemCalls> sys_calls = nullptr);
};

}  // namespace stc::fs