#pragma once
#include <nlohmann/json.hpp>
#include <string>

class EnvironmentProcessor {
 public:
  /**
   * @brief Обрабатывает переменные окружения в JSON-конфигурации
   * @param config Ссылка на JSON-объект для модификации
   *
   * @note Рекурсивно обходит все узлы JSON, заменяя строки вида $ENV{VAR}
   *       на значения переменных окружения
   */
  void process(nlohmann::json &config) const;

  /**
   * @brief Заменяет переменные окружения в строке
   * @param value Строка для обработки
   *
   * @details Находит все вхождения шаблонов $ENV{...} и заменяет их
   *          на соответствующие значения переменных окружения
   */
  void resolveVariable(std::string &value) const;

 private:
  /**
   * @brief Рекурсивный обход JSON с применением функции к каждому узлу
   * @param node Текущий JSON-узел
   * @param func Функция для применения к строковым значениям
   */
  void walkJson(nlohmann::json &node,
                std::function<void(std::string &)> func) const;
};