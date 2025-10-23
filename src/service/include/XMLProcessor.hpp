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
     * Подготавливает объект к последующей обработке XML-данных, обеспечивая доступ 
     * к конфигурационным параметрам через config_. Позволяет гибко настраивать 
     * поведение объекта в зависимости от переданных настроек.
     * Это основной способ инициализации объекта перед началом работы с XML-данными. 
     * Принимает параметр config типа const SourceConfig&. Использование ссылки (&) 
     * позволяет избежать лишних копирований объекта SourceConfig, а ключевое слово 
     * const гарантирует, что конфигурация не будет изменена внутри конструктора.
     *
     * @warning Конфигурация должна быть валидирована заранее методом SourceConfig::validate().
     * @note Конструктор запрещает неявное преобразование типов.
     *
     * @code
       SourceConfig cfg = ...;
       XMLProcessor processor(cfg); // Создание объекта с определенной конфигурацией
       XMLProcessor processor = cfg; // Ошибка компиляции: неявное преобразование типов запрещено.
       @endcode
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
     * @brief Структура для хранения информации об объекте для фильтрации
     */
  struct ObjectBoundary {
    xmlNodePtr objectNode;      // Узел объекта для фильтрации
    xmlNodePtr containerNode;   // Родительский контейнер объекта
    int depth;                  // Глубина вложенности от корня
    std::string objectPath;     // Путь к объекту для отладки
        
    ObjectBoundary() : objectNode(nullptr), containerNode(nullptr), depth(0) {}
    ObjectBoundary(xmlNodePtr obj, xmlNodePtr container, int d, const std::string& path)
            : objectNode(obj), containerNode(container), depth(d), objectPath(path) {}
  };

  /**
     * @brief Структура для хранения результатов анализа узла
     */
    struct NodeAnalysisResult {
        xmlNodePtr node;
        std::vector<bool> criteriaResults;
        bool shouldRemove;
        
        NodeAnalysisResult() : node(nullptr), shouldRemove(false) {}
    };

  /**
     * @brief Парсит XML-файл в структуру libxml2
     * @param[in] path Путь к XML-файлу (in)
     * @return xmlDocPtr Указатель на разобранный xmlDocPtr
     * @throw std::runtime_error Если парсинг не удался
     *
     * @details
     * Функция обеспечивает надежное чтение и проверку XML-файла, используя внешнюю библиотеку 
     * (libxml2) и интеграцию с системой логирования (stc::CompositeLogger). При работе метода 
     * из результата разбора удаляются не информативные пробелы XML_PARSE_NOBLANKS.
     * Функция гарантирует, что документ корректно загружен (возвращает указатель на документ), 
     * либо логгирует ошибку в файл и завершает выполнение программы (std::runtime_error).
     *
     * @warning Во избежание утечки ресурсов после использования указателя необходимо освободить 
     * его используя xmlFreeDoc().
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
     * @brief Получает текстовое представление зарегистрированных пространств имен из
     XML документа.
     * @param[in] doc Указатель на разобранный документ xmlDocPtr
     * @return 
     *   - При успешном выполненнии возвращаетмя строка (std::string) вида "prefix:URI\n" или при отстутсвии префикса "default:URI\n";
     *   - При отсутствии корневого элемента возвращается пустая строка;
     *
     * @details
     * Предоставляет удобный способ получения информации о всех пространствах имён, определённых в корне документа.
     * Функция пепербирает все объявления пространств имён (nsDef) корневого элемента. 
     * Для каждого пространства:
     *   - Если задан префикс (ns->prefix), формирует строку вида префикс:URI.
     *   - Если префикс отсутствует (дефолтное пространство), добавляет default:URI.
     * Результат представляется пользователю в виде форматированой строки, т.е. склеивается в одну строку с переносами (\n).
     * Пример выовда:
     * ```  
     * entry:http://example.com/entries  
     * default:http://example.com/default  
     * ```
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
     * @brief Создает пустые выходные XML-документы для отфильтрованных данных (processed) и исключенных данных (excluded).
     * @param[in] srcDoc   Указатель на исходный XML-документ xmlDocPtr
     * @param[out] procDoc Указатель на новый XML-документ для отфилтрованных данных xmlDocPtr processed.
     * @param[out] exclDoc Указатель на новый XML-документ для исключенных данных xmlDocPtr excluded.
     *
     * @details
     * Метод создает копии структуры исходного XML-документа, сохранив имя корневого элемента, все пространства имен (включая привязку) и его аттрибуты.
     * Эти документы (procDoc и exclDoc) используются для последующего добавления фильтрованных данных (например, записей, соответствующих или не соответствующих критериям). Это позволяет сохранить целостность структуры XML при разделении данных.
     * 
     * @warning Переменные procDoc и exclDoc должны быть объявлены как xmlDocPtr в коде до вызова метода.
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

  std::vector<NodeAnalysisResult> collectAndAnalyzeNodes(xmlXPathContextPtr ctx);
    std::map<xmlNodePtr, ObjectBoundary> findOptimalObjectBoundaries(
        const std::vector<xmlNodePtr>& nodesToRemove);
    xmlNodePtr findNearestCommonContainer(const std::vector<xmlNodePtr>& nodes);
    int calculateNodeDepth(xmlNodePtr node);
    std::string buildNodePath(xmlNodePtr node);
    void buildOutputStructure(
    xmlDocPtr srcDoc, 
    const std::map<xmlNodePtr, ObjectBoundary>& objectsToRemove,
    xmlDocPtr targetDoc, 
    bool isExcludedDoc);
    /**
     * @brief Рекурсивно копирует узлы с фильтрацией
     * @param srcNode Исходный узел
     * @param targetParent Родительский узел в целевом документе
     * @param objectsToRemove Карта объектов, помеченных для удаления
     * @param includeRemoved true - копировать объекты, помеченные для удаления
     * @param includeUnmatched true - копировать объекты, НЕ помеченные для удаления
     */
    void copyNodeWithFiltering(xmlNodePtr srcNode, 
                              xmlNodePtr targetParent,
                              const std::map<xmlNodePtr, ObjectBoundary>& objectsToRemove,
                              bool includeRemoved,
                              bool includeUnmatched);
    bool shouldKeepContainer(xmlNodePtr containerNode, 
                           const std::map<xmlNodePtr, ObjectBoundary>& removedObjects);
    bool isNodeMarkedForRemoval(xmlNodePtr node, 
                               const std::map<xmlNodePtr, ObjectBoundary>& objectsToRemove);
    void copyNodeAttributes(xmlNodePtr srcNode, xmlNodePtr targetNode);
    void copyNodeNamespace(xmlNodePtr srcNode, xmlNodePtr targetNode, xmlDocPtr targetDoc);

   /**
     * @brief Обновить счётчик записей в документе
     * @param doc Целевой документ (processed или excluded)
     * @param recordCountConfig Конфигурация счётчика из config
     * @param newCount Новое значение счётчика
     * @return true если счётчик был успешно обновлён, false если элемент не найден
     */
    bool updateRecordCount(xmlDocPtr doc, 
                          const RecordCountConfig& recordCountConfig,
                          int newCount);

    /**
     * @brief Найти элемент по XPath для обновления счётчика
     * @param doc Документ для поиска
     * @param xpath XPath выражение
     * @return Найденный узел или nullptr
     */
    xmlNodePtr findRecordCountElement(xmlDocPtr doc, const std::string& xpath);

    /**
     * @brief Обновить значение атрибута или текстового содержимого
     * @param node Узел для обновления
     * @param attributeName Имя атрибута для обновления
     * @param newValue Новое значение
     * @return true если успешно обновлено
     */
    bool updateNodeValue(xmlNodePtr node, const std::string& attributeName, const std::string& newValue);

    int readRecordCountFromSource(xmlDocPtr srcDoc);
};