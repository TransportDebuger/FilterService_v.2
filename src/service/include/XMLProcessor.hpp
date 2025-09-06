/**
 * @file XMLProcessor.hpp
 * @author Artem Ulyanov
 * @company STC Ltd.
 * @date June 2025
 * @brief Класс для парсинга, фильтрации и сохранения результатов обработки
 * XML-файлов
 *
 * @details
 * XMLProcessor выполняет многокритериальную фильтрацию XML-документов с помощью
 * XPath:
 * - парсинг через libxml2 (xmlReadFile)
 * - извлечение значений узлов и атрибутов
 * - сравнение с данными из CSV (FilterListManager)
 * - поддержка логических операторов: AND, OR, MAJORITY, WEIGHTED
 * - автоматическая или ручная регистрация пространств имён
 *
 * Используемые паттерны:
 * - Strategy для выбора логики фильтрации
 * - Builder для создания выходных документов
 * - Observer для логирования событий
 */
#pragma once

#include <libxml/parser.h>
#include <libxml/tree.h>
#include <libxml/xpath.h>
#include <libxml/xpathInternals.h>

#include <filesystem>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

#include "../include/FilterListManager.hpp"
#include "../include/sourceconfig.hpp"

/**
 * @class XMLProcessor
 * @brief Основной класс для обработки одиночного XML-файла
 *
 * @ingroup Core
 *
 * @details
 * Создаёт независимые XML-документы для "processed" и "excluded" данных
 * по каждому элементу записи, соответствующему XPath-критериям.
 */
class XMLProcessor {
 public:
  /**
     * @brief Конструктор, сохраняет ссылку на конфигурацию источника
     * @param[in] config Конфигурация источника (in)
     *
     * @details
     * Сохраняет ссылку на объект SourceConfig, содержащий критерии и пути.
     *
     * @warning Конфигурация должна быть валидирована заранее
     (SourceConfig::validate).
     *
     * @code
     SourceConfig cfg = ...;
     XMLProcessor proc(cfg);
     * @endcode
     */
  explicit XMLProcessor(const SourceConfig &config);

  /**
     * @brief Основной метод обработки XML-файла
     * @param[in] xmlPath Путь к входному XML-файлу (in)
     * @return true  — обработка прошла успешно, false — в случае ошибки
     * @throw std::runtime_error При критических ошибках парсинга или сохранения
     *
     * @details
     * 1. Парсит документ через parseXML()
     * 2. Регистрирует пространства имён (registerNamespaces)
     * 3. Извлекает уникальные элементы (findParentEntry)
     * 4. Применяет критерии (evaluateEntryAgainstCriteria)
     * 5. Сохраняет результаты в директориях processed/excluded
     *
     * @note Использует нативный парсер libxml2, требует инициализации libxml в
     main
     * @warning При ошибках форматирования XML возвращает false без выброса
     исключений
     *
     * @code
     XMLProcessor proc(cfg);
     if (!proc.process("data/input.xml")) {
         std::cerr << "Processing failed\n";
     }
     @endcode
     */
  bool process(const std::string &xmlPath);

 private:
  const SourceConfig &config_;

  /**
     * @brief Парсит XML-файл в структуру libxml2
     * @param[in] path Путь к XML-файлу (in)
     * @return xmlDocPtr Указатель на разобранный xmlDocPtr (out)
     * @throw std::runtime_error Если парсинг не удался
     *
     * @details
     * Вызывает xmlReadFile с флагом XML_PARSE_NOBLANKS
     * и проверяет результат на null.
     *
     * @note Не забывайте вызывать xmlFreeDoc() после использования.
     *
     * @code
     xmlDocPtr doc = parseXML("data/input.xml");
     // ...
     xmlFreeDoc(doc);
     @endcode
     */
  xmlDocPtr parseXML(const std::string &path);

  /**
     * @brief Извлекает строковое значение из узла или атрибута
     * @param[in] node Узел libxml2 (in)
     * @param[in] crit Критерий фильтрации (in)
     * @return std::string Извлечённое значение (out)
     *
     * @details
     * Если crit.attribute не пустой — читает атрибут,
     * иначе — текстовое содержимое узла.
     *
     * @note Освобождает xmlChar* через xmlFree().
     *
     * @code
     auto val = extractValue(node, config_.xml_filter.criteria[0]);
     @endcode
     */
  std::string extractValue(xmlNodePtr node,
                           const SourceConfig::XmlFilterCriterion &crit);
  // std::vector<bool> evaluateCriteria(xmlDocPtr doc);

  /**
     * @brief Применяет логику объединения результатов критериев
     * @param[in] results Вектор булевых значений по критериям (in)
     * @return bool Итоговое решение для элемента (out)
     *
     * @details
     * Осуществляет:
     * - AND: все true
     * - OR: хотя бы один true
     * - MAJORITY: больше половины true
     * - WEIGHTED: сумма весов true / общая сумма >= threshold
     *
     * @throw std::invalid_argument При неизвестном logic_operator
     *
     * @code
     bool ok = applyLogic({true, false, true});
     @endcode
     */
  bool applyLogic(const std::vector<bool> &results);

  /**
     * @brief Регистрирует пространства имён в XPath-контексте
     * @param[in] ctx XPath-контекст (in,out)
     * @param[in] doc Исходный xmlDocPtr (in)
     *
     * @details
     * Если в config_.xml_filter.namespaces не пуст, вызывает
     registerConfiguredNamespaces(),
     * иначе — registerNamespacesFromDocument().
     *
     * @note Всегда вызывается перед выполнением XPath-запросов.
     *
     * @code
     xmlXPathContextPtr ctx = xmlXPathNewContext(doc);
     registerNamespaces(ctx, doc);
     @endcode
     */
  void registerNamespaces(xmlXPathContextPtr ctx, xmlDocPtr doc);

  /**
     * @brief Регистрирует пространства имён, заданные в конфиге
     * @param[in] ctx XPath-контекст (in,out)
     *
     * @details
     * Проходит по config_.xml_filter.namespaces и вызывает
     xmlXPathRegisterNs().
     *
     * @code
     registerConfiguredNamespaces(ctx);
     @endcode
     */
  void registerConfiguredNamespaces(xmlXPathContextPtr ctx);

  /**
     * @brief Автоматически регистрирует пространства имён из документа
     * @param[in] ctx XPath-контекст (in,out)
     * @param[in] doc xmlDocPtr для чтения корневых nsDef (in)
     *
     * @details
     * Обходит список nsDef корневого элемента, регистрирует префикс/URI.
     *
     * @note Регистрирует default-неймспейс под префиксом "default".
     *
     * @code
     registerNamespacesFromDocument(ctx, doc);
     @endcode
     */
  void registerNamespacesFromDocument(xmlXPathContextPtr ctx, xmlDocPtr doc);

  /**
     * @brief Получает текстовое представление зарегистрированных ns из
     документа
     * @param[in] doc xmlDocPtr (in)
     * @return std::string Строка вида "prefix:URI\n" (out)
     *
     * @details
     * Проходит по nsDef корня, формирует строку для логирования.
     *
     * @code
     auto nsList = getDocumentNamespaces(doc);
     @endcode
     */
  std::string getDocumentNamespaces(xmlDocPtr doc);

  /**
   * @brief Извлекает родительский элемент записи для XPath-ноды
   * @param[in] node XML-узел критериев (in)
   * @return xmlNodePtr Родительский элемент entry или исходный узел (out)
   *
   * @details
   * Поднимается вверх по дереву до первого узла с именем "entry", "record" или
   * "item", либо до корня.
   *
   * @note Используется для группировки совпавших критериев по записям.
   *
   * @code
   * xmlNodePtr parent = findParentEntry(childNode);
   * @endcode
   */
  xmlNodePtr findParentEntry(xmlNodePtr node);

  /**
     * @brief Выполняет проверку каждого элемента записи по критериям
     * @param[in] entry Указатель на элемент родительского узла (in)
     * @param[in] doc   Полный xmlDocPtr для XPath-контекста (in)
     * @return bool true если запись соответствует логике фильтрации, иначе
     false
     * @throw std::runtime_error При ошибках выполнения XPath
     *
     * @details
     * 1. Создаёт свой XPath-контекст, регистрирует пространства имён
     * 2. Для каждого критерия извлекает значения, выполняет сравнение
     * 3. Применяет оператор AND/OR/MAJORITY/WEIGHTED через applyLogic()
     *
     * @note Вызывает xmlXPathFreeContext() и xmlXPathFreeObject() после
     использования
     *
     * @code
     bool ok = evaluateEntryAgainstCriteria(entry, doc);
     @endcode
     */
  bool evaluateEntryAgainstCriteria(xmlNodePtr entry, xmlDocPtr doc);

  /**
   * @brief Преобразует абсолютный XPath в относительный
   * @param[in] xpath Исходное XPath-выражение (in)
   * @return std::string Относительное XPath (out)
   *
   * @details
   * Удаляет начальные '/' или '//' и префиксы корневых элементов,
   * добавляет "./" для позиционирования относительно текущего узла.
   *
   * @note Удобно при выполнении xpath относительно entry.
   *
   * @code
   * auto rel = makeRelativeXPath("//entry/id");
   * @endcode
   */
  std::string makeRelativeXPath(const std::string &xpath);

  /**
     * @brief Создаёт пустые выходные XML-документы для processed и excluded
     * @param[in] srcDoc   Исходный xmlDocPtr (in)
     * @param[out] procDoc Указатель для нового xmlDocPtr processed (out)
     * @param[out] exclDoc Указатель для нового xmlDocPtr excluded (out)
     *
     * @details
     * Копирует корневой узел без дочерних элементов для каждого документа
     * и устанавливает их root через xmlDocSetRootElement().
     *
     * @code
     xmlDocPtr p, e;
     createOutputDocuments(src, p, e);
     @endcode
     */
  void createOutputDocuments(xmlDocPtr srcDoc, xmlDocPtr &procDoc,
                             xmlDocPtr &exclDoc);

  /**
     * @brief Сохраняет результаты обработки в файлы
     * @param[in] xmlPath   Исходный путь к файлу (in)
     * @param[in] procDoc   xmlDocPtr processed (in)
     * @param[in] exclDoc   xmlDocPtr excluded (in)
     * @param[in] hasClean  Наличие processed-записей (in)
     * @param[in] hasMatch  Наличие excluded-записей (in)
     *
     * @details
     * 1. Создаёт директории config_.processed_dir и config_.excluded_dir
     * 2. Сериализует procDoc и exclDoc через xmlSaveFormatFileEnc()
     * 3. Логирует пути сохранённых файлов
     *
     * @throw std::runtime_error При ошибках I/O или кодирования
     *
     * @code
     saveResults("input.xml", pDoc, eDoc, true, true);
     @endcode
     */
  void saveResults(const std::string &xmlPath, xmlDocPtr procDoc,
                   xmlDocPtr exclDoc, bool hasClean, bool hasMatch);
};