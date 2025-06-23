#include "../include/argumentparser.hpp"

#include <algorithm>
#include <iostream>

using namespace std;

const vector<string> ArgumentParser::validLogLevels = {
    "debug", "info", "warning", "error", "critical"};

const vector<string> ArgumentParser::validLogTypes = {"console", "sync_file",
                                                      "async_file"};

ParsedArgs ArgumentParser::parse(int argc, char **argv) {
  ParsedArgs args;

  for (int i = 1; i < argc; ++i) {
    string arg = argv[i];

    if (arg == "--help" || arg == "-h") {
      printHelp();
      exit(EXIT_SUCCESS);
    } else if (arg == "--reload" || arg == "-r") {
      throw runtime_error(
          "ArgumentParser: Reload should be handled before parsing");
    } else if (arg == "--daemon") {
      args.daemon_mode = true;
    } else if (arg.compare(0, 10, "--override") == 0) {
      parseOverride(arg, args);
    } else if (arg.compare(0, 10, "--log-type") == 0) {
      parseLogType(arg, args, i, argc, argv);
      args.use_cli_logging = true;
    } else if (arg.compare(0, 12, "--config-file") == 0) {
      parseConfigFile(arg, args, i, argc, argv);
    } else if (arg.compare(0, 11, "--log-level") == 0) {
      parseLogLevel(arg, args, i, argc, argv);
      args.use_cli_logging = true;
    } else if (arg.compare(0, 13, "--environment") == 0) {
      if (i + 1 < argc) {
        args.environment = argv[++i];
      } else {
        throw invalid_argument("--environment requires a value");
      }
    } else {
      throw invalid_argument("ArgumentParser: Unknown argument: " + arg);
      printHelp();
    }
  }

  validateLogTypes(args.logger_types);
  return args;
}

void ArgumentParser::parseOverride(const string &arg, ParsedArgs &args) {
  size_t eqPos = arg.find('=');
  if (eqPos == string::npos) {
    throw invalid_argument(
        "ArgumentParser: Invalid override format. Use --override=key:value");
  }

  string overrideStr = arg.substr(eqPos + 1);
  size_t colonPos = overrideStr.find(':');
  if (colonPos == string::npos) {
    throw invalid_argument(
        "ArgumentParser: Invalid override format. Use key:value");
  }

  string key = overrideStr.substr(0, colonPos);
  string value = overrideStr.substr(colonPos + 1);
  args.overrides[key] = value;
}

void ArgumentParser::parseLogType(const string &arg, ParsedArgs &args, int &i,
                                  int argc, char **argv) {
  size_t eqPos = arg.find('=');
  string value;

  if (eqPos != string::npos) {
    value = arg.substr(eqPos + 1);
  } else if (i + 1 < argc) {
    value = argv[++i];
  } else {
    throw invalid_argument("ArgumentParser: --log-type requires a value");
  }

  size_t pos = 0;
  while ((pos = value.find(',')) != string::npos) {
    string type = value.substr(0, pos);
    args.logger_types.push_back(type);
    value.erase(0, pos + 1);
  }

  if (!value.empty()) {
    args.logger_types.push_back(value);
  }

  args.use_cli_logging = true;
}

void ArgumentParser::parseConfigFile(const string &arg, ParsedArgs &args,
                                     int &i, int argc, char **argv) {
  size_t eqPos = arg.find('=');

  if (eqPos != string::npos) {
    args.config_path = arg.substr(eqPos + 1);
  } else if (i + 1 < argc) {
    args.config_path = argv[++i];
  } else {
    throw invalid_argument("ArgumentParser: --config-file requires a value");
  }
}

void ArgumentParser::parseLogLevel(const string &arg, ParsedArgs &args, int &i,
                                   int argc, char **argv) {
  size_t eqPos = arg.find('=');
  std::string value;
  if (eqPos != string::npos) {
    value = arg.substr(eqPos + 1);
  } else if (i + 1 < argc) {
    value = argv[++i];
  } else {
    throw invalid_argument("ArgumentParser: --log-level requires a value");
    printHelp();
  }

  if (find(validLogLevels.begin(), validLogLevels.end(), args.log_level) ==
      validLogLevels.end()) {
    throw invalid_argument("ArgumentParser: Invalid log level: " +
                           args.log_level.value());
    printHelp();
  }

  args.log_level = value;
}

void ArgumentParser::validateLogTypes(const vector<string> &types) {
  for (const auto &type : types) {
    if (find(validLogTypes.begin(), validLogTypes.end(), type) ==
        validLogTypes.end()) {
      throw invalid_argument("ArgumentParser: Invalid logger type: " + type);
    }
  }
}

void ArgumentParser::printHelp() const {
  cout << "XML Filter Service\n\n"
       << "Usage:\n"
       << " service [options]\n\n"
       << "Options:\n"
       << " --help, -h          Show this help message\n"
       << " --config-file=FILE  Configuration file path\n"
       << " --override=KEY:VAL  Override config parameter\n"
       << " --log-type=TYPES    Logger types (comma-separated)\n"
       << " --log-level=LEVEL   Logging level "
          "[debug|info|warning|error|critical]\n"
       << " --daemon            Run as daemon\n";
}