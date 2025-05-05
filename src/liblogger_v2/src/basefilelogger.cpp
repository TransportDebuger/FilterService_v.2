#include "stc/basefilelogger.hpp"

#include <chrono>
#include <iomanip>

namespace stc {

void BaseFileLogger::init(const LogLevel level) {
  setLogLevel(level);
  reopenFiles();
}

void BaseFileLogger::setLogLevel(LogLevel level) {
  currentLevel_.store(level, std::memory_order_release);
}

void BaseFileLogger::setRotationConfig(const RotationConfig& config) {
  std::lock_guard<std::mutex> lock(mutex_);
  rotationConfig_ = config;
}

RotationConfig BaseFileLogger::getRotationConfig() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return rotationConfig_;
}

void BaseFileLogger::flush() {
  std::lock_guard<std::mutex> lock(mutex_);
  if (mainLogFile_.is_open()) mainLogFile_.flush();
  if (fallbackLogFile_.is_open()) fallbackLogFile_.flush();
}

void BaseFileLogger::log(LogLevel level, const std::string& message) {
  if (shouldSkipLog(level)) return;

  auto now = std::chrono::system_clock::now();
  std::string timestamp = TimeFormatter::format(now);
  
  std::ostringstream formatted;
  formatted << timestamp 
            << " [" << leveltoString(level) << "] " 
            << message << "\n";

  rotateIfNeeded(formatted.str());
  writeToFile(formatted.str()); // Вызов абстрактного метода
}

bool BaseFileLogger::shouldSkipLog(LogLevel level) const {
  return static_cast<int>(level) <
         static_cast<int>(currentLevel_.load(std::memory_order_acquire));
}

void BaseFileLogger::reopenFiles() {
  std::lock_guard<std::mutex> lock(mutex_);
  mainLogFile_.open(mainLogPath_, std::ios::app);
  if (!mainLogFile_.is_open()) {
    fallbackLogFile_.open(fallbackLogPath_, std::ios::app);
  }
}

void BaseFileLogger::rotateIfNeeded(const std::string& message) {
  namespace fs = std::filesystem;
  std::lock_guard<std::mutex> lock(mutex_);

  if (!rotationConfig_.enabled || !mainLogFile_.is_open()) return;

  // Ротация по размеру
  if (rotationConfig_.type == RotationType::SIZE) {
    auto currentSize = fs::file_size(mainLogPath_);
    if (currentSize + message.size() > rotationConfig_.maxFileSizeBytes) {
      mainLogFile_.close();
      fs::rename(mainLogPath_, mainLogPath_ + ".1");
      mainLogFile_.open(mainLogPath_, std::ios::app);
    }
  }

  // Ротация по времени
  if (rotationConfig_.type == RotationType::TIME) {
    auto now = std::chrono::system_clock::now();
    if (now - rotationConfig_.lastRotationTime >
        rotationConfig_.rotationInterval) {
      mainLogFile_.close();
      std::string newName = mainLogPath_ + "_" + TimeFormatter::format(now);
      fs::rename(mainLogPath_, newName);
      rotationConfig_.lastRotationTime = now;
      mainLogFile_.open(mainLogPath_, std::ios::app);
    }
  }
}

}  // namespace stc