/**
 * @file logger.hpp
 * @author Artem Ulyanov
 * @date March 2025
 * @brief Заголовочный файл класса Logger и вспомогательных компонентов.
 * 
 * @details Содержит объявление статического класса Logger, реализующего систему логирования с поддержкой:
 *          - Многоуровневого журналирования (DEBUG, INFO, WARNING, ERROR, FATAL).
 *          - Ротации лог-файлов по размеру.
 *          - Потокобезопасности через механизмы блокировок (std::mutex).
 *          - Fallback-режима с буферизацией сообщений в памяти при недоступности файла.
 * 
 * @note Основные компоненты:
 *       - Класс Logger: ядро системы логирования.
 *       - Класс FallbackBufferGuard: RAII-обёртка для гарантированного сброса буфера при завершении.
 *       - Перечисление LogLevel: уровни детализации сообщений.
 * 
 * @warning Для корректной работы необходимо:
 *          - Инициализировать логгер через Logger::init() перед использованием.
 *          - Избегать ручного управления ресурсами (файлами, буферами) в обход методов класса.
 * 
 * @section Потокобезопасность
 * @par Гарантии:
 * - Все публичные методы класса Logger защищены мьютексом @b mutex_, что исключает состояние гонки при одновременном вызове из разных потоков.
 * - Операции с fallback-буфером (запись, сброс) защищены отдельным мьютексом @b fallbackMutex_.
 * - Ротация логов и запись в файл выполняются атомарно в рамках критических секций.
 * 
 * @par Ограничения:
 * - Изменение параметров логгера (например, setLogPath(), setLogRotation()) во время активной записи логов может привести к кратковременной блокировке потоков.
 * - Вызов методов логирования (debug(), info(), error()) из высоконагруженных потоков может создать contention из-за блокировок.
 * - Рекурсивный вызов методов Logger из одного потока безопасен, так как используется std::mutex (не рекурсивный мьютекс).
 * 
 * @par Рекомендации:
 * - Для снижения нагрузки используйте асинхронное логирование или буферизацию сообщений вне класса.
 * - Избегайте вызова методов настройки логгера (setLogPath(), setLevel()) в критических по времени участках кода.
 * 
 * @see Logger
 * @see FallbackBufferGuard
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
 * @def DEFAULT_LOGSIZE
 * @brief Макрос, задающий размер лог-файла по умолчанию (10 МБ) в байтах.
 * @details Используется, если параметр размера не указан явно в setLogRotation().
 * @value 10485760 (10 * 1024 * 1024 байт).
 * @see Logger::setLogRotation
 */
#define DEFAULT_LOGSIZE 10485760

/**
 * @class FallbackBufferGuard
 * @brief RAII-класс для автоматического управления fallback-буфером логгера (Logger).
 * @details Обеспечивает безопасный сброс данных из буфера в стандартный поток ошибок (stderr) 
 *          при уничтожении объекта. Используется для гарантированного сохранения логов даже 
 *          при аварийном завершении программы.
 * 
 * @note Не владеет ресурсами буфера, а лишь управляет их освобождением. 
 *       Работает с переданными по ссылке:
 *       - буфером сообщений (std::stringstream),
 *       - флагом использования буфера (bool),
 *       - мьютексом для синхронизации доступа (std::mutex).
 * 
 * @warning Не должен создаваться вручную. Экземпляр класса является статическим членом Logger.
 */
class FallbackBufferGuard {
public:
    /**
 * @brief Конструктор класса FallbackBufferGuard.
 * @param buffer    Ссылка на буфер сообщений (std::stringstream), 
 *                  куда временно сохраняются логи при недоступности файла.
 * @param usedFlag  Ссылка на флаг (bool), указывающий, 
 *                  что fallback-буфер был использован.
 * @param mutex     Ссылка на мьютекс (std::mutex), 
 *                  обеспечивающий потокобезопасный доступ к буферу.
 * @warning Не создавайте экземпляры этого класса вручную. 
 *          Объект должен быть статическим членом класса Logger.
 */
    FallbackBufferGuard(std::stringstream& buffer, bool& usedFlag, std::mutex& mutex)
        : buffer_(buffer), usedFlag_(usedFlag), mutex_(mutex) {}
    
    /**
 * @brief Деструктор класса FallbackBufferGuard.
 * @details При уничтожении объекта:
 *          - Если буфер был использован (usedFlag_ == true), 
 *            его содержимое сбрасывается в stderr.
 *          - Буфер и флаг сбрасываются в исходное состояние.
 *          - Операции защищены мьютексом для потокобезопасности.
 */
    ~FallbackBufferGuard();
    
private:
    /**
 * @brief Ссылка на буфер сообщений.
 * @details Внешний буфер, переданный из класса Logger. 
 *          Не владеет ресурсом.
 */
    std::stringstream& buffer_; ///< Ссылка на буфер для хранения сообщений.
    
    /**
 * @brief Ссылка на флаг использования буфера.
 * @details Указывает, были ли данные записаны в fallback-буфер.
 *          Управляется классом Logger.
 */
    bool& usedFlag_;

    /**
 * @brief Ссылка на мьютекс для синхронизации.
 * @details Обеспечивает эксклюзивный доступ к буферу и флагу 
 *          из разных потоков.
 */
    std::mutex& mutex_;
};

/**
 * @class Logger
 * @brief Статический класс для управления логированием в приложении.
 * @details Обеспечивает запись логов в файл и/или консоль с поддержкой:
 *          - Уровней логирования (DEBUG, INFO, WARNING, ERROR, FATAL).
 *          - Ротации лог-файлов по размеру.
 *          - Fallback-буфера для сохранения логов при недоступности файла.
 *          - Потокобезопасности через мьютексы.
 * 
 * @note Все методы и поля класса статические. Для использования необходимо:
 *       1. Вызвать Logger::init() или Logger::initFallback().
 *       2. Настроить параметры (уровень логирования, путь к файлу и т.д.).
 *       3. Использовать методы debug(), info(), error() и др. для записи логов.
 * 
 * @warning При изменении настроек (например, setLogPath()) во время работы
 *          требуется синхронизация с другими потоками.
 */
class Logger {
public:

    /**
 * @enum Logger::LogLevel
 * @brief Уровни детализации журналирования.
 * @details Определяет степень важности сообщений, влияющую на их фильтрацию и запись.
 *          Сообщения с уровнем ниже текущего минимального (minLevel_) игнорируются.
 * 
 * @var Logger::LogLevel::DEBUG
 * @brief Подробные отладочные сообщения.
 * @details Используется для диагностики работы приложения. 
 *          Пример: вывод внутренних состояний, значений переменных.
 * 
 * @var Logger::LogLevel::INFO
 * @brief Информационные сообщения о ходе выполнения.
 * @details Отражает ключевые этапы работы. 
 *          Пример: "Сервис запущен", "Конфигурация загружена".
 * 
 * @var Logger::LogLevel::WARNING
 * @brief Предупреждения о нештатных ситуациях.
 * @details Указывает на потенциальные проблемы, не останавливающие работу. 
 *          Пример: "Неверный формат записи в кэше".
 * 
 * @var Logger::LogLevel::ERROR
 * @brief Ошибки, нарушающие часть функциональности.
 * @details Приложение продолжает работать, но некоторые функции недоступны. 
 *          Пример: "Сбой подключения к базе данных".
 * 
 * @var Logger::LogLevel::FATAL
 * @brief Критические ошибки, требующие завершения работы.
 * @details Приложение не может продолжать выполнение. 
 *          Пример: "Недостаточно памяти", "Повреждение конфигурационного файла".
 * 
 * @note Минимальный уровень задаётся через setLevel(). По умолчанию: INFO.
 * @see Logger::setLevel
 * @see Logger::log
 */
    enum class LogLevel {
        DEBUG,
        INFO,
        WARNING,
        ERROR,
        FATAL
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

    static void flushFallbackBuffer();
    static void close();

private:
    /**
 * @var Logger::logFile_
 * @brief Дескриптор открытого лог-файла.
 * @details Управляется методами init(), close(), rotateLog(). 
 *          Если файл не открыт, логи перенаправляются в fallback-буфер.
 * @note Потокобезопасность: доступ синхронизируется мьютексом mutex_.
 */
    static std::ofstream logFile_;
    
    /**
 * @var Logger::minLevel_
 * @brief Минимальный уровень логирования.
 * @details Сообщения с уровнем ниже этого значения игнорируются.
 *          Значение по умолчанию: LogLevel::INFO.
 * @see LogLevel
 * @see setLevel()
 */
    static LogLevel minLevel_; 
    
    /**
 * @var Logger::mutex_
 * @brief Основной мьютекс для синхронизации операций.
 * @details Защищает:
 *          - Открытие/закрытие файла
 *          - Изменение настроек (уровня, пути)
 *          - Запись логов
 * @warning Не рекурсивный. Рекурсивные вызовы методов Logger могут вызвать deadlock.
 */
    static std::mutex mutex_;  
    
    /**
 * @var Logger::filename_
 * @brief Путь к текущему лог-файлу.
 * @details Значение по умолчанию: "./filter-service.log".
 *          Может быть изменён через setLogPath().
 * @note Потокобезопасность: изменения защищены мьютексом mutex_.
 */
    static std::string filename_;
   
    /**
 * @var Logger::rotateBySize_
 * @brief Флаг активации ротации логов по размеру.
 * @details true - ротация включена, false - отключена.
 *          Управляется через setLogRotation().
 * @note Потокобезопасность: изменения защищены мьютексом mutex_.
 */
    static bool rotateBySize_;
    
    /**
 * @var Logger::maxSize_
 * @brief Максимальный размер лог-файла в байтах.
 * @details При достижении этого размера выполняется ротация.
 *          Значение по умолчанию: DEFAULT_LOGSIZE (10 МБ).
 * @see setLogRotation()
 */
    static size_t maxSize_;
    
    /**
 * @var Logger::fallbackBuffer_
 * @brief Буфер для временного хранения логов при недоступности файла.
 * @details Используется в fallback-режиме. Содержит сообщения в формате:
 *          "[ВРЕМЯ] [УРОВЕНЬ] ТЕКСТ".
 * @note Потокобезопасность: доступ защищён fallbackMutex_.
 */
    static std::stringstream fallbackBuffer_;
    
    /**
 * @var Logger::fallbackUsed_
 * @brief Флаг использования fallback-режима.
 * @details true - буфер содержит данные, false - буфер пуст.
 * @note Потокобезопасность: доступ защищён fallbackMutex_.
 */
    static bool fallbackUsed_;
    
    /**
 * @var Logger::fallbackMutex_
 * @brief Мьютекс для синхронизации доступа к fallback-буферу.
 * @details Защищает:
 *          - Запись в fallbackBuffer_
 *          - Изменение fallbackUsed_
 * @note Отдельный от основного мьютекса для минимизации блокировок.
 */
    static std::mutex fallbackMutex_;
    
    /**
 * @var Logger::fallbackGuard_
 * @brief RAII-объект для автоматического сброса fallback-буфера.
 * @details Гарантирует вывод данных из буфера в stderr при завершении программы.
 * @see FallbackBufferGuard
 */
    static FallbackBufferGuard fallbackGuard_;

    static std::string getCurrentTime(bool forFilename);
    static void log(LogLevel level, const std::string& message);
    static void rotateLog();
    static bool needsRotation();
    static std::string logLevelToStr(LogLevel level);
};