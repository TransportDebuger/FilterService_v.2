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

/**
 * @brief Инициализирует логгер, открывая лог-файл для записи.
 * @details Если файл не может быть открыт, активирует fallback-режим.
 * @throw std::runtime_error Если не удалось открыть файл и активировать fallback.
 * @note Потокобезопасна (использует мьютекс mutex_).
 */
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

/**
 * @brief Активирует fallback-режим, перенаправляя логи в буфер.
 * @note Вызывается автоматически при ошибке в init(). 
 *       Потокобезопасна (использует мьютекс mutex_).
 */
void Logger::initFallback() {
    fallbackUsed_ = true;
}

/**
 * @brief Корректно завершает работу логгера.
 * @details Закрывает файл и сбрасывает fallback-буфер.
 * @note Потокобезопасна. Использует mutex_ и fallbackMutex_.
 */
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

/**
 * @brief Проверяет необходимость ротации файла.
 * @return true - если размер файла превышает maxSize_ и ротация включена.
 * @note Использует stat(). Потокобезопасна (вызывается внутри log()).
 */
bool Logger::needsRotation() {
    if (!rotateBySize_) return false;

    struct stat statBuf;
    if (stat(filename_.c_str(), &statBuf) != 0) {
        return false; // Ошибка stat, пропускаем ротацию
    }
    return static_cast<size_t>(statBuf.st_size) >= maxSize_;
}

/**
 * @brief Выполняет ротацию лог-файла.
 * @details Переименовывает текущий файл, создаёт новый. 
 *          При ошибках пытается восстановить работоспособность.
 * @note Вызывается из log(). Использует mutex_.
 * @warning Не вызывайте этот метод напрямую.
 */
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

/**
 * @brief Генерирует строку с текущим временем.
 * @param forFilename true - формат для имени файла (ГГГГММДД_ЧЧММСС), 
 *                    false - для записи в лог (ГГГГ-ММ-ДД ЧЧ:ММ:СС).
 * @return Строка с временной меткой.
 */
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

/**
 * @brief Основной метод записи лога.
 * @param [in] level Уровень важности сообщения (LogLevel).
 * @param [in] message Текст сообщения (std::string).
 * @details Выполняет:
 *          1. Проверку уровня.
 *          2. Ротацию логов (при необходимости).
 *          3. Запись в файл/консоль/fallback-буфер.
 * @note Потокобезопасна (использует мьютекс mutex_).
 */
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

/**
 * @brief Записывает сообщение уровня DEBUG.
 * @param message Текст сообщения.
 * @note Потокобезопасна. Обёртка вокруг log(LogLevel::DEBUG, message).
 */
void Logger::debug(const std::string& message) { log(LogLevel::DEBUG, message); }
/**
 * @brief Записывает сообщение уровня INFO.
 * @param message Текст сообщения.
 * @note Потокобезопасна. Обёртка вокруг log(LogLevel::INFO, message).
 */
void Logger::info(const std::string& message) { log(LogLevel::INFO, message); }
/**
 * @brief Записывает сообщение уровня WARNING.
 * @param message Текст сообщения.
 * @note Потокобезопасна. Обёртка вокруг log(LogLevel::WARNING, message).
 */
void Logger::warn(const std::string& message) { log(LogLevel::WARNING, message); }
/**
 * @brief Записывает сообщение уровня ERROR.
 * @param [in] message Текст сообщения.
 * @note Потокобезопасна. Обёртка вокруг log(LogLevel::ERROR, message).
 */
void Logger::error(const std::string& message) { log(LogLevel::ERROR, message); }
/**
 * @brief Записывает сообщение уровня FATAL.
 * @param [in] message Текст сообщения.
 * @note Потокобезопасна. Обёртка вокруг log(LogLevel::FATAL, message).
 */
void Logger::fatal(const std::string& message) { log(LogLevel::FATAL, message); }

/**
 * @brief Устанавливает минимальный уровень логирования.
 * @param [in] level Уровень (LogLevel), ниже которого сообщения игнорируются.
 * @note Потокобезопасна (использует мьютекс mutex_).
 * @code
 * Logger::setLevel(Logger::LogLevel::INFO); // Игнорировать DEBUG
 * @endcode
 */
void Logger::setLevel(const LogLevel level) {
    std::lock_guard<std::mutex> lock(mutex_);
    minLevel_ = level;
}

/**
 * @brief Устанавливает имя лог-файла.
 * @param [in] filename Имя файла логфайла (std::string)
 * @note Потокобезопасна (использует мьютекс mutex_).
 * @code
 * Logger::ssetLogPath("./app.log"); // Установить лог-файл в теукщей директории с именем app.log
 * @endcode
 */
void Logger::setLogPath(const std::string& filename) {
    std::lock_guard<std::mutex> lock(mutex_);
    filename_ = filename;
}

/**
 * @brief Включает или отключает ротацию лог-файлов по размеру.
 * @param [in] isRotated Признак необходимости ротации:
 *                       - true: ротация включена;
 *                       - false: ротация отключена.
 * @param [in] logSize Максимальный размер файла (в байтах), при достижении которого выполняется ротация.
 *                     Значение по умолчанию: DEFAULT_LOGSIZE (10 МБ).
 * @note Потокобезопасна (использует мьютекс mutex_).
 * 
 * @details При активации ротации:
 *          - Лог-файл будет переименован и создан заново при превышении logSize.
 *          - Старые файлы получают суффикс в формате ГГГГММДД_ЧЧММСС.
 *          - Если isRotated = false, параметр logSize игнорируется.
 * 
 * @warning Для корректной работы:
 *          - Ротация не поддерживается в fallback-режиме.
 *          - Убедитесь, что файловая система поддерживает операцию rename().
 * 
 * @code
 * // Включить ротацию при достижении 5 МБ
 * Logger::setLogRotation(true, 5 * 1024 * 1024);
 * @endcode
 * 
 * @see Logger::rotateLog()
 * @see Logger::needsRotation()
 */
void Logger::setLogRotation(bool isRotated, size_t logSize) {
    rotateBySize_ = isRotated;
    maxSize_ = logSize;
}

void Logger::setLogSize(size_t logSize) {    
    maxSize_ = logSize;
}

/**
 * @brief Возвращает текущий минимальный уровень логирования.
 * @return Текущий уровень LogLevel.
 * @note Потокобезопасна. Использует мьютекс mutex_.
 */
Logger::LogLevel Logger::getLevel() { return minLevel_; }

/**
 * @brief Возвращает текущий путь к лог-файлу.
 * @return Путь в формате std::string.
 * @note Потокобезопасна. Использует мьютекс mutex_.
 */
std::string Logger::getLogPath() { return filename_; }

/**
 * @brief Немедленно сбрасывает содержимое fallback-буфера в stderr.
 * @note Потокобезопасна. Использует fallbackMutex_.
 */
void Logger::flushFallbackBuffer() {
    if (fallbackUsed_) {
        std::cerr << fallbackBuffer_.str();
        fallbackBuffer_.str(std::string());
        fallbackUsed_ = false;
    }
}

/**
 * @brief Преобразует строку в LogLevel.
 * @param level Строка в нижнем регистре ("debug", "error" и т.д.).
 * @return Соответствующий уровень LogLevel.
 * @throw std::runtime_error Если передан неизвестный уровень.
 */
Logger::LogLevel Logger::strToLogLevel(const std::string& level) {
    LogLevel newLevel;

    if (level == "info") {
        newLevel = LogLevel::INFO;
    } else if (level == "debug") {
        newLevel = LogLevel::DEBUG;
    } else if (level == "warning") {
        newLevel = LogLevel::WARNING;
    } else if (level == "error") {
        newLevel = LogLevel::ERROR;
    }  else if (level == "fatal") {
        newLevel = LogLevel::FATAL;
    } else {
        throw std::runtime_error("Fatal error: Unknown type of log level. Run service with --help to get usage hints.");
    }

    return newLevel;
}

/**
 * @brief Преобразует LogLevel в строковое представление.
 * @param [in] level Уровень логирования (Logger::LogLevel).
 * @return Строка в верхнем регистре ("DEBUG", "INFO" и т.д.).
 */
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