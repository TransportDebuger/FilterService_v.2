/**
@file i_directory_monitor.hpp
@brief Абстрактный интерфейс монитора изменений в файловой системе.
@version 1.0.0
@date 2026-07-22
*/
#pragma once

#include <functional>
#include <memory>
#include <string>

namespace stc::fs {

/**
@class IDirectoryMonitor
@brief Инкапсулирует механизм обнаружения событий файловой системы.
*/
class IDirectoryMonitor {
 public:
  /// @brief Типы событий, генерируемых монитором.
  enum class Event {
    Created,  ///< Файл или директория были созданы.
    Deleted,  ///< Файл или директория были удалены.
    Modified,  ///< Содержимое файла было изменено.
    Renamed  ///< Файл или директория были переименованы.
  };

  /// @brief Сигнатура callback-функции для обработки событий.
  using Callback = std::function<void(Event, const std::string&)>;

  /// @brief Виртуальный деструктор.
  virtual ~IDirectoryMonitor() = default;

  /**
  @brief Запускает фоновый поток мониторинга.
  @throw std::runtime_error При невозможности инициализации системных
  дескрипторов.
  */
  virtual void Start() = 0;

  /**
  @brief Останавливает фоновый поток и освобождает системные ресурсы.
  */
  virtual void Stop() = 0;
};

}  // namespace stc::fs