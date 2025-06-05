#include "stc/DaemonManager.hpp"
#include <fstream>
#include <sys/stat.h>
#include <unistd.h>
#include <signal.h>

namespace stc {

DaemonManager::DaemonManager(const std::string& pidPath) 
    : mPidPath(std::filesystem::absolute(pidPath)) 
{
    checkExistingProcess();
}

DaemonManager::~DaemonManager() {
    cleanup();
}

void DaemonManager::daemonize() {
    // Первый fork
    if (pid_t pid = fork(); pid < 0) {
        throw std::system_error(errno, std::system_category(), "DaemonManager: daemonize(): First fork failed");
    } else if (pid > 0) {
        _exit(EXIT_SUCCESS);
    }

    // Создаем новую сессию
    if (setsid() < 0) {
        throw std::system_error(errno, std::system_category(), "DaemmonManager: daemonize(): setsid failed");
    }

    // Второй fork
    if (pid_t pid = fork(); pid < 0) {
        throw std::system_error(errno, std::system_category(), "DaemonManager: daemonize(): Second fork failed");
    } else if (pid > 0) {
        _exit(EXIT_SUCCESS);
    }

    // Настройка окружения
    umask(0);
    if (chdir("/") < 0) {
        throw std::system_error(errno, std::system_category(), "DaemonManager: daemonize(): chdir failed");
    }

    // Закрываем стандартные дескрипторы
    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);
}

void DaemonManager::writePid() {
    std::ofstream file(mPidPath);
    if (!file) {
        throw std::system_error(errno, std::system_category(), 
            "DaemonManager: writePid(): Failed to open PID file: " + mPidPath.string());
    }
    file << getpid() << '\n';
    mPidWritten = true;
    
    // Устанавливаем права
    if (chmod(mPidPath.c_str(), 0644) < 0) {
        throw std::system_error(errno, std::system_category(), 
            "DaemonManager: writePid(): Failed to set PID file permissions");
    }
}

void DaemonManager::cleanup() noexcept {
    if (mPidWritten) {
        std::error_code ec;
        std::filesystem::remove(mPidPath, ec);
    }
}

void DaemonManager::checkExistingProcess() {
    if (!std::filesystem::exists(mPidPath)) return;

    std::ifstream file(mPidPath);
    if (!file) return;

    pid_t oldPid;
    if (!(file >> oldPid)) return;

    if (kill(oldPid, 0) == 0 || errno == EPERM) {
        throw std::runtime_error(
            "DaemonManager: cleanup(): Process already running with PID: " + std::to_string(oldPid)
        );
    }

    removeStalePid();
}

void DaemonManager::removeStalePid() noexcept {
    std::error_code ec;
    std::filesystem::remove(mPidPath, ec);
}

} // namespace stc