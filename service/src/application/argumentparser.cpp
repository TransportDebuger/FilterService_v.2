/**
 * @file argumentparser.cpp
 * @brief Реализация парсера аргументов командной строки.
 * @version 3.0.0
 * @date 2026-07-28
 */
#include "argumentparser.hpp"

#include <stdexcept>
#include <string>

namespace stc {

CliArguments ArgumentParser::Parse(int argc, char **argv) {
  CliArguments args;
  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];
    if (arg == "--help" || arg == "-h") {
      args.help_requested = true;
    } else if (arg == "--version" || arg == "-v") {
      args.version_requested = true;
    } else if (arg == "--reload" || arg == "-r") {
      args.reload_requested = true;
    } else if (arg.starts_with("--override")) {
      ParseOverride(arg, args);
    } else if (arg.starts_with("--config-file")) {
      ParseConfigFile(arg, args, i, argc, argv);
    } else if (arg.starts_with("--environment")) {
      ParseEnvironment(arg, args, i, argc, argv);
    } else {
      throw std::invalid_argument("ArgumentParser: Unknown argument: " + arg);
    }
  }
  return args;
}

void ArgumentParser::ParseOverride(const std::string &arg, CliArguments &args) {
  size_t eq_pos = arg.find('=');
  if (eq_pos == std::string::npos) {
    throw std::invalid_argument(
        "Invalid override format. Use --override=key:value");
  }
  std::string override_str = arg.substr(eq_pos + 1);
  size_t colon_pos = override_str.find(':');
  if (colon_pos == std::string::npos) {
    throw std::invalid_argument("Invalid override format. Use key:value");
  }
  std::string key = override_str.substr(0, colon_pos);
  std::string value = override_str.substr(colon_pos + 1);
  args.overrides[key] = value;
}

void ArgumentParser::ParseConfigFile(const std::string &arg, CliArguments &args,
                                     int &i, int argc, char **argv) {
  if (arg.starts_with("--config-file=")) {
    args.config_path = arg.substr(14);
  } else if (i + 1 < argc) {
    args.config_path = argv[++i];
  } else {
    throw std::invalid_argument("--config-file requires a value");
  }
}

void ArgumentParser::ParseEnvironment(const std::string &arg,
                                      CliArguments &args, int &i, int argc,
                                      char **argv) {
  if (arg.starts_with("--environment=")) {
    args.environment = arg.substr(14);
  } else if (i + 1 < argc) {
    args.environment = argv[++i];
  } else {
    throw std::invalid_argument("--environment requires a value");
  }
}

}  // namespace stc