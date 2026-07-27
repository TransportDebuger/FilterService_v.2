/**
@file XMLProcessor.hpp
@brief Потоковый процессор для обработки, фильтрации и сохранения результатов XML-файлов.
@version 3.0.1
@date 2026-07-24
*/
#pragma once

#include <libxml/parser.h>
#include <libxml/tree.h>
#include <libxml/xpath.h>
#include <libxml/xpathInternals.h>
#include <libxml/xmlreader.h>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "../domain/FilterListManager.hpp"
#include "../domain/sourceconfig.hpp"
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
@brief Выполняет многокритериальную потоковую фильтрацию XML-документов.
*/
class XMLProcessor {
public:
    /**
    @brief Конструктор процессора.
    @param[in] config Конфигурация источника данных.
    @param[in] logger Диспетчер логирования.
    @param[in] filter_list_manager Менеджер списков фильтрации (инъекция зависимости).
    @throw std::invalid_argument Если filter_list_manager равен nullptr.
    */
    explicit XMLProcessor(const SourceConfig& config,
                          std::shared_ptr<stc::logger::ILogger> logger,
                          std::shared_ptr<FilterListManager> filter_list_manager);

    /**
    @brief Обрабатывает XML-файл по указанному пути.
    @param[in] xmlPath Путь к входному файлу.
    @return ProcessingResult Структура с результатом и статистикой обработки.
    @throw std::runtime_error При критических ошибках парсинга или записи.
    */
    ProcessingResult process(const std::string& xmlPath);

private:
    /** @private 
     * @brief Конфигурация источника.*/
    const SourceConfig& config_;
    /** @private 
     * @brief Диспетчер логирования.*/
    std::shared_ptr<stc::logger::ILogger> logger_;
    /** @private 
     * @brief Менеджер списков фильтрации.*/
    std::shared_ptr<FilterListManager> filter_list_manager_;

    // --- Методы потоковой обработки ---

    /**
     * @private
     * @brief Выполняет потоковую обработку на границах записей.
     * @param[in] xmlPath Путь к входному файлу.
     * @param[out] result Структура для накопления статистики.
     * @throw std::runtime_error При ошибках ввода-вывода или парсинга.
     */
    void streamingProcess(const std::string& xmlPath, ProcessingResult& result);

    /**
     * @private
     * @brief Определяет, является ли узел границей «записи» в бизнес-логике.
     * @param[in] node XML-узел.
     * @return true Если узел является записью.
     */
    bool isRecordNode(xmlNodePtr node) const;

    /**
     * @private
     * @brief Потоково дописывает содержимое временного файла в целевой файл.
     * @param[in] destPath Путь к целевому файлу (открывается в режиме "ab").
     * @param[in] srcPath Путь к временному файлу.
     */
    void appendFileContent(const std::string& destPath, const std::string& srcPath) const;

    // --- Методы оценки и работы с XPath (сохранение 100% поведения) ---

    std::string extractValue(xmlNodePtr node, const SourceConfig::XmlFilterCriterion& crit);
    bool applyLogic(const std::vector<bool>& results);
    void registerNamespaces(xmlXPathContextPtr ctx, xmlDocPtr doc);
    void registerConfiguredNamespaces(xmlXPathContextPtr ctx);
    void registerNamespacesFromDocument(xmlXPathContextPtr ctx, xmlDocPtr doc);
    std::string getDocumentNamespaces(xmlDocPtr doc);
    std::string makeRelativeXPath(const std::string& xpath);
    bool evaluateEntryAgainstCriteria(xmlNodePtr entry, xmlDocPtr doc);
    bool updateRecordCount(xmlDocPtr doc, const RecordCountConfig& recordCountConfig, int newCount);
    xmlNodePtr findRecordCountElement(xmlDocPtr doc, const std::string& xpath);
    bool updateNodeValue(xmlNodePtr node, const std::string& attributeName, const std::string& newValue);
    int readRecordCountFromSource(xmlDocPtr srcDoc);
};

} // namespace stc