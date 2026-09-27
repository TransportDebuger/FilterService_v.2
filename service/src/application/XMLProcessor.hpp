/**
@file XMLProcessor.hpp
@brief Потоковый процессор для обработки, фильтрации и сохранения результатов
XML-файлов с поддержкой иерархических групп.
@version 5.0.0
@date 2026-09-27
*/
#pragma once
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <libxml/xmlreader.h>
#include <libxml/xmlstring.h>
#include <cstdio>
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
    bool bypass{false};
    size_t records_processed{0};
    size_t records_matched{0};
    size_t records_kept{0};
    size_t bytes_processed{0};
    enum class ErrorType { NONE, PARSE, WRITE, STRUCTURE };
    ErrorType error_type{ErrorType::NONE};
};

/**
@class XMLProcessor
@brief Выполняет многокритериальную потоковую фильтрацию XML-документов
с поддержкой иерархических групп и ленивой записью.
*/
class XMLProcessor {
public:
    /**
    @brief Конструктор процессора.
    @param[in] config Конфигурация источника данных.
    @param[in] logger Диспетчер логирования.
    @param[in] filter_list_manager Менеджер списков фильтрации.
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
    /// @private Конфигурация источника данных.
    const SourceConfig& config_;

    /// @private Диспетчер логирования.
    std::shared_ptr<stc::logger::ILogger> logger_;

    /// @private Менеджер списков фильтрации.
    std::shared_ptr<FilterListManager> filter_list_manager_;

    /**
    @struct RootInfo
    @brief Информация о корневом элементе документа.
    */
    struct RootInfo {
        std::string name;
        std::string namespace_uri;
        std::string namespace_prefix;
        std::vector<std::pair<std::string, std::string>> attributes;
        std::vector<std::pair<std::string, std::string>> namespace_declarations;
        std::string xml_declaration;
    };

    /**
    @struct Property
    @brief Свойство объекта или группы.
    */
    struct Property {
        std::string name;
        std::string value;
    };

    /**
    @struct ObjectInfo
    @brief Информация об объекте для оценки критериев.
    */
    struct ObjectInfo {
        std::vector<Property> properties;
        bool has_cdata{false};
    };

    /**
    @struct GroupBuffer
    @brief Буфер для группы в стеке однопроходной обработки.
    */
    struct GroupBuffer {
        /// @private Имя элемента группы.
        std::string name;

        /// @private Открывающий тег группы с атрибутами.
        std::string openingTag;

        /// @private Свойства группы в формате внешнего XML для записи в файлы.
        std::vector<std::string> propertiesXml;

        /// @private Свойства группы для оценки критериев объектов.
        std::vector<Property> properties;

        /// @private Флаг записи открывающего тега в processed.
        bool writtenToProcessed{false};

        /// @private Флаг записи открывающего тега в excluded.
        bool writtenToExcluded{false};
    };

    /// @private Стек групп для однопроходной обработки.
    using GroupStack = std::vector<GroupBuffer>;

    /// @private Выполняет однопроходную потоковую обработку файла.
    void streamingProcess(const std::string& xmlPath, ProcessingResult& result);

    /// @private Проверяет режим bypass.
    bool isBypassMode() const;

    /// @private Извлекает информацию о корневом элементе.
    bool extractRootInfo(xmlTextReaderPtr reader, RootInfo& root_info);

    /// @private Распознаёт объект по имени и пространству имён.
    bool isObject(xmlTextReaderPtr reader) const;

    /// @private Извлекает свойства объекта из узла.
    void extractObjectPropertiesFromNode(xmlNodePtr node,
                                         ObjectInfo& obj_info) const;

    /// @private Оценивает объект по критериям, включая свойства групп из стека.
    bool evaluateObject(const ObjectInfo& obj_info,
                        const GroupStack& groupStack) const;

    /// @private Извлекает значение свойства по пути из объекта и групп.
    std::string extractPropertyValue(
        const ObjectInfo& obj_info,
        const GroupStack& groupStack,
        const SourceConfig::XmlFilterCriterion& criterion) const;

    /// @private Нормализует значение свойства.
    static std::string normalizeValue(const std::string& value);

    /// @private Применяет логический оператор к результатам критериев.
    bool applyLogic(const std::vector<bool>& results) const;

    /// @private Записывает корневой элемент с заполнителем счётчика.
    void writeRootElementWithPlaceholder(FILE* file, const RootInfo& root_info,
                                         size_t& placeholderPosition) const;

    /// @private Записывает закрывающий тег корневого элемента.
    void writeRootEndTag(FILE* file, const RootInfo& root_info) const;

    /// @private Формирует открывающий тег элемента из текущей позиции читателя.
    std::string buildOpeningTag(xmlTextReaderPtr reader) const;

    /// @private Извлекает свойства группы из узла для оценки критериев.
    void extractGroupProperties(xmlTextReaderPtr reader,
                                GroupBuffer& group) const;

    /// @private Записывает открывающие теги групп и их свойства в файл.
    void flushGroupsBeforeObject(FILE* file, GroupStack& stack,
                                 bool forExcluded);

    /// @private Записывает свойства группы и закрывающий тег в файл.
    void flushGroupClosing(FILE* file, GroupBuffer& group) const;

    /// @private Обновляет счётчик в файле по позиции заполнителя.
    void updateRecordCountInFile(const std::string& filePath, int count,
                                 size_t placeholderPosition) const;

    /// @private Формирует путь к временному файлу.
    std::string getTempFilePath(const std::string& filename,
                                const std::string& prefix) const;

    /// @private Удаляет временные файлы.
    void cleanupTempFiles(const std::vector<std::string>& paths) const;
};

}  // namespace stc