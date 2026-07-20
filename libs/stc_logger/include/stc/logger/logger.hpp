/**
 * @file logger.hpp
 * @brief Объявление класса диспетчеризации и маршрутизации логов.
 * @version 3.0.0
 * @author Artem Ulyanov (aka s21::provemet)
 * @date 2026-07-17
 */
#pragma once

#include <memory>
#include <shared_mutex>
#include <source_location>
#include <string>
#include <string_view>
#include <vector>

#include "stc/logger/core/ilog_filter.hpp"
#include "stc/logger/core/ilog_sink.hpp"
#include "stc/logger/ilogger.hpp"

namespace stc::logger {

/**
 * @class Logger
 * @brief Центральный диспетчер библиотеки, отвечающий за прием, фильтрацию
 *        и маршрутизацию записей лога к зарегистрированным приемникам.
 */
class Logger final : public ILogger {
 public:
  /**
   * @brief Конструирует экземпляр логгера с заданным именем.
   * @param name Уникальное имя экземпляра логгера.
   */
  explicit Logger(std::string name);

  /// @brief Деструктор.
  ~Logger() override;

  /// @brief Конструктор копирования удален.
  Logger(const Logger&) = delete;

  /// @brief Оператор присваивания копированием удален.
  Logger& operator=(const Logger&) = delete;

  /**
   * @brief Добавляет новый приемник (Sink) в цепочку диспетчеризации.
   * @param sink Умный указатель на реализацию ILogSink.
   * @throw std::invalid_argument Если передан nullptr.
   */
  void AddSink(std::shared_ptr<ILogSink> sink);

  /**
   * @brief Добавляет глобальный фильтр, применяемый ко всем сообщениям.
   * @param filter Умный указатель на реализацию ILogFilter.
   * @throw std::invalid_argument Если передан nullptr.
   */
  void AddGlobalFilter(std::shared_ptr<ILogFilter> filter);

  /**
   * @brief Основной метод регистрации события с автоматическим захватом
   *        контекста.
   * @param level Уровень важности сообщения.
   * @param message Текстовое содержимое сообщения.
   * @param location Контекст вызова (файл, функция, строка).
   */
  void Log(
      LogLevel level, std::string_view message,
      std::source_location location = std::source_location::current()) override;

  /**
   * @brief Регистрация сообщения уровня Trace.
   * @param message Текстовое содержимое сообщения.
   * @param location Контекст вызова.
   */
  void Trace(
      std::string_view message,
      std::source_location location = std::source_location::current()) override;

  /**
   * @brief Регистрация сообщения уровня Debug.
   * @param message Текстовое содержимое сообщения.
   * @param location Контекст вызова.
   */
  void Debug(
      std::string_view message,
      std::source_location location = std::source_location::current()) override;

  /**
   * @brief Регистрация сообщения уровня Info.
   * @param message Текстовое содержимое сообщения.
   * @param location Контекст вызова.
   */
  void Info(
      std::string_view message,
      std::source_location location = std::source_location::current()) override;

  /**
   * @brief Регистрация сообщения уровня Warning.
   * @param message Текстовое содержимое сообщения.
   * @param location Контекст вызова.
   */
  void Warning(
      std::string_view message,
      std::source_location location = std::source_location::current()) override;

  /**
   * @brief Регистрация сообщения уровня Error.
   * @param message Текстовое содержимое сообщения.
   * @param location Контекст вызова.
   */
  void Error(
      std::string_view message,
      std::source_location location = std::source_location::current()) override;

  /**
   * @brief Регистрация сообщения уровня Critical.
   * @param message Текстовое содержимое сообщения.
   * @param location Контекст вызова.
   */
  void Critical(
      std::string_view message,
      std::source_location location = std::source_location::current()) override;

  /**
   *  @brief Принудительно сбрасывает внутренние буферы всех зарегистрированных
   *         Sinks.
   */
  void Flush() override;

 private:
  /**
   * @private
   * @brief Уникальное имя экземпляра логгера.
   */
  std::string name_;
  /**
   * @private
   * @brief Список зарегистрированных приемников
   */
  std::vector<std::shared_ptr<ILogSink>> sinks_;
  /**
   * @private
   * @brief Список глобальных фильтров.
   */
  std::vector<std::shared_ptr<ILogFilter>> global_filters_;

  /**
   * @private
   * @brief Мьютекс для защиты векторов sinks_ и global_filters_.
   */
  mutable std::shared_mutex mutex_;
};

}  // namespace stc::logger