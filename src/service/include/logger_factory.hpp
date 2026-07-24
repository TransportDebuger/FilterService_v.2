/**
@file logger_factory.hpp
@brief Фабрика для создания и конфигурирования диспетчера логирования.
@version 1.0.0
@date 2026-07-17
*/
#pragma once

#include <memory>
#include <vector>

#include "logging_config.hpp"
#include "stc/logger/ilogger.hpp"

namespace stc {

/**
@class LoggerFactory
@brief Предоставляет статические методы для сборки экземпляра stc::logger::ILogger.
*/
class LoggerFactory {
public:
    /**
    @brief Создает и конфигурирует диспетчер логирования на основе DTO.
    @param[in] sinks_configs Вектор строгих структур конфигурации приемников.
    @return std::shared_ptr<stc::logger::ILogger> Указатель на настроенный логгер.
    @throw std::invalid_argument При наличии неподдерживаемых типов в DTO.
    */
    static std::shared_ptr<stc::logger::ILogger> Create(
        const std::vector<LoggerSinkConfig>& sinks_configs);

private:
    /**
    @private
    @brief Преобразует строковое представление уровня в строгий enum.
    @param[in] level_str Строка уровня (trace, debug, info, warning, error, critical).
    @return stc::logger::LogLevel Соответствующее значение перечисления.
    */
    static stc::logger::LogLevel ParseLogLevel(const std::string& level_str);

    /**
    @private
    @brief Создает экземпляр форматтера по его строковому идентификатору.
    @param[in] type Тип форматтера (text, json, xml).
    @return std::shared_ptr<stc::logger::ILogFormatter> Указатель на форматтер.
    */
    static std::shared_ptr<stc::logger::ILogFormatter> CreateFormatter(
        const std::string& type);

    /**
    @private
    @brief Создает стратегию ротации на основе опциональных параметров DTO.
    @param[in] config Опциональная структура параметров ротации.
    @return std::shared_ptr<stc::logger::IRotationPolicy> Указатель на политику или nullptr.
    */
    static std::shared_ptr<stc::logger::IRotationPolicy> CreateRotationPolicy(
        const std::optional<RotationConfig>& config);

    /**
    @private
    @brief Агрегирует компоненты (Formatter, Filter, Policy) в готовый Sink.
    @param[in] config Конфигурация конкретного приемника.
    @return std::shared_ptr<stc::logger::ILogSink> Указатель на созданный Sink.
    @throw std::invalid_argument Если тип приемника не поддерживается.
    */
    static std::shared_ptr<stc::logger::ILogSink> CreateSink(
        const LoggerSinkConfig& config);
};

} // namespace stc