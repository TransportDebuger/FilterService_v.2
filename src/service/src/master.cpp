#include "../include/master.hpp"

#include "stc/CompositeLogger.hpp"
#include <algorithm>

Master::Master(std::function<nlohmann::json()> configProvider)
    : getConfig_(std::move(configProvider)) {
  stc::MetricsCollector::instance().registerCounter("workers_created");
  stc::MetricsCollector::instance().registerCounter("workers_terminated");
  stc::MetricsCollector::instance().registerCounter("reload_attempts");
}

Master::~Master() { stop(); }

bool Master::start() {
  State expected = State::STOPPED;
  if (!state_.compare_exchange_strong(expected, State::STARTING)) {
    stc::CompositeLogger::instance().warning("Master already running");
    return false;
  }

  try {
    auto config = getConfig_();
    validateConfig(config);
    stc::MetricsCollector::instance().registerCounter("files_processed",
                                                      "Total files processed");
    stc::MetricsCollector::instance().registerCounter("files_failed",
                                                      "Total failed files");
    spawnWorkers();

    state_.store(State::RUNNING);
    stc::CompositeLogger::instance().info(
        "Master started with " + std::to_string(getWorkerCount()) + " workers");
    return true;
  } catch (const std::exception &e) {
    state_.store(State::FATAL);
    stc::CompositeLogger::instance().critical("Start failed: " +
                                              std::string(e.what()));
    return false;
  }
}

void Master::stop() noexcept {
  State current = state_.exchange(State::STOPPED);
  if (current != State::STOPPED) {
    terminateWorkers();
    stc::MetricsCollector::instance().incrementCounter("workers_terminated",
                                                       workers_.size());
    stc::CompositeLogger::instance().info("Master stopped");
  }
}

void Master::reload() {
  State expected = State::RUNNING;
    stc::CompositeLogger::instance().info("Master: reload procedure started.");
    
    if (!state_.compare_exchange_strong(expected, State::RELOADING)) {
        stc::CompositeLogger::instance().warning("Reload: Invalid state");
        return;
    }

    WorkersContainer newWorkers;
    try {
        auto config = getConfig_();
        validateConfig(config);
        
        stc::CompositeLogger::instance().info("Master: Creating new workers for reload");
        
        // Создаем и ЗАПУСКАЕМ новых воркеров
        newWorkers.access([&](auto &workers) {
            for (const auto &src : config["sources"]) {
                SourceConfig cfg = SourceConfig::fromJson(src);
                if (!cfg.enabled) continue;
                
                try {
                    auto worker = std::make_unique<Worker>(cfg);
                    worker->start();
                    workers.emplace_back(std::move(worker));
                    stc::MetricsCollector::instance().incrementCounter("workers_created");
                } catch (const std::exception &e) {
                    stc::CompositeLogger::instance().error("Worker creation failed: " +
                                                         std::string(e.what()));
                }
            }
        });
        
        size_t oldCount = workers_.size();
        size_t newCount = newWorkers.size();
        
        // Заменяем старых воркеров новыми
        stc::CompositeLogger::instance().info("Master: Replacing " + std::to_string(oldCount) + 
                                              " old workers with " + std::to_string(newCount) + " new workers");
        
        workers_.swap(newWorkers);
        
        stc::CompositeLogger::instance().info("Master: Worker replacement completed");
        
        state_.store(State::RUNNING);
        stc::CompositeLogger::instance().info("Master: Reload completed successfully");
        
        // newWorkers теперь содержит старые воркеры, которые будут уничтожены
        stc::CompositeLogger::instance().debug("Master: Old workers will be destroyed automatically");
        
    } catch (const std::exception &e) {
        state_.store(State::FATAL);
        stc::CompositeLogger::instance().error("Reload failed: " + std::string(e.what()));
    }
}

void Master::spawnWorkers() {
  auto config = getConfig_();
    stc::CompositeLogger::instance().debug("Master: Workers creation started, number of workers: " + 
        std::to_string(config["sources"].size()));
    
    workers_.access([&](auto &workers) {
        for (const auto &src : config["sources"]) {
            SourceConfig cfg = SourceConfig::fromJson(src);
            stc::CompositeLogger::instance().debug("Master: Attempt to create worker for source: " + cfg.name);
            
            if (!cfg.enabled) {
                stc::CompositeLogger::instance().debug("Master: Worker isn't enabled in config file. Skipping creation.");
                continue;
            }

            try {
                auto worker = std::make_unique<Worker>(cfg);
                
                // ВАЖНО: Запускаем воркера при создании!
                worker->start();
                
                workers.push_back(std::move(worker));
                stc::MetricsCollector::instance().incrementCounter("workers_created");
            } catch (const std::exception &e) {
                stc::CompositeLogger::instance().error("Worker creation failed: " + 
                    std::string(e.what()));
            }
        }
    });
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
        stc::CompositeLogger::instance().warning("Master: Worker isn't alive, attempt to restart worker...");
        w->restart();
        stc::MetricsCollector::instance().incrementCounter("workers_restarted");
      }
    }
  });
}

void Master::validateConfig(const nlohmann::json &config) const {
  stc::CompositeLogger::instance().debug("Master: Sources configuration validation started");
  if (!config.contains("sources") || !config["sources"].is_array()) {
     stc::CompositeLogger::instance().critical("Master: Config has invalid sources configuration or does not have it");
    throw std::runtime_error("Invalid sources configuration");
  }
  stc::CompositeLogger::instance().debug("Master: Sources configuration present.");
  if (std::any_of(config["sources"].begin(), config["sources"].end(),
                [](const auto& src) {
                    return !src.contains("type") || src["type"] != "local";
                })) {
    stc::CompositeLogger::instance().error("Master: Config contains unsupported source type or haven't type member in config.");
    throw std::runtime_error("Unsupported source type");
  }
  stc::CompositeLogger::instance().debug("Master: Sources vaildation configuration successfuly ended.");
}

void Master::restartAllMonitoring() {
    workers_.access([](auto &workers) {
        for (auto& worker : workers) {
            if (worker) {  // Проверяем на nullptr
                worker->restartMonitoring();
            }
        }
    });
    stc::CompositeLogger::instance().info("All workers monitoring restarted");
}

size_t Master::getWorkerCount() const { return workers_.size(); }

Master::State Master::getState() const noexcept { return state_.load(); }

