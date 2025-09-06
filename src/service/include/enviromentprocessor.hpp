/**
 * @file environmentprocessor.hpp
 * @brief Подстановка значений переменных окружения в JSON-конфигурацию
 *
 * @author Artem Ulyanov
 * @company STC Ltd.
 * @date May 2025
 *
 * @details
 * Модуль реализует рекурсивную подстановку шаблонов `$ENV{VAR}` в любом
 * строковом узле JSON-объекта конфигурации. Поддерживает вложенные объекты
 * и массивы, создавая механизмы обхода на всех уровнях. Использует
 * стандартную функцию `getenv()` для получения значений переменных
 * окружения.
 *
 * @note Не потокобезопасенне, не обеспечивает внутреннюю синхронизацию и
 * должен вызываться в однопоточном режиме или защищаться внешними мьютексами.
 * @warning Шаблоны вида `$ENV{VAR}` остаются неизменными, если переменная
 *          не установлена в окружении
 *
 * @version 1.0
 * @since 1.0
 */

#pragma once

#include <nlohmann/json.hpp>
#include <string>

/**
 * @defgroup Configuration Компоненты управления конфигурацией
 */

/**
 * @defgroup InternalMethods Внутренние (приватные) методы классов
 */

/**
 * @class EnvironmentProcessor
 * @brief Обработка шаблонов переменных окружения в конфиге
 *
 * @details
 * Обходит JSON-дерево, заменяя все вхождения `$ENV{VAR}` в строках на
 * значения переменных окружения. Поддержка объектов, массивов и строковых
 * узлов обеспечивает универсальность даже для сложных конфигураций.
 *
 * @ingroup Configuration
 */
class EnvironmentProcessor {
 public:
  /**
     * @brief Выполняет подстановку переменных среды в JSON
     *
     * @param[in,out] config JSON-объект для обработки; строковые узлы
     *                       изменяются на значения переменных среды
     *
     * @throw std::bad_alloc Если не хватает памяти для обхода JSON [1]
     *
     * @note Вызов разбивает JSON на узлы и передаёт каждую строку в
     *       `resolveVariable()`
     * @warning Не бросает исключений при отсутствии переменной окружения
     *
     * @code
     nlohmann::json cfg = R"({
       "path": "$ENV{HOME}/app",
       "nested": { "user": "$ENV{USER}" }
     })"_json;
     EnvironmentProcessor ep;
     ep.process(cfg);
     // После выполнения cfg["path"] == "/home/alice/app"
     @endcode
     */
  void process(nlohmann::json &config) const;

  /**
     * @brief Заменяет все шаблоны `$ENV{VAR}` в строке
     *
     * @param[in,out] value Исходная строка; в ней все вхождения
     *                      `$ENV{VAR}` заменяются на getenv("VAR")
     *
     * @details
     * Выполняет поиск префикса `$ENV{`, извлекает имя переменной до `}`,
     * получает её значение через `getenv()` и заменяет в строке.
     *
     * @warning Не поддерживает вложенные или пересекающиеся шаблоны
     *
     * @code
     std::string s = "Service: $ENV{SERVICE_NAME}";
     EnvironmentProcessor ep;
     ep.resolveVariable(s);
     // При SERVICE_NAME=filter => s == "Service: filter"
     @endcode
     */
  void resolveVariable(std::string &value) const;

 private:
  /**
     * @brief Рекурсивный обход JSON-узлов с применением функции к строкам
     * @ingroup InternalMethods
     *
     * @param[in,out] node JSON-узел (объект, массив или строка)
     * @param[in] func Функция-обработчик, применяемая к каждому строковому узлу
     *
     * @details
     * - Если `node` объект — обходит все поля
     * - Если `node` массив — обходит каждый элемент
     * - Если `node` строка — применяет `func` к значению
     *
     * @warning Игнорирует числовые, логические и null-узлы
     *
     * @code
     nlohmann::json j = R"({
       "arr": ["$ENV{A}", {"k":"$ENV{B}"}]
     })"_json;
     EnvironmentProcessor ep;
     ep.walkJson(j, [](std::string &s){ s += "_OK"; });
     @endcode
     */
  void walkJson(nlohmann::json &node,
                std::function<void(std::string &)> func) const;
};