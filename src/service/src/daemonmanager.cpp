#include "../include/daemonmanager.hpp"

#include <signal.h>

#include <fstream>
#include <system_error>

DaemonManager::DaemonManager(const std::string& pid_file, bool check_existing)
    : pid_path_(pid_file) {
  if (check_existing) {
    checkExistingProcess();
  }
}

DaemonManager::~DaemonManager() { cleanup(); }

void DaemonManager::daemonize() {
  // Первый fork
  if (pid_t pid = fork(); pid < 0) {
    throw std::runtime_error("DaemonManager: First fork failed: " +
                             std::to_string(errno));
  } else if (pid > 0) {
    _exit(EXIT_SUCCESS);  // Завершаем родительский процесс
  }

  // Создаем новую сессию
  if (setsid() < 0) {
    throw std::runtime_error("DaemonManager: setsid failed: " +
                             std::to_string(errno));
  }

  // Второй fork
  if (pid_t pid = fork(); pid < 0) {
    throw std::runtime_error("DaemonManager: Second fork failed: " +
                             std::to_string(errno));
  } else if (pid > 0) {
    _exit(EXIT_SUCCESS);  // Завершаем родительский процесс
  }

  // Устанавливаем маску прав
  umask(0);

  // Меняем рабочий каталог
  if (chdir("/") < 0) {
    throw std::runtime_error("DaemonManager: chdir failed: " +
                             std::to_string(errno));
  }

  // Перенаправляем стандартные дескрипторы
  close(STDIN_FILENO);
  close(STDOUT_FILENO);
  close(STDERR_FILENO);

  open("/dev/null", O_RDONLY);  // stdin
  open("/dev/null", O_WRONLY);  // stdout
  open("/dev/null", O_RDWR);    // stderr
}

void DaemonManager::writePid() {
  std::lock_guard lock(pid_mutex_);

  std::ofstream file(pid_path_, std::ios::trunc);
  if (!file.is_open()) {
    throw std::system_error(
        errno, std::system_category(),
        "DaemonManager: Failed to open PID file: " + pid_path_);
  }

  file << getpid() << '\n';
  if (!file.good()) {
    throw std::system_error(
        errno, std::system_category(),
        "DaemonManager: Failed to write PID to file: " + pid_path_);
  }

  pid_written_ = true;

  // Устанавливаем права на файл
  if (chmod(pid_path_.c_str(), PID_FILE_MODE) < 0) {
    throw std::system_error(
        errno, std::system_category(),
        "DaemonManager: Failed to set PID file permissions");
  }
}

void DaemonManager::cleanup() noexcept {
  std::lock_guard lock(pid_mutex_);
  if (pid_written_ && !pid_path_.empty()) {
    remove(pid_path_.c_str());
    pid_written_ = false;
  }
}

void DaemonManager::checkExistingProcess() {
  std::ifstream file(pid_path_);
  if (!file) return;

  pid_t old_pid;
  if (!(file >> old_pid)) return;

  // Проверяем существование процесса
  if (kill(old_pid, 0) == 0 || errno == EPERM) {
    throw std::runtime_error(
        "DaemonManager: Process already running with PID: " +
        std::to_string(old_pid));
  }

  // Удаляем устаревший PID-файл
  removeStalePid();
}

void DaemonManager::removeStalePid() noexcept {
  std::lock_guard lock(pid_mutex_);
  remove(pid_path_.c_str());
}