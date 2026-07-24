/**
@file master.cpp
@brief Реализация оркестратора рабочих потоков.
@version 3.0.0
@date 2026-07-21
*/
#include "../include/master.hpp"

#include <algorithm>
#include <stdexcept>

namespace stc {

Master::Master(std::function<nlohmann::json()> configProvider,
               std::shared_ptr<stc::logger::ILogger> logger,
               std::shared_ptr<stc::metrics::IMetricsRegistry> registry,
               GlobalMetricsDescriptors global_metrics)
    : getConfig_(std::move(configProvider))
    , logger_(std::move(logger))
    , metrics_registry_(std::move(registry))
    , global_metrics_(std::move(global_metrics))
{
    if (!metrics_registry_) {
        throw std::invalid_argument("Master: MetricsRegistry cannot be null");
    }
}

Master::~Master() { 
    stop(); 
}

bool Master::start() {
    State expected = State::STOPPED;
    if (!state_.compare_exchange_strong(expected, State::STARTING)) {
        logger_->Warning("Master already running");
        return false;
    }

    try {
        auto config = getConfig_();
        validateConfig(config);
        
        spawnWorkers();
        
        state_.store(State::RUNNING);
        logger_->Info("Master started with " + std::to_string(getWorkerCount()) + " workers");
        return true;
    } catch (const std::exception &e) {
        state_.store(State::FATAL);
        logger_->Critical("Start failed: " + std::string(e.what()));
        return false;
    }
}

void Master::stop() noexcept {
    State current = state_.exchange(State::STOPPED);
    if (current != State::STOPPED) {
        terminateWorkers();
        if (global_metrics_.active_workers) {
            global_metrics_.active_workers->Set(0.0);
        }
        logger_->Info("Master stopped");
    }
}

void Master::reload() {
    State expected = State::RUNNING;
    logger_->Info("Master: reload procedure started.");
    
    if (!state_.compare_exchange_strong(expected, State::RELOADING)) {
        logger_->Warning("Reload: Invalid state");
        return;
    }

    WorkersContainer newWorkers;
    try {
        auto config = getConfig_();
        validateConfig(config);
        
        logger_->Info("Master: Creating new workers for reload");
        newWorkers.access([&](auto &workers) {
            for (const auto &src : config["sources"]) {
                SourceConfig cfg = SourceConfig::fromJson(src);
                if (!cfg.enabled) continue;
                
                try {
                    // Получаем или создаем дескрипторы метрик для этого источника
                    auto source_metrics = getOrCreateSourceMetrics(cfg.name);
                    
                    // Инъекция логгера и метрик в новый Worker
                    auto worker = std::make_unique<Worker>(cfg, logger_, global_metrics_, source_metrics);
                    worker->start();
                    workers.emplace_back(std::move(worker));
                } catch (const std::exception &e) {
                    logger_->Error("Worker creation failed for source " + cfg.name + ": " + std::string(e.what()));
                }
            }
        });

        size_t oldCount = workers_.size();
        size_t newCount = newWorkers.size();
        logger_->Info("Master: Replacing " + std::to_string(oldCount) + " old workers with " + std::to_string(newCount) + " new workers");
        
        workers_.swap(newWorkers);
        
        // Обновляем общий счетчик активных воркеров
        if (global_metrics_.active_workers) {
            global_metrics_.active_workers->Set(static_cast<double>(workers_.size()));
        }

        logger_->Info("Master: Worker replacement completed");
        state_.store(State::RUNNING);
        logger_->Info("Master: Reload completed successfully");
    } catch (const std::exception &e) {
        state_.store(State::FATAL);
        logger_->Error("Reload failed: " + std::string(e.what()));
    }
}

void Master::spawnWorkers() {
    auto config = getConfig_();
    logger_->Debug("Master: Workers creation started, number of sources: " + std::to_string(config["sources"].size()));
    
    workers_.access([&](auto &workers) {
        for (const auto &src : config["sources"]) {
            SourceConfig cfg = SourceConfig::fromJson(src);
            logger_->Debug("Master: Attempt to create worker for source: " + cfg.name);
            
            if (!cfg.enabled) {
                logger_->Debug("Master: Worker isn't enabled in config file. Skipping creation.");
                continue;
            }
            
            try {
                // Получаем или создаем дескрипторы метрик для этого источника
                auto source_metrics = getOrCreateSourceMetrics(cfg.name);
                
                // Инъекция логгера и метрик в Worker
                auto worker = std::make_unique<Worker>(cfg, logger_, global_metrics_, source_metrics);
                worker->start();
                workers.push_back(std::move(worker));
            } catch (const std::exception &e) {
                logger_->Error("Worker creation failed for source " + cfg.name + ": " + std::string(e.what()));
            }
        }
    });
    
    // Обновляем общий счетчик активных воркеров
    if (global_metrics_.active_workers) {
        global_metrics_.active_workers->Set(static_cast<double>(workers_.size()));
    }
}

void Master::terminateWorkers() {
    workers_.access([](auto &workers) {
        for (auto &w : workers) {
            w->stopGracefully();
        }
    });
}

void Master::healthCheck() {
    workers_.access([&](auto &workers) {
        for (auto &w : workers) {
            if (!w->isAlive()) {
                logger_->Warning("Master: Worker isn't alive, attempt to restart worker...");
                w->restart();
            }
        }
    });
}

void Master::validateConfig(const nlohmann::json &config) const {
    logger_->Debug("Master: Sources configuration validation started");
    if (!config.contains("sources") || !config["sources"].is_array()) {
        logger_->Critical("Master: Config has invalid sources configuration or does not have it");
        throw std::runtime_error("Invalid sources configuration");
    }
    logger_->Debug("Master: Sources configuration present.");
    
    // Валидация типов источников (пока поддерживаем только local, но структура готова к расширению)
    if (std::any_of(config["sources"].begin(), config["sources"].end(),
        [](const auto& src) { 
            return !src.contains("type") || 
                   (src["type"] != "local" && src["type"] != "smb" && src["type"] != "ftp"); 
        })) {
        logger_->Error("Master: Config contains unsupported source type.");
        throw std::runtime_error("Unsupported source type");
    }
    logger_->Debug("Master: Sources validation configuration successfully ended.");
}

void Master::restartAllMonitoring() {
    workers_.access([](auto &workers) {
        for (auto& worker : workers) {
            if (worker) {
                worker->restartMonitoring();
            }
        }
    });
    logger_->Info("All workers monitoring restarted");
}

size_t Master::getWorkerCount() const { 
    return workers_.size(); 
}

Master::State Master::getState() const noexcept { 
    return state_.load(); 
}

SourceMetricsDescriptors Master::getOrCreateSourceMetrics(const std::string& source_name) {
    auto it = source_metrics_cache_.find(source_name);
    if (it != source_metrics_cache_.end()) {
        return it->second; // Переиспользуем существующие дескрипторы (монотонность для Prometheus)
    }

    SourceMetricsDescriptors sm;
    std::string prefix = "source_" + source_name + "_";

    // Счетчики (Counters)
    sm.files_processed = metrics_registry_->RegisterCounter(prefix + "files_processed_total", "Files processed for " + source_name);
    sm.files_failed = metrics_registry_->RegisterCounter(prefix + "files_failed_total", "Failed files for " + source_name);
    sm.files_failed_parse = metrics_registry_->RegisterCounter(prefix + "files_failed_parse_total", "Parse errors for " + source_name);
    sm.files_failed_write = metrics_registry_->RegisterCounter(prefix + "files_failed_write_total", "Write errors for " + source_name);
    
    sm.records_processed = metrics_registry_->RegisterCounter(prefix + "records_processed_total", "Records processed for " + source_name);
    sm.records_matched = metrics_registry_->RegisterCounter(prefix + "records_matched_total", "Records matched for " + source_name);
    sm.bytes_processed = metrics_registry_->RegisterCounter(prefix + "bytes_processed_total", "Bytes processed for " + source_name);
    
    // Датчики (Gauges)
    sm.templates_count = metrics_registry_->RegisterGauge(prefix + "comparison_templates_count", "Templates count for " + source_name);
    sm.worker_state = metrics_registry_->RegisterGauge(prefix + "worker_state", "Worker state (0=idle, 1=processing) for " + source_name);
    sm.last_file_processed_timestamp = metrics_registry_->RegisterGauge(prefix + "last_file_processed_timestamp_seconds", "Unix timestamp of last processed file for " + source_name);
    sm.duration_avg = metrics_registry_->RegisterGauge(prefix + "processing_duration_avg_seconds", "Average processing duration for " + source_name);

    // Гистограмма (Histogram)
    sm.duration_hist = metrics_registry_->RegisterHistogram(
        prefix + "processing_duration_seconds", 
        "Processing duration for " + source_name, 
        {0.01, 0.05, 0.1, 0.5, 1.0, 5.0, 10.0, 30.0, 60.0, 120.0, 300.0});

    source_metrics_cache_.emplace(source_name, sm);
    logger_->Debug("Master: Registered new source metrics for: " + source_name);
    return sm;
}

} // namespace stc