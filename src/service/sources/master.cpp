#include "../includes/master.hpp"
#include "../includes/logger.hpp"
#include <csignal>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdexcept>

Master::Master() {
    Logger::debug("Master process created");
}

Master::~Master() {
    if (state_ != State::STOPPED) {
        stop();
    }
    Logger::debug("Master process destroyed");
}

bool Master::start(const std::string& config_path) {
    std::lock_guard<std::mutex> lock(workers_mutex_);
    
    if (state_ != State::STOPPED) {
        Logger::error("Start attempted while not in STOPPED state");
        return false;
    }

    config_path_ = config_path;
    state_ = State::RELOADING;

    try {
        // Загрузка конфигурации
        if (!config_.loadConfig(config_path_)) {
            throw std::runtime_error("Failed to load configuration");
        }

        // Валидация конфигурации
        validateConfig();

        // Запуск worker процессов
        spawnWorkers();
        
        state_ = State::RUNNING;
        Logger::info("Master started successfully with " + 
                    std::to_string(worker_pids_.size()) + " workers");
        return true;
    } catch (const std::exception& e) {
        state_ = State::ERROR;
        Logger::error(std::string("Start failed: ") + e.what());
        cleanupWorkers();
        return false;
    }
}

void Master::stop() {
    std::lock_guard<std::mutex> lock(workers_mutex_);
    
    if (state_ == State::STOPPED) {
        return;
    }

    Logger::info("Stopping master process");
    state_ = State::STOPPED;
    cleanupWorkers();
}

void Master::reload() {
    std::lock_guard<std::mutex> lock(workers_mutex_);
    
    if (state_ != State::RUNNING) {
        Logger::warning("Reload attempted while not in RUNNING state");
        return;
    }

    Logger::info("Reloading configuration");
    state_ = State::RELOADING;

    try {
        // Загрузка новой конфигурации
        ConfigManager new_config;
        if (!new_config.loadConfig(config_path_)) {
            throw std::runtime_error("Failed to reload configuration");
        }

        // Валидация новой конфигурации
        config_ = std::move(new_config);
        validateConfig();

        // Остановка текущих workers
        cleanupWorkers();

        // Запуск новых workers
        spawnWorkers();

        state_ = State::RUNNING;
        Logger::info("Reload completed successfully");
    } catch (const std::exception& e) {
        state_ = State::ERROR;
        Logger::error(std::string("Reload failed: ") + e.what());
    }
}

void Master::spawnWorkers() {
    for (const auto& source : config_.getSources()) {
        if (!source.enabled) continue;

        auto worker = std::make_unique<Worker>(source);
        pid_t pid = fork();

        if (pid == 0) { // Child process
            // Настройка сигналов для worker
            setupWorkerSignals();
            
            // Запуск worker
            worker->start();
            _exit(EXIT_SUCCESS);
        } else if (pid > 0) { // Parent process
            Logger::info("Started worker PID: " + std::to_string(pid) + 
                       " for source: " + source.name);
            worker_pids_[pid] = worker.get();
            workers_.push_back(std::move(worker));
        } else {
            throw std::runtime_error("fork() failed");
        }
    }
}

void Master::terminateWorker(pid_t pid) {
    // Graceful shutdown
    kill(pid, SIGTERM);
    
    // Ожидание завершения
    for (int i = 0; i < 10; ++i) {
        if (waitpid(pid, nullptr, WNOHANG) != 0) {
            Logger::debug("Worker " + std::to_string(pid) + " terminated gracefully");
            return;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    // Принудительное завершение
    kill(pid, SIGKILL);
    waitpid(pid, nullptr, 0);
    Logger::warning("Worker " + std::to_string(pid) + " was forcefully terminated");
}

void Master::cleanupWorkers() {
    for (const auto& [pid, _] : worker_pids_) {
        terminateWorker(pid);
    }
    worker_pids_.clear();
    workers_.clear();
}

void Master::healthCheck() {
    std::lock_guard<std::mutex> lock(workers_mutex_);
    
    if (state_ != State::RUNNING) {
        return;
    }

    for (auto it = worker_pids_.begin(); it != worker_pids_.end(); ) {
        pid_t pid = it->first;
        int status;
        pid_t result = waitpid(pid, &status, WNOHANG);

        if (result == 0) {
            // Worker still running
            ++it;
        } else if (result == -1) {
            // Error
            Logger::error("Error checking worker PID: " + std::to_string(pid));
            it = worker_pids_.erase(it);
        } else {
            // Worker terminated
            Logger::warning("Worker PID: " + std::to_string(pid) + 
                          " terminated with status: " + std::to_string(status));
            it = worker_pids_.erase(it);
            
            // Автоматический перезапуск
            if (state_ == State::RUNNING) {
                restartWorker(pid);
            }
        }
    }
}

void Master::restartWorker(pid_t failed_pid) {
    try {
        // Находим конфигурацию для упавшего worker
        for (auto& worker : workers_) {
            if (worker_pids_[failed_pid] == worker.get()) {
                auto config = worker->getConfig();
                
                // Запускаем новый worker
                auto new_worker = std::make_unique<Worker>(config);
                pid_t new_pid = fork();
                
                if (new_pid == 0) {
                    setupWorkerSignals();
                    new_worker->start();
                    _exit(EXIT_SUCCESS);
                } else if (new_pid > 0) {
                    Logger::info("Restarted worker (new PID: " + std::to_string(new_pid) + 
                               " for source: " + config.name);
                    worker_pids_[new_pid] = new_worker.get();
                    workers_.push_back(std::move(new_worker));
                } else {
                    throw std::runtime_error("fork() failed during restart");
                }
                
                break;
            }
        }
    } catch (const std::exception& e) {
        Logger::error(std::string("Failed to restart worker: ") + e.what());
    }
}

void Master::validateConfig() const {
    if (config_.getSources().empty()) {
        throw std::runtime_error("No enabled sources in configuration");
    }
}

void Master::setupWorkerSignals() {
    // Сбрасываем все обработчики сигналов для worker процессов
    signal(SIGTERM, SIG_DFL);
    signal(SIGINT, SIG_DFL);
    signal(SIGHUP, SIG_DFL);
}

Master::State Master::getState() const {
    return state_;
}

std::string Master::stateToString() const {
    switch(state_) {
        case State::STOPPED: return "STOPPED";
        case State::RUNNING: return "RUNNING";
        case State::RELOADING: return "RELOADING";
        case State::ERROR: return "ERROR";
        default: return "UNKNOWN";
    }
}