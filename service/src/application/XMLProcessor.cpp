/**
@file XMLProcessor.cpp
@brief Реализация потокового процессора для обработки, фильтрации и сохранения
результатов XML-файлов с поддержкой иерархических групп.
@version 5.0.0
@date 2026-09-27
*/
#include "XMLProcessor.hpp"
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <filesystem>
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

std::string XMLProcessor::normalizeValue(const std::string& value) {
    return FilterListManager::normalizeValue(value);
}

std::string XMLProcessor::getTempFilePath(const std::string& filename,
                                          const std::string& prefix) const {
    std::string tempDir = config_.temp_dir;
    if (tempDir.empty()) {
        tempDir = "/var/xmlfilter/temp";
    }

    std::error_code ec;
    fs::create_directories(tempDir, ec);

    std::string tempPath = (fs::path(tempDir) / (prefix + "_" + filename)).string();
    return tempPath;
}

void XMLProcessor::cleanupTempFiles(const std::vector<std::string>& paths) const {
    for (const auto& path : paths) {
        std::error_code ec;
        fs::remove(path, ec);
    }
}

void XMLProcessor::extractObjectPropertiesFromNode(xmlNodePtr node,
                                                   ObjectInfo& obj_info) const {
    if (!node) {
        return;
    }

    // Извлекаем атрибуты объекта (с префиксом "@")
    for (xmlAttrPtr attr = node->properties; attr; attr = attr->next) {
        if (attr->children && attr->children->content) {
            std::string aname = reinterpret_cast<const char*>(attr->name);
            std::string avalue =
                reinterpret_cast<const char*>(attr->children->content);
            obj_info.properties.push_back({"@" + aname, avalue});
        }
    }

    // Обходим дочерние элементы объекта
    for (xmlNodePtr child = node->children; child; child = child->next) {
        if (child->type == XML_ELEMENT_NODE) {
            std::string childName = reinterpret_cast<const char*>(child->name);

            // Извлекаем атрибуты дочернего элемента
            for (xmlAttrPtr attr = child->properties; attr; attr = attr->next) {
                if (attr->children && attr->children->content) {
                    std::string aname =
                        reinterpret_cast<const char*>(attr->name);
                    std::string avalue =
                        reinterpret_cast<const char*>(attr->children->content);
                    obj_info.properties.push_back(
                        {childName + "/@" + aname, avalue});
                }
            }

            // Проверяем наличие CDATA внутри дочернего элемента
            bool hasCdata = false;
            for (xmlNodePtr subchild = child->children; subchild;
                 subchild = subchild->next) {
                if (subchild->type == XML_CDATA_SECTION_NODE) {
                    hasCdata = true;
                    obj_info.has_cdata = true;
                    break;
                }
            }

            // Извлекаем текстовое содержимое дочернего элемента
            if (!hasCdata) {
                for (xmlNodePtr subchild = child->children; subchild;
                     subchild = subchild->next) {
                    if (subchild->type == XML_TEXT_NODE && subchild->content) {
                        std::string value =
                            reinterpret_cast<const char*>(subchild->content);
                        obj_info.properties.push_back({childName, value});
                    }
                }
            }
        }
    }
}

void XMLProcessor::extractGroupProperties(xmlTextReaderPtr reader,
                                          GroupBuffer& group) const {
    // Извлекаем атрибуты группы
    if (xmlTextReaderMoveToFirstAttribute(reader) == 1) {
        do {
            const xmlChar* attrName = xmlTextReaderConstName(reader);
            const xmlChar* attrValue = xmlTextReaderConstValue(reader);
            if (attrName && attrValue) {
                std::string aname = reinterpret_cast<const char*>(attrName);
                std::string avalue = reinterpret_cast<const char*>(attrValue);
                group.properties.push_back({"@" + aname, avalue});
            }
        } while (xmlTextReaderMoveToNextAttribute(reader) == 1);
        xmlTextReaderMoveToElement(reader);
    }
}

bool XMLProcessor::evaluateObject(const ObjectInfo& obj_info,
                                  const GroupStack& groupStack) const {
    std::vector<bool> activeResults;
    bool hasActiveCriterion = false;

    for (const auto& criterion : config_.xml_filter.criteria) {
        if (!criterion.required) {
            continue;
        }

        hasActiveCriterion = true;

        std::string value = extractPropertyValue(obj_info, groupStack, criterion);

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

std::string XMLProcessor::extractPropertyValue(
    const ObjectInfo& obj_info,
    const GroupStack& groupStack,
    const SourceConfig::XmlFilterCriterion& criterion) const {
    std::string path = criterion.path;

    // Проверяем, является ли путь свойством группы
    if (path.starts_with("ancestor::")) {
        path = path.substr(10);  // Удаляем префикс "ancestor::"
    }

    size_t slashPos = path.find('/');
    if (slashPos != std::string::npos) {
        std::string firstPart = path.substr(0, slashPos);
        std::string propertyPath = path.substr(slashPos + 1);

        // Проверяем, является ли первая часть именем группы в стеке
        for (auto it = groupStack.rbegin(); it != groupStack.rend(); ++it) {
            if (it->name == firstPart) {
                // Это свойство группы: ищем в свойствах группы
                for (const auto& prop : it->properties) {
                    if (prop.name == propertyPath) {
                        return normalizeValue(prop.value);
                    }
                }
                return "";
            }
        }

        // ИСПРАВЛЕНИЕ: если первая часть не является именем группы,
        // это свойство объекта (например, "docNumber/@value")
        for (const auto& prop : obj_info.properties) {
            if (prop.name == path) {
                return normalizeValue(prop.value);
            }
        }
        return "";
    }

    // Свойство объекта без слэша (атрибут объекта или текстовое свойство)
    if (path.starts_with("@")) {
        for (const auto& prop : obj_info.properties) {
            if (prop.name == path) {
                return normalizeValue(prop.value);
            }
        }
        return "";
    }

    // Обычное текстовое свойство объекта
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

bool XMLProcessor::applyLogic(const std::vector<bool>& results) const {
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

std::string XMLProcessor::buildOpeningTag(xmlTextReaderPtr reader) const {
    std::string tag;
    tag.reserve(256);

    const xmlChar* full_name = xmlTextReaderConstName(reader);
    if (!full_name) return "";

    tag += '<';
    tag += reinterpret_cast<const char*>(full_name);

    if (xmlTextReaderMoveToFirstAttribute(reader) == 1) {
        do {
            const xmlChar* attr_name = xmlTextReaderConstName(reader);
            const xmlChar* attr_value = xmlTextReaderConstValue(reader);
            if (attr_name && attr_value) {
                tag += ' ';
                tag += reinterpret_cast<const char*>(attr_name);
                tag += "=\"";
                tag += reinterpret_cast<const char*>(attr_value);
                tag += '"';
            }
        } while (xmlTextReaderMoveToNextAttribute(reader) == 1);

        xmlTextReaderMoveToElement(reader);
    }

    tag += '>';
    return tag;
}

void XMLProcessor::writeRootElementWithPlaceholder(FILE* file,
                                                   const RootInfo& root_info,
                                                   size_t& placeholderPosition) const {
    if (!file) return;

    placeholderPosition = 0;

    if (!root_info.xml_declaration.empty()) {
        fprintf(file, "%s\n", root_info.xml_declaration.c_str());
    } else {
        fprintf(file, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
    }

    std::string rootTag = "<" + root_info.name;

    for (const auto& ns : root_info.namespace_declarations) {
        if (ns.first.empty()) {
            rootTag += " xmlns=\"" + ns.second + "\"";
        } else {
            rootTag += " xmlns:" + ns.first + "=\"" + ns.second + "\"";
        }
    }

    // Определяем имя атрибута счётчика
    std::string counterAttr;
    if (config_.xml_filter.record_count_config.enabled) {
        const std::string& counterPath =
            config_.xml_filter.record_count_config.path;
        if (counterPath.starts_with("@")) {
            counterAttr = counterPath.substr(1);
        }
    }

    bool countWritten = false;
    for (const auto& attr : root_info.attributes) {
        if (!counterAttr.empty() && attr.first == counterAttr) {
            // Записываем заполнитель вместо счётчика
            placeholderPosition = static_cast<size_t>(ftell(file)) +
                                  rootTag.size() + 1 + counterAttr.size() + 2;
            rootTag += " " + counterAttr + "=\"0000000000\"";
            countWritten = true;
        } else {
            rootTag += " " + attr.first + "=\"" + attr.second + "\"";
        }
    }

    if (!countWritten && !counterAttr.empty()) {
        placeholderPosition = static_cast<size_t>(ftell(file)) +
                              rootTag.size() + 1 + counterAttr.size() + 2;
        rootTag += " " + counterAttr + "=\"0000000000\"";
    }

    rootTag += ">\n";
    fwrite(rootTag.c_str(), 1, rootTag.size(), file);
}

void XMLProcessor::writeRootEndTag(FILE* file, const RootInfo& root_info) const {
    if (!file) return;
    fprintf(file, "</%s>\n", root_info.name.c_str());
}

void XMLProcessor::flushGroupsBeforeObject(FILE* file, GroupStack& stack,
                                           bool forExcluded) {
    if (!file) return;

    for (auto& group : stack) {
        bool alreadyWritten = forExcluded ? group.writtenToExcluded
                                          : group.writtenToProcessed;

        if (!alreadyWritten) {
            // Записываем открывающий тег группы
            fwrite(group.openingTag.c_str(), 1, group.openingTag.size(), file);
            fwrite("\n", 1, 1, file);

            // Записываем свойства группы
            for (const auto& prop : group.propertiesXml) {
                fwrite(prop.c_str(), 1, prop.size(), file);
                fwrite("\n", 1, 1, file);
            }

            // Помечаем группу как записанную
            if (forExcluded) {
                group.writtenToExcluded = true;
            } else {
                group.writtenToProcessed = true;
            }
        }
    }
}

void XMLProcessor::flushGroupClosing(FILE* file, GroupBuffer& group) const {
    if (!file) return;

    std::string closingTag = "</" + group.name + ">";
    fwrite(closingTag.c_str(), 1, closingTag.size(), file);
    fwrite("\n", 1, 1, file);
}

void XMLProcessor::updateRecordCountInFile(const std::string& filePath,
                                           int count,
                                           size_t placeholderPosition) const {
    if (placeholderPosition == 0) return;

    FILE* file = fopen(filePath.c_str(), "r+b");
    if (!file) return;

    char buffer[16];
    snprintf(buffer, sizeof(buffer), "%010d", count);

    if (fseek(file, static_cast<long>(placeholderPosition), SEEK_SET) == 0) {
        fwrite(buffer, 1, 10, file);
    }

    fclose(file);
}

void XMLProcessor::streamingProcess(const std::string& xmlPath,
                                    ProcessingResult& result) {
    if (logger_) {
        logger_->Debug("XMLProcessor::streamingProcess: starting single pass for " +
                       xmlPath);
    }

    xmlTextReaderPtr reader = xmlReaderForFile(xmlPath.c_str(), nullptr,
                                               XML_PARSE_NOBLANKS |
                                               XML_PARSE_NONET |
                                               XML_PARSE_HUGE);
    if (!reader) {
        result.error_type = ProcessingResult::ErrorType::PARSE;
        throw std::runtime_error("Failed to open XML file: " + xmlPath);
    }

    if (isBypassMode()) {
        if (logger_) {
            logger_->Debug("XMLProcessor::streamingProcess: bypass mode detected");
        }
        result.bypass = true;
        result.success = true;
        xmlFreeTextReader(reader);
        return;
    }

    RootInfo root_info;
    if (!extractRootInfo(reader, root_info)) {
        xmlFreeTextReader(reader);
        result.error_type = ProcessingResult::ErrorType::PARSE;
        throw std::runtime_error("Failed to extract root info from XML file");
    }

    std::string filename = fs::path(xmlPath).filename().string();
    std::string processedTmpPath = getTempFilePath(filename, "proc");
    std::string excludedTmpPath = getTempFilePath(filename, "excl");

    FILE* processedFile = fopen(processedTmpPath.c_str(), "wb");
    FILE* excludedFile = nullptr;

    if (!config_.excluded_dir.empty()) {
        excludedFile = fopen(excludedTmpPath.c_str(), "wb");
    }

    if (!processedFile) {
        if (excludedFile) fclose(excludedFile);
        xmlFreeTextReader(reader);
        result.error_type = ProcessingResult::ErrorType::WRITE;
        throw std::runtime_error("Failed to open temporary processed file");
    }

    // Записываем корневые элементы с заполнителями для счётчиков
    size_t processedCountPos = 0;
    size_t excludedCountPos = 0;

    writeRootElementWithPlaceholder(processedFile, root_info, processedCountPos);
    if (excludedFile) {
        writeRootElementWithPlaceholder(excludedFile, root_info, excludedCountPos);
    }

    // Инициализируем стек групп
    GroupStack groupStack;

    int totalRecords = 0;
    int matchedRecords = 0;

    try {
        int ret = xmlTextReaderRead(reader);
        while (ret == 1) {
            int type = xmlTextReaderNodeType(reader);

            if (type == XML_READER_TYPE_ELEMENT) {
                bool isEmpty = (xmlTextReaderIsEmptyElement(reader) == 1);

                if (isObject(reader)) {
    totalRecords++;

    if (totalRecords % 1000 == 0) {
        if (logger_) {
            logger_->Debug("Processing progress: " +
                           std::to_string(totalRecords) +
                           " objects processed, groups in stack=" +
                           std::to_string(groupStack.size()));
        }
    }

    bool matched = false;
    xmlChar* outerXml = nullptr;

    if (!isEmpty) {
        outerXml = xmlTextReaderReadOuterXml(reader);
        if (outerXml) {
            // ДИАГНОСТИКА: логируем первые 200 символов объекта
            if (totalRecords <= 3) {
                if (logger_) {
                    std::string preview(reinterpret_cast<const char*>(outerXml));
                    if (preview.size() > 200) preview = preview.substr(0, 200);
                    logger_->Debug("Object " + std::to_string(totalRecords) +
                                   " outerXml preview: " + preview);
                }
            }

            xmlDocPtr objDoc = xmlReadDoc(outerXml, nullptr, nullptr,
                                          XML_PARSE_NOBLANKS |
                                          XML_PARSE_NONET |
                                          XML_PARSE_HUGE);
            if (objDoc) {
                xmlNodePtr objRoot = xmlDocGetRootElement(objDoc);
                if (objRoot) {
                    ObjectInfo obj_info;
                    extractObjectPropertiesFromNode(objRoot, obj_info);

                    // ДИАГНОСТИКА: логируем количество извлечённых свойств
                    if (totalRecords <= 3) {
                        if (logger_) {
                            logger_->Debug("Object " + std::to_string(totalRecords) +
                                           " properties count=" +
                                           std::to_string(obj_info.properties.size()));
                            for (const auto& prop : obj_info.properties) {
                                logger_->Debug("  Property: " + prop.name +
                                               " = " + prop.value);
                            }
                        }
                    }

                    matched = evaluateObject(obj_info, groupStack);

                    // ДИАГНОСТИКА: логируем результат оценки
                    if (totalRecords <= 3) {
                        if (logger_) {
                            logger_->Debug("Object " + std::to_string(totalRecords) +
                                           " matched=" + std::to_string(matched));
                        }
                    }
                } else {
                    if (logger_) {
                        logger_->Warning("Object " + std::to_string(totalRecords) +
                                         " objRoot is nullptr");
                    }
                }
                xmlFreeDoc(objDoc);
            } else {
                if (logger_) {
                    logger_->Warning("Object " + std::to_string(totalRecords) +
                                     " objDoc is nullptr (parse failed)");
                }
            }
        }
    }

    if (matched) {
        matchedRecords++;
        if (excludedFile) {
            flushGroupsBeforeObject(excludedFile, groupStack, true);
            if (outerXml) {
                fwrite(outerXml, 1, xmlStrlen(outerXml), excludedFile);
                fwrite("\n", 1, 1, excludedFile);
            }
        }
    } else {
        flushGroupsBeforeObject(processedFile, groupStack, false);
        if (outerXml) {
            fwrite(outerXml, 1, xmlStrlen(outerXml), processedFile);
            fwrite("\n", 1, 1, processedFile);
        }
    }

    if (outerXml) {
        xmlFree(outerXml);
    }

    ret = xmlTextReaderNext(reader);
    continue;
} else if (isEmpty) {
    xmlChar* outerXml = xmlTextReaderReadOuterXml(reader);
    if (outerXml && !groupStack.empty()) {
        std::string propXml(reinterpret_cast<const char*>(outerXml));

        GroupBuffer& currentGroup = groupStack.back();

        // Если группа записана в processed, записываем свойство в processed
        if (currentGroup.writtenToProcessed) {
            fwrite(propXml.c_str(), 1, propXml.size(), processedFile);
            fwrite("\n", 1, 1, processedFile);
        } else {
            // Группа ещё не записана, добавляем свойство в буфер
            currentGroup.propertiesXml.push_back(propXml);
        }

        // Если группа записана в excluded, записываем свойство в excluded
        if (currentGroup.writtenToExcluded && excludedFile) {
            fwrite(propXml.c_str(), 1, propXml.size(), excludedFile);
            fwrite("\n", 1, 1, excludedFile);
        }

        xmlFree(outerXml);
    } else if (outerXml) {
        xmlFree(outerXml);
    }

    ret = xmlTextReaderNext(reader);
    continue;
} else {
                    // ===== НЕПУСТОЙ ЭЛЕМЕНТ (группа) =====
                    GroupBuffer group;
                    const xmlChar* name = xmlTextReaderConstName(reader);
                    if (name) {
                        group.name = reinterpret_cast<const char*>(name);
                    }
                    group.openingTag = buildOpeningTag(reader);
                    group.writtenToProcessed = false;
                    group.writtenToExcluded = false;

                    extractGroupProperties(reader, group);

                    groupStack.push_back(std::move(group));

                    ret = xmlTextReaderRead(reader);
                    continue;
                }
            } else if (type == XML_READER_TYPE_END_ELEMENT) {
                // ===== ВЫХОД ИЗ ГРУППЫ =====
                if (!groupStack.empty()) {
                    GroupBuffer group = std::move(groupStack.back());
                    groupStack.pop_back();

                    if (group.writtenToProcessed) {
                        flushGroupClosing(processedFile, group);
                    }
                    if (group.writtenToExcluded && excludedFile) {
                        flushGroupClosing(excludedFile, group);
                    }
                }
            }

            ret = xmlTextReaderRead(reader);
        }

        xmlFreeTextReader(reader);

    } catch (const std::exception&) {
        fclose(processedFile);
        if (excludedFile) fclose(excludedFile);
        xmlFreeTextReader(reader);
        throw;
    }

    // Записываем закрывающие теги корневых элементов
    writeRootEndTag(processedFile, root_info);
    if (excludedFile) {
        writeRootEndTag(excludedFile, root_info);
    }

    fclose(processedFile);
    if (excludedFile) fclose(excludedFile);

    int processedCount = totalRecords - matchedRecords;
    int excludedCount = matchedRecords;

    // Обновляем счётчики во временных файлах
    if (processedCountPos > 0) {
        updateRecordCountInFile(processedTmpPath, processedCount,
                                processedCountPos);
    }
    if (excludedFile && excludedCountPos > 0) {
        updateRecordCountInFile(excludedTmpPath, excludedCount,
                                excludedCountPos);
    }

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

    if (excludedFile && excludedCount > 0 && !config_.excluded_dir.empty()) {
        std::string excludedDstPath = (fs::path(config_.excluded_dir) /
                                       config_.getExcludedFileName(filename)).string();
        fs::create_directories(config_.excluded_dir, ec);
        fs::rename(excludedTmpPath, excludedDstPath, ec);
        if (ec) {
            result.error_type = ProcessingResult::ErrorType::WRITE;
            throw std::runtime_error("Failed to move excluded file: " +
                                     ec.message());
        }
    } else if (excludedFile) {
        fs::remove(excludedTmpPath, ec);
    }

    result.success = true;
    result.records_processed = static_cast<size_t>(totalRecords);
    result.records_matched = static_cast<size_t>(matchedRecords);
    result.records_kept = static_cast<size_t>(processedCount);

    if (logger_) {
        logger_->Debug("XMLProcessor::streamingProcess: completed, total=" +
                       std::to_string(totalRecords) + ", matched=" +
                       std::to_string(matchedRecords) + ", kept=" +
                       std::to_string(processedCount));
    }
}

}  // namespace stc