#include "../include/pid_file_manager.hpp"
#include <fstream>
#include <stdexcept>
#include <unistd.h>
#include <cerrno>
#include <system_error>

PidFileManager::PidFileManager(std::string path)
    : path_(std::move(path)) {}

PidFileManager::~PidFileManager() {
}

void PidFileManager::write() {
    std::ofstream out(path_, std::ios::trunc);
    if (!out) {
        throw std::runtime_error("Cannot open PID file for writing: " + path_);
    }
    out << getpid() << "\n";
    if (!out) {
        throw std::runtime_error("Failed to write PID to file: " + path_);
    }
}

std::optional<pid_t> PidFileManager::read() const {
    std::ifstream in(path_);
    if (!in) return std::nullopt;
    pid_t pid;
    in >> pid;
    if (!in) return std::nullopt;
    return pid;
}

bool PidFileManager::exists() const {
    std::ifstream in(path_);
    return static_cast<bool>(in);
}

void PidFileManager::remove() {
    std::remove(path_.c_str());
}