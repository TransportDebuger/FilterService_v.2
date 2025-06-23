#pragma once
#include <atomic>
#include <functional>
#include <memory>

#include "../include/AdapterFactory.hpp"
#include "../include/workercontainer.hpp"
#include "stc/MetricsCollector.hpp"
#include "stc/SignalRouter.hpp"

class Master {
 public:
  enum class State { STOPPED, STARTING, RUNNING, RELOADING, FATAL };

  explicit Master(std::function<nlohmann::json()> configProvider);
  ~Master();

  bool start();
  void stop() noexcept;
  void reload();
  void healthCheck();
  State getState() const noexcept;
  size_t getWorkerCount() const;

 private:
  void spawnWorkers();
  void terminateWorkers();
  void validateConfig(const nlohmann::json &config) const;

  WorkersContainer workers_;
  std::function<nlohmann::json()> getConfig_;
  // std::unique_ptr<AdapterFactory> factory_;
  std::shared_ptr<stc::SignalRouter> signalRouter_;
  std::atomic<State> state_{State::STOPPED};
  mutable std::mutex configMutex_;
};