#include "../include/signal_handler.hpp"

#include <csignal>
#include <iostream>
#include <stdexcept>

#include "stc/compositelogger.hpp"

SignalHandler& SignalHandler::instance() {
  static SignalHandler instance;
  return instance;
}

SignalHandler::SignalHandler() { registerDefaultHandlers(); }

SignalHandler::~SignalHandler() { restoreAllHandlers(); }

void SignalHandler::registerHandler(int signum, Callback callback) {
  if (!isValidSignal(signum)) {
    throw std::invalid_argument("Invalid signal number");
  }

  std::lock_guard<std::mutex> lock(mutex_);

  // Save original handler
  saveOriginalHandler(signum);

  // Set new handler
  handlers_[signum] = callback;
  setSignalHandler(signum);
}

void SignalHandler::unregisterHandler(int signum) {
  if (!isValidSignal(signum)) {
    throw std::invalid_argument("Invalid signal number");
  }

  std::lock_guard<std::mutex> lock(mutex_);

  // Restore original handler
  restoreHandler(signum);

  // Remove handler from map
  handlers_.erase(signum);
}

bool SignalHandler::shouldStop() const noexcept { return stop_flag_.load(); }

bool SignalHandler::shouldReload() const noexcept {
  return reload_flag_.load();
}

void SignalHandler::resetFlags() noexcept {
  stop_flag_.store(false);
  reload_flag_.store(false);
}

void SignalHandler::restoreHandler(int signum) {
  if (!isValidSignal(signum)) {
    throw std::invalid_argument("Invalid signal number");
  }

  std::lock_guard<std::mutex> lock(mutex_);

  if (original_handlers_.count(signum)) {
    if (sigaction(signum, &original_handlers_.at(signum), nullptr) == -1) {
      // Log an error using cerr since Logger might not be initialized yet
      std::cerr << "Error restoring signal handler for signal " << signum
                << std::endl;
    }
    original_handlers_.erase(signum);
  }
}

void SignalHandler::restoreAllHandlers() {
  std::lock_guard<std::mutex> lock(mutex_);

  for (auto const& [signum, action] : original_handlers_) {
    if (sigaction(signum, &action, nullptr) == -1) {
      // Log an error using cerr since Logger might not be initialized yet
      std::cerr << "Error restoring signal handler for signal " << signum
                << std::endl;
    }
  }
  original_handlers_.clear();
}

bool SignalHandler::isValidSignal(int signum) noexcept {
  return signum > 0 && signum < NSIG;
}

void SignalHandler::saveOriginalHandler(int signum) {
  // std::lock_guard<std::mutex> lock(mutex_);
  struct sigaction current_action;
  if (sigaction(signum, nullptr, &current_action) == 0) {
    original_handlers_[signum] = current_action;
  } else {
    // Log an error using cerr since Logger might not be initialized yet
    std::cerr << "Error saving original signal handler for signal " << signum
              << std::endl;
  }
}

void SignalHandler::setSignalHandler(int signum) {
  struct sigaction sa;
  sa.sa_handler = handleSignal;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = 0;

  if (sigaction(signum, &sa, nullptr) == -1) {
    // Log an error using cerr since Logger might not be initialized yet
    std::cerr << "Error setting signal handler for signal " << signum
              << std::endl;
  }
}

void SignalHandler::handleSignal(int signum) noexcept {
  SignalHandler& instance = SignalHandler::instance();
  std::lock_guard<std::mutex> lock(instance.mutex_);

  switch (signum) {
    case SIGTERM:
    case SIGINT:
      instance.stop_flag_.store(true);
      break;
    case SIGHUP:
      instance.reload_flag_.store(true);
      break;
    default:
      if (instance.handlers_.count(signum)) {
        instance.handlers_.at(signum)(signum);
      }
      break;
  }
}

void SignalHandler::registerDefaultHandlers() {
  registerHandler(SIGTERM, [](int signum) {
    stc::CompositeLogger::instance().debug(
        "ServiceController: Received signal " + std::to_string(signum));
    SignalHandler::instance().stop_flag_.store(true);
  });

  registerHandler(SIGINT, [](int signum) {
    stc::CompositeLogger::instance().debug(
        "ServiceController: Received signal " + std::to_string(signum));
    SignalHandler::instance().stop_flag_.store(true);
  });

  registerHandler(SIGHUP, [](int signum) {
    stc::CompositeLogger::instance().debug(
        "ServiceController: Received signal " + std::to_string(signum));
    SignalHandler::instance().reload_flag_.store(true);
  });
}