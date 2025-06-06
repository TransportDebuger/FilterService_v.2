#include "stc/consolelogger.hpp"
#include "stc/syncfilelogger.hpp"
#include "stc/asyncfilelogger.hpp"

int main() {
    stc::ConsoleLogger::instance().init(stc::LogLevel::LOG_DEBUG);
    stc::ConsoleLogger::instance().debug("Debug message");
    stc::ConsoleLogger::instance().info("Info message");
    stc::ConsoleLogger::instance().warning("Warning message");
    stc::ConsoleLogger::instance().error("Error message");
    stc::ConsoleLogger::instance().critical("Critical message");
    stc::SyncFileLogger::instance().init(stc::LogLevel::LOG_DEBUG);
    stc::SyncFileLogger::instance().setMainLogPath("sync.log");
    stc::SyncFileLogger::instance().debug("Sync Debug message");
    stc::SyncFileLogger::instance().info("Sync Info message");
    stc::SyncFileLogger::instance().warning("Sync Warning message");
    stc::SyncFileLogger::instance().error("Sync Error message");
    stc::SyncFileLogger::instance().critical("Sync Critical message");
    stc::AsyncFileLogger::instance().init(stc::LogLevel::LOG_DEBUG);
    stc::AsyncFileLogger::instance().setMainLogPath("async.log");
    stc::AsyncFileLogger::instance().debug("Async Debug message");
    stc::AsyncFileLogger::instance().info("Async Info message");
    stc::AsyncFileLogger::instance().warning("Async Warning message");
    stc::AsyncFileLogger::instance().error("Async Error message");
    stc::AsyncFileLogger::instance().critical("Async Critical message");
    return 0;
}