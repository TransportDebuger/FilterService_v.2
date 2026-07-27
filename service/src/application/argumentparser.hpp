/**
@file argumentparser.hpp
@brief Парсер аргументов командной строки.
@version 2.1.0
@date 2026-07-24
*/
#pragma once

#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace stc {

/**
@struct ParsedArgs
@brief Контейнер для хранения результатов парсинга аргументов командной строки.
*/
struct ParsedArgs {
    std::string config_path_ = "/etc/xmlfilter/config.json";
    std::unordered_map<std::string, std::string> overrides_;
    std::vector<std::string> logger_types_;
    std::optional<std::string> log_level_;
    bool help_message_ = false;
    bool version_message_ = false;
    bool reload_ = false;
    std::string environment_ = "production";
    bool use_cli_logging_ = false;
};

/**
@class ArgumentParser
@brief Обеспечивает разбор, валидацию и обработку аргументов командной строки.
*/
class ArgumentParser {
public:
    /**
    @brief Выполняет полный цикл обработки аргументов командной строки.
    @param[in] argc Количество аргументов.
    @param[in] argv Массив строк аргументов.
    @return ParsedArgs Структура с результатами парсинга.
    @throw std::invalid_argument При некорректных аргументах или их значениях.
    */
    ParsedArgs Parse(int argc, char **argv);

private:
    void ParseOverride(const std::string &arg, ParsedArgs &args);
    void ParseLogType(const std::string &arg, ParsedArgs &args, int &i, int argc, char **argv);
    void ParseConfigFile(const std::string &arg, ParsedArgs &args, int &i, int argc, char **argv);
    void ParseLogLevel(const std::string &arg, ParsedArgs &args, int &i, int argc, char **argv);
    void ValidateLogTypes(const std::vector<std::string> &types);

    static const std::vector<std::string> valid_log_levels_;
    static const std::vector<std::string> valid_log_types_;
};

} // namespace stc