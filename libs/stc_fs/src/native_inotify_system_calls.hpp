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

  /**
    @brief Инициализирует низкоуровневый механизм мониторинга файловой системы.
    @return int Дескриптор механизма мониторинга или -1 при ошибке.
    */
  int Init() override;

  /**
    @brief Регистрирует наблюдение за изменениями в указанной директории.
    @param[in] fd Дескриптор механизма мониторинга.
    @param[in] path Абсолютный путь к наблюдаемой директории.
    @param[in] mask Битовая маска типов событий.
    @return int Дескриптор наблюдения (watch descriptor) или -1 при ошибке.
    */
  int AddWatch(int fd, const std::string& path, uint32_t mask) override;

  /**
    @brief Читает данные о событиях из механизма мониторинга в указанный буфер.
    @param[in] fd Дескриптор механизма мониторинга.
    @param[out] buffer Указатель на буфер памяти для приема данных.
    @param[in] count Максимальный размер буфера в байтах.
    @return ssize_t Количество успешно прочитанных байт или -1 при ошибке.
    */
  ssize_t Read(int fd, void* buffer, std::size_t count) override;

  /**
    @brief Отменяет наблюдение за ранее зарегистрированным путем.
    @param[in] fd Дескриптор механизма мониторинга.
    @param[in] wd Дескриптор наблюдения, полученный от AddWatch().
    @return int 0 при успешном выполнении или -1 при ошибке.
    */
  int RemoveWatch(int fd, int wd) override;

  /**
    @brief Закрывает файловый дескриптор механизма мониторинга.
    @param[in] fd Файловый дескриптор, ранее полученный от Init().
    @return int 0 при успешном выполнении или -1 при ошибке.
    */
  int Close(int fd) override;

  /**
    @brief Получает статистическую информацию о файловой системе для указанного
    пути.
    @param[in] path Путь к файлу или директории внутри целевой файловой системы.
    @param[out] buf Указатель на структуру для приема метаданных файловой
    системы.
    @return int 0 при успешном выполнении или -1 при ошибке.
    */
  int StatFs(const std::string& path, struct statfs* buf) override;
};

}  // namespace stc::fs