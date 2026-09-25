/**
@file XMLProcessor.hpp
@brief Потоковый процессор для обработки, фильтрации и сохранения результатов
XML-файлов.
@version 4.0.0
@date 2026-09-06
*/
#pragma once
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <libxml/xmlreader.h>
#include <libxml/xmlstring.h>
#include <libxml/xmlwriter.h>
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
@brief Выполняет многокритериальную потоковую фильтрацию XML-документов.
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

    /// @private Выполняет потоковую обработку файла.
    void streamingProcess(const std::string& xmlPath, ProcessingResult& result);

    /// @private Проверяет режим bypass.
    bool isBypassMode() const;

    /// @private Копирует файл без изменений.
    void copyFileBypass(const std::string& srcPath, const std::string& dstPath);

    /// @private Извлекает информацию о корневом элементе.
    bool extractRootInfo(xmlTextReaderPtr reader, RootInfo& root_info);

    /// @private Распознаёт объект по имени и пространству имён.
    bool isObject(xmlTextReaderPtr reader) const;

    /// @private Извлекает свойства объекта.
    void extractObjectProperties(xmlTextReaderPtr reader, ObjectInfo& obj_info);

    /// @private Оценивает объект по критериям.
    bool evaluateObject(const ObjectInfo& obj_info);

    /// @private Извлекает значение свойства по пути.
    std::string extractPropertyValue(
        const ObjectInfo& obj_info,
        const SourceConfig::XmlFilterCriterion& criterion);

    /// @private Нормализует значение свойства.
    static std::string normalizeValue(const std::string& value);

    /// @private Применяет логический оператор к результатам критериев.
    bool applyLogic(const std::vector<bool>& results);

    /// @private Записывает открывающий тег корневого элемента.
    void writeRootElement(FILE* file, const RootInfo& root_info, int record_count);

    /// @private Записывает закрывающий тег корневого элемента.
    void writeRootEndTag(FILE* file, const RootInfo& root_info);

    /// @private Записывает объект в файл.
    void writeObject(FILE* file, xmlTextReaderPtr reader);

    /// @private Обновляет значение счётчика в корневом элементе.
    void updateRecordCount(RootInfo& root_info, int record_count);

    /// @private Формирует путь к временному файлу.
    std::string getTempFilePath(const std::string& filename,
                                const std::string& prefix);

    /// @private Удаляет временные файлы.
    void cleanupTempFiles(const std::vector<std::string>& paths);

    /// @private Извлекает свойства объекта из узла.
    void extractObjectPropertiesFromNode(xmlNodePtr node, ObjectInfo& obj_info);
};

}  // namespace stc