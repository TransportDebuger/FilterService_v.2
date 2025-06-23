/**
 * @file AdapterFactory.hpp
 * @brief Фабрика для создания адаптеров файловых хранилищ
 *
 * @details Реализует паттерн Abstract Factory для создания адаптеров
 *          различных типов хранилищ с автоматической регистрацией
 */

#pragma once
#include "../include/FtpFileAdapter.hpp"
#include "../include/LocalStorageAdapter.hpp"
#include "../include/SmbFileAdapter.hpp"
#include "../include/filestorageinterface.hpp"
#include "../include/sourceconfig.hpp"
#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <unordered_map>

/**
 * @class AdapterFactory
 * @brief Фабрика для создания адаптеров файловых хранилищ
 *
 * @note Реализует паттерн Abstract Factory с поддержкой регистрации
 *       новых типов адаптеров во время выполнения[14][15]
 */
class AdapterFactory {
public:
  /// Тип функции-фабрики для создания адаптеров
  using CreatorFunction = std::function<std::unique_ptr<FileStorageInterface>(
      const SourceConfig &)>;

  /**
   * @brief Получить единственный экземпляр фабрики (Singleton)
   * @return Ссылка на экземпляр фабрики
   */
  static AdapterFactory &instance();

  /**
   * @brief Создает адаптер для указанного типа хранилища
   * @param config Конфигурация источника данных
   * @return std::unique_ptr<FileStorageInterface> Умный указатель на созданный
   * адаптер
   * @throw std::invalid_argument При неподдерживаемом типе хранилища
   *
   * @note Использует smart pointers для автоматического управления
   * памятью[27][28]
   */
  std::unique_ptr<FileStorageInterface>
  createAdapter(const SourceConfig &config);

  /**
   * @brief Регистрирует новый тип адаптера в фабрике
   * @param type Строковое обозначение типа хранилища
   * @param creator Функция создания адаптера
   *
   * @note Позволяет расширять фабрику без изменения основного кода[23][25]
   */
  void registerAdapter(const std::string &type, CreatorFunction creator);

  /**
   * @brief Проверяет поддержку типа хранилища
   * @param type Строковое обозначение типа
   * @return true если тип поддерживается
   */
  bool isSupported(const std::string &type) const noexcept;

  /**
   * @brief Получает список всех поддерживаемых типов
   * @return std::vector<std::string> Список типов хранилищ
   */
  std::vector<std::string> getSupportedTypes() const;

private:
  AdapterFactory();
  ~AdapterFactory() = default;

  // Запрещаем копирование и присваивание
  AdapterFactory(const AdapterFactory &) = delete;
  AdapterFactory &operator=(const AdapterFactory &) = delete;
  AdapterFactory(AdapterFactory &&) = delete;
  AdapterFactory &operator=(AdapterFactory &&) = delete;

  /**
   * @brief Регистрирует встроенные адаптеры
   */
  void registerBuiltinAdapters();

  /**
   * @brief Валидирует обязательные параметры подключения
   * @param config Конфигурация источника
   * @param required_fields Список обязательных полей
   * @throw std::invalid_argument При отсутствии обязательных полей
   */
  void
  validateRequiredFields(const SourceConfig &config,
                         const std::vector<std::string> &required_fields) const;

  /// Реестр зарегистрированных типов адаптеров
  std::unordered_map<std::string, CreatorFunction> creators_;

  /// Мьютекс для потокобезопасности
  mutable std::mutex mutex_;
};