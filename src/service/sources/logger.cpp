/**
 * @file logger.cpp
 * @author Artem Ulyanov
 * @date March 2025
 * @brief Реализация статического класса Logger и его вспомогательных компонентов.
 * 
 * @details Этот файл содержит реализацию методов для:
 *          - Записи логов в файл и консоль с поддержкой уровней (DEBUG, INFO, WARNING, ERROR, FATAL).
 *          - Ротации лог-файлов при достижении заданного размера.
 *          - Управления fallback-буфером для сохранения логов при ошибках файловой системы.
 *          - Потокобезопасного доступа к ресурсам через мьютексы.
 * 
 * @note Особенности реализации:
 *       - Все статические члены класса инициализируются в этом файле.
 *       - Для форматирования времени используется std::put_time и std::chrono.
 *       - Обработка ошибок файловых операций (открытие/закрытие/переименование) интегрирована в методы.
 * 
 * @warning Внимание:
 *          - Методы класса не должны вызываться до инициализации (Logger::init()).
 *          - Рекурсивный вызов методов Logger из одного потока может привести к deadlock 
 *            из-за использования нерекурсивных мьютексов.
 * 
 * @section Потокобезопасность
 * @par Реализация:
 * - Каждый публичный метод защищён мьютексом @b mutex_ для обеспечения атомарности операций.
 * - Fallback-буфер и связанные с ним флаги защищены отдельным мьютексом @b fallbackMutex_.
 * - Критические секции минимальны по времени выполнения для снижения contention.
 * 
 * @par Примеры:
 * @code
 * Logger::init(); // Инициализация перед использованием
 * Logger::info("Сервис запущен"); // Потокобезопасная запись
 * Logger::close(); // Корректное завершение работы
 * @endcode
 * 
 * @see Logger
 * @see FallbackBufferGuard
 */


#include "../includes/logger.hpp"
#include "logger.hpp"
//#include "logger.hpp"

/**
 * @def COLOR_FATAL
 * @brief ANSI-код цвета для сообщений уровня FATAL (красный).
 * @details Используется для окрашивания вывода в консоль.
 * @note Сбрасывается макросом COLOR_RESET.
 */
#define COLOR_FATAL "\033[31m"
/**
 * @def COLOR_ERROR
 * @brief ANSI-код цвета для сообщений уровня ERROR (жёлтый).
 * @details Используется для окрашивания вывода в консоль.
 * @note Сбрасывается макросом COLOR_RESET.
 */
#define COLOR_ERROR "\033[33m"
/**
 * @def COLOR_WARN
 * @brief ANSI-код цвета для сообщений уровня WARNING (пурпурный).
 * @details Используется для окрашивания вывода в консоль.
 * @note Сбрасывается макросом COLOR_RESET.
 */
#define COLOR_WARN "\033[35m"
/**
 * @def COLOR_DEBUG
 * @brief ANSI-код цвета для сообщений уровня DEBUG (зелёный).
 * @details Используется для окрашивания вывода в консоль.
 * @note Сбрасывается макросом COLOR_RESET.
 */
#define COLOR_DEBUG "\033[32m"
/**
 * @def COLOR_INFO
 * @brief ANSI-код цвета для сообщений уровня INFO (голубой).
 * @details Используется для окрашивания вывода в консоль.
 * @note Сбрасывается макросом COLOR_RESET.
 */
#define COLOR_INFO "\033[36m"
/**
 * @def COLOR_RESET
 * @brief ANSI-код сброса цвета консоли.
 * @details Возвращает цвет терминала к стандартному.
 * @warning Всегда должен вызываться после COLOR_* макросов.
 */
#define COLOR_RESET "\033[0m"

// Статические переменные
std::mutex Logger::mutex_;
std::ofstream Logger::logFile_;
std::stringstream Logger::fallbackBuffer_;
std::mutex Logger::fallbackMutex_;
FallbackBufferGuard Logger::fallbackGuard_(
    Logger::fallbackBuffer_, 
    Logger::fallbackUsed_, 
    Logger::fallbackMutex_
);
std::string Logger::filename_ = "./filter-service.log";
bool Logger::rotateBySize_ = false;
size_t Logger::maxSize_ = DEFAULT_LOGSIZE;
Logger::LogLevel Logger::minLevel_ = Logger::LogLevel::DEBUG;
bool Logger::fallbackUsed_ = false;

void Logger::init() {
    std::lock_guard<std::mutex> lock(mutex_);

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
    std::lock_guard<std::mutex> fallbackLock(fallbackMutex_);
    fallbackUsed_ = true;
}

void Logger::close() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (logFile_.is_open()) {
        logFile_.flush(); // Принудительный сброс буфера
        logFile_.close();
    }
    
    // Сброс fallback буфера
    std::lock_guard<std::mutex> fallbackLock(fallbackMutex_);
    if (fallbackUsed_) {
        std::cerr << fallbackBuffer_.str();
        flushFallbackBuffer();
        fallbackUsed_ = false; // Сброс флага
    }
}

bool Logger::needsRotation() {
    if (!rotateBySize_) return false;

    struct stat statBuf;
    if (stat(filename_.c_str(), &statBuf) != 0) {
        return false; // Ошибка stat, пропускаем ротацию
    }
    return static_cast<size_t>(statBuf.st_size) >= maxSize_;
}

void Logger::rotateLog() {
    
    if (!logFile_.is_open()) {
        std::cerr << COLOR_ERROR << "[ERROR] Log file is not open, cannot rotate." << COLOR_RESET << std::endl;
        return;
    }

    logFile_.close();

    std::string oldLog = filename_ + "." + getCurrentTime(true);

    if (rename(filename_.c_str(), oldLog.c_str()) != 0) {
        std::cerr << "Failed to rename log file during rotation" << std::endl;
        
        try {
            logFile_.open(filename_, std::ios::app); // Восстановление
        } catch (const std::exception& e) {
            std::cerr << "Failed to reopen log file after failed rotation: " << e.what() << std::endl;
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
            logFile_ << "[" << getCurrentTime(false) << "] [ERROR] Error reopening log file after rotation: " << e.what() << std::endl;
            logFile_.flush(); // Принудительно сбрасываем буфер
            std::cerr << COLOR_ERROR << "[ERROR] Error reopening log file after rotation: " << e.what() << COLOR_RESET << std::endl;
        } else {
            std::lock_guard<std::mutex> fallbackLock(fallbackMutex_);
            fallbackBuffer_ << "[" << getCurrentTime(false) << "] [ERROR] Error reopening log file after rotation: " << e.what() << std::endl;
            fallbackUsed_ = true;            
            std::cerr << "Error reopening log file after rotation: " << e.what() << std::endl;
        }
        // Продолжаем логирование в старом файле
    }
}

std::string Logger::getCurrentTime(bool forFilename) {
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

void Logger::log(LogLevel level, const std::string& message) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (level < minLevel_) { return; }
    
    std::string color;
    switch (level) {
        case LogLevel::DEBUG:   color = COLOR_DEBUG; break;
        case LogLevel::INFO:    color = COLOR_INFO; break;
        case LogLevel::WARNING: color = COLOR_WARN; break;
        case LogLevel::ERROR:   color = COLOR_ERROR; break;
        case LogLevel::FATAL:   color = COLOR_FATAL; break;
    }
    
    std::string levelStr = logLevelToStr(level);
    
    if (logFile_.is_open()) {
        // Проверяем ротацию
        if (needsRotation()) {
            try {
                rotateLog();
            } catch (const std::exception& e) {
                if (logFile_.is_open()) {
                    logFile_ << "[" << getCurrentTime(false) << "] [ERROR] Failed to rotate log file: " << e.what() << std::endl;
                    logFile_.flush(); // Принудительно сбрасываем буфер
                    std::cerr << COLOR_ERROR << "[ERROR] Failed to rotate log file: " << e.what() << COLOR_RESET << std::endl;
                } else {
                    std::lock_guard<std::mutex> fallbackLock(fallbackMutex_);
                    fallbackBuffer_ << "[" << getCurrentTime(false) << "] [ERROR] Failed to rotate log file: " << e.what() << std::endl;
                    fallbackUsed_ = true;
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
            std::lock_guard<std::mutex> fallbackLock(fallbackMutex_);
            fallbackBuffer_ << "[" << getCurrentTime(false) << "] [" << levelStr << "] " << message << std::endl;
            fallbackUsed_ = true;
        }
        
        // Вывод в консоль с цветом
        std::cerr << color << "[" << getCurrentTime(false) << "] [" << levelStr << "] " << message << COLOR_RESET << std::endl;
    } else {
        // Если файл не открыт, используем fallback и выводим в консоль с цветом
        std::lock_guard<std::mutex> fallbackLock(fallbackMutex_);
        std::cerr << color << "[" << getCurrentTime(false) << "] [" << levelStr << "] " << message << COLOR_RESET << std::endl;
        fallbackBuffer_ << "[" << getCurrentTime(false) << "] [" << levelStr << "] " << message << std::endl;
        fallbackUsed_ = true;
    }
}

void Logger::debug(const std::string& message) { log(LogLevel::DEBUG, message); }

void Logger::info(const std::string& message) { log(LogLevel::INFO, message); }

void Logger::warn(const std::string& message) { log(LogLevel::WARNING, message); }

void Logger::error(const std::string& message) { log(LogLevel::ERROR, message); }

void Logger::fatal(const std::string& message) { log(LogLevel::FATAL, message); }

void Logger::setLevel(const LogLevel level) {
    std::lock_guard<std::mutex> lock(mutex_);
    minLevel_ = level;
}

void Logger::setLogPath(const std::string& filename) {
    std::lock_guard<std::mutex> lock(mutex_);
    filename_ = filename;
}

void Logger::setLogRotation(bool isRotated, size_t logSize) {
    rotateBySize_ = isRotated;
    maxSize_ = logSize;
}

void Logger::setLogSize(size_t logSize) {    
    maxSize_ = logSize;
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

Logger::LogLevel Logger::strToLogLevel(const std::string& level) {
    LogLevel newLevel;
    std::string lowLevel = level;
    std::transform(lowLevel.begin(), lowLevel.end(), lowLevel.begin(), ::tolower);

    if (lowLevel == "info") {
        newLevel = LogLevel::INFO;
    } else if (lowLevel == "debug") {
        newLevel = LogLevel::DEBUG;
    } else if (lowLevel == "warning") {
        newLevel = LogLevel::WARNING;
    } else if (lowLevel == "error") {
        newLevel = LogLevel::ERROR;
    }  else if (lowLevel == "fatal") {
        newLevel = LogLevel::FATAL;
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
        case Logger::LogLevel::FATAL:   strLevel = "FATAL"; break;
    }
    return strLevel;
}

FallbackBufferGuard::~FallbackBufferGuard() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (usedFlag_) {
        std::cerr << buffer_.str(); // Сброс в stderr
        buffer_.str("");            // Очистка буфера
        usedFlag_ = false;
    }
}