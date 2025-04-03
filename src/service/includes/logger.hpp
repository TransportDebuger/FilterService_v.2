/**
 * @file logger.hpp
 * @author Artem Ulyanov
 * @version 1
 * @date March, 2025
 * @brief Заголовочный файл описания класса Logger и макросов.
 * @details Файл содержит определение макросов и класса Logger, реализующего функциональность процесса журналирования.
*/

#pragma once

#include <string>
#include <fstream>
#include <mutex>
#include <stdexcept>
#include <iostream>
#include <chrono>
#include <sstream>
#include <ctime>
#include <iomanip>
#include <sys/stat.h>

/**
 *   @brief Макрос определения размера логфайла (в байтах).
*/
#define DEFAULT_LOGSIZE 10485760

/**
 *   @brief Класс логгера
 *   @details Класс реализующий функциональность логгера
*/
class Logger {
public:
    /**
     *   @brief Перечисление уровней детализации журналирования.
    */
    enum class LogLevel {
        DEBUG,   ///< Подробные сообщения, используемые во время отладки приложения
        INFO,    ///< Информационные сообщения о том, что происходит в приложении
        WARNING, ///< Предупреждения о возникновении нежелательных сиутаций
        ERROR,   ///< Ошибки при которых приложение способно продолжить работать
        FATAL    ///< Фатальные ошибки, приводящие к завершению работы приложения
    };

    static void init();
    static void initFallback();
    
    static void setLevel(const LogLevel level);
    static void setLogPath(const std::string& filename);
    static void setLogRotation(bool isRotated, size_t logSize = DEFAULT_LOGSIZE);
    static void setLogSize(size_t logSize);
    static LogLevel getLevel();
    static std::string getLogPath();
    static LogLevel strToLogLevel(const std::string& level);

    static void debug(const std::string& message);
    static void info(const std::string& message);
    static void warn(const std::string& message);
    static void error(const std::string& message);
    static void fatal(const std::string& message);

    static void rotateLog();
    static void flushFallbackBuffer();
    static void close();

private:
    static std::ofstream logFile_;  ///< Дескриптор лог-файла
    static LogLevel minLevel_;      ///< Минимально заданный уровень логирования для логера.
    static std::mutex mutex_;  
    static std::string filename_;   ///< Имя логфайла
    static bool rotateBySize_;      ///< Признак необходимости ротации лога (если true, то осуществляется ротация лог-файлов)
    static size_t maxSize_;         ///< Максимальный размер лог-файла (игнорируется при rotateBySize_ = false)
    static std::stringstream fallbackBuffer_; ///< FallBack буфер сообщений логирования
    static bool fallbackUsed_;      ///< Признак использования fallback логирования

    static std::string getCurrentTime(bool forFilename);
    static void log(LogLevel level, const std::string& message);
    static bool needsRotation();
    static std::string logLevelToStr(LogLevel level);
};