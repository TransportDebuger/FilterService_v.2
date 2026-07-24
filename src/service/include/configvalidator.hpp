/**
@file configvalidator.hpp
@brief Валидация структуры JSON-конфигурации.
@version 2.0.0
@date 2026-07-20
*/
#pragma once

#include <nlohmann/json.hpp>

namespace stc {

/**
@class ConfigValidator
@brief Предоставляет методы валидации JSON-конфига сервиса.
*/
class ConfigValidator {
public:
    /**
    @brief Проверяет наличие обязательных корневых секций.
    @param[in] config JSON-объект конфигурации.
    @return true Если обе секции присутствуют и корректны.
    @throw std::runtime_error При отсутствии или некорректном типе секции.
    */
    bool validateRoot(const nlohmann::json &config) const;

    /**
    @brief Проверяет массив источников файлов.
    @param[in] sources JSON-массив объектов-источников.
    @return true Если все элементы массива валидны.
    @throw std::runtime_error При некорректном формате или отсутствии полей.
    */
    bool validateSources(const nlohmann::json &sources) const;

    /**
    @brief Проверяет секцию логирования конфигурации.
    @param[in] logging JSON-массив конфигураций логгеров.
    @return true Если все записи корректны.
    @throw std::runtime_error При нарушении структуры или типов.
    */
    bool validateLogging(const nlohmann::json &logging) const;

private:
    /**
    @private
    @brief Проверяет обязательные поля FTP/SFTP-источника.
    @param[in] source JSON-объект одного источника.
    @throw std::runtime_error При отсутствии или неверном типе полей.
    */
    void validateFtpFields(const nlohmann::json &source) const;
};

} // namespace stc