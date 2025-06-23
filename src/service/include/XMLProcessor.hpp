/**
 * @file XMLProcessor.hpp
 * @brief Класс для обработки и фильтрации XML-файлов
 *
 * @details Поддерживает многокритериальную фильтрацию:
 *  - Использование XPath для выбора узлов
 *  - Сравнение значений узлов или атрибутов с данными из CSV
 *  - Логические операторы: AND, OR, MAJORITY, WEIGHTED
 *  - Автоматическую регистрацию пространств имен.
 */
#pragma once

#include "../include/FilterListManager.hpp"
#include "../include/sourceconfig.hpp"
#include <filesystem>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <libxml/xpath.h>
#include <libxml/xpathInternals.h>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

class XMLProcessor {
public:
  explicit XMLProcessor(const SourceConfig &config);
  bool process(const std::string &xmlPath);

private:
  const SourceConfig &config_;

  // Основные методы обработки
  xmlDocPtr parseXML(const std::string &path);
  std::string extractValue(xmlNodePtr node,
                           const SourceConfig::XmlFilterCriterion &crit);
  std::vector<bool> evaluateCriteria(xmlDocPtr doc);
  bool applyLogic(const std::vector<bool> &results);

  // Управление пространствами имён
  void registerNamespaces(xmlXPathContextPtr ctx, xmlDocPtr doc);
  void registerConfiguredNamespaces(xmlXPathContextPtr ctx);
  void registerNamespacesFromDocument(xmlXPathContextPtr ctx, xmlDocPtr doc);
  std::string getDocumentNamespaces(xmlDocPtr doc);

  // Универсальная обработка элементов
  xmlNodePtr findParentEntry(xmlNodePtr node);
  bool evaluateEntryAgainstCriteria(xmlNodePtr entry, xmlDocPtr doc);
  std::string makeRelativeXPath(const std::string &xpath);

  // Создание выходных документов
  void createOutputDocuments(xmlDocPtr srcDoc, xmlDocPtr &procDoc,
                             xmlDocPtr &exclDoc);
  void saveResults(const std::string &xmlPath, xmlDocPtr procDoc,
                   xmlDocPtr exclDoc, bool hasClean, bool hasMatch);
};