/**
@file async_file_sink.hpp
@brief Объявление асинхронного файлового приемника с пакетной записью.
@version 3.1.0
@author Artem Ulyanov (aka s21::provemet)
@date 2026-07-26
*/
#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <fstream>
#include <memory>
#include <mutex>
#include <queue>
#include <stop_token>
#include <string>
#include <thread>
#include <functional>

#include "stc/logger/core/ilog_filter.hpp"
#include "stc/logger/core/ilog_formatter.hpp"
#include "stc/logger/core/ilog_sink.hpp"
#include "stc/logger/sinks/file/rotation_policy.hpp"

namespace stc::logger {

/**
@brief Тип обратного вызова для уведомления об ошибках ввода-вывода.
@param ec Код ошибки файловой системы.
@param context Контекст возникновения ошибки (например, "OpenFile").
@since 3.1.0
*/
using ErrorCallback = std::function<void(const std::error_code& ec, std::string_view context)>;

/**
@enum OverflowPolicy
@brief Определяет поведение асинхронного приемника при переполнении
       внутренней очереди сообщений.
@since 3.1.0
*/
enum class OverflowPolicy {
  kBlock,  ///< Блокировать вызывающий поток до освобождения места.
  kDrop,  ///< Отбрасывать новые сообщения (инкрементирует счетчик dropped).
  kFailFast  ///< Бросать std::overflow_error.
};

/**
@class AsyncFileSink
@brief Асинхронный файловый приемник с пакетной записью (Batching).
*/
class AsyncFileSink final : public ILogSink {
 public:
   /**
  @brief Конструктор асинхронного приемника.
  @param file_path Путь к лог-файлу.
  @param formatter Форматтер (используется диспетчером, здесь хранится для контракта).
  @param filter Опциональный локальный фильтр.
  @param rotation_policy Опциональная стратегия ротации.
  @param max_batch_size Максимальный размер пакета (в байтах) перед записью на диск.
  @param flush_interval Максимальное время ожидания перед записью неполного пакета.
  @param max_queue_size Максимальный размер очереди. 0 означает отсутствие ограничений.
  @param overflow_policy Поведение при достижении max_queue_size.
  @param error_callback Опциональный callback для уведомлений об ошибках I/O.
  @throw std::invalid_argument Если formatter равен nullptr.
  */
  explicit AsyncFileSink(
      std::string file_path, std::shared_ptr<ILogFormatter> formatter,
      std::shared_ptr<ILogFilter> filter = nullptr,
      std::shared_ptr<IRotationPolicy> rotation_policy = nullptr,
      std::size_t max_batch_size = 64 * 1024,
      std::chrono::milliseconds flush_interval = std::chrono::milliseconds(100),
      std::size_t max_queue_size = 0,
      OverflowPolicy overflow_policy = OverflowPolicy::kDrop,
      ErrorCallback error_callback = nullptr);

  /**
  @brief Деструктор. Запрашивает остановку фонового потока, дожидается
         записи всех оставшихся сообщений и корректно закрывает файл.
  */
  ~AsyncFileSink() override;
  AsyncFileSink(const AsyncFileSink&) = delete;
  AsyncFileSink& operator=(const AsyncFileSink&) = delete;

  /**
  @brief Помещает отформатированное сообщение в очередь.
  @param record Оригинальная запись (не используется в данной реализации).
  @param formatted_message Уже отформатированная строка для помещения в
         очередь.
  */
  void Write(const LogRecord& record,
             std::string_view formatted_message) override;

  /**
  @brief Принудительно сбрасывает буфер на диск.
  @details Блокирует вызывающий поток до завершения записи всей текущей
           очереди.
  */
  void Flush() override;

  /**
  @brief Возвращает форматтер, связанный с этим приемником.
  @return Умный указатель на реализацию ILogFormatter.
  */
  std::shared_ptr<ILogFormatter> GetFormatter() const noexcept override;

  /**
  @brief Возвращает локальный фильтр приемника.
  @return Умный указатель на реализацию ILogFilter или nullptr, если фильтр
          не задан.
  */
  std::shared_ptr<ILogFilter> GetFilter() const noexcept override;

  /**
  @brief Возвращает количество сообщений, отброшенных из-за переполнения
  очереди.
  @return Число отброшенных записей.
  */
  std::uint64_t GetDroppedRecordsCount() const noexcept;

 private:
  /**
  @private
  @brief Основной цикл фонового потока (Consumer).
  @param stoken Токен остановки для graceful shutdown (C++20 std::jthread).
  */
  void WorkerLoop(std::stop_token stoken);

  /**
  @private
  @brief Открывает файл и инициализирует current_size_.
  */
  void OpenFile();

  /**
  @private
  @brief Проверяет условия ротации и выполняет её при необходимости.
  @param now Текущее системное время для передачи в политику ротации.
  */
  void RotateIfNeeded(std::chrono::system_clock::time_point now);

  // --- Конфигурация и зависимости ---
  /// @private
  std::string file_path_;  ///< Путь к лог-файлу.
  /// @private
  std::shared_ptr<ILogFormatter>
      formatter_;  ///< Форматтер для контракта ILogSink.
  /// @private
  std::shared_ptr<ILogFilter> filter_;  ///< Локальный фильтр приемника.
  /// @private
  std::shared_ptr<IRotationPolicy>
      rotation_policy_;  ///< Стратегия ротации (может быть nullptr).
  /// @private
  std::size_t max_batch_size_;  ///< Максимальный размер пакета в байтах.
  /// @private
  std::chrono::milliseconds
      flush_interval_;  ///< Максимальное время ожидания неполного пакета.
  /// @private
  std::size_t
      max_queue_size_;  ///< Максимальный размер очереди (0 = без ограничений).
  /// @private
  OverflowPolicy
      overflow_policy_;  ///< Политика поведения при переполнении очереди.
  /// @private
  ErrorCallback error_callback_;

  // --- Файловый поток (используется ТОЛЬКО в фоновом потоке) ---
  /// @private
  std::ofstream file_stream_;  ///< Поток файлового ввода-вывода.
  /// @private
  std::uint64_t current_size_ =
      0;  ///< Текущий размер файла в байтах (отслеживается в памяти).

  // --- Очередь и синхронизация (Producer-Consumer) ---
  /// @private
  std::queue<std::string> queue_;  ///< Очередь отформатированных сообщений.
  /// @private
  std::mutex queue_mutex_;  ///< Мьютекс для защиты очереди.
  /// @private
  std::condition_variable queue_cv_;  ///< CV для уведомления Consumer о новых
                                      ///< сообщениях и освобождении места.

  // --- Механизм принудительного Flush ---
  /// @private
  std::atomic<bool> flush_requested_{
      false};  ///< Флаг запроса принудительного сброса.
  /// @private
  std::mutex flush_mutex_;  ///< Мьютекс для синхронизации ожидания Flush.
  /// @private
  std::condition_variable
      flush_cv_;  ///< CV для уведомления Producer о завершении Flush.

  // --- Метрики ---
  /// @private
  std::atomic<std::uint64_t> dropped_records_{
      0};  ///< Счетчик отброшенных сообщений.

  // --- Фоновый поток ---
  /// @private
  std::jthread worker_thread_;  ///< Фоновый поток (C++20, автоматически
                                ///< join-ится в деструкторе).
};

}  // namespace stc::logger