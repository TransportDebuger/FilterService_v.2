#pragma once
#include <string>

#include "../includes/logger.hpp"
#include "../includes/master.hpp"

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

    bool run_as_daemon_ = false;
    std::string config_path_ = "./config.json";                        ///< Путь к конфигруационному файлу по умолчанию.
    //std::string log_path_ = "/etc/xml_filter_service/config.json";   ///< Путь к конфигруационному файлу по умолчанию.
    //Master master_;                                                    ///< Экземпляр мастер-процесса сервиса.
};