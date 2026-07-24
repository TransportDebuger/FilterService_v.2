/**
@file XMLProcessor.hpp
@brief Обработка, фильтрация и сохранение результатов XML-файлов.
@version 2.0.0
@date 2026-07-17
*/
#pragma once

#include <libxml/parser.h>
#include <libxml/tree.h>
#include <libxml/xpath.h>
#include <libxml/xpathInternals.h>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "../include/sourceconfig.hpp"
#include "stc/logger/ilogger.hpp"

namespace stc {

/**
@struct ProcessingResult
@brief Результат обработки XML-файла, содержащий статистику для метрик.
*/
struct ProcessingResult {
    bool success{false};
    size_t records_processed{0};
    size_t records_matched{0};
    size_t bytes_processed{0};
    
    enum class ErrorType { NONE, PARSE, WRITE };
    ErrorType error_type{ErrorType::NONE};
};

/**
@class XMLProcessor
@brief Выполняет многокритериальную фильтрацию XML-документов.
*/
class XMLProcessor {
public:
    /**
    @brief Конструктор процессора.
    @param[in] config Конфигурация источника данных.
    @param[in] logger Диспетчер логирования.
    */
    explicit XMLProcessor(const SourceConfig& config, 
                          std::shared_ptr<stc::logger::ILogger> logger);

    /**
    @brief Обрабатывает XML-файл по указанному пути.
    @param[in] xmlPath Путь к входному файлу.
    @return ProcessingResult Структура с результатом и статистикой обработки.
    @throw std::runtime_error При критических ошибках парсинга (если success == false).
    */
    ProcessingResult process(const std::string& xmlPath);

private:
    /// @private Конфигурация источника.
    const SourceConfig& config_;
    
    /// @private Диспетчер логирования, полученный через DI.
    std::shared_ptr<stc::logger::ILogger> logger_;

    /// @private Границы объекта для фильтрации.
    struct ObjectBoundary {
        xmlNodePtr objectNode;
        xmlNodePtr containerNode;
        int depth;
        std::string objectPath;
        ObjectBoundary() : objectNode(nullptr), containerNode(nullptr), depth(0) {}
        ObjectBoundary(xmlNodePtr obj, xmlNodePtr container, int d, const std::string& path)
            : objectNode(obj), containerNode(container), depth(d), objectPath(path) {}
    };

    /// @private Результат анализа узла.
    struct NodeAnalysisResult {
        xmlNodePtr node;
        std::vector<bool> criteriaResults;
        bool shouldRemove;
        NodeAnalysisResult() : node(nullptr), shouldRemove(false) {}
    };

    /// @private Парсит XML в xmlDocPtr.
    xmlDocPtr parseXML(const std::string& path);
    
    /// @private Извлекает значение из узла или атрибута.
    std::string extractValue(xmlNodePtr node, const SourceConfig::XmlFilterCriterion& crit);
    
    /// @private Применяет логику объединения критериев.
    bool applyLogic(const std::vector<bool>& results);
    
    /// @private Регистрирует пространства имён в XPath-контексте.
    void registerNamespaces(xmlXPathContextPtr ctx, xmlDocPtr doc);
    
    /// @private Регистрирует пространства имён из конфигурации.
    void registerConfiguredNamespaces(xmlXPathContextPtr ctx);
    
    /// @private Автоматически регистрирует пространства имён из документа.
    void registerNamespacesFromDocument(xmlXPathContextPtr ctx, xmlDocPtr doc);
    
    /// @private Получает текстовое представление пространств имён.
    std::string getDocumentNamespaces(xmlDocPtr doc);
    
    /// @private Извлекает родительский элемент записи.
    xmlNodePtr findParentEntry(xmlNodePtr node);
    
    /// @private Проверяет элемент записи по критериям.
    bool evaluateEntryAgainstCriteria(xmlNodePtr entry, xmlDocPtr doc);
    
    /// @private Преобразует абсолютный XPath в относительный.
    std::string makeRelativeXPath(const std::string& xpath);
    
    /// @private Создает выходные XML-документы.
    void createOutputDocuments(xmlDocPtr srcDoc, xmlDocPtr& procDoc, xmlDocPtr& exclDoc);
    
    /// @private Сохраняет результаты обработки в файлы.
    void saveResults(const std::string& xmlPath, xmlDocPtr procDoc, xmlDocPtr exclDoc, bool hasClean, bool hasMatch);
    
    /// @private Собирает и анализирует узлы.
    std::vector<NodeAnalysisResult> collectAndAnalyzeNodes(xmlXPathContextPtr ctx);
    
    /// @private Находит оптимальные границы объектов.
    std::map<xmlNodePtr, ObjectBoundary> findOptimalObjectBoundaries(const std::vector<xmlNodePtr>& nodesToRemove);
    
    /// @private Находит ближайший общий контейнер.
    xmlNodePtr findNearestCommonContainer(const std::vector<xmlNodePtr>& nodes);
    
    /// @private Вычисляет глубину узла.
    int calculateNodeDepth(xmlNodePtr node);
    
    /// @private Строит путь к узлу.
    std::string buildNodePath(xmlNodePtr node);
    
    /// @private Формирует структуру выходного документа.
    void buildOutputStructure(xmlDocPtr srcDoc, const std::map<xmlNodePtr, ObjectBoundary>& objectsToRemove, xmlDocPtr targetDoc, bool isExcludedDoc);
    
    /// @private Рекурсивно копирует узлы с фильтрацией.
    void copyNodeWithFiltering(xmlNodePtr srcNode, xmlNodePtr targetParent, const std::map<xmlNodePtr, ObjectBoundary>& objectsToRemove, bool includeRemoved, bool includeUnmatched);
    
    /// @private Проверяет, помечен ли узел для удаления.
    bool isNodeMarkedForRemoval(xmlNodePtr node, const std::map<xmlNodePtr, ObjectBoundary>& objectsToRemove);
    
    /// @private Копирует атрибуты узла.
    void copyNodeAttributes(xmlNodePtr srcNode, xmlNodePtr targetNode);
    
    /// @private Копирует пространство имён узла.
    void copyNodeNamespace(xmlNodePtr srcNode, xmlNodePtr targetNode, xmlDocPtr targetDoc);
    
    /// @private Обновляет счётчик записей в документе.
    bool updateRecordCount(xmlDocPtr doc, const RecordCountConfig& recordCountConfig, int newCount);
    
    /// @private Находит элемент для обновления счётчика.
    xmlNodePtr findRecordCountElement(xmlDocPtr doc, const std::string& xpath);
    
    /// @private Обновляет значение атрибута или текста.
    bool updateNodeValue(xmlNodePtr node, const std::string& attributeName, const std::string& newValue);
    
    /// @private Читает исходное количество записей.
    int readRecordCountFromSource(xmlDocPtr srcDoc);
};

} // namespace stc