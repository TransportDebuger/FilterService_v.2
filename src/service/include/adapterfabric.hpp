#pragma once
#include <memory>

#include "../include/filestorageinterface.hpp"

/**
 * @class AdapterFactory
 * @brief Абстрактная фабрика для создания адаптеров файловых хранилищ
 *
 * @note Реализует паттерн Abstract Factory для создания семейств связанных
 * объектов
 */
class AdapterFactory {
 public:
  virtual ~AdapterFactory() = default;

  /**
   * @brief Создает адаптер для указанного типа хранилища
   * @param config Конфигурация источника данных
   * @return std::unique_ptr<FileStorageInterface> Умный указатель на созданный
   * адаптер
   */
  virtual std::unique_ptr<FileStorageInterface> createAdapter();
      //const SourceConfig& config) = 0;

 protected:
  /**
   * @brief Валидирует обязательные параметры подключения
   * @param config Конфигурация источника
   * @param required_fields Список обязательных полей
   * @throw std::invalid_argument При отсутствии обязательных полей
   */
  void validateRequiredFields(
      const SourceConfig& config,
      const std::vector<std::string>& required_fields) const {
    for (const auto& field : required_fields) {
      if (config.params.find(field) == config.params.end()) {
        throw std::invalid_argument("Missing required field: " + field);
      }
    }
  }
};