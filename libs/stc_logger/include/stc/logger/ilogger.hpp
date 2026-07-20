/**
 * @file ilogger.hpp
 * @brief Определение абстрактного интерфейса диспетчера логирования.
 * @version 3.0.0
 * @author Artem Ulyanov (aka s21::provemet)
 * @date 2026-07-17
 */

#pragma once

#include <source_location>
#include <string_view>

#include "stc/logger/core/log_level.hpp"

namespace stc::logger {

/**
 * @class ILogger
 * @brief Абстрактный базовый класс, определяющий контракт для всех реализаций
 * логгеров.
 */
class ILogger {
 public:
  /// @brief Виртуальный деструктор по умолчанию.
  virtual ~ILogger() = default;

  /**
   * @brief Основной метод регистрации события.
   * @param level Уровень важности сообщения.
   * @param message Текстовое содержимое сообщения.
   * @param location Контекст вызова (захватывается автоматически).
   */
  virtual void Log(
      LogLevel level, std::string_view message,
      std::source_location location = std::source_location::current()) = 0;

  /// @brief Регистрация сообщения уровня Trace.
  virtual void Trace(
      std::string_view message,
      std::source_location location = std::source_location::current()) = 0;

  /// @brief Регистрация сообщения уровня Debug.
  virtual void Debug(
      std::string_view message,
      std::source_location location = std::source_location::current()) = 0;

  /// @brief Регистрация сообщения уровня Info.
  virtual void Info(
      std::string_view message,
      std::source_location location = std::source_location::current()) = 0;

  /// @brief Регистрация сообщения уровня Warning.
  virtual void Warning(
      std::string_view message,
      std::source_location location = std::source_location::current()) = 0;

  /// @brief Регистрация сообщения уровня Error.
  virtual void Error(
      std::string_view message,
      std::source_location location = std::source_location::current()) = 0;

  /// @brief Регистрация сообщения уровня Critical.
  virtual void Critical(
      std::string_view message,
      std::source_location location = std::source_location::current()) = 0;

  /// @brief Принудительно сбрасывает внутренние буферы приемников.
  virtual void Flush() = 0;
};

}  // namespace stc::logger