/**
@file master.cpp
@brief Реализация оркестратора рабочих потоков.
@version 4.0.0
@date 2026-07-24
*/
#include "master.hpp"

#include <algorithm>
#include <stdexcept>

namespace stc {

Master::Master(std::shared_ptr<stc::logger::ILogger> logger,
               std::shared_ptr<stc::metrics::IMetricsRegistry> registry,
               GlobalMetricsDescriptors global_metrics,
               std::shared_ptr<FilterListManager> filter_list_manager)
    : logger_(std::move(logger)),
      metrics_registry_(std::move(registry)),
      global_metrics_(std::move(global_metrics)),
      filter_list_manager_(std::move(filter_list_manager)) {
  if (!metrics_registry_)
    throw std::invalid_argument("Master: MetricsRegistry cannot be null");
  if (!filter_list_manager_)
    throw std::invalid_argument("Master: FilterListManager cannot be null");
}

Master::~Master() { stop(); }

bool Master::start(const ApplicationConfiguration& config) {
  State expected = State::STOPPED;
  if (!state_.compare_exchange_strong(expected, State::STARTING)) {
    if (logger_) logger_->Warning("Master already running");
    return false;
  }
  try {
    spawnWorkers(config.sources, workers_);
    state_.store(State::RUNNING);
    if (logger_)
      logger_->Info("Master started with " + std::to_string(getWorkerCount()) +
                    " workers");
    return true;
  } catch (const std::exception& e) {
    state_.store(State::FATAL);
    if (logger_) logger_->Critical("Start failed: " + std::string(e.what()));
    return false;
  }
}

void Master::stop() noexcept {
  State current = state_.exchange(State::STOPPED);
  if (current != State::STOPPED) {
    terminateWorkers(workers_);
    if (global_metrics_.active_workers)
      global_metrics_.active_workers->Set(0.0);
    if (logger_) logger_->Info("Master stopped");
  }
}

void Master::reload(const ApplicationConfiguration& new_config) {
  State expected = State::RUNNING;
  if (!state_.compare_exchange_strong(expected, State::RELOADING)) {
    if (logger_) logger_->Warning("Reload: Invalid state");
    return;
  }
  if (logger_) logger_->Info("Master: reload procedure started.");
  // 1. СНАЧАЛА останавливаем старые воркеры и ждем их завершения
  WorkersContainer oldWorkers;
  workers_.swap(
      oldWorkers);  // Перемещаем старые в oldWorkers, workers_ теперь пуст
  terminateWorkers(
      oldWorkers);  // Блокирующе ждем завершения всех старых потоков
  // 2. ТОЛЬКО ПОСЛЕ полной остановки старых, создаем и запускаем новые
  WorkersContainer newWorkers;
  spawnWorkers(new_config.sources, newWorkers);
  // 3. Помещаем новые воркеры в основной контейнер
  workers_.swap(newWorkers);

  if (global_metrics_.active_workers) {
    global_metrics_.active_workers->Set(static_cast<double>(workers_.size()));
  }
  state_.store(State::RUNNING);
  if (logger_) logger_->Info("Master: Reload completed successfully");
}

void Master::spawnWorkers(const std::vector<SourceConfig>& sources,
                          WorkersContainer& target) {
  for (const auto& cfg : sources) {
    if (!cfg.enabled) continue;
    try {
      auto source_metrics = getOrCreateSourceMetrics(cfg.name);
      auto worker = std::make_unique<Worker>(cfg, logger_, filter_list_manager_,
                                             global_metrics_, source_metrics);
      worker->start();
      target.access([&](auto& w) { w.push_back(std::move(worker)); });
    } catch (const std::exception& e) {
      if (logger_)
        logger_->Error("Worker creation failed for source " + cfg.name + ": " +
                       std::string(e.what()));
    }
  }
}

void Master::terminateWorkers(WorkersContainer& target) {
  target.access([](auto& workers) {
    for (auto& w : workers) {
      if (w) w->stopGracefully();
    }
  });
}

void Master::healthCheck() {
  workers_.access([&](auto& workers) {
    for (auto& w : workers) {
      if (w && !w->isAlive()) {
        if (logger_)
          logger_->Warning("Master: Worker isn't alive, attempt to restart...");
        w->restart();
      }
    }
  });
}

void Master::restartAllMonitoring() {
  workers_.access([](auto& workers) {
    for (auto& worker : workers) {
      if (worker) worker->restartMonitoring();
    }
  });
  if (logger_) logger_->Info("All workers monitoring restarted");
}

size_t Master::getWorkerCount() const { return workers_.size(); }

Master::State Master::getState() const noexcept { return state_.load(); }

SourceMetricsDescriptors Master::getOrCreateSourceMetrics(
    const std::string& source_name) {
  auto it = source_metrics_cache_.find(source_name);
  if (it != source_metrics_cache_.end()) return it->second;

  SourceMetricsDescriptors sm;
  std::string prefix = "source_" + source_name + "_";
  sm.files_processed = metrics_registry_->RegisterCounter(
      prefix + "files_processed_total", "Files processed");
  sm.files_failed = metrics_registry_->RegisterCounter(
      prefix + "files_failed_total", "Failed files");
  sm.files_failed_parse = metrics_registry_->RegisterCounter(
      prefix + "files_failed_parse_total", "Parse errors");
  sm.files_failed_write = metrics_registry_->RegisterCounter(
      prefix + "files_failed_write_total", "Write errors");
  sm.records_processed = metrics_registry_->RegisterCounter(
      prefix + "records_processed_total", "Records processed");
  sm.records_matched = metrics_registry_->RegisterCounter(
      prefix + "records_matched_total", "Records matched");
  sm.bytes_processed = metrics_registry_->RegisterCounter(
      prefix + "bytes_processed_total", "Bytes processed");
  sm.templates_count = metrics_registry_->RegisterGauge(
      prefix + "comparison_templates_count", "Templates count");
  sm.worker_state =
      metrics_registry_->RegisterGauge(prefix + "worker_state", "Worker state");
  sm.last_file_processed_timestamp = metrics_registry_->RegisterGauge(
      prefix + "last_file_processed_timestamp_seconds",
      "Last processed timestamp");
  sm.duration_avg = metrics_registry_->RegisterGauge(
      prefix + "processing_duration_avg_seconds", "Avg duration");
  sm.duration_hist = metrics_registry_->RegisterHistogram(
      prefix + "processing_duration_seconds", "Processing duration",
      {0.01, 0.05, 0.1, 0.5, 1.0, 5.0, 10.0, 30.0, 60.0, 120.0, 300.0});

  source_metrics_cache_.emplace(source_name, sm);
  return sm;
}

}  // namespace stc