#pragma once
#include <string>

#include "../includes/logger.hpp"
//#include "../includes/master.hpp"
//#include "../includes/daemonizer.hpp"

/*!
    \brief Основной контроллер сервиса.
*/
class ServiceController {
public:
    int run(int argc, char** argv);                                    ///< Функция запуска работы контроллера.
    
private:
    bool parseArguments(int argc, char** argv);                        ///< Разбор параметров командной строки для инициализации контроллера
    void initialize();                                                 ///< Функция инициализации контроллера
    void mainLoop();                                                   ///< Функция выполнения процесса
    void printHelp() const;                                            ///< Функци печати справки при ошибках

    //bool run_as_daemon_ = false;
    LogLevel level_ = LogLevel::INFO;                                  ///< Значение по умолчанию уровня логирования. Может быть переопределено параметром запуска или файлом конфигурации.
    std::string config_path_ = "./config.json";  ///< Путь к конфигруационному файлу по умолчанию.
    //std::string log_path_ = "/etc/xml_filter_service/config.json";   ///< Путь к конфигруационному файлу по умолчанию.
    std::string log_path_ = "./config.json";                           ///< Путь к конфигруационному файлу по умолчанию.
    //Master master_;                                                  ///< 
};