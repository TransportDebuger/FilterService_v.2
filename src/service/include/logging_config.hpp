/**
@file logging_config.hpp
@brief Строгие структуры данных (DTO) для конфигурации подсистемы логирования.
@version 1.0.0
@date 2026-07-20
*/
#pragma once

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>

namespace stc {

/**
@struct RotationConfig
@brief Параметры стратегии ротации лог-файлов.
*/
struct RotationConfig {
    std::string type; ///< Тип ротации: "size", "time" или "circular".
    std::uint64_t max_size_bytes{0}; ///< Максимальный размер файла в байтах (для "size" и "circular").
    std::size_t max_archives{0}; ///< Максимальное количество хранимых архивов (для "size" и "circular").
    std::chrono::seconds interval_sec{0}; ///< Интервал ротации в секундах (для "time").
    std::string time_format; ///< Формат времени для имени архива (для "time", совместим с strftime).
};

/**
@struct LoggerSinkConfig
@brief Параметры конфигурации одного приемника (Sink) логирования.
*/
struct LoggerSinkConfig {
    std::string type; ///< Тип приемника: "console", "sync_file" или "async_file".
    std::string level; ///< Уровень логирования: "trace", "debug", "info", "warning", "error", "critical".
    std::string formatter; ///< Тип форматтера: "text", "json" или "xml".
    std::string file_path; ///< Путь к лог-файлу (обязателен для "sync_file" и "async_file").
    std::optional<RotationConfig> rotation; ///< Опциональные параметры ротации.
};

} // namespace stc