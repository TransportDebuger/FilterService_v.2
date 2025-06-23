#include "../include/XMLProcessor.hpp"

#include <libxml/xpath.h>

#include <filesystem>

#include "stc/compositelogger.hpp"

namespace fs = std::filesystem;

XMLProcessor::XMLProcessor(const SourceConfig &config) : config_(config) {}

xmlDocPtr XMLProcessor::parseXML(const std::string &path) {
  xmlDocPtr doc = xmlReadFile(path.c_str(), nullptr, XML_PARSE_NOBLANKS);
  if (!doc) {
    throw std::runtime_error("Failed to parse XML: " + path);
  }
  return doc;
}

std::string XMLProcessor::extractValue(
    xmlNodePtr node, const SourceConfig::XmlFilterCriterion &crit) {
  if (!crit.attribute.empty()) {
    xmlChar *prop = xmlGetProp(node, BAD_CAST crit.attribute.c_str());
    if (prop) {
      std::string val(reinterpret_cast<char *>(prop));
      xmlFree(prop);
      return val;
    }
  }
  xmlChar *text = xmlNodeGetContent(node);
  std::string val(reinterpret_cast<char *>(text));
  xmlFree(text);
  return val;
}

void XMLProcessor::registerNamespaces(xmlXPathContextPtr ctx, xmlDocPtr doc) {
  if (!config_.xml_filter.namespaces.empty()) {
    // Используем настроенные пространства имён
    registerConfiguredNamespaces(ctx);
    stc::CompositeLogger::instance().debug(
        "Using configured namespaces (" +
        std::to_string(config_.xml_filter.namespaces.size()) + " entries)");
  } else if (config_.xml_filter.auto_register_namespaces) {
    // Автоматически извлекаем из документа
    registerNamespacesFromDocument(ctx, doc);
    stc::CompositeLogger::instance().debug(
        "Auto-registered namespaces from document");
  }
}

void XMLProcessor::registerConfiguredNamespaces(xmlXPathContextPtr ctx) {
  for (const auto &ns : config_.xml_filter.namespaces) {
    xmlXPathRegisterNs(ctx, BAD_CAST ns.prefix.c_str(),
                       BAD_CAST ns.uri.c_str());
    stc::CompositeLogger::instance().debug(
        "Registered configured namespace: " + ns.prefix + " -> " + ns.uri);
  }
}

void XMLProcessor::registerNamespacesFromDocument(xmlXPathContextPtr ctx,
                                                  xmlDocPtr doc) {
  xmlNodePtr root = xmlDocGetRootElement(doc);
  if (!root) return;

  // Извлекаем все объявления пространств имён из корневого элемента
  xmlNsPtr ns = root->nsDef;
  while (ns) {
    if (ns->prefix) {
      xmlXPathRegisterNs(ctx, ns->prefix, ns->href);
      stc::CompositeLogger::instance().debug(
          "Auto-registered namespace: " + std::string((char *)ns->prefix) +
          " -> " + std::string((char *)ns->href));
    } else {
      // Регистрируем дефолтное пространство имён с префиксом "default"
      xmlXPathRegisterNs(ctx, BAD_CAST "default", ns->href);
      stc::CompositeLogger::instance().debug(
          "Auto-registered default namespace: " +
          std::string((char *)ns->href));
    }
    ns = ns->next;
  }
}

std::string XMLProcessor::getDocumentNamespaces(xmlDocPtr doc) {
  std::string result;
  xmlNodePtr root = xmlDocGetRootElement(doc);
  if (!root) return result;

  xmlNsPtr ns = root->nsDef;
  while (ns) {
    if (ns->prefix) {
      result += std::string((char *)ns->prefix) + ":" +
                std::string((char *)ns->href) + "\n";
    } else {
      result += "default:" + std::string((char *)ns->href) + "\n";
    }
    ns = ns->next;
  }
  return result;
}

xmlNodePtr XMLProcessor::findParentEntry(xmlNodePtr node) {
  xmlNodePtr current = node;

  // Поднимаемся по дереву, пока не найдём элемент с типом "entry" или подобным
  while (current && current->type == XML_ELEMENT_NODE) {
    // Проверяем различные возможные имена родительских элементов
    if (xmlStrcmp(current->name, BAD_CAST "entry") == 0 ||
        xmlHasProp(current, BAD_CAST "xsi:type") != nullptr ||
        xmlStrcmp(current->name, BAD_CAST "record") == 0 ||
        xmlStrcmp(current->name, BAD_CAST "item") == 0) {
      return current;
    }
    current = current->parent;
  }

  // Если специфичный родитель не найден, возвращаем исходный узел
  return node;
}

std::string XMLProcessor::makeRelativeXPath(const std::string &xpath) {
  // Если XPath уже относительный, возвращаем как есть
  if (xpath.find("//") != 0 && xpath.find("/") != 0) {
    return xpath;
  }

  std::string relative = xpath;

  // Удаляем начальные слэши
  if (relative.find("//") == 0) {
    relative = relative.substr(2);
  } else if (relative.find("/") == 0) {
    relative = relative.substr(1);
  }

  // Если XPath начинается с известных корневых элементов, убираем их
  std::vector<std::string> root_elements = {"entry/", "record/", "item/"};
  for (const auto &root : root_elements) {
    if (relative.find(root) == 0) {
      relative = relative.substr(root.length());
      break;
    }
  }

  // Добавляем префикс для поиска в потомках
  return "./" + relative;
}

bool XMLProcessor::evaluateEntryAgainstCriteria(xmlNodePtr entry,
                                                xmlDocPtr doc) {
  std::vector<bool> criteriaResults;

  for (const auto &criterion : config_.xml_filter.criteria) {
    bool criterionMatched = false;

    // Создаём контекст XPath для данного элемента
    xmlXPathContextPtr entryCtx = xmlXPathNewContext(doc);
    entryCtx->node = entry;  // Устанавливаем контекст на конкретный элемент

    // Регистрируем пространства имён
    registerNamespaces(entryCtx, doc);

    // Модифицируем XPath для поиска относительно текущего элемента
    std::string relativeXPath = makeRelativeXPath(criterion.xpath);

    xmlXPathObjectPtr result =
        xmlXPathEvalExpression(BAD_CAST relativeXPath.c_str(), entryCtx);

    if (result && result->nodesetval) {
      for (int i = 0; i < result->nodesetval->nodeNr; ++i) {
        xmlNodePtr node = result->nodesetval->nodeTab[i];
        std::string value = extractValue(node, criterion);

        stc::CompositeLogger::instance().debug("Checking value '" + value +
                                               "' against column '" +
                                               criterion.csv_column + "'");

        if (FilterListManager::instance().contains(criterion.csv_column,
                                                   value)) {
          criterionMatched = true;
          stc::CompositeLogger::instance().debug("Value '" + value +
                                                 "' found in filter list");
          break;
        }
      }
    }

    if (result) xmlXPathFreeObject(result);
    xmlXPathFreeContext(entryCtx);

    criteriaResults.push_back(criterionMatched);
  }

  // Применяем логический оператор к результатам
  return applyLogic(criteriaResults);
}

bool XMLProcessor::applyLogic(const std::vector<bool> &results) {
  const auto &op = config_.xml_filter.logic_operator;
  if (op == "AND") {
    return std::all_of(results.begin(), results.end(),
                       [](bool v) { return v; });
  }
  if (op == "OR") {
    return std::any_of(results.begin(), results.end(),
                       [](bool v) { return v; });
  }
  if (op == "MAJORITY") {
    int count = std::count(results.begin(), results.end(), true);
    return count > results.size() / 2;
  }
  if (op == "WEIGHTED") {
    double score = 0, total = 0;
    for (size_t i = 0; i < results.size(); ++i) {
      double w = config_.xml_filter.criteria[i].weight;
      total += w;
      if (results[i]) score += w;
    }
    return score / total >= config_.xml_filter.threshold;
  }
  return false;
}

void XMLProcessor::createOutputDocuments(xmlDocPtr srcDoc, xmlDocPtr &procDoc,
                                         xmlDocPtr &exclDoc) {
  procDoc = xmlNewDoc(BAD_CAST "1.0");
  exclDoc = xmlNewDoc(BAD_CAST "1.0");

  xmlNodePtr srcRoot = xmlDocGetRootElement(srcDoc);
  xmlNodePtr procRoot = xmlCopyNode(srcRoot, 0);  // Копируем без детей
  xmlNodePtr exclRoot = xmlCopyNode(srcRoot, 0);  // Копируем без детей

  xmlDocSetRootElement(procDoc, procRoot);
  xmlDocSetRootElement(exclDoc, exclRoot);
}

void XMLProcessor::saveResults(const std::string &xmlPath, xmlDocPtr procDoc,
                               xmlDocPtr exclDoc, bool hasClean,
                               bool hasMatch) {
  fs::path inputPath(xmlPath);
  std::string filename = inputPath.filename().string();

  // Создаём директории, если они не существуют
  fs::create_directories(config_.processed_dir);
  fs::create_directories(config_.excluded_dir);

  // Сохраняем "чистые" данные
  if (hasClean) {
    std::string processedPath = (fs::path(config_.processed_dir) /
                                 config_.getFilteredFileName(filename))
                                    .string();
    xmlSaveFormatFileEnc(processedPath.c_str(), procDoc, "UTF-8", 1);

    stc::CompositeLogger::instance().info("Saved processed data to: " +
                                          processedPath);
  }

  // Сохраняем отфильтрованные данные
  if (hasMatch) {
    std::string excludedPath =
        (fs::path(config_.excluded_dir) / config_.getExcludedFileName(filename))
            .string();
    xmlSaveFormatFileEnc(excludedPath.c_str(), exclDoc, "UTF-8", 1);

    stc::CompositeLogger::instance().info("Saved excluded data to: " +
                                          excludedPath);
  }
}

// ГЛАВНЫЙ МЕТОД: Полностью переработанный process
bool XMLProcessor::process(const std::string &xmlPath) {
  try {
    // 1. Парсим исходный документ
    xmlDocPtr srcDoc = parseXML(xmlPath);

    // Логируем найденные пространства имён
    std::string namespaces = getDocumentNamespaces(srcDoc);
    if (!namespaces.empty()) {
      stc::CompositeLogger::instance().debug("Document namespaces:\n" +
                                             namespaces);
    }

    // 2. Создаём документы для результатов
    xmlDocPtr procDoc, exclDoc;
    createOutputDocuments(srcDoc, procDoc, exclDoc);
    xmlNodePtr procRoot = xmlDocGetRootElement(procDoc);
    xmlNodePtr exclRoot = xmlDocGetRootElement(exclDoc);

    // 3. Создаём контекст XPath и регистрируем пространства имён
    xmlXPathContextPtr ctx = xmlXPathNewContext(srcDoc);
    registerNamespaces(ctx, srcDoc);

    bool hasMatch = false;
    bool hasClean = false;

    // 4. Получаем все уникальные элементы записей из всех XPath-критериев
    std::set<xmlNodePtr> processedEntries;

    for (const auto &criterion : config_.xml_filter.criteria) {
      xmlXPathObjectPtr result =
          xmlXPathEvalExpression(BAD_CAST criterion.xpath.c_str(), ctx);

      if (result && result->nodesetval) {
        for (int i = 0; i < result->nodesetval->nodeNr; ++i) {
          xmlNodePtr node = result->nodesetval->nodeTab[i];

          // Находим родительский элемент записи
          xmlNodePtr parentEntry = findParentEntry(node);
          if (parentEntry &&
              processedEntries.find(parentEntry) == processedEntries.end()) {
            processedEntries.insert(parentEntry);

            // 5. Оцениваем все критерии для данного элемента
            bool entryMatched =
                evaluateEntryAgainstCriteria(parentEntry, srcDoc);

            // 6. Распределяем элемент между документами
            if (entryMatched) {
              hasMatch = true;
              xmlNodePtr copy = xmlDocCopyNode(parentEntry, exclDoc, 1);
              xmlAddChild(exclRoot, copy);

              stc::CompositeLogger::instance().debug(
                  "Entry matched criteria - moved to excluded document");
            } else {
              hasClean = true;
              xmlNodePtr copy = xmlDocCopyNode(parentEntry, procDoc, 1);
              xmlAddChild(procRoot, copy);

              stc::CompositeLogger::instance().debug(
                  "Entry did not match criteria - moved to processed document");
            }
          }
        }
      }

      if (result) xmlXPathFreeObject(result);
    }

    xmlXPathFreeContext(ctx);
    xmlFreeDoc(srcDoc);

    // 7. Сохраняем результаты
    saveResults(xmlPath, procDoc, exclDoc, hasClean, hasMatch);

    // Освобождаем ресурсы
    xmlFreeDoc(procDoc);
    xmlFreeDoc(exclDoc);

    stc::CompositeLogger::instance().info(
        "XML processing completed successfully for: " + xmlPath);

    return true;

  } catch (const std::exception &e) {
    stc::CompositeLogger::instance().error("XMLProcessor error: " +
                                           std::string(e.what()));
    return false;
  }
}