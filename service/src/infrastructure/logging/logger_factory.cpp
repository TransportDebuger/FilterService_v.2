/**
@file logger_factory.cpp
@brief Реализация фабрики диспетчера логирования.
@version 1.0.0
@date 2026-07-17
*/
#include "logger_factory.hpp"

#include <stdexcept>
#include <string>

#include "stc/logger/logger.hpp"
#include "stc/logger/filters/level_filter.hpp"
#include "stc/logger/formatters/json_formatter.hpp"
#include "stc/logger/formatters/text_formatter.hpp"
#include "stc/logger/formatters/xml_formatter.hpp"
#include "stc/logger/sinks/console/console_sink.hpp"
#include "stc/logger/sinks/file/async_file_sink.hpp"
#include "stc/logger/sinks/file/circular_rotation_policy.hpp"
#include "stc/logger/sinks/file/file_sink.hpp"
#include "stc/logger/sinks/file/size_rotation_policy.hpp"
#include "stc/logger/sinks/file/time_rotation_policy.hpp"

namespace stc {

stc::logger::LogLevel LoggerFactory::ParseLogLevel(const std::string& level_str) {
    if (level_str == "trace") return stc::logger::LogLevel::kTrace;
    if (level_str == "debug") return stc::logger::LogLevel::kDebug;
    if (level_str == "info") return stc::logger::LogLevel::kInfo;
    if (level_str == "warning") return stc::logger::LogLevel::kWarning;
    if (level_str == "error") return stc::logger::LogLevel::kError;
    if (level_str == "critical") return stc::logger::LogLevel::kCritical;
    
    // Defensive programming: fallback на Info вместо генерации исключения,
    // так как валидация уже должна была быть пройдена в ConfigValidator.
    return stc::logger::LogLevel::kInfo; 
}

std::shared_ptr<stc::logger::ILogFormatter> LoggerFactory::CreateFormatter(
    const std::string& type) {
    if (type == "json") return std::make_shared<stc::logger::JsonFormatter>();
    if (type == "xml") return std::make_shared<stc::logger::XmlFormatter>();
    return std::make_shared<stc::logger::TextFormatter>();
}

std::shared_ptr<stc::logger::IRotationPolicy> LoggerFactory::CreateRotationPolicy(
    const std::optional<RotationConfig>& config) {
    if (!config.has_value()) {
        return nullptr;
    }

    const auto& rc = config.value();
    if (rc.type == "size") {
        return std::make_shared<stc::logger::SizeRotationPolicy>(
            rc.max_size_bytes, rc.max_archives);
    }
    if (rc.type == "time") {
        return std::make_shared<stc::logger::TimeRotationPolicy>(
            rc.interval_sec, rc.time_format);
    }
    if (rc.type == "circular") {
        return std::make_shared<stc::logger::CircularRotationPolicy>(
            rc.max_size_bytes, rc.max_archives);
    }
    
    return nullptr;
}

std::shared_ptr<stc::logger::ILogSink> LoggerFactory::CreateSink(
    const LoggerSinkConfig& config) {
    
    auto formatter = CreateFormatter(config.formatter);
    auto filter = std::make_shared<stc::logger::LevelFilter>(
        ParseLogLevel(config.level));
    auto rotation = CreateRotationPolicy(config.rotation);

    if (config.type == "console") {
        return std::make_shared<stc::logger::ConsoleSink>(
            std::move(formatter), std::move(filter));
    }
    
    if (config.type == "sync_file") {
        return std::make_shared<stc::logger::FileSink>(
            config.file_path, std::move(formatter), std::move(filter), std::move(rotation));
    }
    
    if (config.type == "async_file") {
        return std::make_shared<stc::logger::AsyncFileSink>(
            config.file_path, std::move(formatter), std::move(filter), std::move(rotation));
    }

    throw std::invalid_argument(
        "LoggerFactory: Unsupported sink type: " + config.type);
}

std::shared_ptr<stc::logger::ILogger> LoggerFactory::Create(
    const std::vector<LoggerSinkConfig>& sinks_configs) {
    
    auto logger = std::make_shared<stc::logger::Logger>("XmlFilterService");

    if (sinks_configs.empty()) {
        // Fallback: если конфигурация не содержит синков, гарантируем 
        // наличие хотя бы консольного вывода для предотвращения "немого" режима.
        auto fallback_formatter = std::make_shared<stc::logger::TextFormatter>();
        auto fallback_filter = std::make_shared<stc::logger::LevelFilter>(
            stc::logger::LogLevel::kInfo);
        logger->AddSink(std::make_shared<stc::logger::ConsoleSink>(
            std::move(fallback_formatter), std::move(fallback_filter)));
        return logger;
    }

    for (const auto& sink_config : sinks_configs) {
        auto sink = CreateSink(sink_config);
        logger->AddSink(std::move(sink));
    }

    return logger;
}

} // namespace stc