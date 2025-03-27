#include "../includes/logger.hpp"

// Статические переменные
std::ofstream Logger::logFile_;
std::mutex Logger::mutex_;
std::string Logger::filename_;
bool Logger::rotateBySize_ = true;
size_t Logger::maxSize_ = DEFAULT_LOGSIZE;
LogLevel Logger::minLevel_ = LogLevel::INFO;

// Инициализация логгера
void Logger::init(const std::string& filename, bool rotateBySize, size_t maxSize) {
    std::lock_guard<std::mutex> lock(mutex_);
    filename_ = filename;
    rotateBySize_ = rotateBySize;
    maxSize_ = maxSize;

    // Открываем файл в режиме добавления
    logFile_.open(filename_, std::ios::app);
    if (!logFile_.is_open()) {
        throw std::runtime_error("Cannot open log file: " + filename_);
    }
}

// Закрытие файла (вызывается при завершении)
void Logger::close() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (logFile_.is_open()) {
        logFile_.close();
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

    if (logFile_.is_open()) {
        logFile_.close();
    }

    // Генерируем имя для старого лога (добавляем timestamp)
    std::string oldLog = filename_ + "." + getCurrentTime(true);

    // Переименовываем текущий лог
    if (rename(filename_.c_str(), oldLog.c_str())) {
        logFile_.open(filename_, std::ios::app); // Пытаемся восстановить
        throw std::runtime_error("Failed to rotate log file");
    }

    // Открываем новый файл
    logFile_.open(filename_, std::ios::app);
    if (!logFile_.is_open()) {
        throw std::runtime_error("Cannot reopen log file after rotation");
    }
}

// Форматирование времени
std::string Logger::getCurrentTime(bool forFilename) {
    time_t now = time(nullptr);
    struct tm tmStruct;
    localtime_r(&now, &tmStruct);

    char buffer[80];
    if (forFilename) {
        strftime(buffer, sizeof(buffer), "%Y%m%d_%H%M%S", &tmStruct);
    } else {
        strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &tmStruct);
    }
    return std::string(buffer);
}

// Основная функция логирования
void Logger::log(LogLevel level, const std::string& message) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (level < minLevel_) { return; }
    
    if (!logFile_.is_open()) {
        throw std::runtime_error("Log file is not open");
    }

    // Проверяем ротацию
    if (needsRotation()) {
        rotateLog();
    }

    // Уровни логирования
    const char* levelStr = "";
    switch (level) {
        case LogLevel::DEBUG:   levelStr = "DEBUG"; break;
        case LogLevel::INFO:    levelStr = "INFO"; break;
        case LogLevel::WARNING: levelStr = "WARN"; break;
        case LogLevel::ERROR:   levelStr = "ERROR"; break;
    }

    // Форматированная запись
    logFile_ << "[" << getCurrentTime(false) << "] [" << levelStr << "] " << message << std::endl;
    logFile_.flush(); // Принудительно сбрасываем буфер
}

// Публичные методы
void Logger::info(const std::string& message) { log(LogLevel::INFO, message); }
void Logger::warn(const std::string& message) { log(LogLevel::WARNING, message); }
void Logger::error(const std::string& message) { log(LogLevel::ERROR, message); }

void Logger::setMinLevel(LogLevel level) {
    std::lock_guard<std::mutex> lock(mutex_);
    minLevel_ = level;
}