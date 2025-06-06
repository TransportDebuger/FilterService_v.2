#include "../include/configmanager.hpp"

#include <iostream>

int main() {
    try {
        ConfigManager::instance().loadFromFile("config.json");

        if (!ConfigManager::instance().isValid()) {
            std::cerr << "Invalid configuration!" << std::endl;
            return 1;
        }

        auto loggingConfig = ConfigManager::instance().getLoggingConfig("development");
        std::cout << "Logging config for development: " << loggingConfig.dump(2) << std::endl;

        // При необходимости перезагрузить конфигурацию:
        // ConfigManager::instance().reload();

    } catch (const std::exception& ex) {
        std::cerr << "Config error: " << ex.what() << std::endl;
        return 1;
    }
    return 0;
}