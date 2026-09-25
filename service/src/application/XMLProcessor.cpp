/**
@file XMLProcessor.cpp
@brief Реализация потокового процессора для обработки, фильтрации и сохранения
результатов XML-файлов.
@version 4.0.0
@date 2026-09-06
*/
#include "XMLProcessor.hpp"
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <stdexcept>

namespace fs = std::filesystem;

namespace stc {

XMLProcessor::XMLProcessor(const SourceConfig& config,
                           std::shared_ptr<stc::logger::ILogger> logger,
                           std::shared_ptr<FilterListManager> filter_list_manager)
    : config_(config),
      logger_(std::move(logger)),
      filter_list_manager_(std::move(filter_list_manager)) {
    if (!filter_list_manager_) {
        throw std::invalid_argument("FilterListManager cannot be null");
    }
}

ProcessingResult XMLProcessor::process(const std::string& xmlPath) {
    ProcessingResult result;

    try {
        result.bytes_processed = fs::file_size(xmlPath);
    } catch (const fs::filesystem_error& e) {
        if (logger_) {
            logger_->Warning("Failed to get file size: " + std::string(e.what()));
        }
        result.bytes_processed = 0;
    }

    try {
        streamingProcess(xmlPath, result);
        result.success = true;

        if (logger_) {
            logger_->Info("XML streaming processing completed: " + xmlPath);
        }
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->Error("XMLProcessor error: " + std::string(e.what()));
        }
        result.success = false;

        if (result.error_type == ProcessingResult::ErrorType::NONE) {
            result.error_type = ProcessingResult::ErrorType::PARSE;
        }
    }

    return result;
}

bool XMLProcessor::isBypassMode() const {
    for (const auto& criterion : config_.xml_filter.criteria) {
        if (criterion.required) {
            return false;
        }
    }
    return true;
}

void XMLProcessor::copyFileBypass(const std::string& srcPath,
                                  const std::string& dstPath) {
    std::error_code ec;
    fs::create_directories(fs::path(dstPath).parent_path(), ec);
    if (ec) {
        throw std::runtime_error("Failed to create directory for bypass file: " +
                                 ec.message());
    }

    fs::copy_file(srcPath, dstPath, fs::copy_options::overwrite_existing, ec);
    if (ec) {
        throw std::runtime_error("Failed to copy file in bypass mode: " +
                                 ec.message());
    }
}

bool XMLProcessor::extractRootInfo(xmlTextReaderPtr reader,
                                   RootInfo& root_info) {
    int ret = xmlTextReaderRead(reader);
    while (ret == 1) {
        int type = xmlTextReaderNodeType(reader);
        if (type == XML_READER_TYPE_ELEMENT) {
            const xmlChar* name = xmlTextReaderConstName(reader);
            if (name) {
                root_info.name = reinterpret_cast<const char*>(name);
            }

            const xmlChar* nsUri = xmlTextReaderConstNamespaceUri(reader);
            if (nsUri) {
                root_info.namespace_uri = reinterpret_cast<const char*>(nsUri);
            }

            const xmlChar* nsPrefix = xmlTextReaderConstPrefix(reader);
            if (nsPrefix) {
                root_info.namespace_prefix = reinterpret_cast<const char*>(nsPrefix);
            }

            if (xmlTextReaderMoveToFirstAttribute(reader) == 1) {
                do {
                    const xmlChar* attrName = xmlTextReaderConstName(reader);
                    const xmlChar* attrValue = xmlTextReaderConstValue(reader);

                    if (attrName && attrValue) {
                        std::string aname = reinterpret_cast<const char*>(attrName);
                        std::string avalue = reinterpret_cast<const char*>(attrValue);

                        if (aname == "xmlns") {
                            root_info.namespace_declarations.push_back({"", avalue});
                        } else if (aname.starts_with("xmlns:")) {
                            std::string prefix = aname.substr(6);
                            root_info.namespace_declarations.push_back({prefix, avalue});
                        } else {
                            root_info.attributes.push_back({aname, avalue});
                        }
                    }
                } while (xmlTextReaderMoveToNextAttribute(reader) == 1);

                xmlTextReaderMoveToElement(reader);
            }

            return true;
        }
        ret = xmlTextReaderRead(reader);
    }

    return false;
}

bool XMLProcessor::isObject(xmlTextReaderPtr reader) const {
    int type = xmlTextReaderNodeType(reader);
    if (type != XML_READER_TYPE_ELEMENT) {
        return false;
    }

    const xmlChar* localName = xmlTextReaderConstLocalName(reader);
    if (!localName) {
        return false;
    }

    std::string local = reinterpret_cast<const char*>(localName);

    if (local != config_.xml_filter.object_name) {
        return false;
    }

    if (config_.xml_filter.object_namespace_uri.empty()) {
        return true;
    }

    const xmlChar* nsUri = xmlTextReaderConstNamespaceUri(reader);
    if (!nsUri) {
        return false;
    }

    std::string uri = reinterpret_cast<const char*>(nsUri);
    return uri == config_.xml_filter.object_namespace_uri;
}

void XMLProcessor::extractObjectProperties(xmlTextReaderPtr reader,
                                           ObjectInfo& obj_info) {
    // Извлекаем атрибуты объекта
    if (xmlTextReaderMoveToFirstAttribute(reader) == 1) {
        do {
            const xmlChar* attrName = xmlTextReaderConstName(reader);
            const xmlChar* attrValue = xmlTextReaderConstValue(reader);
            if (attrName && attrValue) {
                std::string aname = reinterpret_cast<const char*>(attrName);
                std::string avalue = reinterpret_cast<const char*>(attrValue);
                obj_info.properties.push_back({"@" + aname, avalue});
            }
        } while (xmlTextReaderMoveToNextAttribute(reader) == 1);
        xmlTextReaderMoveToElement(reader);
    }

    // Читаем поддерево объекта для извлечения свойств-элементов
    int depth = 0;
    std::string currentPropertyName;
    bool insideProperty = false;
    bool hasCdata = false;

    int ret = xmlTextReaderRead(reader);
    while (ret == 1) {
        int type = xmlTextReaderNodeType(reader);

        if (type == XML_READER_TYPE_ELEMENT) {
            depth++;
            const xmlChar* localName = xmlTextReaderConstLocalName(reader);
            if (localName) {
                currentPropertyName = reinterpret_cast<const char*>(localName);
                insideProperty = true;
                hasCdata = false;

                // Извлекаем атрибуты свойства
                if (xmlTextReaderMoveToFirstAttribute(reader) == 1) {
                    do {
                        const xmlChar* attrName = xmlTextReaderConstName(reader);
                        const xmlChar* attrValue = xmlTextReaderConstValue(reader);
                        if (attrName && attrValue) {
                            std::string aname = reinterpret_cast<const char*>(attrName);
                            std::string avalue = reinterpret_cast<const char*>(attrValue);
                            obj_info.properties.push_back(
                                {currentPropertyName + "/@" + aname, avalue});
                        }
                    } while (xmlTextReaderMoveToNextAttribute(reader) == 1);
                    xmlTextReaderMoveToElement(reader);
                }
            }
        } else if (type == XML_READER_TYPE_END_ELEMENT) {
            depth--;
            if (depth < 0) {
                break;  // Достигли закрывающего тега объекта
            }
            insideProperty = false;
        } else if (type == XML_READER_TYPE_TEXT ||
                   type == XML_READER_TYPE_CDATA) {
            if (type == XML_READER_TYPE_CDATA) {
                hasCdata = true;
                obj_info.has_cdata = true;
            }
            if (insideProperty && !hasCdata) {
                const xmlChar* text = xmlTextReaderConstValue(reader);
                if (text) {
                    std::string value = reinterpret_cast<const char*>(text);
                    obj_info.properties.push_back({currentPropertyName, value});
                }
            }
        }

        ret = xmlTextReaderRead(reader);
    }
}

std::string XMLProcessor::extractPropertyValue(
    const ObjectInfo& obj_info,
    const SourceConfig::XmlFilterCriterion& criterion) {
    std::string path = criterion.path;
    std::string attributeName = criterion.attribute;

    // Если путь содержит атрибут (например, "docNumber/@value")
    size_t atPos = path.find("/@");
    if (atPos != std::string::npos) {
        std::string propertyName = path.substr(0, atPos);
        std::string attrName = path.substr(atPos + 2);

        std::string searchKey = propertyName + "/@" + attrName;
        for (const auto& prop : obj_info.properties) {
            if (prop.name == searchKey) {
                return normalizeValue(prop.value);
            }
        }
        return "";
    }

    // Если задан отдельный атрибут в критерии
    if (!attributeName.empty()) {
        std::string searchKey = path + "/@" + attributeName;
        for (const auto& prop : obj_info.properties) {
            if (prop.name == searchKey) {
                return normalizeValue(prop.value);
            }
        }
        return "";
    }

    // Если путь начинается с "@", это атрибут самого объекта
    if (path.starts_with("@")) {
        std::string searchKey = "@" + path.substr(1);
        for (const auto& prop : obj_info.properties) {
            if (prop.name == searchKey) {
                return normalizeValue(prop.value);
            }
        }
        return "";
    }

    // Обычное текстовое свойство
    for (const auto& prop : obj_info.properties) {
        if (prop.name == path) {
            if (obj_info.has_cdata) {
                return "";
            }
            return normalizeValue(prop.value);
        }
    }

    return "";
}

bool XMLProcessor::evaluateObject(const ObjectInfo& obj_info) {
    std::vector<bool> activeResults;
    bool hasActiveCriterion = false;

    for (const auto& criterion : config_.xml_filter.criteria) {
        if (!criterion.required) {
            continue;
        }

        hasActiveCriterion = true;

        std::string value = extractPropertyValue(obj_info, criterion);

        bool matched = false;
        try {
            matched = filter_list_manager_->contains(criterion.csv_column, value);
        } catch (const std::invalid_argument&) {
            matched = false;
        }

        activeResults.push_back(matched);
    }

    if (!hasActiveCriterion) {
        return false;
    }

    return applyLogic(activeResults);
}

bool XMLProcessor::applyLogic(const std::vector<bool>& results) {
    if (results.empty()) {
        return false;
    }

    const std::string& op = config_.xml_filter.logic_operator;

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
        double score = 0.0;
        double total = 0.0;
        size_t activeIndex = 0;

        for (const auto& criterion : config_.xml_filter.criteria) {
            if (!criterion.required) continue;

            if (activeIndex < results.size()) {
                total += criterion.weight;
                if (results[activeIndex]) {
                    score += criterion.weight;
                }
                activeIndex++;
            }
        }

        if (total <= 0.0) return false;
        return (score / total) >= config_.xml_filter.threshold;
    }

    return false;
}

std::string XMLProcessor::normalizeValue(const std::string& value) {
    return FilterListManager::normalizeValue(value);
}

void XMLProcessor::writeRootElement(FILE* file,
                                    const RootInfo& root_info,
                                    int record_count) {
    if (!file) return;

    if (!root_info.xml_declaration.empty()) {
        fprintf(file, "%s\n", root_info.xml_declaration.c_str());
    } else {
        fprintf(file, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
    }

    fprintf(file, "<%s", root_info.name.c_str());

    for (const auto& ns : root_info.namespace_declarations) {
        if (ns.first.empty()) {
            fprintf(file, " xmlns=\"%s\"", ns.second.c_str());
        } else {
            fprintf(file, " xmlns:%s=\"%s\"", ns.first.c_str(), ns.second.c_str());
        }
    }

    for (const auto& attr : root_info.attributes) {
        fprintf(file, " %s=\"%s\"", attr.first.c_str(), attr.second.c_str());
    }

    if (record_count >= 0 && config_.xml_filter.record_count_config.enabled) {
        const std::string& counterPath = config_.xml_filter.record_count_config.path;
        if (counterPath.starts_with("@")) {
            std::string attrName = counterPath.substr(1);
            fprintf(file, " %s=\"%d\"", attrName.c_str(), record_count);
        }
    }

    fprintf(file, ">\n");
}

void XMLProcessor::writeRootEndTag(FILE* file, const RootInfo& root_info) {
    if (!file) return;
    fprintf(file, "</%s>\n", root_info.name.c_str());
}

void XMLProcessor::writeObject(FILE* file, xmlTextReaderPtr reader) {
    if (!file || !reader) return;

    xmlChar* outerXml = xmlTextReaderReadOuterXml(reader);
    if (outerXml) {
        fwrite(outerXml, 1, xmlStrlen(outerXml), file);
        fwrite("\n", 1, 1, file);
        xmlFree(outerXml);
    }
}

void XMLProcessor::updateRecordCount(RootInfo& root_info, int record_count) {
    if (!config_.xml_filter.record_count_config.enabled) {
        return;
    }

    const std::string& counterPath = config_.xml_filter.record_count_config.path;

    if (counterPath.starts_with("@")) {
        std::string attrName = counterPath.substr(1);

        bool found = false;
        for (auto& attr : root_info.attributes) {
            if (attr.first == attrName) {
                attr.second = std::to_string(record_count);
                found = true;
                break;
            }
        }

        if (!found) {
            root_info.attributes.push_back({attrName, std::to_string(record_count)});
        }
    }
}

std::string XMLProcessor::getTempFilePath(const std::string& filename,
                                          const std::string& prefix) {
    std::string tempDir = config_.temp_dir;
    if (tempDir.empty()) {
        tempDir = "/var/xmlfilter/temp";
    }

    std::error_code ec;
    fs::create_directories(tempDir, ec);

    std::string tempPath = (fs::path(tempDir) / (prefix + "_" + filename)).string();
    return tempPath;
}

void XMLProcessor::cleanupTempFiles(const std::vector<std::string>& paths) {
    for (const auto& path : paths) {
        std::error_code ec;
        fs::remove(path, ec);
    }
}

void XMLProcessor::streamingProcess(const std::string& xmlPath,
                                    ProcessingResult& result) {
    // ===== ПЕРВЫЙ ПРОХОД: подсчёт объектов и определение результатов фильтрации =====
    xmlTextReaderPtr readerPass1 = xmlReaderForFile(xmlPath.c_str(), nullptr,
                                                    XML_PARSE_NOBLANKS |
                                                    XML_PARSE_NONET |
                                                    XML_PARSE_HUGE);
    if (!readerPass1) {
        result.error_type = ProcessingResult::ErrorType::PARSE;
        throw std::runtime_error("Failed to open XML file: " + xmlPath);
    }

    if (isBypassMode()) {
        result.bypass = true;
        result.success = true;
        xmlFreeTextReader(readerPass1);
        return;
    }

    RootInfo root_info;
    int totalRecords = 0;
    int matchedRecords = 0;
    std::vector<bool> matchedFlags;  // Флаги совпадения для каждого объекта

    try {
        if (!extractRootInfo(readerPass1, root_info)) {
            result.error_type = ProcessingResult::ErrorType::PARSE;
            throw std::runtime_error("Failed to extract root info from XML file");
        }

        int ret = xmlTextReaderRead(readerPass1);
        while (ret == 1) {
            if (isObject(readerPass1)) {
                totalRecords++;

                xmlNodePtr subtree = xmlTextReaderExpand(readerPass1);
                if (subtree) {
                    xmlDocPtr tmpDoc = xmlNewDoc(BAD_CAST "1.0");
                    xmlNodePtr copiedSubtree = xmlCopyNode(subtree, 1);
                    xmlDocSetRootElement(tmpDoc, copiedSubtree);

                    ObjectInfo obj_info;
                    extractObjectPropertiesFromNode(copiedSubtree, obj_info);

                    bool matched = evaluateObject(obj_info);
                    matchedFlags.push_back(matched);
                    if (matched) {
                        matchedRecords++;
                    }

                    xmlFreeDoc(tmpDoc);
                }

                ret = xmlTextReaderNext(readerPass1);
                continue;
            }

            ret = xmlTextReaderRead(readerPass1);
        }

        xmlFreeTextReader(readerPass1);

    } catch (const std::exception&) {
        xmlFreeTextReader(readerPass1);
        throw;
    }

    // ===== ВТОРОЙ ПРОХОД: запись выходных файлов с обновлёнными счётчиками =====
    int processedCount = totalRecords - matchedRecords;
    int excludedCount = matchedRecords;

    RootInfo processedRootInfo = root_info;
    RootInfo excludedRootInfo = root_info;

    updateRecordCount(processedRootInfo, processedCount);
    updateRecordCount(excludedRootInfo, excludedCount);

    std::string filename = fs::path(xmlPath).filename().string();
    std::string processedTmpPath = getTempFilePath(filename, "proc");
    std::string excludedTmpPath = getTempFilePath(filename, "excl");

    FILE* processedFile = fopen(processedTmpPath.c_str(), "wb");
    FILE* excludedFile = nullptr;

    if (!config_.excluded_dir.empty()) {
        excludedFile = fopen(excludedTmpPath.c_str(), "wb");
    }

    if (!processedFile || (!config_.excluded_dir.empty() && !excludedFile)) {
        if (processedFile) fclose(processedFile);
        if (excludedFile) fclose(excludedFile);
        result.error_type = ProcessingResult::ErrorType::WRITE;
        throw std::runtime_error("Failed to open temporary files");
    }

    // Записываем корневые элементы с обновлёнными счётчиками
    writeRootElement(processedFile, processedRootInfo, -1);
    if (excludedFile) {
        writeRootElement(excludedFile, excludedRootInfo, -1);
    }

    xmlTextReaderPtr readerPass2 = xmlReaderForFile(xmlPath.c_str(), nullptr,
                                                    XML_PARSE_NOBLANKS |
                                                    XML_PARSE_NONET |
                                                    XML_PARSE_HUGE);
    if (!readerPass2) {
        fclose(processedFile);
        if (excludedFile) fclose(excludedFile);
        result.error_type = ProcessingResult::ErrorType::PARSE;
        throw std::runtime_error("Failed to open XML file for second pass: " + xmlPath);
    }

    try {
        // Пропускаем корневой элемент
        extractRootInfo(readerPass2, root_info);

        int objectIndex = 0;
        int ret = xmlTextReaderRead(readerPass2);
        while (ret == 1) {
            if (isObject(readerPass2)) {
                bool matched = (objectIndex < static_cast<int>(matchedFlags.size()))
                                   ? matchedFlags[objectIndex]
                                   : false;
                objectIndex++;

                xmlNodePtr subtree = xmlTextReaderExpand(readerPass2);
                if (subtree) {
                    xmlDocPtr tmpDoc = xmlNewDoc(BAD_CAST "1.0");
                    xmlNodePtr copiedSubtree = xmlCopyNode(subtree, 1);
                    xmlDocSetRootElement(tmpDoc, copiedSubtree);

                    xmlBufferPtr buf = xmlBufferCreate();
                    xmlNodeDump(buf, tmpDoc, copiedSubtree, 0, 0);

                    if (matched) {
                        if (excludedFile) {
                            fwrite(xmlBufferContent(buf), 1, xmlBufferLength(buf), excludedFile);
                            fwrite("\n", 1, 1, excludedFile);
                        }
                    } else {
                        if (processedFile) {
                            fwrite(xmlBufferContent(buf), 1, xmlBufferLength(buf), processedFile);
                            fwrite("\n", 1, 1, processedFile);
                        }
                    }

                    xmlBufferFree(buf);
                    xmlFreeDoc(tmpDoc);
                }

                ret = xmlTextReaderNext(readerPass2);
                continue;
            }

            ret = xmlTextReaderRead(readerPass2);
        }

        xmlFreeTextReader(readerPass2);

    } catch (const std::exception&) {
        fclose(processedFile);
        if (excludedFile) fclose(excludedFile);
        xmlFreeTextReader(readerPass2);
        throw;
    }

    // Записываем закрывающие теги корневых элементов
    writeRootEndTag(processedFile, processedRootInfo);
    if (excludedFile) {
        writeRootEndTag(excludedFile, excludedRootInfo);
    }

    fclose(processedFile);
    if (excludedFile) fclose(excludedFile);

    // Перемещаем временные файлы в целевые директории
    std::string processedDstPath = (fs::path(config_.processed_dir) /
                                    config_.getFilteredFileName(filename)).string();
    std::error_code ec;
    fs::create_directories(config_.processed_dir, ec);
    fs::rename(processedTmpPath, processedDstPath, ec);
    if (ec) {
        result.error_type = ProcessingResult::ErrorType::WRITE;
        throw std::runtime_error("Failed to move processed file: " + ec.message());
    }

    if (excludedFile && matchedRecords > 0 && !config_.excluded_dir.empty()) {
        std::string excludedDstPath = (fs::path(config_.excluded_dir) /
                                       config_.getExcludedFileName(filename)).string();
        fs::create_directories(config_.excluded_dir, ec);
        fs::rename(excludedTmpPath, excludedDstPath, ec);
        if (ec) {
            result.error_type = ProcessingResult::ErrorType::WRITE;
            throw std::runtime_error("Failed to move excluded file: " + ec.message());
        }
    }

    result.success = true;
    result.records_processed = static_cast<size_t>(totalRecords);
    result.records_matched = static_cast<size_t>(matchedRecords);
    result.records_kept = static_cast<size_t>(processedCount);
}

void XMLProcessor::extractObjectPropertiesFromNode(xmlNodePtr node,
                                                   ObjectInfo& obj_info) {
    // Извлекаем атрибуты объекта
    for (xmlAttrPtr attr = node->properties; attr; attr = attr->next) {
        xmlChar* attrValue = xmlNodeListGetString(node->doc, attr->children, 1);
        if (attrValue) {
            std::string aname = reinterpret_cast<const char*>(attr->name);
            std::string avalue = reinterpret_cast<const char*>(attrValue);
            obj_info.properties.push_back({"@" + aname, avalue});
            xmlFree(attrValue);
        }
    }

    // Обходим дочерние элементы объекта для извлечения свойств-элементов
    for (xmlNodePtr child = node->children; child; child = child->next) {
        if (child->type == XML_ELEMENT_NODE) {
            std::string childName = reinterpret_cast<const char*>(child->name);

            // Извлекаем атрибуты свойства
            for (xmlAttrPtr attr = child->properties; attr; attr = attr->next) {
                xmlChar* attrValue = xmlNodeListGetString(child->doc, attr->children, 1);
                if (attrValue) {
                    std::string aname = reinterpret_cast<const char*>(attr->name);
                    std::string avalue = reinterpret_cast<const char*>(attrValue);
                    obj_info.properties.push_back(
                        {childName + "/@" + aname, avalue});
                    xmlFree(attrValue);
                }
            }

            // Извлекаем текстовое содержимое свойства
            xmlChar* text = xmlNodeGetContent(child);
            if (text) {
                std::string value = reinterpret_cast<const char*>(text);
                obj_info.properties.push_back({childName, value});
                xmlFree(text);
            }
        } else if (child->type == XML_CDATA_SECTION_NODE) {
            obj_info.has_cdata = true;
        }
    }
}

}  // namespace stc