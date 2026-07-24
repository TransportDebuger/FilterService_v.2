/**
@file i_file_system_system_calls.hpp
@brief Интерфейс для абстрагирования системных вызовов файловой системы.
@version 1.0.0
@date 2026-07-22
*/
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

// Опережающее объявление структуры statfs для избежания включения системных
// заголовков в публичный API
struct statfs;

namespace stc::fs {

/**
@class IFileSystemSystemCalls
@brief Абстрагирует низкоуровневые системные вызовы (inotify, statfs) для
обеспечения тестируемости.
*/
class IFileSystemSystemCalls {
 public:
  /// @brief Виртуальный деструктор.
  virtual ~IFileSystemSystemCalls() = default;

  /**
  @brief Инициализирует механизм мониторинга (аналог inotify_init1).
  @return int Файловый дескриптор или -1 при ошибке.
  */
  virtual int Init() = 0;

  /**
  @brief Добавляет наблюдение за директорией (аналог inotify_add_watch).
  @param[in] fd Файловый дескриптор механизма мониторинга.
  @param[in] path Абсолютный путь к директории.
  @param[in] mask Битовая маска событий.
  @return int Дескриптор наблюдения (watch descriptor) или -1 при ошибке.
  */
  virtual int AddWatch(int fd, const std::string& path, uint32_t mask) = 0;

  /**
  @brief Читает буфер событий (аналог read для inotify_fd).
  @param[in] fd Файловый дескриптор.
  @param[out] buffer Указатель на буфер для чтения.
  @param[in] count Размер буфера.
  @return ssize_t Количество прочитанных байт или -1 при ошибке.
  */
  virtual ssize_t Read(int fd, void* buffer, std::size_t count) = 0;

  /**
  @brief Удаляет наблюдение за директорией (аналог inotify_rm_watch).
  @param[in] fd Файловый дескриптор механизма мониторинга.
  @param[in] wd Дескриптор наблюдения.
  @return int 0 при успехе, -1 при ошибке.
  */
  virtual int RemoveWatch(int fd, int wd) = 0;

  /**
    @brief Закрывает файловый дескриптор механизма мониторинга.
    @param[in] fd Дескриптор, ранее полученный от Init().
    @return int 0 при успешном выполнении или -1 при ошибке.
    */
  virtual int Close(int fd) = 0;

  /**
    @brief Получает информацию о файловой системе для указанного пути.
    @param[in] path Путь к файлу или директории внутри целевой файловой системы.
    @param[out] buf Указатель на структуру для приема метаданных файловой
    системы.
    @return int 0 при успешном выполнении или -1 при ошибке.
    */
  virtual int StatFs(const std::string& path, struct statfs* buf) = 0;
};

}  // namespace stc::fs