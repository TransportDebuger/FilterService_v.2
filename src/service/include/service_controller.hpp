#pragma once
#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <string>

#include "../include/argumentparser.hpp"
#include "../include/configmanager.hpp"
#include "../include/master.hpp"
#include "stc/DaemonManager.hpp"
#include "stc/SignalRouter.hpp"

class ServiceController {
 public:
  int run(int argc, char **argv);

 private:
  void initialize(const ParsedArgs &args);
  void initLogger(const ParsedArgs &args);
  void mainLoop();
  void handleShutdown();

  std::unique_ptr<Master> master_;
  std::unique_ptr<stc::DaemonManager> daemon_;
  std::string config_path_ = "config.json";
  std::atomic<bool> running_{false};

  std::mutex mtx_;              // Мьютекс для condition_variable
  std::condition_variable cv_;  // Условная переменная для прерываемого ожидания
};