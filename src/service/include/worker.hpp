#pragma once
// #include "../includes/file_monitor.hpp"
#include <atomic>
#include <condition_variable>
#include <filesystem>
#include <mutex>
#include <thread>

#include "../include/sourceconfig.hpp"
#include "stc/compositelogger.hpp"

class Worker {
 public:
  explicit Worker(const SourceConfig& config);
  ~Worker();

  void start();
  void stop();
  void pause();
  void resume();
  bool isAlive() const;
  void restart();
  void stopGracefully();
  const SourceConfig& getConfig() const { return config_; }
  bool isRunning() const { return running_; }

 private:
  void run();
  void validatePaths() const;
  std::string getFileHash(const std::string& file_path) const;

  SourceConfig config_;
  stc::CompositeLogger& compositeLogger_;
  // std::unique_ptr<FileMonitor> monitor_;
  // std::unique_ptr<Processor> processor_;

  std::atomic<bool> running_{false};
  std::atomic<bool> paused_{false};
  std::atomic<bool> processing_{false};
  std::thread worker_thread_;
  mutable std::mutex state_mutex_;
  std::condition_variable cv_;
};