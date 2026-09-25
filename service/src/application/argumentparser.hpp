/**
 * @file argumentparser.hpp
 * @brief Парсер аргументов командной строки и DTO для CLI-параметров.
 * @version 3.0.0
 * @date 2026-07-28
 */
#pragma once

#include <string>
#include <unordered_map>

namespace stc {

/**
 * @struct CliArguments
 * @brief Строгая структура, агрегирующая результаты парсинга командной строки.
 */
struct CliArguments {
  /// @brief Путь к файлу конфигурации.
  std::string config_path = "/etc/xmlfilter/config.json";
  /// @brief Имя целевого окружения.
  std::string environment = "production";
  /// @brief Карта переопределений параметров конфигурации.
  std::unordered_map<std::string, std::string> overrides;
  /// @brief Флаг запроса вывода справочной информации.
  bool help_requested = false;
  /// @brief Флаг запроса вывода информации о версии.
  bool version_requested = false;
  /// @brief Флаг запроса перезагрузки работающего экземпляра сервиса.
  bool reload_requested = false;
};

/**
@class ArgumentParser
@brief Обеспечивает разбор и валидацию аргументов командной строки.
*/
class ArgumentParser {
 public:
  /// @brief Конструктор по умолчанию.
  ArgumentParser() = default;

  /// @brief Деструктор.
  ~ArgumentParser() = default;

  /**
   * @brief Выполняет полный цикл обработки аргументов командной строки.
   * @param[in] argc Количество аргументов.
   * @param[in] argv Массив строк аргументов.
   * @return CliArguments Структура с результатами парсинга.
   * @throw std::invalid_argument При некорректных аргументах или их значениях.
   */
  CliArguments Parse(int argc, char **argv);

 private:
  /**
   * @private
   * @brief Парсит аргумент переопределения конфигурации.
   * @param[in] arg Строка аргумента.
   * @param[out] args Целевая структура для сохранения.
   * @throw std::invalid_argument При некорректном формате переопределения.
   */
  void ParseOverride(const std::string &arg, CliArguments &args);

  /**
   * @private
   * @brief Парсит аргумент пути к файлу конфигурации.
   * @param[in] arg Строка аргумента.
   * @param[out] args Целевая структура.
   * @param[in,out] i Индекс текущего аргумента.
   * @param[in] argc Общее количество аргументов.
   * @param[in] argv Массив аргументов.
   * @throw std::invalid_argument Если отсутствует значение.
   */
  void ParseConfigFile(const std::string &arg, CliArguments &args, int &i,
                       int argc, char **argv);

  /**
   * @private
   * @brief Парсит аргумент имени окружения.
   * @param[in] arg Строка аргумента.
   * @param[out] args Целевая структура.
   * @param[in,out] i Индекс текущего аргумента.
   * @param[in] argc Общее количество аргументов.
   * @param[in] argv Массив аргументов.
   * @throw std::invalid_argument Если отсутствует значение.
   */
  void ParseEnvironment(const std::string &arg, CliArguments &args, int &i,
                        int argc, char **argv);
};

}  // namespace stc