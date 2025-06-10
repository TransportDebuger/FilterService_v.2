/**
 * @file DaemonManager.hpp
 * @brief Управление демонизацией процессов и работой с PID-файлами
 * 
 * @author Artem Ulyanov
 * @date Май 2025
 * @version 1.0
 * @license MIT
 * 
 * @details Библиотека предоставляет безопасное управление:
 * - Демонизацией процессов через двойной fork
 * - Созданием/удалением PID-файлов
 * - Проверкой дублирующихся процессов
 * - Очисткой ресурсов при завершении
 */

#pragma once
#include <string>
#include <system_error>
#include <filesystem>

namespace stc {

/**
 * @class DaemonManager
 * @brief Класс для управления жизненным циклом демонизированных процессов
 * 
 * @details Класс является RAII-оберткой и предназначен для управление демонизацией процесса, работой с PID-файлом, проверкой наличия уже запущенного процесса, корректным завершением.
 * Класс не зависит от бизнес-логики, не содержит специфичных для предметной области деталей и может использоваться при реализации любых системных демонов и сервисов (которые должны работать в фоне, запускаться из systemd, init, cron, docker и др.) на Linux/Unix.
 * 
 * Преимущества:
 *  - Гарантирует атомарность создания и удаления PID-файла.
 *  - Защищает от двойного запуска сервиса.
 *  - Универсальная обработка ошибок демонизации и управления жизненным циклом.
 * 
 * @note Основные возможности:
 * - Автоматическая демонизация через double-fork
 * - Потокобезопасное управление PID-файлами
 * - Graceful shutdown с очисткой ресурсов
 * - Защита от множественных запусков
 * 
 * @warning 
 * - Не используйте несколько экземпляров для одного PID-файла
 * - Требует прав на запись в целевой директории
 */
class DaemonManager {
public:

    /**
     * @brief Конструктор с инициализацией пути к PID-файлу
     * @param pidPath Путь к PID-файлу (может быть относительным)
     * @throw std::system_error При ошибках:
     * - Невозможно разрешить абсолютный путь
     * - Обнаружен работающий процесс
     * 
     * @code
     * try {
     *     DaemonManager dm("/var/run/myapp.pid");
     * } catch (const std::exception& e) {
     *     // Обработка ошибок
     * }
     * @endcode
     */
    explicit DaemonManager(const std::string& pidPath);

    /**
     * @brief Деструктор, автоматически вызывает cleanup()
     * @note Гарантирует удаление PID-файла если он был создан
     */
    ~DaemonManager();

    DaemonManager(const DaemonManager& other) = delete;
    DaemonManager& operator=(const DaemonManager& other) = delete;
    
    /**
     * @brief Демонизирует текущий процесс
     * @throw std::system_error При ошибках:
     * - fork()
     * - setsid()
     * - chdir()
     * 
     * @note Последовательность действий:
     * 1. Первый fork() и завершение родителя
     * 2. Создание новой сессии через setsid()
     * 3. Второй fork() для полного отсоединения
     * 4. Смена рабочей директории на /
     * 5. Сброс umask
     * 6. Закрытие стандартных дескрипторов
     */
    void daemonize();

    /**
     * @brief Записывает PID текущего процесса в файл
     * @throw std::system_error При ошибках:
     * - Открытия файла
     * - Записи данных
     * - Установки прав (chmod)
     * 
     * @note Устанавливает права 0644 на файл
     */
    void writePid();

    /**
     * @brief Удаляет PID-файл если он был создан
     * @note Вызывается автоматически в деструкторе
     */
    void cleanup() noexcept;

private:
    
    /**
     * @brief Проверяет наличие работающего процесса по PID-файлу
     * @throw std::runtime_error Если процесс уже запущен
     */
    void checkExistingProcess();

    /**
     * @brief Удаляет устаревший PID-файл
     * @note Вызывается при обнаружении PID несуществующего процесса
     */
    void removeStalePid() noexcept;
    
    std::filesystem::path mPidPath; ///< Абсолютный путь к PID-файлу. Должен быть уникальным для каждого демона.
    bool mPidWritten = false;       ///< Флаг успешной записи PID. Предотвращает удаление чужих файлов.
};

} // namespace stc

/**
 * @example Пример использования
 * @code
 * try {
 *     stc::DaemonManager dm("/var/run/myapp.pid");
 *     dm.daemonize();
 *     dm.writePid();
 *     
 *     // Основная логика демона
 *     
 * } catch (const std::system_error& e) {
 *     std::cerr << "System error: " << e.what() << std::endl;
 *     return EXIT_FAILURE;
 * } catch (const std::exception& e) {
 *     std::cerr << "Error: " << e.what() << std::endl;
 *     return EXIT_FAILURE;
 * }
 * @endcode
 */