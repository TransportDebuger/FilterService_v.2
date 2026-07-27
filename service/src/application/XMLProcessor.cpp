/**
@file XMLProcessor.cpp
@brief Реализация потокового процессора XML-файлов.
@version 3.0.1
@date 2026-07-24
*/
#include "XMLProcessor.hpp"
#include <libxml/xpath.h>
#include <filesystem>
#include <algorithm>
#include <map>
#include <cstdio>
#include <stdexcept>

namespace fs = std::filesystem;
namespace stc {

XMLProcessor::XMLProcessor(const SourceConfig &config,
                           std::shared_ptr<stc::logger::ILogger> logger,
                           std::shared_ptr<FilterListManager> filter_list_manager)
    : config_(config),
      logger_(std::move(logger)),
      filter_list_manager_(std::move(filter_list_manager)) {
    if (!filter_list_manager_) {
        throw std::invalid_argument("XMLProcessor: FilterListManager cannot be null");
    }
}

ProcessingResult XMLProcessor::process(const std::string &xmlPath) {
    ProcessingResult result;
    try {
        result.bytes_processed = fs::file_size(xmlPath);
    } catch (const fs::filesystem_error& e) {
        if (logger_) logger_->Warning("Failed to get file size: " + std::string(e.what()));
        result.bytes_processed = 0;
    }

    try {
        streamingProcess(xmlPath, result);
        result.success = true;
        if (logger_) logger_->Info("XML streaming processing completed: " + xmlPath);
    } catch (const std::exception& e) {
        if (logger_) logger_->Error("XMLProcessor error: " + std::string(e.what()));
        result.success = false;
        if (result.error_type == ProcessingResult::ErrorType::NONE) {
            result.error_type = ProcessingResult::ErrorType::PARSE;
        }
    }
    return result;
}

void XMLProcessor::streamingProcess(const std::string &xmlPath, ProcessingResult& result) {
    fs::path inputPath(xmlPath);
    std::string filename = inputPath.filename().string();
    
    fs::create_directories(config_.processed_dir);
    fs::create_directories(config_.excluded_dir);

    std::string procTmpPath = (fs::temp_directory_path() / ("proc_" + filename + ".tmp")).string();
    std::string exclTmpPath = (fs::temp_directory_path() / ("excl_" + filename + ".tmp")).string();

    FILE* procTmpFile = fopen(procTmpPath.c_str(), "wb");
    FILE* exclTmpFile = fopen(exclTmpPath.c_str(), "wb");
    if (!procTmpFile || !exclTmpFile) {
        if (procTmpFile) fclose(procTmpFile);
        if (exclTmpFile) fclose(exclTmpFile);
        throw std::runtime_error("Failed to create temporary files for streaming");
    }

    auto cleanupTmp = [&]() {
        fclose(procTmpFile);
        fclose(exclTmpFile);
        fs::remove(procTmpPath);
        fs::remove(exclTmpPath);
    };

    xmlTextReaderPtr reader = xmlReaderForFile(xmlPath.c_str(), nullptr, XML_PARSE_NOBLANKS);
    if (!reader) {
        cleanupTmp();
        throw std::runtime_error("Failed to create xmlTextReader for: " + xmlPath);
    }

    xmlDocPtr shellDoc = xmlNewDoc(BAD_CAST "1.0");
    xmlNodePtr rootShell = nullptr;
    int totalRecords = 0;
    int matchedRecords = 0;

    try {
        while (xmlTextReaderRead(reader) == 1) {
            int type = xmlTextReaderNodeType(reader);
            if (type == XML_READER_TYPE_ELEMENT) {
                xmlNodePtr expandedRoot = xmlTextReaderExpand(reader);
                if (expandedRoot) {
                    rootShell = xmlCopyNode(expandedRoot, 0); 
                    xmlFreeNode(expandedRoot);
                }
                xmlDocSetRootElement(shellDoc, rootShell);
                break;
            }
        }

        if (!rootShell) {
            xmlFreeTextReader(reader);
            xmlFreeDoc(shellDoc);
            cleanupTmp();
            throw std::runtime_error("No root element found in XML");
        }

        while (xmlTextReaderRead(reader) == 1) {
            int type = xmlTextReaderNodeType(reader);
            if (type == XML_READER_TYPE_ELEMENT) {
                xmlNodePtr subtree = xmlTextReaderExpand(reader);
                if (!subtree) continue;

                totalRecords++;
                bool isMatch = false;

                if (isRecordNode(subtree)) {
                    xmlDocPtr tmpDoc = xmlNewDoc(BAD_CAST "1.0");
                    xmlNodePtr copiedSubtree = xmlCopyNode(subtree, 1);
                    xmlDocSetRootElement(tmpDoc, copiedSubtree);
                    
                    xmlXPathContextPtr ctx = xmlXPathNewContext(tmpDoc);
                    registerNamespaces(ctx, tmpDoc);
                    isMatch = evaluateEntryAgainstCriteria(copiedSubtree, tmpDoc);
                    
                    xmlXPathFreeContext(ctx);
                    xmlFreeDoc(tmpDoc);

                    if (isMatch) matchedRecords++;
                }

                xmlBufferPtr buf = xmlBufferCreate();
                xmlNodeDump(buf, subtree->doc, subtree, 0, 0);
                FILE* targetFile = isMatch ? exclTmpFile : procTmpFile;
                fwrite(xmlBufferContent(buf), 1, xmlBufferLength(buf), targetFile);
                xmlBufferFree(buf);

                xmlFreeNode(subtree);
            }
        }
    } catch (...) {
        xmlFreeTextReader(reader);
        xmlFreeDoc(shellDoc);
        cleanupTmp();
        throw;
    }

    xmlFreeTextReader(reader);

    int processedCount = totalRecords - matchedRecords;
    if (config_.xml_filter.record_count_config.enabled) {
        updateRecordCount(shellDoc, config_.xml_filter.record_count_config, processedCount);
    }

    // --- ЗАПИСЬ ОБРАБОТАННОГО ФАЙЛА (Через FILE* для исключения OOM) ---
    std::string procPath = (fs::path(config_.processed_dir) / config_.getFilteredFileName(filename)).string();
    FILE* procOut = fopen(procPath.c_str(), "wb");
    if (!procOut) throw std::runtime_error("Failed to open output file: " + procPath);

    fprintf(procOut, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
    fprintf(procOut, "<%s", (const char*)rootShell->name);
    for (xmlNsPtr ns = rootShell->nsDef; ns; ns = ns->next) {
        if (ns->prefix) fprintf(procOut, " xmlns:%s=\"%s\"", (const char*)ns->prefix, (const char*)ns->href);
        else fprintf(procOut, " xmlns=\"%s\"", (const char*)ns->href);
    }
    for (xmlAttrPtr attr = rootShell->properties; attr; attr = attr->next) {
        xmlChar* value = xmlNodeListGetString(shellDoc, attr->children, 1);
        fprintf(procOut, " %s=\"%s\"", (const char*)attr->name, (const char*)value);
        xmlFree(value);
    }
    fprintf(procOut, ">\n");
    fclose(procOut);

    appendFileContent(procPath, procTmpPath);

    FILE* procOutEnd = fopen(procPath.c_str(), "ab");
    if (procOutEnd) {
        fprintf(procOutEnd, "</%s>\n", (const char*)rootShell->name);
        fclose(procOutEnd);
    }
    result.records_processed = processedCount;

    // --- ЗАПИСЬ ИСКЛЮЧЕННОГО ФАЙЛА ---
    if (matchedRecords > 0) {
        std::string exclPath = (fs::path(config_.excluded_dir) / config_.getExcludedFileName(filename)).string();
        FILE* exclOut = fopen(exclPath.c_str(), "wb");
        if (!exclOut) throw std::runtime_error("Failed to open excluded output file: " + exclPath);

        fprintf(exclOut, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
        fprintf(exclOut, "<%s", (const char*)rootShell->name);
        for (xmlNsPtr ns = rootShell->nsDef; ns; ns = ns->next) {
            if (ns->prefix) fprintf(exclOut, " xmlns:%s=\"%s\"", (const char*)ns->prefix, (const char*)ns->href);
            else fprintf(exclOut, " xmlns=\"%s\"", (const char*)ns->href);
        }
        for (xmlAttrPtr attr = rootShell->properties; attr; attr = attr->next) {
            xmlChar* value = xmlNodeListGetString(shellDoc, attr->children, 1);
            fprintf(exclOut, " %s=\"%s\"", (const char*)attr->name, (const char*)value);
            xmlFree(value);
        }
        fprintf(exclOut, ">\n");
        fclose(exclOut);

        appendFileContent(exclPath, exclTmpPath);

        FILE* exclOutEnd = fopen(exclPath.c_str(), "ab");
        if (exclOutEnd) {
            fprintf(exclOutEnd, "</%s>\n", (const char*)rootShell->name);
            fclose(exclOutEnd);
        }
        result.records_matched = matchedRecords;
    }

    xmlFreeDoc(shellDoc);
    cleanupTmp();
}

bool XMLProcessor::isRecordNode(xmlNodePtr node) const {
    if (!node || node->type != XML_ELEMENT_NODE) return false;
    if (xmlStrcmp(node->name, BAD_CAST "entry") == 0 ||
        xmlStrcmp(node->name, BAD_CAST "record") == 0 ||
        xmlStrcmp(node->name, BAD_CAST "item") == 0) {
        return true;
    }
    if (xmlHasProp(node, BAD_CAST "xsi:type") != nullptr) {
        return true;
    }
    return true; 
}

void XMLProcessor::appendFileContent(const std::string& destPath, const std::string& srcPath) const {
    FILE* dest = fopen(destPath.c_str(), "ab");
    FILE* src = fopen(srcPath.c_str(), "rb");
    if (!dest || !src) {
        if (dest) fclose(dest);
        if (src) fclose(src);
        return;
    }
    char buffer[8192];
    size_t bytes;
    while ((bytes = fread(buffer, 1, sizeof(buffer), src)) > 0) {
        fwrite(buffer, 1, bytes, dest);
    }
    fclose(src);
    fclose(dest);
}

// ==============================================================================
// Методы оценки XPath и работы с DOM (Остаются без изменений)
// ==============================================================================

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

void XMLProcessor::registerNamespaces(xmlXPathContextPtr ctx, xmlDocPtr doc) {
    if (!config_.xml_filter.namespaces.empty()) {
        registerConfiguredNamespaces(ctx);
    } else if (config_.xml_filter.auto_register_namespaces) {
        registerNamespacesFromDocument(ctx, doc);
    }
}

void XMLProcessor::registerConfiguredNamespaces(xmlXPathContextPtr ctx) {
    for (const auto &ns : config_.xml_filter.namespaces) {
        xmlXPathRegisterNs(ctx, BAD_CAST ns.prefix.c_str(), BAD_CAST ns.uri.c_str());
    }
}

void XMLProcessor::registerNamespacesFromDocument(xmlXPathContextPtr ctx, xmlDocPtr doc) {
    xmlNodePtr root = xmlDocGetRootElement(doc);
    if (!root) return;
    xmlNsPtr ns = root->nsDef;
    while (ns) {
        if (ns->prefix) xmlXPathRegisterNs(ctx, ns->prefix, ns->href);
        else xmlXPathRegisterNs(ctx, BAD_CAST "default", ns->href);
        ns = ns->next;
    }
}

std::string XMLProcessor::getDocumentNamespaces(xmlDocPtr doc) {
    std::string result;
    xmlNodePtr root = xmlDocGetRootElement(doc);
    if (!root) return result;
    xmlNsPtr ns = root->nsDef;
    while (ns) {
        if (ns->prefix) result += std::string((char *)ns->prefix) + ":" + std::string((char *)ns->href) + "\n";
        else result += "default:" + std::string((char *)ns->href) + "\n";
        ns = ns->next;
    }
    return result;
}

std::string XMLProcessor::makeRelativeXPath(const std::string &xpath) {
    if (!xpath.starts_with("//") && !xpath.starts_with("/")) return xpath;
    std::string relative = xpath;
    if (relative.starts_with("//")) relative = relative.substr(2);
    else if (relative.starts_with("/")) relative = relative.substr(1);
    
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
                if (filter_list_manager_->contains(criterion.csv_column, value)) {
                    criterionMatched = true;
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

bool XMLProcessor::updateRecordCount(xmlDocPtr doc, const RecordCountConfig& recordCountConfig, int newCount) {
    if (!recordCountConfig.enabled || !doc) return false;
    xmlNodePtr countElement = findRecordCountElement(doc, recordCountConfig.xpath);
    if (!countElement) return false;
    return updateNodeValue(countElement, recordCountConfig.attribute, std::to_string(newCount));
}

xmlNodePtr XMLProcessor::findRecordCountElement(xmlDocPtr doc, const std::string& xpath) {
    if (!doc || xpath.empty()) return nullptr;
    xmlXPathContextPtr ctx = xmlXPathNewContext(doc);
    if (!ctx) return nullptr;
    registerNamespaces(ctx, doc);
    
    xmlXPathObjectPtr result = xmlXPathEvalExpression(BAD_CAST xpath.c_str(), ctx);
    xmlNodePtr foundNode = nullptr;
    if (result && result->nodesetval && result->nodesetval->nodeNr > 0) {
        foundNode = result->nodesetval->nodeTab[0];
    } else {
        if (result) xmlXPathFreeObject(result);
        std::string altXpath = xpath;
        size_t pos = altXpath.find("Export");
        if (pos != std::string::npos) {
            altXpath.replace(pos, 6, "ns4:Export");
            result = xmlXPathEvalExpression(BAD_CAST altXpath.c_str(), ctx);
            if (result && result->nodesetval && result->nodesetval->nodeNr > 0) {
                foundNode = result->nodesetval->nodeTab[0];
            }
        }
    }
    if (result) xmlXPathFreeObject(result);
    xmlXPathFreeContext(ctx);
    return foundNode;
}

bool XMLProcessor::updateNodeValue(xmlNodePtr node, const std::string& attributeName, const std::string& newValue) {
    if (!node || attributeName.empty()) return false;
    xmlAttrPtr attr = node->properties;
    bool foundAttr = false;
    while (attr) {
        std::string attrName = reinterpret_cast<const char*>(attr->name);
        if (attrName == attributeName || attrName.find(':' + attributeName) != std::string::npos) {
            xmlSetProp(node, attr->name, BAD_CAST newValue.c_str());
            foundAttr = true;
            break;
        }
        attr = attr->next;
    }
    if (!foundAttr) {
        xmlSetProp(node, BAD_CAST attributeName.c_str(), BAD_CAST newValue.c_str());
    }
    return true;
}

int XMLProcessor::readRecordCountFromSource(xmlDocPtr srcDoc) {
    if (!config_.xml_filter.record_count_config.enabled || !srcDoc) return 0;
    xmlNodePtr element = findRecordCountElement(srcDoc, config_.xml_filter.record_count_config.xpath);
    if (!element) return 0;
    xmlChar* attrValue = xmlGetProp(element, BAD_CAST config_.xml_filter.record_count_config.attribute.c_str());
    if (!attrValue) return 0;
    int count = std::stoi(reinterpret_cast<const char*>(attrValue));
    xmlFree(attrValue);
    return count;
}

} // namespace stc