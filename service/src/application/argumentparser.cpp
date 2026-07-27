/**
@file argumentparser.cpp
@brief Реализация парсера аргументов командной строки.
@version 2.1.0
@date 2026-07-24
*/
#include "argumentparser.hpp"
#include <algorithm>
#include <iostream>
#include <stdexcept>

namespace stc {

const std::vector<std::string> ArgumentParser::valid_log_levels_ = {
    "debug", "info", "warning", "error", "critical"
};

const std::vector<std::string> ArgumentParser::valid_log_types_ = {
    "console", "sync_file", "async_file"
};

ParsedArgs ArgumentParser::Parse(int argc, char **argv) {
    ParsedArgs args;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        
        if (arg == "--help" || arg == "-h") {
            args.help_message_ = true;
        } else if (arg == "--version" || arg == "-v") {
            args.version_message_ = true;
        } else if (arg == "--reload" || arg == "-r") {
            args.reload_ = true;
        } else if (arg.starts_with("--override")) {
            ParseOverride(arg, args);
        } else if (arg.starts_with("--log-type")) {
            ParseLogType(arg, args, i, argc, argv);
            args.use_cli_logging_ = true;
        } else if (arg.starts_with("--config-file")) {
            ParseConfigFile(arg, args, i, argc, argv);
        } else if (arg.starts_with("--log-level")) {
            ParseLogLevel(arg, args, i, argc, argv);
            args.use_cli_logging_ = true;
        } else if (arg.starts_with("--environment")) {
            if (arg.starts_with("--environment=")) {
                args.environment_ = arg.substr(14);
            } else if (i + 1 < argc) {
                args.environment_ = argv[++i];
            } else {
                throw std::invalid_argument("--environment requires a value");
            }
        } else {
            throw std::invalid_argument("ArgumentParser: Unknown argument: " + arg);
        }
    }
    ValidateLogTypes(args.logger_types_);
    return args;
}

void ArgumentParser::ParseOverride(const std::string &arg, ParsedArgs &args) {
    size_t eq_pos = arg.find('=');
    if (eq_pos == std::string::npos) {
        throw std::invalid_argument("Invalid override format. Use --override=key:value");
    }
    std::string override_str = arg.substr(eq_pos + 1);
    size_t colon_pos = override_str.find(':');
    if (colon_pos == std::string::npos) {
        throw std::invalid_argument("Invalid override format. Use key:value");
    }
    std::string key = override_str.substr(0, colon_pos);
    std::string value = override_str.substr(colon_pos + 1);
    args.overrides_[key] = value;
}

void ArgumentParser::ParseLogType(const std::string &arg, ParsedArgs &args, int &i, int argc, char **argv) {
    std::string value;
    if (arg.starts_with("--log-type=")) {
        value = arg.substr(11);
    } else if (i + 1 < argc) {
        value = argv[++i];
    } else {
        throw std::invalid_argument("--log-type requires a value");
    }
    
    size_t pos = 0;
    while ((pos = value.find(',')) != std::string::npos) {
        args.logger_types_.push_back(value.substr(0, pos));
        value.erase(0, pos + 1);
    }
    if (!value.empty()) {
        args.logger_types_.push_back(value);
    }
}

void ArgumentParser::ParseConfigFile(const std::string &arg, ParsedArgs &args, int &i, int argc, char **argv) {
    if (arg.starts_with("--config-file=")) {
        args.config_path_ = arg.substr(14);
    } else if (i + 1 < argc) {
        args.config_path_ = argv[++i];
    } else {
        throw std::invalid_argument("--config-file requires a value");
    }
}

void ArgumentParser::ParseLogLevel(const std::string &arg, ParsedArgs &args, int &i, int argc, char **argv) {
    std::string value;
    if (arg.starts_with("--log-level=")) {
        value = arg.substr(12);
    } else if (i + 1 < argc) {
        value = argv[++i];
    } else {
        throw std::invalid_argument("--log-level requires a value");
    }
    
    if (std::find(valid_log_levels_.begin(), valid_log_levels_.end(), value) == valid_log_levels_.end()) {
        throw std::invalid_argument("Invalid log level: " + value);
    }
    args.log_level_ = value;
}

void ArgumentParser::ValidateLogTypes(const std::vector<std::string> &types) {
    for (const auto &type : types) {
        if (std::find(valid_log_types_.begin(), valid_log_types_.end(), type) == valid_log_types_.end()) {
            throw std::invalid_argument("Invalid logger type: " + type);
        }
    }
}

} // namespace stc