#pragma once
#include <atomic>
#include <memory>
#include <string>

#include "stc/DaemonManager.hpp"
#include "stc/SignalRouter.hpp"

#include "../include/argumentparser.hpp"
#include "../include/configmanager.hpp"
#include "../include/master.hpp"

class ServiceController {
 public:
  int run(int argc, char** argv);

 private:
  void initialize(const ParsedArgs& args);
  void initLogger(const ParsedArgs& args);
  void mainLoop();
  void handleShutdown();

  std::unique_ptr<Master> master_;
  std::unique_ptr<stc::DaemonManager> daemon_;
  std::unique_ptr<stc::SignalRouter> signal_router_;
  std::string config_path_ = "config.json";
  std::atomic<bool> running_{false};
};