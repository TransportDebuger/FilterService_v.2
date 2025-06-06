#include "../include/master.hpp"

#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <atomic>
#include <chrono>
#include <mutex>
#include <thread>

#include "stc/asyncfilelogger.hpp"
#include "stc/compositelogger.hpp"
#include "stc/consolelogger.hpp"
#include "stc/syncfilelogger.hpp"
// #include "../includes/AdapterFabric.hpp"

Master::Master() {
  stc::CompositeLogger::instance().debug("Master process created");
}

Master::~Master() {
  if (state_ != State::STOPPED) {
    stop();
  }
  stc::CompositeLogger::instance().debug("Master process destroyed");
}

bool Master::start(const std::string& config_path) {
  std::lock_guard<std::mutex> lock(workers_mutex_);

  if (state_ != State::STOPPED) {
    stc::CompositeLogger::instance().error(
        "Start attempted while not in STOPPED state");
    return false;
  }

  // config_path_ = config_path;
  state_ = State::RELOADING;

  try {
    //     // Загрузка конфигурации
    //     if (!config_.loadConfig(config_path_)) {
    //         throw std::runtime_error("Failed to load configuration");
    //     }

    //     // Валидация конфигурации
    //     validateConfig();

    //     // Запуск worker процессов
    initSignalHandlers();  // Регистрация SIGCHLD
    spawnWorkers();

    state_ = State::RUNNING;
    stc::CompositeLogger::instance().info(
        (std::string) "Master started successfully with " +
        // std::to_string(worker_pids_.size()) +
        (std::string) " workers");
  } catch (const std::exception& e) {
    state_ = State::FATAL;
    stc::CompositeLogger::instance().error(std::string("Start failed: ") +
                                           e.what());
    //     cleanupWorkers();
    return false;
  }
  return true;
}

void Master::stop() {
  std::lock_guard<std::mutex> lock(workers_mutex_);

  if (state_ == State::STOPPED) {
    return;
  }

  stc::CompositeLogger::instance().info("Stopping master process");
  state_ = State::STOPPED;
  // cleanupWorkers();
}

void Master::reload() {
  std::lock_guard<std::mutex> lock(workers_mutex_);

  if (state_ != State::RUNNING) {
    stc::CompositeLogger::instance().warning(
        "Reload attempted while not in RUNNING state");
    return;
  }

  stc::CompositeLogger::instance().info("Reloading configuration");
  state_ = State::RELOADING;

  try {
    // Загрузка новой конфигурации
    // ConfigManager new_config;
    //     if (!new_config.loadConfig(config_path_)) {
    //         throw std::runtime_error("Failed to reload configuration");
    //     }

    //     // Валидация новой конфигурации
    //     config_ = std::move(new_config);
    //     validateConfig();

    //     // Остановка текущих workers
    //     cleanupWorkers();

    //     // Запуск новых workers
    //     spawnWorkers();

    state_ = State::RUNNING;
    stc::CompositeLogger::instance().info("Reload completed successfully");
  } catch (const std::exception& e) {
    state_ = State::FATAL;
    stc::CompositeLogger::instance().error(std::string("Reload failed: ") +
                                           e.what());
  }
}

void Master::initSignalHandlers() {
  SignalHandler::instance().registerHandler(SIGCHLD, [this](int signum)) {
    pid_t pid;
    int status;

    // Перезапуск упавших worker'ов
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
      std::lock_guard lock(workers_mutex_);
      auto it = std::find_if(
          workers_.begin(), workers_.end(),
          [pid](const auto& worker) { return worker->getPid() == pid; });

      if (it != workers_.end()) {
        stc::CompositeLogger::instance().warning(
            "Worker " + std::to_string(pid) + " crashed. Restarting...");
        (*it)->restart();
      }
    }
  };
}

void Master::spawnWorkers() {
  std::lock_guard<std::mutex> lock(workers_mutex_);
  try {
    auto config = ConfigManager::instance().getMergedConfig(ENV_TYPE);
    auto sources = config["sources"];

    for (const auto& src : sources) {
      SourceConfig config = SourceConfig::fromJson(src);
      if (!config.enabled) continue;

      auto worker = std::make_unique<Worker>(config);
      worker->start();
      workers_.push_back(std::move(worker));
    }
  } catch (const std::exception& e) {
    stc::CompositeLogger::instance().critical("Failed to spawn workers: " +
                                              std::string(e.what()));
    throw;
  }
}

void Master::terminateWorker(pid_t pid) {
  // Graceful shutdown
  // kill(pid, SIGTERM);

  // // Ожидание завершения
  // for (int i = 0; i < 10; ++i) {
  //     if (waitpid(pid, nullptr, WNOHANG) != 0) {
  //         stc::ConsoleLogger::instance().debug("Worker " +
  //         std::to_string(pid) + " terminated gracefully"); return;
  //     }
  //     std::this_thread::sleep_for(std::chrono::milliseconds(100));
  // }

  // // Принудительное завершение
  // kill(pid, SIGKILL);
  // waitpid(pid, nullptr, 0);
  // stc::ConsoleLogger::instance().warning("Worker " + std::to_string(pid) + "
  // was forcefully terminated");
}

void Master::cleanupWorkers() {
  std::lock_guard lock(workers_mutex_);

  for (auto& worker : workers_) {
    worker->stop();  // Graceful shutdown
  }
  workers_.clear();

  stc::CompositeLogger::instance().info("All workers stopped");
}

void Master::healthCheck() {
  std::lock_guard lock(workers_mutex_);
  for (auto& worker : workers_) {
    if (!worker->isAlive()) {
      worker->restart();
      stc::CompositeLogger::instance().warning("Restarted crashed worker");
    }
  }
}

void Master::restartWorker(pid_t failed_pid) {
  // try {
  //     // Находим конфигурацию для упавшего worker
  //     for (auto& worker : workers_) {
  //         if (worker_pids_[failed_pid] == worker.get()) {
  //             auto config = worker->getConfig();

  //             // Запускаем новый worker
  //             auto new_worker = std::make_unique<Worker>(config);
  //             pid_t new_pid = fork();

  //             if (new_pid == 0) {
  //                 setupWorkerSignals();
  //                 new_worker->start();
  //                 _exit(EXIT_SUCCESS);
  //             } else if (new_pid > 0) {
  //                 stc::ConsoleLogger::instance().info("Restarted worker (new
  //                 PID: " + std::to_string(new_pid) +
  //                            " for source: " + config.name);
  //                 worker_pids_[new_pid] = new_worker.get();
  //                 workers_.push_back(std::move(new_worker));
  //             } else {
  //                 throw std::runtime_error("fork() failed during restart");
  //             }

  //             break;
  //         }
  //     }
  // } catch (const std::exception& e) {
  //     stc::ConsoleLogger::instance().error(std::string("Failed to restart
  //     worker: ") + e.what());
  // }
}

void Master::validateConfig() const {
  // if (config_.getSources().empty()) {
  //     throw std::runtime_error("No enabled sources in configuration");
  // }
}

void Master::setupWorkerSignals() {
  // Сбрасываем все обработчики сигналов для worker процессов
  signal(SIGTERM, SIG_DFL);
  signal(SIGINT, SIG_DFL);
  signal(SIGHUP, SIG_DFL);
}

Master::State Master::getState() const { return state_; }

std::string Master::stateToString() const {
  switch (state_) {
    case State::STOPPED:
      return "STOPPED";
    case State::RUNNING:
      return "RUNNING";
    case State::RELOADING:
      return "RELOADING";
    case State::FATAL:
      return "ERROR";
    default:
      return "UNKNOWN";
  }
}