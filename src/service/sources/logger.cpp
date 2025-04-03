/*!
  \file logger.hpp
  \author Artem Ulyanov
  \version 1
  \date March, 2024
  \brief Файл реализации методов системы логгирования в проекте filter_service.
  \details Файл является неотъемлимой частью проекта filter_service.
    Данный файл содержит реализацию методов необходимых для выполнения функций ведения журнала.
*/


#include "../includes/logger.hpp"
#include "logger.hpp"
//#include "logger.hpp"

#define COLOR_ERROR "\033[31m"
#define COLOR_WARN "\033[33m"
#define COLOR_DEBUG "\033[32m"
#define COLOR_INFO "\033[36m"
#define COLOR_RESET "\033[0m"

// Статические переменные
std::ofstream Logger::logFile_;
std::mutex Logger::mutex_;
std::string Logger::filename_;
bool Logger::rotateBySize_ = true;
size_t Logger::maxSize_ = DEFAULT_LOGSIZE;
Logger::LogLevel Logger::minLevel_ = Logger::LogLevel::INFO;
std::stringstream Logger::fallbackBuffer_;
bool Logger::fallbackUsed_ = false;

// Инициализация логгера
void Logger::init(bool rotateBySize, size_t maxSize) {
    std::lock_guard<std::mutex> lock(mutex_);
    rotateBySize_ = rotateBySize;
    maxSize_ = maxSize;

    try {
        logFile_.open(filename_, std::ios::app);
        if (!logFile_.is_open()) {
            throw std::runtime_error("Cannot open log file: " + filename_);
        }
    } catch (const std::exception& e) {
        std::cerr << "Error initializing logger: " << e.what() << std::endl;
        initFallback();
    }
}

void Logger::initFallback() {
    fallbackUsed_ = true;
}

// Закрытие файла (вызывается при завершении)
void Logger::close() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (logFile_.is_open()) {
        logFile_.flush(); // Принудительный сброс буфера
        logFile_.close();
    }
    
    // Сброс fallback буфера
    if (fallbackUsed_) {
        std::cerr << fallbackBuffer_.str();
        flushFallbackBuffer();
        fallbackUsed_ = false; // Сброс флага
    }
}

// Проверка необходимости ротации
bool Logger::needsRotation() {
    if (!rotateBySize_) return false;

    struct stat statBuf;
    if (stat(filename_.c_str(), &statBuf) != 0) {
        return false; // Ошибка stat, пропускаем ротацию
    }
    return static_cast<size_t>(statBuf.st_size) >= maxSize_;
}

// Ротация логов
void Logger::rotateLog() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!logFile_.is_open()) {
        std::cerr << "Log file is not open, cannot rotate." << std::endl;
        return;
    }

    logFile_.close();

    std::string oldLog = filename_ + "." + getCurrentTime(true);

    if (rename(filename_.c_str(), oldLog.c_str()) != 0) {
        if (logFile_.is_open()) {
            error("Failed to rename log file during rotation");
        } else {
            std::cerr << "Failed to rename log file during rotation" << std::endl;
        }
        
        try {
            logFile_.open(filename_, std::ios::app); // Восстановление
        } catch (const std::exception& e) {
            if (logFile_.is_open()) {
                error("Failed to reopen log file after failed rotation: " + std::string(e.what()));
            } else {
                std::cerr << "Failed to reopen log file after failed rotation: " << e.what() << std::endl;
            }
            // Продолжаем логирование в старом файле
        }
        return;
    }

    try {
        logFile_.open(filename_, std::ios::app);
        if (!logFile_.is_open()) {
            throw std::runtime_error("Cannot reopen log file after rotation");
        }
    } catch (const std::exception& e) {
        if (logFile_.is_open()) {
            error("Error reopening log file after rotation: " + std::string(e.what()));
        } else {
            std::cerr << "Error reopening log file after rotation: " << e.what() << std::endl;
        }
        // Продолжаем логирование в старом файле
    }
}

// Форматирование времени
std::string Logger::getCurrentTime(bool forFilename) {
    std::lock_guard<std::mutex> lock(mutex_); // Синхронизация для многопоточной среды
    
    auto now = std::chrono::system_clock::now();
    auto now_time = std::chrono::system_clock::to_time_t(now);
    auto now_tm = *std::localtime(&now_time);

    std::ostringstream oss;
    if (forFilename) {
        oss << std::put_time(&now_tm, "%Y%m%d_%H%M%S");
    } else {
        oss << std::put_time(&now_tm, "%Y-%m-%d %H:%M:%S");
    }
    return oss.str();
}

// Основная функция логирования
void Logger::log(LogLevel level, const std::string& message) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (level < minLevel_) { return; }
    
    std::string color;
    switch (level) {
        case LogLevel::DEBUG:   color = COLOR_DEBUG; break;
        case LogLevel::INFO:    color = COLOR_INFO; break;
        case LogLevel::WARNING: color = COLOR_WARN; break;
        case LogLevel::ERROR:   color = COLOR_ERROR; break;
    }
    
    std::string levelStr = logLevelToStr(level);
    
    if (logFile_.is_open()) {
        // Проверяем ротацию
        if (needsRotation()) {
            try {
                rotateLog();
            } catch (const std::exception& e) {
                if (logFile_.is_open()) {
                    log(LogLevel::ERROR, "Failed to rotate log file: " + std::string(e.what()));
                } else {
                    std::cerr << color << "Failed to rotate log file: " << e.what() << COLOR_RESET << std::endl;
                }
                // Продолжаем логирование в старом файле
            }
        }

        // Форматированная запись в лог-файл без цвета
        try {
            logFile_ << "[" << getCurrentTime(false) << "] [" << levelStr << "] " << message << std::endl;
            logFile_.flush(); // Принудительно сбрасываем буфер
        } catch (const std::exception& e) {
            std::cerr << color << "Error writing to log file: " << e.what() << COLOR_RESET << std::endl;
            // Переход на fallback
            fallbackBuffer_ << "[" << getCurrentTime(false) << "] [" << levelStr << "] " << message << std::endl;
            fallbackUsed_ = true;
        }
        
        // Вывод в консоль с цветом
        std::cerr << color << "[" << getCurrentTime(false) << "] [" << levelStr << "] " << message << COLOR_RESET << std::endl;
    } else {
        // Если файл не открыт, используем fallback и выводим в консоль с цветом
        std::cerr << color << "[" << getCurrentTime(false) << "] [" << levelStr << "] " << message << COLOR_RESET << std::endl;
        fallbackBuffer_ << "[" << getCurrentTime(false) << "] [" << levelStr << "] " << message << std::endl;
        fallbackUsed_ = true;
    }
}

// Публичные методы
void Logger::debug(const std::string& message) { log(LogLevel::DEBUG, message); }
void Logger::info(const std::string& message) { log(LogLevel::INFO, message); }
void Logger::warn(const std::string& message) { log(LogLevel::WARNING, message); }
void Logger::error(const std::string& message) { log(LogLevel::ERROR, message); }

void Logger::setLevel(LogLevel level) {
    std::lock_guard<std::mutex> lock(mutex_);
    minLevel_ = level;
}

void Logger::setLogPath(std::string filename) {
    filename_ = filename;
}

Logger::LogLevel Logger::getLevel() { return minLevel_; }

std::string Logger::getLogPath() { return filename_; }

void Logger::flushFallbackBuffer() {
    if (fallbackUsed_) {
        std::cerr << fallbackBuffer_.str();
        fallbackBuffer_.str(std::string());
        fallbackUsed_ = false;
    }
}

Logger::LogLevel Logger::strToLogLevel(std::string level) {
    LogLevel newLevel;

    if (level == "info") {
        newLevel = LogLevel::INFO;
    } else if (level == "debug") {
        newLevel = LogLevel::DEBUG;
    } else if (level == "warning") {
        newLevel = LogLevel::WARNING;
    } else if (level == "error") {
        newLevel = LogLevel::ERROR;
    } else {
        throw std::runtime_error("Fatal error: Unknown type of log level. Run service with --help to get usage hints.");
    }

    return newLevel;
}

std::string Logger::logLevelToStr(Logger::LogLevel level) { 
    std::string strLevel = "";
    switch (level) {
        case Logger::LogLevel::DEBUG:   strLevel = "DEBUG"; break;
        case Logger::LogLevel::INFO:    strLevel = "INFO"; break;
        case Logger::LogLevel::WARNING: strLevel = "WARN"; break;
        case Logger::LogLevel::ERROR:   strLevel = "ERROR"; break;
    }
    return strLevel; 
}