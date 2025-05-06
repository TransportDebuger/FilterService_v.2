#include "stc/basefilelogger.hpp"

#include <iostream>
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
  std::lock_guard lock(mutex_);
  try {
      // Закрываем старые файлы, если они были открыты
      if (mainLogFile_.is_open()) {
          mainLogFile_.close();
      }
      if (fallbackLogFile_.is_open()) {
          fallbackLogFile_.close();
      }

      // Открываем основной лог-файл
      mainLogFile_.open(mainLogPath_, std::ios::app);
      if (!mainLogFile_.is_open()) {
          std::cerr << "[LOGGER ERROR] Cannot open main log file: " << mainLogPath_ << std::endl;

          // Открываем резервный лог-файл
          fallbackLogFile_.open(fallbackLogPath_, std::ios::app);
          if (!fallbackLogFile_.is_open()) {
              std::cerr << "[LOGGER ERROR] Cannot open fallback log file: " << fallbackLogPath_ << std::endl;
          }
      }
  } catch (const std::exception& e) {
      std::cerr << "[LOGGER ERROR] Exception during file open: " << e.what() << std::endl;
  } catch (...) {
      std::cerr << "[LOGGER ERROR] Unknown exception during file open." << std::endl;
  }
}

void BaseFileLogger::rotateIfNeeded(const std::string& message) {
  namespace fs = std::filesystem;
    std::lock_guard lock(mutex_);
    try {
        if (!rotationConfig_.enabled || !mainLogFile_.is_open()) return;

        bool needRotate = false;
        if (rotationConfig_.type == RotationType::SIZE) {
            auto currentSize = fs::file_size(mainLogPath_);
            if (currentSize + message.size() > rotationConfig_.maxFileSizeBytes) {
                needRotate = true;
            }
        }
        if (rotationConfig_.type == RotationType::TIME) {
            auto now = std::chrono::system_clock::now();
            if (now - rotationConfig_.lastRotationTime > rotationConfig_.rotationInterval) {
                needRotate = true;
                rotationConfig_.lastRotationTime = now;
            }
        }
        if (!needRotate) return;

        mainLogFile_.close();

        // Шаг 1: Переименовать исходный файл во временный (атомарно)
        std::string tempName = mainLogPath_ + ".rotating";
        fs::rename(mainLogPath_, tempName);

        // Шаг 2: Открыть новый файл для логирования
        mainLogFile_.open(mainLogPath_, std::ios::app);
        if (!mainLogFile_.is_open()) {
            std::cerr << "[LOGGER ERROR] Cannot open new log file after rotation: " << mainLogPath_ << std::endl;
            // Попробовать открыть fallback-файл или вернуть ошибку
            return;
        }

        // Шаг 3: Переименовать временный файл в архивное имя
        std::string rotatedName;
        if (rotationConfig_.type == RotationType::SIZE) {
            rotatedName = mainLogPath_ + ".1";
        } else if (rotationConfig_.type == RotationType::TIME) {
            auto now = std::chrono::system_clock::now();
            rotatedName = mainLogPath_ + "_" + TimeFormatter::format(now);
        } else {
            rotatedName = mainLogPath_ + ".old";
        }
        fs::rename(tempName, rotatedName);
    } catch (const std::exception& e) {
        std::cerr << "[LOGGER ERROR] Exception during atomic log rotation: " << e.what() << std::endl;
    }
}

void BaseFileLogger::setMainLogPath(const std::string& path) {
  if (mainLogPath_ != path) {
      {
        std::lock_guard lock(mutex_);
        mainLogPath_ = path;
      }
      reopenFiles();
  }
}

void BaseFileLogger::setFallbackLogPath(const std::string& path) {
  if (fallbackLogPath_ != path) {
      {
        std::lock_guard lock(mutex_);
        fallbackLogPath_ = path;
      }
      reopenFiles();
  }
}

std::string BaseFileLogger::getMainLogPath() const {
  std::lock_guard lock(mutex_);
  return mainLogPath_;
}

std::string BaseFileLogger::getFallbackLogPath() const {
  std::lock_guard lock(mutex_);
  return fallbackLogPath_;
}

}  // namespace stc