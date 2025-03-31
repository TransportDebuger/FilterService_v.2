/*!
  \file logger.hpp
  \author Artem Ulyanov
  \version 1
  \date March, 2024
  \brief Файл описания системы логгирования в проекте filter_service.
  \details Файл является неотъемлимой частью проекта filter_service.
    Данный файл содержит описание макросов, перечислений, классов, из свойств и методов необходимых для реализации функций ведения журнала.
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

/*!
    \brief Размер файла журнала по умолчению.
*/
#define DEFAULT_LOGSIZE 10485760

/*!
    \brief Перечисление уровней детализации журналирования
*/
enum class LogLevel {
    DEBUG, ///< Отладочные сообщения
    INFO, ///< Информационные сообщения
    WARNING, ///< Предупреждения
    ERROR ///< Ошибки
};

/*!
    \brief Класс логгера
    \details Класс реализующий функциональность логгера
*/
class Logger {
public:
    static void init(const std::string& filename, bool rotateBySize = true, size_t maxSize = DEFAULT_LOGSIZE);
    static void initFallback();
    static void setMinLevel(LogLevel level);

    static void debug(const std::string& message);
    static void info(const std::string& message);
    static void warn(const std::string& message);
    static void error(const std::string& message);

    static void rotateLog();
    static void Logger::flushFallbackBuffer();
    static void close();

private:
    static std::ofstream logFile_;
    static LogLevel minLevel_;
    static std::mutex mutex_;
    static std::string filename_;
    static bool rotateBySize_;
    static size_t maxSize_;
    static std::stringstream fallbackBuffer_;
    static bool fallbackUsed_;

    static std::string getCurrentTime(bool forFilename) ;
    static void log(LogLevel level, const std::string& message);
    static bool needsRotation();
};

LogLevel strToLogLevel(std::string level);
std::string logLevelToStr(LogLevel level);