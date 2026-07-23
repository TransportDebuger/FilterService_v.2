/**
@file native_inotify_system_calls.hpp
@brief Нативная реализация системных вызовов для Linux (inotify, statfs).
@version 1.0.0
@date 2026-07-22
*/
#pragma once

#include "stc/fs/i_file_system_system_calls.hpp"

namespace stc::fs {

/**
@class NativeInotifySystemCalls
@brief Делегирует вызовы непосредственно в API ядра Linux.
@private Внутренний класс, не экспортируемый в публичный API.
*/
class NativeInotifySystemCalls : public IFileSystemSystemCalls {
 public:
  /// @brief Виртуальный деструктор.
  ~NativeInotifySystemCalls() override;

  /// @brief Инициализирует inotify (inotify_init1).
  int Init() override;

  /// @brief Добавляет наблюдение (inotify_add_watch).
  int AddWatch(int fd, const std::string& path, uint32_t mask) override;

  /// @brief Читает буфер событий (read).
  ssize_t Read(int fd, void* buffer, std::size_t count) override;

  /// @brief Удаляет наблюдение (inotify_rm_watch).
  int RemoveWatch(int fd, int wd) override;

  /// @brief Закрывает файловый дескриптор (close).
  int Close(int fd) override;

  /// @brief Получает информацию о ФС (statfs).
  int StatFs(const std::string& path, struct statfs* buf) override;
};

}  // namespace stc::fs