/**
@file XMLProcessor.cpp
@brief Реализация процессора XML-файлов.
@version 2.0.0
@date 2026-07-17
*/
#include "../include/XMLProcessor.hpp"
#include <libxml/xpath.h>
#include <filesystem>
#include <algorithm>
#include <map>

namespace fs = std::filesystem;

namespace stc {

XMLProcessor::XMLProcessor(const SourceConfig &config, 
                           std::shared_ptr<stc::logger::ILogger> logger) 
    : config_(config), logger_(std::move(logger)) {}

ProcessingResult XMLProcessor::process(const std::string &xmlPath) {
    ProcessingResult result;
    
    // 1. Получаем размер файла для метрики bytes_processed
    try {
        result.bytes_processed = fs::file_size(xmlPath);
    } catch (const fs::filesystem_error& e) {
        logger_->Warning("Failed to get file size for " + xmlPath + ": " + e.what());
        result.bytes_processed = 0;
    }

    try {
        xmlDocPtr srcDoc = parseXML(xmlPath);
        if (!srcDoc) {
            result.error_type = ProcessingResult::ErrorType::PARSE;
            throw std::runtime_error("Failed to parse XML document " + xmlPath);
        }

        std::string namespaces = getDocumentNamespaces(srcDoc);
        if (!namespaces.empty()) {
            logger_->Debug("XML Processor: Document namespaces: \n" + namespaces);
        }

        xmlDocPtr procDoc = nullptr, exclDoc = nullptr;
        logger_->Debug("XML Processor: Creating output documents");
        createOutputDocuments(srcDoc, procDoc, exclDoc);
        
        if (!procDoc || !exclDoc) {
            if (procDoc) xmlFreeDoc(procDoc);
            if (exclDoc) xmlFreeDoc(exclDoc);
            xmlFreeDoc(srcDoc);
            result.error_type = ProcessingResult::ErrorType::PARSE;
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
            result.error_type = ProcessingResult::ErrorType::PARSE;
            throw std::runtime_error("Failed to get root elements from output documents");
        }

        xmlXPathContextPtr ctx = xmlXPathNewContext(srcDoc);
        if (!ctx) {
            xmlFreeNode(procRoot);
            xmlFreeNode(exclRoot);
            xmlFreeDoc(procDoc);
            xmlFreeDoc(exclDoc);
            xmlFreeDoc(srcDoc);
            result.error_type = ProcessingResult::ErrorType::PARSE;
            throw std::runtime_error("Failed to create XPath context");
        }

        registerNamespaces(ctx, srcDoc);
        auto analysisResults = collectAndAnalyzeNodes(ctx);
        
        std::vector<xmlNodePtr> nodesToRemove;
        for (const auto& res : analysisResults) {
            if (res.shouldRemove) {
                nodesToRemove.push_back(res.node);
            }
        }
        
        logger_->Debug("Found " + std::to_string(nodesToRemove.size()) + " nodes matching criteria");
        
        auto objectBoundaries = findOptimalObjectBoundaries(nodesToRemove);
        logger_->Debug("Determined " + std::to_string(objectBoundaries.size()) + " object boundaries");
        
        buildOutputStructure(srcDoc, objectBoundaries, procDoc, false);
        buildOutputStructure(srcDoc, objectBoundaries, exclDoc, true);
        
        int removedCount = static_cast<int>(objectBoundaries.size());
        int originalRecordCount = readRecordCountFromSource(srcDoc);
        
        // Заполняем метрики записей
        result.records_processed = static_cast<size_t>(originalRecordCount);
        result.records_matched = static_cast<size_t>(removedCount);

        procRoot = xmlDocGetRootElement(procDoc);
        int processedCount = originalRecordCount ? originalRecordCount - removedCount : 0;
        
        logger_->Info("Record distribution: original=" + std::to_string(originalRecordCount) +
                      ", processed=" + std::to_string(processedCount) +
                      ", excluded=" + std::to_string(removedCount));

        if (config_.xml_filter.record_count_config.enabled) {
            logger_->Info("Final record count: original=" + std::to_string(originalRecordCount) +
                          ", removed=" + std::to_string(removedCount) +
                          ", processed=" + std::to_string(processedCount));
            updateRecordCount(procDoc, config_.xml_filter.record_count_config, processedCount);
            updateRecordCount(exclDoc, config_.xml_filter.record_count_config, removedCount);
        }

        exclRoot = xmlDocGetRootElement(exclDoc);
        bool hasClean = (procRoot && xmlChildElementCount(procRoot) > 0);
        bool hasMatch = (exclRoot && xmlChildElementCount(exclRoot) > 0);

        // 2. Отдельный блок для операций записи для корректной классификации ошибки
        try {
            saveResults(xmlPath, procDoc, exclDoc, hasClean, hasMatch);
        } catch (const std::exception& e) {
            result.error_type = ProcessingResult::ErrorType::WRITE;
            logger_->Error("XMLProcessor saveResults failed: " + std::string(e.what()));
            // Освобождаем ресурсы перед пробросом исключения
            xmlXPathFreeContext(ctx);
            xmlFreeDoc(srcDoc);
            xmlFreeDoc(procDoc);
            xmlFreeDoc(exclDoc);
            throw; 
        }

        // Успешное завершение
        result.success = true;

        xmlXPathFreeContext(ctx);
        xmlFreeDoc(srcDoc);
        xmlFreeDoc(procDoc);
        xmlFreeDoc(exclDoc);
        
        logger_->Info("XML processing completed successfully: " + xmlPath);
        
    } catch (const std::exception& e) {
        logger_->Error("XMLProcessor error: " + std::string(e.what()));
        result.success = false;
        // Если тип ошибки не был установлен явно (например, WRITE), считаем ошибкой парсинга/анализа
        if (result.error_type == ProcessingResult::ErrorType::NONE) {
            result.error_type = ProcessingResult::ErrorType::PARSE;
        }
    }
    
    return result;
}

xmlDocPtr XMLProcessor::parseXML(const std::string &path) {
    logger_->Info("Starting parse XML: " + path);
    xmlDocPtr doc = xmlReadFile(path.c_str(), nullptr, XML_PARSE_NOBLANKS);
    if (!doc) {
        logger_->Error("Failed to parse XML: " + path);
        throw std::runtime_error("Failed to parse XML: " + path);
    }
    return doc;
}

std::string XMLProcessor::extractValue(xmlNodePtr node, const SourceConfig::XmlFilterCriterion &crit) {
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
        registerConfiguredNamespaces(ctx);
        logger_->Debug("Using configured namespaces (" + std::to_string(config_.xml_filter.namespaces.size()) + " entries)");
    } else if (config_.xml_filter.auto_register_namespaces) {
        registerNamespacesFromDocument(ctx, doc);
        logger_->Debug("Auto-registered namespaces from document");
    }
}

void XMLProcessor::registerConfiguredNamespaces(xmlXPathContextPtr ctx) {
    for (const auto &ns : config_.xml_filter.namespaces) {
        xmlXPathRegisterNs(ctx, BAD_CAST ns.prefix.c_str(), BAD_CAST ns.uri.c_str());
        logger_->Debug("Registered configured namespace: " + ns.prefix + " -> " + ns.uri);
    }
}

void XMLProcessor::registerNamespacesFromDocument(xmlXPathContextPtr ctx, xmlDocPtr doc) {
    xmlNodePtr root = xmlDocGetRootElement(doc);
    if (!root) return;

    xmlNsPtr ns = root->nsDef;
    while (ns) {
        if (ns->prefix) {
            xmlXPathRegisterNs(ctx, ns->prefix, ns->href);
            logger_->Debug("Auto-registered namespace: " + std::string((char *)ns->prefix) + " -> " + std::string((char *)ns->href));
        } else {
            xmlXPathRegisterNs(ctx, BAD_CAST "default", ns->href);
            logger_->Debug("Auto-registered default namespace: " + std::string((char *)ns->href));
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
            result += std::string((char *)ns->prefix) + ":" + std::string((char *)ns->href) + "\n";
        } else {
            result += "default:" + std::string((char *)ns->href) + "\n";
        }
        ns = ns->next;
    }
    return result;
}

xmlNodePtr XMLProcessor::findParentEntry(xmlNodePtr node) {
    xmlNodePtr current = node;
    while (current && current->type == XML_ELEMENT_NODE) {
        if (xmlStrcmp(current->name, BAD_CAST "entry") == 0 ||
            xmlHasProp(current, BAD_CAST "xsi:type") != nullptr ||
            xmlStrcmp(current->name, BAD_CAST "record") == 0 ||
            xmlStrcmp(current->name, BAD_CAST "item") == 0) {
            return current;
        }
        current = current->parent;
    }
    return node;
}

std::string XMLProcessor::makeRelativeXPath(const std::string &xpath) {
    if (!xpath.starts_with("//") && !xpath.starts_with("/")) {
        return xpath;
    }
    std::string relative = xpath;
    if (relative.starts_with("//")) {
        relative = relative.substr(2);
    } else if (relative.starts_with("/")) {
        relative = relative.substr(1);
    }

    std::vector<std::string> root_elements = {"entry/", "record/", "item/"};
    for (const auto &root : root_elements) {
        if (relative.starts_with(root)) {
            relative = relative.substr(root.length());
            break;
        }
    }
    return "./" + relative;
}

bool XMLProcessor::evaluateEntryAgainstCriteria(xmlNodePtr entry, xmlDocPtr doc) {
    std::vector<bool> criteriaResults;
    for (const auto &criterion : config_.xml_filter.criteria) {
        bool criterionMatched = false;
        xmlXPathContextPtr entryCtx = xmlXPathNewContext(doc);
        entryCtx->node = entry;
        registerNamespaces(entryCtx, doc);

        std::string relativeXPath = makeRelativeXPath(criterion.xpath);
        xmlXPathObjectPtr result = xmlXPathEvalExpression(BAD_CAST relativeXPath.c_str(), entryCtx);

        if (result && result->nodesetval) {
            for (int i = 0; i < result->nodesetval->nodeNr; ++i) {
                xmlNodePtr node = result->nodesetval->nodeTab[i];
                std::string value = extractValue(node, criterion);
                logger_->Debug("Checking value '" + value + "' against column '" + criterion.csv_column + "'");
                
                // Примечание: FilterListManager все еще является синглтоном, 
                // его рефакторинг на DI планируется следующим этапом.
                if (FilterListManager::instance().contains(criterion.csv_column, value)) {
                    criterionMatched = true;
                    logger_->Debug("Value '" + value + "' found in filter list");
                    break;
                }
            }
        }
        if (result) xmlXPathFreeObject(result);
        xmlXPathFreeContext(entryCtx);
        criteriaResults.push_back(criterionMatched);
    }
    return applyLogic(criteriaResults);
}

bool XMLProcessor::applyLogic(const std::vector<bool> &results) {
    const auto &op = config_.xml_filter.logic_operator;
    if (op == "AND") return std::all_of(results.begin(), results.end(), [](bool v) { return v; });
    if (op == "OR") return std::any_of(results.begin(), results.end(), [](bool v) { return v; });
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
        logger_->Error("Unable to find root element in source document.");
        return;
    }
    logger_->Debug("XML Processor: Initializing output documents");
    procDoc = xmlNewDoc(BAD_CAST "1.0");
    exclDoc = xmlNewDoc(BAD_CAST "1.0");

    if (!procDoc || !exclDoc) {
        logger_->Error("XML Processor: Unable to allocate memory for output documents.");
        return;
    }

    logger_->Debug("XML Processor: copying root element name");
    xmlNodePtr procRoot = xmlNewNode(nullptr, srcRoot->name);
    xmlNodePtr exclRoot = xmlNewNode(nullptr, srcRoot->name);

    logger_->Debug("XML Processor: copying attributes with values to destination root node");
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

    logger_->Debug("XML Processor: copying namespaces to destination root node");
    xmlNsPtr ns = srcRoot->nsDef;
    while (ns) {
        xmlNewNs(procRoot, ns->href, ns->prefix);
        xmlNewNs(exclRoot, ns->href, ns->prefix);
        ns = ns->next;
    }

    logger_->Debug("XML Processor: setting namespaces for destination root node");
    if (srcRoot->ns) {
        xmlNsPtr procNs = xmlSearchNs(nullptr, procRoot, srcRoot->ns->prefix);
        xmlNsPtr exclNs = xmlSearchNs(nullptr, exclRoot, srcRoot->ns->prefix);
        if (procNs) xmlSetNs(procRoot, procNs);
        if (exclNs) xmlSetNs(exclRoot, exclNs);
    }

    logger_->Debug("XML Processor: binding root element to destination documents");
    xmlDocSetRootElement(procDoc, procRoot);
    xmlDocSetRootElement(exclDoc, exclRoot);
    logger_->Debug("XML Processor: root element copied with all attributes and namespaces");
}

void XMLProcessor::saveResults(const std::string &xmlPath, xmlDocPtr procDoc, xmlDocPtr exclDoc, bool hasClean, bool hasMatch) {
    fs::path inputPath(xmlPath);
    std::string filename = inputPath.filename().string();

    fs::create_directories(config_.processed_dir);
    fs::create_directories(config_.excluded_dir);

    if (hasClean) {
        std::string processedPath = (fs::path(config_.processed_dir) / config_.getFilteredFileName(filename)).string();
        xmlSaveFormatFileEnc(processedPath.c_str(), procDoc, "UTF-8", 1);
        logger_->Info("Saved processed data to: " + processedPath);
    }

    if (hasMatch) {
        std::string excludedPath = (fs::path(config_.excluded_dir) / config_.getExcludedFileName(filename)).string();
        xmlSaveFormatFileEnc(excludedPath.c_str(), exclDoc, "UTF-8", 1);
        logger_->Info("Saved excluded data to: " + excludedPath);
    }
}

std::vector<XMLProcessor::NodeAnalysisResult> XMLProcessor::collectAndAnalyzeNodes(xmlXPathContextPtr ctx) {
    std::vector<NodeAnalysisResult> results;
    std::map<xmlNodePtr, std::vector<bool>> nodeMap;

    for (size_t criterionIndex = 0; criterionIndex < config_.xml_filter.criteria.size(); ++criterionIndex) {
        const auto& criterion = config_.xml_filter.criteria[criterionIndex];
        logger_->Debug("Processing criterion " + std::to_string(criterionIndex + 1) + ": " + criterion.xpath);

        xmlXPathObjectPtr xpathResult = xmlXPathEvalExpression(BAD_CAST criterion.xpath.c_str(), ctx);
        if (!xpathResult) {
            logger_->Warning("XPath evaluation failed for: " + criterion.xpath);
            continue;
        }

        if (xpathResult->nodesetval) {
            for (int i = 0; i < xpathResult->nodesetval->nodeNr; ++i) {
                xmlNodePtr node = xpathResult->nodesetval->nodeTab[i];
                std::string value = extractValue(node, criterion);
                bool matches = FilterListManager::instance().contains(criterion.csv_column, value);
                
                logger_->Debug("Node value '" + value + "' " + (matches ? "matches" : "doesn't match") + " filter column '" + criterion.csv_column + "'");

                if (nodeMap.find(node) == nodeMap.end()) {
                    nodeMap[node].resize(config_.xml_filter.criteria.size(), false);
                }
                nodeMap[node][criterionIndex] = matches;
            }
        }
        xmlXPathFreeObject(xpathResult);
    }

    logger_->Debug("Found " + std::to_string(nodeMap.size()) + " nodes with criteria results");

    std::map<xmlNodePtr, std::vector<bool>> objectCriteriaResults;
    std::map<xmlNodePtr, std::vector<xmlNodePtr>> objectChildNodes;

    for (const auto& pair : nodeMap) {
        xmlNodePtr node = pair.first;
        const std::vector<bool>& nodeCriteria = pair.second;

        xmlNodePtr parentObject = node->parent;
        while (parentObject && parentObject->type != XML_ELEMENT_NODE) {
            parentObject = parentObject->parent;
        }

        if (!parentObject) {
            logger_->Warning("Could not find parent object for node: " + std::string(reinterpret_cast<const char*>(node->name)));
            continue;
        }

        if (objectCriteriaResults.find(parentObject) == objectCriteriaResults.end()) {
            objectCriteriaResults[parentObject].resize(config_.xml_filter.criteria.size(), false);
        }

        for (size_t i = 0; i < nodeCriteria.size(); ++i) {
            if (nodeCriteria[i]) {
                objectCriteriaResults[parentObject][i] = true;
            }
        }
        objectChildNodes[parentObject].push_back(node);
    }

    logger_->Debug("Grouped into " + std::to_string(objectCriteriaResults.size()) + " parent objects");

    for (const auto& objPair : objectCriteriaResults) {
        xmlNodePtr parentObject = objPair.first;
        const std::vector<bool>& criteria = objPair.second;

        bool shouldRemove = applyLogic(criteria);

        std::string criteriaStr = "[";
        for (size_t i = 0; i < criteria.size(); ++i) {
            criteriaStr += (criteria[i] ? "T" : "F");
            if (i < criteria.size() - 1) criteriaStr += ", ";
        }
        criteriaStr += "]";

        logger_->Debug("Object at " + buildNodePath(parentObject) + " criteria: " + criteriaStr + " -> should be " + (shouldRemove ? "REMOVED" : "KEPT"));

        if (shouldRemove) {
            NodeAnalysisResult result;
            result.node = parentObject;
            result.criteriaResults = criteria;
            result.shouldRemove = true;
            results.push_back(result);

            logger_->Debug("  -> Object has " + std::to_string(objectChildNodes[parentObject].size()) + " matching child nodes:");
            for (xmlNodePtr childNode : objectChildNodes[parentObject]) {
                logger_->Debug("    - " + std::string(reinterpret_cast<const char*>(childNode->name)));
            }
        }
    }

    logger_->Info("Analysis complete: " + std::to_string(results.size()) + " objects marked for removal");
    return results;
}

std::map<xmlNodePtr, XMLProcessor::ObjectBoundary> XMLProcessor::findOptimalObjectBoundaries(const std::vector<xmlNodePtr>& nodesToRemove) {
    std::map<xmlNodePtr, ObjectBoundary> boundaries;
    if (nodesToRemove.empty()) return boundaries;

    logger_->Debug("Finding optimal boundaries for " + std::to_string(nodesToRemove.size()) + " nodes");

    for (xmlNodePtr objectNode : nodesToRemove) {
        xmlNodePtr container = objectNode->parent;
        while (container && container->type != XML_ELEMENT_NODE) {
            container = container->parent;
        }
        if (!container) {
            container = xmlDocGetRootElement(objectNode->doc);
        }

        ObjectBoundary boundary(objectNode, container, calculateNodeDepth(objectNode), buildNodePath(objectNode));
        boundaries[objectNode] = boundary;
        logger_->Debug("Object boundary: " + boundary.objectPath + " (depth: " + std::to_string(boundary.depth) + ")");
    }
    return boundaries;
}

xmlNodePtr XMLProcessor::findNearestCommonContainer(const std::vector<xmlNodePtr>& nodes) {
    if (nodes.empty()) return nullptr;
    if (nodes.size() == 1) {
        xmlNodePtr parent = nodes[0]->parent;
        while (parent && parent->type != XML_ELEMENT_NODE) parent = parent->parent;
        return parent;
    }

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

    xmlNodePtr commonAncestor = nullptr;
    size_t minPathLength = paths[0].size();
    for (const auto& path : paths) minPathLength = std::min(minPathLength, path.size());

    for (size_t i = 0; i < minPathLength; ++i) {
        xmlNodePtr candidate = paths[0][i];
        bool isCommon = true;
        for (size_t j = 1; j < paths.size(); ++j) {
            if (paths[j][i] != candidate) { isCommon = false; break; }
        }
        if (isCommon) commonAncestor = candidate;
        else break;
    }
    return commonAncestor;
}

void XMLProcessor::buildOutputStructure(xmlDocPtr srcDoc, const std::map<xmlNodePtr, ObjectBoundary>& objectsToRemove, xmlDocPtr targetDoc, bool isExcludedDoc) {
    xmlNodePtr srcRoot = xmlDocGetRootElement(srcDoc);
    xmlNodePtr targetRoot = xmlDocGetRootElement(targetDoc);
    if (!srcRoot || !targetRoot) {
        logger_->Error("Invalid source or target document structure");
        return;
    }

    std::string docType = isExcludedDoc ? "excluded" : "processed";
    logger_->Info("Building " + docType + " document structure");

    bool includeRemoved = isExcludedDoc;
    bool includeUnmatched = !isExcludedDoc;

    xmlNodePtr child = srcRoot->children;
    while (child) {
        if (child->type == XML_ELEMENT_NODE) {
            copyNodeWithFiltering(child, targetRoot, objectsToRemove, includeRemoved, includeUnmatched);
        }
        child = child->next;
    }

    size_t resultCount = xmlChildElementCount(targetRoot);
    logger_->Info("Completed " + docType + " document: " + std::to_string(resultCount) + " top-level elements");
}

void XMLProcessor::copyNodeWithFiltering(xmlNodePtr srcNode, xmlNodePtr targetParent, const std::map<xmlNodePtr, ObjectBoundary>& objectsToRemove, bool includeRemoved, bool includeUnmatched) {
    if (!srcNode || srcNode->type != XML_ELEMENT_NODE) return;

    bool isMarkedForRemoval = isNodeMarkedForRemoval(srcNode, objectsToRemove);

    if (isMarkedForRemoval && includeRemoved) {
        xmlNodePtr copiedNode = xmlDocCopyNode(srcNode, targetParent->doc, 1);
        xmlAddChild(targetParent, copiedNode);
        return;
    }

    if (isMarkedForRemoval && !includeRemoved) return;

    if (!isMarkedForRemoval && includeUnmatched && !includeRemoved) {
        if (xmlChildElementCount(srcNode) == 0) {
            xmlNodePtr copiedNode = xmlDocCopyNode(srcNode, targetParent->doc, 1);
            xmlAddChild(targetParent, copiedNode);
            return;
        }
    }

    xmlNodePtr newNode = xmlNewNode(nullptr, srcNode->name);
    copyNodeAttributes(srcNode, newNode);
    copyNodeNamespace(srcNode, newNode, targetParent->doc);

    bool hasValidChildren = false;
    bool hasTextContent = false;

    xmlNodePtr child = srcNode->children;
    while (child) {
        if (child->type == XML_TEXT_NODE || child->type == XML_CDATA_SECTION_NODE) {
            xmlChar* content = xmlNodeGetContent(child);
            if (content) {
                std::string textContent = reinterpret_cast<const char*>(content);
                if (!textContent.empty() && textContent.find_first_not_of(" \t\n\r") != std::string::npos) {
                    xmlNodePtr textNode = xmlNewText(content);
                    xmlAddChild(newNode, textNode);
                    hasTextContent = true;
                }
                xmlFree(content);
            }
        }
        child = child->next;
    }

    child = srcNode->children;
    while (child) {
        if (child->type == XML_ELEMENT_NODE) {
            size_t childCountBefore = xmlChildElementCount(newNode);
            copyNodeWithFiltering(child, newNode, objectsToRemove, includeRemoved, includeUnmatched);
            size_t childCountAfter = xmlChildElementCount(newNode);
            if (childCountAfter > childCountBefore) hasValidChildren = true;
        }
        child = child->next;
    }

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
                                if (srcVal && dstVal && xmlStrcmp(srcVal, dstVal) == 0) alreadyInResult = true;
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

    if (hasValidChildren || hasTextContent) {
        xmlAddChild(targetParent, newNode);
    } else {
        xmlFreeNode(newNode);
    }
}

void XMLProcessor::copyNodeNamespace(xmlNodePtr srcNode, xmlNodePtr targetNode, xmlDocPtr targetDoc) {
    xmlNsPtr ns = srcNode->nsDef;
    while (ns) {
        xmlNewNs(targetNode, ns->href, ns->prefix);
        ns = ns->next;
    }
    if (srcNode->ns) {
        xmlNsPtr targetNs = xmlSearchNs(targetDoc, targetNode, srcNode->ns->prefix);
        if (targetNs) xmlSetNs(targetNode, targetNs);
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

bool XMLProcessor::isNodeMarkedForRemoval(xmlNodePtr node, const std::map<xmlNodePtr, ObjectBoundary>& objectsToRemove) {
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
        xmlAttrPtr attr = current->properties;
        if (attr) {
            xmlChar* attrValue = xmlGetProp(current, attr->name);
            if (attrValue) {
                element += "[@" + std::string(reinterpret_cast<const char*>(attr->name)) + "='" + std::string(reinterpret_cast<const char*>(attrValue)) + "']";
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

bool XMLProcessor::updateRecordCount(xmlDocPtr doc, const RecordCountConfig& recordCountConfig, int newCount) {
    if (!recordCountConfig.enabled || !doc) {
        logger_->Debug("Record count update skipped: enabled=" + std::string(recordCountConfig.enabled ? "true" : "false") + ", doc=" + std::string(doc ? "not null" : "null"));
        return false;
    }

    logger_->Debug("Attempting to update record count: xpath='" + recordCountConfig.xpath + "', attribute='" + recordCountConfig.attribute + "', newCount=" + std::to_string(newCount));

    xmlNodePtr countElement = findRecordCountElement(doc, recordCountConfig.xpath);
    if (!countElement) {
        logger_->Warning("Record count element not found with xpath: '" + recordCountConfig.xpath + "'");
        return false;
    }

    logger_->Debug("Found record count element: " + std::string(reinterpret_cast<const char*>(countElement->name)));

    bool updated = updateNodeValue(countElement, recordCountConfig.attribute, std::to_string(newCount));
    if (updated) {
        logger_->Info("Record count updated successfully to: " + std::to_string(newCount));
    } else {
        logger_->Error("Failed to update record count value on element: " + std::string(reinterpret_cast<const char*>(countElement->name)));
    }
    return updated;
}

xmlNodePtr XMLProcessor::findRecordCountElement(xmlDocPtr doc, const std::string& xpath) {
    if (!doc || xpath.empty()) return nullptr;

    xmlXPathContextPtr ctx = xmlXPathNewContext(doc);
    if (!ctx) {
        logger_->Error("Failed to create XPath context");
        return nullptr;
    }
    registerNamespaces(ctx, doc);

    logger_->Debug("Searching for record count with xpath: '" + xpath + "'");

    xmlXPathObjectPtr result = xmlXPathEvalExpression(BAD_CAST xpath.c_str(), ctx);
    xmlNodePtr foundNode = nullptr;

    if (result && result->nodesetval && result->nodesetval->nodeNr > 0) {
        foundNode = result->nodesetval->nodeTab[0];
        logger_->Debug("Found record count element with xpath: '" + xpath + "'");
    } else {
        if (result) xmlXPathFreeObject(result);
        std::string altXpath;
        if (xpath.find("Export") != std::string::npos) {
            altXpath = xpath;
            size_t pos = altXpath.find("Export");
            altXpath.replace(pos, 6, "ns4:Export");
            logger_->Debug("First xpath didn't match, trying alternative: '" + altXpath + "'");
            result = xmlXPathEvalExpression(BAD_CAST altXpath.c_str(), ctx);
            if (result && result->nodesetval && result->nodesetval->nodeNr > 0) {
                foundNode = result->nodesetval->nodeTab[0];
                logger_->Debug("Found record count element with alternative xpath: '" + altXpath + "'");
            }
        }
        if (!foundNode) {
            logger_->Warning("No nodes found for record count with xpath: '" + xpath + "' (and alternative paths)");
        }
    }

    if (result) xmlXPathFreeObject(result);
    xmlXPathFreeContext(ctx);
    return foundNode;
}

bool XMLProcessor::updateNodeValue(xmlNodePtr node, const std::string& attributeName, const std::string& newValue) {
    if (!node || attributeName.empty()) {
        logger_->Warning("updateNodeValue: invalid parameters");
        return false;
    }

    logger_->Debug("Updating node '" + std::string(reinterpret_cast<const char*>(node->name)) + "' attribute '" + attributeName + "' to value: " + newValue);

    xmlAttrPtr attr = node->properties;
    bool foundAttr = false;
    while (attr) {
        std::string attrName = reinterpret_cast<const char*>(attr->name);
        if (attrName == attributeName || attrName.find(':' + attributeName) != std::string::npos) {
            xmlChar* content = xmlNodeGetContent(attr->children);
            if (content) xmlFree(content);
            xmlSetProp(node, attr->name, BAD_CAST newValue.c_str());
            logger_->Debug("Updated existing attribute '" + attrName + "' to: " + newValue);
            foundAttr = true;
            break;
        }
        attr = attr->next;
    }

    if (!foundAttr) {
        xmlSetProp(node, BAD_CAST attributeName.c_str(), BAD_CAST newValue.c_str());
        logger_->Debug("Created new attribute '" + attributeName + "' with value: " + newValue);
    }
    return true;
}

int XMLProcessor::readRecordCountFromSource(xmlDocPtr srcDoc) {
    if (!config_.xml_filter.record_count_config.enabled || !srcDoc) return 0;

    xmlNodePtr element = findRecordCountElement(srcDoc, config_.xml_filter.record_count_config.xpath);
    if (!element) {
        logger_->Warning("Could not find record count in source document");
        return 0;
    }

    xmlChar* attrValue = xmlGetProp(element, BAD_CAST config_.xml_filter.record_count_config.attribute.c_str());
    if (!attrValue) {
        logger_->Warning("Record count attribute not found: " + config_.xml_filter.record_count_config.attribute);
        return 0;
    }

    int originalCount = std::stoi(reinterpret_cast<const char*>(attrValue));
    xmlFree(attrValue);
    logger_->Info("Original record count from source: " + std::to_string(originalCount));
    return originalCount;
}

} // namespace stc