#include "../include/XMLProcessor.hpp"

#include <libxml/xpath.h>

#include <filesystem>

#include "stc/compositelogger.hpp"

namespace fs = std::filesystem;

XMLProcessor::XMLProcessor(const SourceConfig &config) : config_(config) {}

bool XMLProcessor::process(const std::string &xmlPath) {
  try {
    xmlDocPtr srcDoc = parseXML(xmlPath);
    if (!srcDoc) {
      throw std::runtime_error("Failed to parse XML document " + xmlPath);
    }

    std::string namespaces = getDocumentNamespaces(srcDoc);
    if (!namespaces.empty()) {
      stc::CompositeLogger::instance().debug("XML Processor: Document namespaces:\n" +
                                             namespaces);
    }

    xmlDocPtr procDoc = NULL, exclDoc = NULL;
    stc::CompositeLogger::instance().debug("XML Processor: Creating output documents");
    createOutputDocuments(srcDoc, procDoc, exclDoc);
    if (!procDoc || !exclDoc) {
      if (procDoc) xmlFreeDoc(procDoc);
      if (exclDoc) xmlFreeDoc(exclDoc);
      xmlFreeDoc(srcDoc);
      throw std::runtime_error("Failed to create output documents");
    }
    xmlNodePtr procRoot = xmlDocGetRootElement(procDoc);
    xmlNodePtr exclRoot = xmlDocGetRootElement(exclDoc);
    if (!procRoot || !exclRoot) {
      if (procRoot) xmlFreeNode(procRoot);
      if (exclRoot) xmlFreeNode(exclRoot);
      xmlFreeDoc(procDoc);
      xmlFreeDoc(exclDoc);
      xmlFreeDoc(srcDoc);
      throw std::runtime_error("Failed to get root elements from output documents");
    }
    
    // 3. Создаём контекст XPath и регистрируем пространства имён
    xmlXPathContextPtr ctx = xmlXPathNewContext(srcDoc);
    if (!ctx) {
      xmlFreeNode(procRoot);
      xmlFreeNode(exclRoot);
      xmlFreeDoc(procDoc);
      xmlFreeDoc(exclDoc);
      xmlFreeDoc(srcDoc);
      throw std::runtime_error("Failed to create XPath context");
    }
    registerNamespaces(ctx, srcDoc);

    auto analysisResults = collectAndAnalyzeNodes(ctx);
    
    std::vector<xmlNodePtr> nodesToRemove;
    for (const auto& result : analysisResults) {
      if (result.shouldRemove) {
        nodesToRemove.push_back(result.node);
      }
    }

    stc::CompositeLogger::instance().debug(
            "Found " + std::to_string(nodesToRemove.size()) + " nodes matching criteria");

    auto objectBoundaries = findOptimalObjectBoundaries(nodesToRemove);
    stc::CompositeLogger::instance().debug(
            "Determined " + std::to_string(objectBoundaries.size()) + " object boundaries");

    buildOutputStructure(srcDoc, objectBoundaries, procDoc, false);  // processed
    buildOutputStructure(srcDoc, objectBoundaries, exclDoc, true);   // excluded
    
    int removedCount = objectBoundaries.size();
    int originalRecordCount = readRecordCountFromSource(srcDoc);

    procRoot = xmlDocGetRootElement(procDoc);
    int processedCount = originalRecordCount ? originalRecordCount - removedCount : 0;
        
    stc::CompositeLogger::instance().info(
      "Record distribution: original=" + std::to_string(originalRecordCount) + 
      ", processed=" + std::to_string(processedCount) + 
      ", excluded=" + std::to_string(removedCount));

    if (config_.xml_filter.record_count_config.enabled) {
      stc::CompositeLogger::instance().info(
                "Final record count: original=" + std::to_string(originalRecordCount) +
                ", removed=" + std::to_string(removedCount) + 
                ", processed=" + std::to_string(processedCount));
            
      updateRecordCount(procDoc, config_.xml_filter.record_count_config, processedCount);
      updateRecordCount(exclDoc, config_.xml_filter.record_count_config, removedCount);
    }
    exclRoot = xmlDocGetRootElement(exclDoc);
    
    bool hasClean = (procRoot && xmlChildElementCount(procRoot) > 0);
    bool hasMatch = (exclRoot && xmlChildElementCount(exclRoot) > 0);
    
    saveResults(xmlPath, procDoc, exclDoc, hasClean, hasMatch);

    xmlXPathFreeContext(ctx);
    xmlFreeDoc(srcDoc);
    xmlFreeDoc(procDoc);
    xmlFreeDoc(exclDoc);
    stc::CompositeLogger::instance().info("XML processing completed successfully: " + xmlPath);
    return true;
  } catch (const std::exception& e) {
    stc::CompositeLogger::instance().error("XMLProcessor error: " + std::string(e.what()));
    return false;
  }
}

xmlDocPtr XMLProcessor::parseXML(const std::string &path) {
  stc::CompositeLogger::instance().info("Starting parse XML: " + path);
  xmlDocPtr doc = xmlReadFile(path.c_str(), nullptr, XML_PARSE_NOBLANKS);
  if (!doc) {
    stc::CompositeLogger::instance().error("Failed to parse XML: " + path);
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
  if (!xpath.starts_with("//") && !xpath.starts_with("/")) {
    return xpath;
  }

  std::string relative = xpath;

  // Удаляем начальные слэши
  if (relative.starts_with("//")) {
    relative = relative.substr(2);
  } else if (relative.starts_with("/")) {
    relative = relative.substr(1);
  }

  // Если XPath начинается с известных корневых элементов, убираем их
  std::vector<std::string> root_elements = {"entry/", "record/", "item/"};
  for (const auto &root : root_elements) {
    if (relative.starts_with(root)) {
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
    size_t count = std::count(results.begin(), results.end(), true);
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

void XMLProcessor::createOutputDocuments(xmlDocPtr srcDoc, xmlDocPtr& procDoc, xmlDocPtr& exclDoc) {
    xmlNodePtr srcRoot = xmlDocGetRootElement(srcDoc);
    if (!srcRoot) {
      stc::CompositeLogger::instance().error("Unable to find root element in source document.");
      return;
    }

    stc::CompositeLogger::instance().debug("XML Processor: Initializing oputput documents");
    procDoc = xmlNewDoc(BAD_CAST "1.0");
    exclDoc = xmlNewDoc(BAD_CAST "1.0");
    if (!procDoc || !exclDoc) {
      stc::CompositeLogger::instance().error("XML Processor: Unable to allocate memory for output documents.");
      return;
    }
    
    stc::CompositeLogger::instance().debug("XML Processor: copying root element name");
    xmlNodePtr procRoot = xmlNewNode(nullptr, srcRoot->name);
    xmlNodePtr exclRoot = xmlNewNode(nullptr, srcRoot->name);
    
    stc::CompositeLogger::instance().debug("XML Processor: copying attributes with values to destination root node");
    xmlAttrPtr attr = srcRoot->properties;
    while (attr) {
        xmlChar* attrValue = xmlGetProp(srcRoot, attr->name);
        if (attrValue) {
            xmlSetProp(procRoot, attr->name, attrValue);
            xmlSetProp(exclRoot, attr->name, attrValue);
            xmlFree(attrValue);
        }
        attr = attr->next;
    }
    
    stc::CompositeLogger::instance().debug("XML Processor: copying namespaces to destination root node");
    xmlNsPtr ns = srcRoot->nsDef;
    while (ns) {
        xmlNewNs(procRoot, ns->href, ns->prefix);
        xmlNewNs(exclRoot, ns->href, ns->prefix);
        ns = ns->next;
    }
    
    stc::CompositeLogger::instance().debug("XML Processor: setting namespaces for destination root node");
    if (srcRoot->ns) {
        xmlNsPtr procNs = xmlSearchNs(nullptr, procRoot, srcRoot->ns->prefix);
        xmlNsPtr exclNs = xmlSearchNs(nullptr, exclRoot, srcRoot->ns->prefix);
        
        if (procNs) xmlSetNs(procRoot, procNs);
        if (exclNs) xmlSetNs(exclRoot, exclNs);
    }
    
    stc::CompositeLogger::instance().debug("XML Processor: binding root element to destination documents");
    xmlDocSetRootElement(procDoc, procRoot);
    xmlDocSetRootElement(exclDoc, exclRoot);
    
    stc::CompositeLogger::instance().debug("XML Processor: root element copied with all attributes and namespaces");
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

std::vector<XMLProcessor::NodeAnalysisResult> 
XMLProcessor::collectAndAnalyzeNodes(xmlXPathContextPtr ctx) {
    std::vector<NodeAnalysisResult> results;
    
    // Карта: узел → результаты по критериям
    std::map<xmlNodePtr, std::vector<bool>> nodeMap;

    // ШАГ 1: Собираем все найденные узлы и результаты по критериям
    for (size_t criterionIndex = 0; criterionIndex < config_.xml_filter.criteria.size(); ++criterionIndex) {
        const auto& criterion = config_.xml_filter.criteria[criterionIndex];
        
        stc::CompositeLogger::instance().debug(
            "Processing criterion " + std::to_string(criterionIndex + 1) + 
            ": " + criterion.xpath);

        xmlXPathObjectPtr xpathResult = xmlXPathEvalExpression(
            BAD_CAST criterion.xpath.c_str(), ctx);

        if (!xpathResult) {
            stc::CompositeLogger::instance().warning(
                "XPath evaluation failed for: " + criterion.xpath);
            continue;
        }

        if (xpathResult->nodesetval) {
            for (int i = 0; i < xpathResult->nodesetval->nodeNr; ++i) {
                xmlNodePtr node = xpathResult->nodesetval->nodeTab[i];
                
                // Извлекаем значение и проверяем соответствие фильтру
                std::string value = extractValue(node, criterion);
                bool matches = FilterListManager::instance().contains(
                    criterion.csv_column, value);

                stc::CompositeLogger::instance().debug(
                    "Node value '" + value + "' " + 
                    (matches ? "matches" : "doesn't match") + 
                    " filter column '" + criterion.csv_column + "'");

                // Инициализируем вектор результатов для узла
                if (nodeMap.find(node) == nodeMap.end()) {
                    nodeMap[node].resize(config_.xml_filter.criteria.size(), false);
                }

                // Сохраняем результат для этого критерия
                nodeMap[node][criterionIndex] = matches;
            }
        }

        xmlXPathFreeObject(xpathResult);
    }

    stc::CompositeLogger::instance().debug(
        "Found " + std::to_string(nodeMap.size()) + " nodes with criteria results");

    // ШАГ 2: Группируем узлы по родительскому объекту
    std::map<xmlNodePtr, std::vector<bool>> objectCriteriaResults;
    std::map<xmlNodePtr, std::vector<xmlNodePtr>> objectChildNodes;

    for (const auto& pair : nodeMap) {
        xmlNodePtr node = pair.first;
        const std::vector<bool>& nodeCriteria = pair.second;
        
        // Находим родительский объект (ближайший родитель)
        xmlNodePtr parentObject = node->parent;
        while (parentObject && parentObject->type != XML_ELEMENT_NODE) {
            parentObject = parentObject->parent;
        }
        
        if (!parentObject) {
            stc::CompositeLogger::instance().warning(
                "Could not find parent object for node: " + 
                std::string(reinterpret_cast<const char*>(node->name)));
            continue;
        }

        // Инициализируем результаты для родительского объекта
        if (objectCriteriaResults.find(parentObject) == objectCriteriaResults.end()) {
            objectCriteriaResults[parentObject].resize(config_.xml_filter.criteria.size(), false);
        }

        // OR-объединение критериев: если хоть один дочерний узел совпал по критерию,
        // считаем что объект совпал по этому критерию
        for (size_t i = 0; i < nodeCriteria.size(); ++i) {
            if (nodeCriteria[i]) {
                objectCriteriaResults[parentObject][i] = true;
            }
        }

        // Запоминаем какие узлы относятся к этому объекту (для отладки)
        objectChildNodes[parentObject].push_back(node);
    }

    stc::CompositeLogger::instance().debug(
        "Grouped into " + std::to_string(objectCriteriaResults.size()) + " parent objects");

    // ШАГ 3: Применяем логические операторы к каждому объекту
    for (const auto& objPair : objectCriteriaResults) {
        xmlNodePtr parentObject = objPair.first;
        const std::vector<bool>& criteria = objPair.second;
        
        // Применяем логику (AND/OR/MAJORITY/WEIGHTED)
        bool shouldRemove = applyLogic(criteria);
        
        // Подробное логирование
        std::string criteriaStr = "[";
        for (size_t i = 0; i < criteria.size(); ++i) {
            criteriaStr += (criteria[i] ? "T" : "F");
            if (i < criteria.size() - 1) criteriaStr += ", ";
        }
        criteriaStr += "]";
        
        stc::CompositeLogger::instance().debug(
            "Object at " + buildNodePath(parentObject) + 
            " criteria: " + criteriaStr +
            " → should be " + (shouldRemove ? "REMOVED" : "KEPT"));

        // Если объект должен быть удалён, добавляем в результаты
        if (shouldRemove) {
            NodeAnalysisResult result;
            result.node = parentObject;
            result.criteriaResults = criteria;
            result.shouldRemove = true;
            results.push_back(result);
            
            // Дополнительное логирование для отладки
            stc::CompositeLogger::instance().debug(
                "  ↳ Object has " + std::to_string(objectChildNodes[parentObject].size()) + 
                " matching child nodes:");
            for (xmlNodePtr childNode : objectChildNodes[parentObject]) {
                stc::CompositeLogger::instance().debug(
                    "    - " + std::string(reinterpret_cast<const char*>(childNode->name)));
            }
        }
    }

    stc::CompositeLogger::instance().info(
        "Analysis complete: " + std::to_string(results.size()) + 
        " objects marked for removal");

    return results;
}

std::map<xmlNodePtr, XMLProcessor::ObjectBoundary> 
XMLProcessor::findOptimalObjectBoundaries(const std::vector<xmlNodePtr>& nodesToRemove) {
    std::map<xmlNodePtr, ObjectBoundary> boundaries;
    
    if (nodesToRemove.empty()) {
        return boundaries;
    }

    stc::CompositeLogger::instance().debug(
        "Finding optimal boundaries for " + std::to_string(nodesToRemove.size()) + " nodes");

    // ИСПРАВЛЕНИЕ: используем сами узлы, а не их родителей!
    for (xmlNodePtr objectNode : nodesToRemove) {
        // Находим контейнер для этого объекта (его родитель)
        xmlNodePtr container = objectNode->parent;
        while (container && container->type != XML_ELEMENT_NODE) {
            container = container->parent;
        }
        
        if (!container) {
            container = xmlDocGetRootElement(objectNode->doc);
        }

        ObjectBoundary boundary(
            objectNode,              // САМ ОБЪЕКТ для удаления (pdp)
            container,               // Его контейнер (pnr)
            calculateNodeDepth(objectNode),
            buildNodePath(objectNode)
        );
        
        boundaries[objectNode] = boundary;  // Ключ - сам объект!
        
        stc::CompositeLogger::instance().debug(
            "Object boundary: " + boundary.objectPath + 
            " (depth: " + std::to_string(boundary.depth) + ")");
    }

    return boundaries;
}

xmlNodePtr XMLProcessor::findNearestCommonContainer(const std::vector<xmlNodePtr>& nodes) {
    if (nodes.empty()) return nullptr;
    if (nodes.size() == 1) {
        xmlNodePtr parent = nodes[0]->parent;
        while (parent && parent->type != XML_ELEMENT_NODE) {
            parent = parent->parent;
        }
        return parent;
    }

    // Строим пути от корня для каждого узла
    std::vector<std::vector<xmlNodePtr>> paths;
    for (xmlNodePtr node : nodes) {
        std::vector<xmlNodePtr> path;
        xmlNodePtr current = node;
        
        while (current && current->type == XML_ELEMENT_NODE) {
            path.insert(path.begin(), current);
            current = current->parent;
        }
        paths.push_back(path);
    }

    // Находим общий префикс путей
    xmlNodePtr commonAncestor = nullptr;
    size_t minPathLength = paths[0].size();
    for (const auto& path : paths) {
        minPathLength = std::min(minPathLength, path.size());
    }

    for (size_t i = 0; i < minPathLength; ++i) {
        xmlNodePtr candidate = paths[0][i];
        bool isCommon = true;
        
        for (size_t j = 1; j < paths.size(); ++j) {
            if (paths[j][i] != candidate) {
                isCommon = false;
                break;
            }
        }
        
        if (isCommon) {
            commonAncestor = candidate;
        } else {
            break;
        }
    }

    return commonAncestor;
}

void XMLProcessor::buildOutputStructure(
    xmlDocPtr srcDoc, 
    const std::map<xmlNodePtr, ObjectBoundary>& objectsToRemove,
    xmlDocPtr targetDoc, 
    bool isExcludedDoc) {
    
    xmlNodePtr srcRoot = xmlDocGetRootElement(srcDoc);
    xmlNodePtr targetRoot = xmlDocGetRootElement(targetDoc);
        
    if (!srcRoot || !targetRoot) {
        stc::CompositeLogger::instance().error(
            "Invalid source or target document structure");
        return;
    }

    std::string docType = isExcludedDoc ? "excluded" : "processed";
    stc::CompositeLogger::instance().info(
        "Building " + docType + " document structure");

    // Для excluded: копируем только помеченные объекты
    // Для processed: копируем только НЕ помеченные объекты
    bool includeRemoved = isExcludedDoc;    // true для excluded
    bool includeUnmatched = !isExcludedDoc;  // true для processed

    // Рекурсивно копируем дочерние элементы корня
    xmlNodePtr child = srcRoot->children;
    while (child) {
        if (child->type == XML_ELEMENT_NODE) {
            copyNodeWithFiltering(child, targetRoot, objectsToRemove, 
                                 includeRemoved, includeUnmatched);
        }
        child = child->next;
    }
    
    size_t resultCount = xmlChildElementCount(targetRoot);
    stc::CompositeLogger::instance().info(
        "Completed " + docType + " document: " + 
        std::to_string(resultCount) + " top-level elements");
}

void XMLProcessor::copyNodeWithFiltering(
    xmlNodePtr srcNode, 
    xmlNodePtr targetParent,
    const std::map<xmlNodePtr, ObjectBoundary>& objectsToRemove,
    bool includeRemoved,
    bool includeUnmatched) {
    
    if (!srcNode || srcNode->type != XML_ELEMENT_NODE) {
        return;
    }

    bool isMarkedForRemoval = isNodeMarkedForRemoval(srcNode, objectsToRemove);
    
    // ===== СЛУЧАЙ 1: Помеченный объект для excluded =====
    if (isMarkedForRemoval && includeRemoved) {
        xmlNodePtr copiedNode = xmlDocCopyNode(srcNode, targetParent->doc, 1);
        xmlAddChild(targetParent, copiedNode);
        return;
    }
    
    // ===== СЛУЧАЙ 2: Помеченный объект для processed - ПОЛНОСТЬЮ ПРОПУСКАЕМ =====
    if (isMarkedForRemoval && !includeRemoved) {
        return;
    }
    
    // ===== СЛУЧАЙ 3: Чистый ЛИСТОВОЙ объект для processed =====
    // Важно: листовой = без дочерних элементов!
    if (!isMarkedForRemoval && includeUnmatched && !includeRemoved) {
        // Проверяем: это листовой элемент (не контейнер)?
        if (xmlChildElementCount(srcNode) == 0) {
            // Это конечный объект - копируем полностью
            xmlNodePtr copiedNode = xmlDocCopyNode(srcNode, targetParent->doc, 1);
            xmlAddChild(targetParent, copiedNode);
            return;
        }
        // Иначе (это контейнер) - продолжаем к рекурсии ниже
    }
    
    // ===== СЛУЧАЙ 4: Контейнер или нужна рекурсия =====
    xmlNodePtr newNode = xmlNewNode(nullptr, srcNode->name);
    
    copyNodeAttributes(srcNode, newNode);
    copyNodeNamespace(srcNode, newNode, targetParent->doc);

    bool hasValidChildren = false;
    bool hasTextContent = false;

    // Обрабатываем текст
    xmlNodePtr child = srcNode->children;
    while (child) {
        if (child->type == XML_TEXT_NODE || child->type == XML_CDATA_SECTION_NODE) {
            xmlChar* content = xmlNodeGetContent(child);
            if (content) {
                std::string textContent = reinterpret_cast<const char*>(content);
                if (!textContent.empty() && 
                    textContent.find_first_not_of(" \t\n\r") != std::string::npos) {
                    xmlNodePtr textNode = xmlNewText(content);
                    xmlAddChild(newNode, textNode);
                    hasTextContent = true;
                }
                xmlFree(content);
            }
        }
        child = child->next;
    }

    // РЕКУРСИВНО обрабатываем дочерние элементы
    child = srcNode->children;
    while (child) {
        if (child->type == XML_ELEMENT_NODE) {
            size_t childCountBefore = xmlChildElementCount(newNode);
            
            copyNodeWithFiltering(child, newNode, objectsToRemove, 
                                 includeRemoved, includeUnmatched);
            
            size_t childCountAfter = xmlChildElementCount(newNode);
            if (childCountAfter > childCountBefore) {
                hasValidChildren = true;
            }
        }
        child = child->next;
    }

    // НОВОЕ: Для excluded - добавляем листовые свойства
    if (includeRemoved && hasValidChildren) {
        child = srcNode->children;
        while (child) {
            if (child->type == XML_ELEMENT_NODE && xmlChildElementCount(child) == 0) {
                bool alreadyInResult = false;
                xmlNodePtr checkChild = newNode->children;
                while (checkChild) {
                    if (checkChild->type == XML_ELEMENT_NODE) {
                        if (xmlStrcmp(checkChild->name, child->name) == 0) {
                            xmlAttrPtr srcAttr = child->properties;
                            xmlAttrPtr dstAttr = checkChild->properties;
                            if (srcAttr && dstAttr) {
                                xmlChar* srcVal = xmlGetProp(child, srcAttr->name);
                                xmlChar* dstVal = xmlGetProp(checkChild, dstAttr->name);
                                if (srcVal && dstVal && 
                                    xmlStrcmp(srcVal, dstVal) == 0) {
                                    alreadyInResult = true;
                                }
                                if (srcVal) xmlFree(srcVal);
                                if (dstVal) xmlFree(dstVal);
                            }
                        }
                    }
                    if (alreadyInResult) break;
                    checkChild = checkChild->next;
                }
                
                if (!alreadyInResult) {
                    xmlNodePtr propertyCopy = xmlDocCopyNode(child, targetParent->doc, 1);
                    xmlAddChild(newNode, propertyCopy);
                }
            }
            child = child->next;
        }
    }

    // Добавляем контейнер только если он не пустой
    if (hasValidChildren || hasTextContent) {
        xmlAddChild(targetParent, newNode);
    } else {
        xmlFreeNode(newNode);
    }
}

void XMLProcessor::copyNodeNamespace(xmlNodePtr srcNode, xmlNodePtr targetNode, xmlDocPtr targetDoc) {
    // Копируем namespace определения
    xmlNsPtr ns = srcNode->nsDef;
    while (ns) {
        xmlNewNs(targetNode, ns->href, ns->prefix);
        ns = ns->next;
    }
    
    // Устанавливаем namespace для элемента
    if (srcNode->ns) {
        xmlNsPtr targetNs = xmlSearchNs(targetDoc, targetNode, srcNode->ns->prefix);
        if (targetNs) {
            xmlSetNs(targetNode, targetNs);
        }
    }
}

void XMLProcessor::copyNodeAttributes(xmlNodePtr srcNode, xmlNodePtr targetNode) {
    xmlAttrPtr attr = srcNode->properties;
    while (attr) {
        xmlChar* attrValue = xmlGetProp(srcNode, attr->name);
        if (attrValue) {
            xmlSetProp(targetNode, attr->name, attrValue);
            xmlFree(attrValue);
        }
        attr = attr->next;
    }
}

bool XMLProcessor::isNodeMarkedForRemoval(xmlNodePtr node, 
                                         const std::map<xmlNodePtr, ObjectBoundary>& objectsToRemove) {
    return objectsToRemove.find(node) != objectsToRemove.end();
}

int XMLProcessor::calculateNodeDepth(xmlNodePtr node) {
    int depth = 0;
    xmlNodePtr current = node;
    
    while (current && current->type == XML_ELEMENT_NODE) {
        depth++;
        current = current->parent;
    }
    
    return depth;
}

std::string XMLProcessor::buildNodePath(xmlNodePtr node) {
    std::vector<std::string> pathElements;
    xmlNodePtr current = node;
    
    while (current && current->type == XML_ELEMENT_NODE) {
        std::string element = reinterpret_cast<const char*>(current->name);
        
        // Добавляем первый атрибут для уникальности (если есть)
        xmlAttrPtr attr = current->properties;
        if (attr) {
            xmlChar* attrValue = xmlGetProp(current, attr->name);
            if (attrValue) {
                element += "[@" + std::string(reinterpret_cast<const char*>(attr->name)) + 
                          "='" + std::string(reinterpret_cast<const char*>(attrValue)) + "']";
                xmlFree(attrValue);
            }
        }
        
        pathElements.insert(pathElements.begin(), element);
        current = current->parent;
    }

    std::string path = "/";
    for (size_t i = 0; i < pathElements.size(); ++i) {
        path += pathElements[i];
        if (i < pathElements.size() - 1) path += "/";
    }
    
    return path;
}

bool XMLProcessor::updateRecordCount(xmlDocPtr doc, 
                                     const RecordCountConfig& recordCountConfig,
                                     int newCount) {
    if (!recordCountConfig.enabled || !doc) {
        stc::CompositeLogger::instance().debug(
            "Record count update skipped: enabled=" + 
            std::string(recordCountConfig.enabled ? "true" : "false") + 
            ", doc=" + std::string(doc ? "not null" : "null"));
        return false;
    }

    stc::CompositeLogger::instance().debug(
        "Attempting to update record count: xpath='" + recordCountConfig.xpath + 
        "', attribute='" + recordCountConfig.attribute + 
        "', newCount=" + std::to_string(newCount));

    // Находим элемент по XPath
    xmlNodePtr countElement = findRecordCountElement(doc, recordCountConfig.xpath);
    
    if (!countElement) {
        stc::CompositeLogger::instance().warning(
            "Record count element not found with xpath: '" + recordCountConfig.xpath + "'");
        return false;
    }

    stc::CompositeLogger::instance().debug(
        "Found record count element: " + 
        std::string(reinterpret_cast<const char*>(countElement->name)));

    // Обновляем значение
    bool updated = updateNodeValue(countElement, recordCountConfig.attribute, 
                                  std::to_string(newCount));

    if (updated) {
        stc::CompositeLogger::instance().info(
            "Record count updated successfully to: " + std::to_string(newCount));
    } else {
        stc::CompositeLogger::instance().error(
            "Failed to update record count value on element: " + 
            std::string(reinterpret_cast<const char*>(countElement->name)));
    }

    return updated;
}

xmlNodePtr XMLProcessor::findRecordCountElement(xmlDocPtr doc, const std::string& xpath) {
    if (!doc || xpath.empty()) {
        return nullptr;
    }

    xmlXPathContextPtr ctx = xmlXPathNewContext(doc);
    if (!ctx) {
        stc::CompositeLogger::instance().error("Failed to create XPath context");
        return nullptr;
    }

    registerNamespaces(ctx, doc);

    stc::CompositeLogger::instance().debug(
        "Searching for record count with xpath: '" + xpath + "'");

    // Первая попытка с исходным XPath
    xmlXPathObjectPtr result = xmlXPathEvalExpression(BAD_CAST xpath.c_str(), ctx);
    
    xmlNodePtr foundNode = nullptr;
    
    if (result && result->nodesetval && result->nodesetval->nodeNr > 0) {
        foundNode = result->nodesetval->nodeTab[0];
        stc::CompositeLogger::instance().debug(
            "Found record count element with xpath: '" + xpath + "'");
    } else {
        // НОВОЕ: Если не нашли, пробуем с namespace префиксом ns4:
        if (result) xmlXPathFreeObject(result);
        
        std::string altXpath;
        
        // Если XPath содержит "Export", пробуем с ns4: префиксом
        if (xpath.find("Export") != std::string::npos) {
            // Заменяем Export на ns4:Export
            altXpath = xpath;
            size_t pos = altXpath.find("Export");
            altXpath.replace(pos, 6, "ns4:Export");  // 6 = длина "Export"
            
            stc::CompositeLogger::instance().debug(
                "First xpath didn't match, trying alternative: '" + altXpath + "'");
            
            result = xmlXPathEvalExpression(BAD_CAST altXpath.c_str(), ctx);
            
            if (result && result->nodesetval && result->nodesetval->nodeNr > 0) {
                foundNode = result->nodesetval->nodeTab[0];
                stc::CompositeLogger::instance().debug(
                    "Found record count element with alternative xpath: '" + altXpath + "'");
            }
        }
        
        if (!foundNode) {
            stc::CompositeLogger::instance().warning(
                "No nodes found for record count with xpath: '" + xpath + 
                "' (and alternative paths)");
        }
    }

    if (result) {
        xmlXPathFreeObject(result);
    }
    xmlXPathFreeContext(ctx);

    return foundNode;
}

bool XMLProcessor::updateNodeValue(xmlNodePtr node, 
                                   const std::string& attributeName, 
                                   const std::string& newValue) {
    if (!node || attributeName.empty()) {
        stc::CompositeLogger::instance().warning("updateNodeValue: invalid parameters");
        return false;
    }

    stc::CompositeLogger::instance().debug(
        "Updating node '" + std::string(reinterpret_cast<const char*>(node->name)) + 
        "' attribute '" + attributeName + "' to value: " + newValue);

    // Проверяем: есть ли атрибут с таким именем (игнорируя namespace)
    xmlAttrPtr attr = node->properties;
    bool foundAttr = false;
    
    while (attr) {
        std::string attrName = reinterpret_cast<const char*>(attr->name);
        
        // Сравниваем имя атрибута (может быть без namespace)
        if (attrName == attributeName || 
            attrName.find(':' + attributeName) != std::string::npos) {
            
            // Обновляем существующий атрибут
            xmlChar* content = xmlNodeGetContent(attr->children);
            if (content) xmlFree(content);
            
            xmlSetProp(node, attr->name, BAD_CAST newValue.c_str());
            
            stc::CompositeLogger::instance().debug(
                "Updated existing attribute '" + attrName + "' to: " + newValue);
            foundAttr = true;
            break;
        }
        attr = attr->next;
    }
    
    if (!foundAttr) {
        // Создаём новый атрибут (без namespace префикса)
        xmlSetProp(node, BAD_CAST attributeName.c_str(), BAD_CAST newValue.c_str());
        
        stc::CompositeLogger::instance().debug(
            "Created new attribute '" + attributeName + "' with value: " + newValue);
    }

    return true;
}

int XMLProcessor::readRecordCountFromSource(xmlDocPtr srcDoc) {
    if (!config_.xml_filter.record_count_config.enabled || !srcDoc) {
        return 0;
    }

    xmlNodePtr element = findRecordCountElement(srcDoc, 
                                               config_.xml_filter.record_count_config.xpath);
    
    if (!element) {
        stc::CompositeLogger::instance().warning("Could not find record count in source document");
        return 0;
    }

    // Получаем значение атрибута
    xmlChar* attrValue = xmlGetProp(element, 
                                    BAD_CAST config_.xml_filter.record_count_config.attribute.c_str());
    
    if (!attrValue) {
        stc::CompositeLogger::instance().warning(
            "Record count attribute not found: " + config_.xml_filter.record_count_config.attribute);
        return 0;
    }

    int originalCount = std::stoi(reinterpret_cast<const char*>(attrValue));
    xmlFree(attrValue);
    
    stc::CompositeLogger::instance().info(
        "Original record count from source: " + std::to_string(originalCount));
    
    return originalCount;
}