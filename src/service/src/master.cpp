#include "../include/master.hpp"
#include "stc/CompositeLogger.hpp"

Master::Master(
    std::unique_ptr<AdapterFactory> factory,
    std::function<nlohmann::json()> configProvider
) : getConfig_(std::move(configProvider)),
    factory_(std::move(factory))
{
    stc::MetricsCollector::instance().registerCounter("workers_created");
    stc::MetricsCollector::instance().registerCounter("workers_terminated");
    stc::MetricsCollector::instance().registerCounter("reload_attempts");
}

Master::~Master() {
    stop();
}

bool Master::start() {
    State expected = State::STOPPED;
    if (!state_.compare_exchange_strong(expected, State::STARTING)) {
        stc::CompositeLogger::instance().warning("Master already running");
        return false;
    }

    try {
        auto config = getConfig_();
        validateConfig(config);
        spawnWorkers();
        
        stc::SignalRouter::instance().registerHandler(SIGHUP, [this](int){
            stc::MetricsCollector::instance().incrementCounter("reload_attempts");
            reload();
        });
        
        state_.store(State::RUNNING);
        stc::CompositeLogger::instance().info(
            "Master started with " + std::to_string(getWorkerCount()) + " workers");
        return true;
    } catch (const std::exception& e) {
        state_.store(State::FATAL);
        stc::CompositeLogger::instance().critical("Start failed: " + std::string(e.what()));
        return false;
    }
}

void Master::stop() noexcept {
    State current = state_.exchange(State::STOPPED);
    if (current != State::STOPPED) {
        terminateWorkers();
        stc::MetricsCollector::instance().incrementCounter("workers_terminated", workers_.size());
        stc::CompositeLogger::instance().info("Master stopped");
    }
}

void Master::reload() {
    State expected = State::RUNNING;
    if (!state_.compare_exchange_strong(expected, State::RELOADING)) {
        stc::CompositeLogger::instance().warning("Reload: Invalid state");
        return;
    }

    WorkersContainer newWorkers;
    try {
        auto config = getConfig_();
        validateConfig(config);
        
        workers_.access([&](auto& workers){
            for (const auto& src : config["sources"]) {
                SourceConfig cfg = SourceConfig::fromJson(src);
                if (!cfg.enabled) continue;
                
                try {
                   workers.emplace_back(
                        std::make_unique<Worker>(
                            cfg,
                            factory_->createAdapter(cfg.type, cfg.path)
                        )
                    );
                    stc::MetricsCollector::instance().incrementCounter("workers_created");
                } catch (const std::exception& e) {
                    stc::CompositeLogger::instance().error(
                        "Worker creation failed: " + std::string(e.what()));
                }
            }
        });
        
        workers_.swap(newWorkers);
        state_.store(State::RUNNING);
        stc::CompositeLogger::instance().info("Reload completed");
    } catch (const std::exception& e) {
        state_.store(State::FATAL);
        stc::CompositeLogger::instance().error("Reload failed: " + std::string(e.what()));
    }
}

void Master::spawnWorkers() {
    auto config = getConfig_();
    workers_.access([&](auto& workers){
        for (const auto& src : config["sources"]) {
            SourceConfig cfg = SourceConfig::fromJson(src);
            if (!cfg.enabled) continue;
            
            try {
                workers.push_back(std::make_unique<Worker>(
                    cfg, 
                    factory_->createAdapter(cfg.type, cfg.path)
                ));
                stc::MetricsCollector::instance().incrementCounter("workers_created");
            } catch (const std::exception& e) {
                stc::CompositeLogger::instance().error(
                    "Worker creation failed: " + std::string(e.what()));
            }
        }
    });
}

void Master::terminateWorkers() {
    workers_.access([](auto& workers){
        for (auto& w : workers) {
            w->stopGracefully();
        }
    });
}

void Master::healthCheck() {
    workers_.access([&](auto& workers){
        for (auto& w : workers) {
            if (!w->isAlive()) {
                w->restart();
                stc::MetricsCollector::instance().incrementCounter("workers_restarted");
            }
        }
    });
}

void Master::validateConfig(const nlohmann::json& config) const {
    if (!config.contains("sources") || !config["sources"].is_array()) {
        throw std::runtime_error("Invalid sources configuration");
    }
    
    for (const auto& src : config["sources"]) {
        if (!src.contains("type") || src["type"] != "local") {
            throw std::runtime_error("Unsupported source type");
        }
    }
}

size_t Master::getWorkerCount() const {
    return workers_.size();
}

Master::State Master::getState() const noexcept {
    return state_.load();
}