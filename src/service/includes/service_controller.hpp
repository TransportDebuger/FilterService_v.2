/**
 * @file service_controller.hpp
 * @author Ваше имя
 * @date Март 2024
 * @brief Основной контроллер сервиса фильтрации XML
 * 
 * @details Класс ServiceController реализует:
 * - Обработку аргументов командной строки
 * - Управление жизненным циклом сервиса
 * - Взаимодействие с модулями логирования и мастер-процессом
 * - Механизмы перезагрузки конфигурации
 * 
 * @section Основные_компоненты
 * - run(): Точка входа в приложение
 * - parseArguments(): Парсинг CLI параметров
 * - sendReloadSignal(): Отправка сигналов работающему процессу
 * 
 * @note Для корректной работы требуется:
 * - Предварительная инициализация Logger
 * - Наличие конфигурационного файла по умолчанию (config.json)
 * 
 * @warning Не поддерживает параллельный запуск нескольких экземпляров
 * 
 * @see Logger
 * @see Master
 * @see SignalHandler
 */

#pragma once

#include <string>

#include "../includes/logger.hpp"
#include "../includes/master.hpp"

/**
 * @class ServiceController
 * @brief Центральный управляющий класс сервиса фильтрации XML
 * 
 * @ingroup CoreComponents
 * 
 * @details Обеспечивает полный цикл работы сервиса, включая:
 * - Парсинг и валидацию аргументов командной строки
 * - Инициализацию и конфигурацию подсистем
 * - Управление главным рабочим циклом
 * - Обработку системных сигналов
 * - Механизмы graceful shutdown и перезагрузки
 * 
 * @section Архитектура
 * Класс выступает фасадом для основных компонентов системы:
 * - Logger - система логирования
 * - Master - управление worker-процессами
 * - SignalHandler - обработка POSIX-сигналов
 * 
 * @section Жизненный_цикл
 * 1. Инициализация:
 *    - Парсинг CLI параметров (parseArguments())
 *    - Создание PID-файла (initialize())
 *    - Старт Master-процесса
 * 2. Главный цикл (mainLoop()):
 *    - Обработка сигналов
 *    - Проверка состояния воркеров
 *    - Ожидание событий
 * 3. Завершение:
 *    - Корректная остановка Master
 *    - Удаление PID-файла
 *    - Закрытие логгера
 * 
 * @subsection Сигналы
 * Обрабатываемые сигналы:
 * - SIGTERM/SIGINT: Graceful shutdown
 * - SIGHUP: Hot reload конфигурации
 * 
 * @note Требования к окружению:
 * - Наличие config.json или указание альтернативного конфига
 * - Права на запись в /var/run/ для PID-файла
 * - Доступ к указанным в конфиге ресурсам
 * 
 * @warning Особенности безопасности:
 * - PID-файл создается с правами 0644
 * - Перезагрузка требует валидных credentials в конфиге
 * - Нет встроенной защиты от race condition при параллельном запуске
 * 
 * @example Пример запуска:
 * @code{.sh}
 * ./service --config /etc/service_conf.json --log-level debug
 * ./service --reload
 * @endcode
 * 
 * @see Документация по архитектуре: docs/architecture.md
 * @see Пример конфигурации: configs/service.example.json
 */
class ServiceController {
public:
    int run(int argc, char** argv);                                    
    
private:
    bool parseArguments(int argc, char** argv);                        
    void initialize();                                                 
    void mainLoop();
    bool sendReloadSignal();                                                   
    void printHelp() const;                                            

    /**
     * @brief Путь к конфигурационному файлу сервиса
     * @details Хранит абсолютный или относительный путь к JSON-конфигурации.
     *          Значение по умолчанию: "./config.json".
     *          Может быть изменен через аргумент --config.
     * @note Файл должен содержать валидные настройки источников и логирования.
     * @see parseArguments()
     */
    std::string config_path_ = "./config.json";                      
    
    /**
     * @brief Ядро управления worker-процессами
     * @details Отвечает за:
     * - Запуск/остановку worker-процессов
     * - Перезагрузку конфигурации
     * - Мониторинг состояния воркеров
     * @warning Все взаимодействия с master_ должны синхронизироваться через его внутренний мьютекс.
     * @see Master
     */
    Master master_;                                                    
};