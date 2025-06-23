#pragma once
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

struct ParsedArgs {
  std::string config_path = "config.json";
  std::unordered_map<std::string, std::string> overrides;
  std::vector<std::string> logger_types;
  std::optional<std::string> log_level;
  bool daemon_mode = false;
  std::string environment = "production";
  bool use_cli_logging = false;
};

class ArgumentParser {
 public:
  ParsedArgs parse(int argc, char **argv);

 private:
  static const std::vector<std::string> validLogLevels;
  static const std::vector<std::string> validLogTypes;

  void parseOverride(const std::string &arg, ParsedArgs &args);
  void parseLogType(const std::string &arg, ParsedArgs &args, int &i, int argc,
                    char **argv);
  void parseConfigFile(const std::string &arg, ParsedArgs &args, int &i,
                       int argc, char **argv);
  void parseLogLevel(const std::string &arg, ParsedArgs &args, int &i, int argc,
                     char **argv);
  void validateLogTypes(const std::vector<std::string> &types);
  void printHelp() const;
};