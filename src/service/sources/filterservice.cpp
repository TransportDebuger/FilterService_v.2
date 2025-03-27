#include "../includes/logger.hpp"

int main(int argc, char** argv) {

    std::string configPath = "/etc/stc_filter_service/config.json";
    std::string serviceLogPath = "stc_filter_service.log";
    bool debugMode = false;

    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "--config" && i + 1 < argc) {
            configPath = argv[i + 1];
        } else if (std::string(argv[i]) == "--debug") {
            debugMode = true;
        }
    }

    Logger::init(serviceLogPath, LogLevel::INFO, true, DEFAULT_LOGSIZE);
    Logger::info("Service started");

    Logger::info("Service stopped");
    Logger::close();
    return 0;
}