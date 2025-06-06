#pragma once

#include <sys/types.h>

#include <atomic>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "../include/configmanager.hpp"
#include "../include/sourceconfig.hpp"
#include "../include/worker.hpp"

class Master {
 public:
  enum class State {
    STOPPED,  // Сервис остановлен
    RUNNING,  // Сервис работает нормально
    RELOADING,  // В процессе перезагрузки конфигурации
    FATAL  // Критическая ошибка
  };
  Master();
  ~Master();

  // Запрет копирования и присваивания
  Master(const Master&) = delete;
  Master& operator=(const Master&) = delete;

  // Основные методы управления
  bool start(const std::string& config_path);
  void stop();
  void reload();
  void initSignalHandlers();

  // Состояние сервиса
  State getState() const;
  std::string stateToString() const;

 private:
  // Методы управления worker процессами
  void spawnWorkers();
  void terminateWorker(pid_t pid);
  void cleanupWorkers();
  void healthCheck();
  void restartWorker(pid_t failed_pid);

  // Вспомогательные методы
  void validateConfig() const;
  void setupWorkerSignals();

  // Состояние сервиса
  std::atomic<State> state_{State::STOPPED};
  std::string config_path_;

  // Контейнеры для worker процессов
  std::vector<std::unique_ptr<Worker>> workers_;
  std::unordered_map<pid_t, Worker*> worker_pids_;
  mutable std::mutex workers_mutex_;
};