#pragma once
#include <string>

#include "../includes/logger.hpp"
#include "../includes/master.hpp"

/*!
    \brief Основной контроллер сервиса.
*/
class ServiceController {
public:
    int run(int argc, char** argv);                                    
    
private:
    bool parseArguments(int argc, char** argv);                        
    void initialize();                                                 
    void mainLoop();                                                   
    void printHelp() const;                                            

    //bool run_as_daemon_ = false;
    std::string config_path_ = "./config.json";                        ///< Путь к конфигруационному файлу по умолчанию.
    Master master_;                                                    ///< Экземпляр мастер-процесса сервиса.
};