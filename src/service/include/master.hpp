#pragma once
#include <atomic>
#include <functional>
#include <memory>
#include "../include/workercontainer.hpp"
#include "../include/metricscollector.hpp"
#include "../include/adapterfabric.hpp"
#include "../include/signalrouter.hpp"

class Master {
public:
    enum class State { STOPPED, STARTING, RUNNING, RELOADING, FATAL };
    
    explicit Master(
        std::unique_ptr<AdapterFactory> factory,
        std::function<nlohmann::json()> configProvider
    );
    ~Master();

    bool start();
    void stop() noexcept;
    void reload();
    State getState() const noexcept;
    size_t getWorkerCount() const;

private:
    void spawnWorkers();
    void terminateWorkers();
    void healthCheck();
    void validateConfig(const nlohmann::json& config) const;

    WorkersContainer workers_;
    std::function<nlohmann::json()> getConfig_;
    std::unique_ptr<AdapterFactory> factory_;
    std::shared_ptr<SignalRouter> signalRouter_;
    std::atomic<State> state_{State::STOPPED};
    MetricsCollector metrics_;
    mutable std::mutex configMutex_;
};