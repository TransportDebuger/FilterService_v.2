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
    \brief Класс логгера
    \details Класс реализующий функциональность логгера
*/
class Logger {
public:
    /*!
        \brief Перечисление уровней детализации журналирования
    */
    enum class LogLevel {
        DEBUG, ///< Отладочные сообщения
        INFO, ///< Информационные сообщения
        WARNING, ///< Предупреждения
        ERROR ///< Ошибки
    };
    static void init();
    static void initFallback();
    
    static void setLevel(LogLevel level);
    static void setLogPath(std::string filename);
    static void setLogRotation(bool isRotated, size_t logSize = DEFAULT_LOGSIZE);
    static void setLogSize(size_t logSize);
    static LogLevel getLevel();
    static std::string getLogPath();
    static LogLevel strToLogLevel(std::string level);


    static void debug(const std::string& message);
    static void info(const std::string& message);
    static void warn(const std::string& message);
    static void error(const std::string& message);

    static void rotateLog();
    static void flushFallbackBuffer();
    static void close();

private:
    static std::ofstream logFile_;  ///< Дескриптор лог-файла
    static LogLevel minLevel_;      ///< Минимально заданный уровень логирования сервиса (может переопределяться параметрами CLI и файлом конфигурации)
    static std::mutex mutex_;  
    static std::string filename_;   ///< Имя логфайла
    static bool rotateBySize_;      ///< Признак необходимости ротации лога (задается параметрами CLI или определяется в файле конфигурации)
    static size_t maxSize_;         ///< Максимальный размер лог-файла (игнорируется при ротации)
    static std::stringstream fallbackBuffer_; ///< FallBack буфер сообщений логирования
    static bool fallbackUsed_;      ///< Признак использования fallback логирования

    static std::string getCurrentTime(bool forFilename) ;
    static void log(LogLevel level, const std::string& message);
    static bool needsRotation();
    static std::string logLevelToStr(LogLevel level);
};