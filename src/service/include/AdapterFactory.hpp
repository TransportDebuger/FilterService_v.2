/**
 * @file AdapterFactory.hpp
 * @brief Фабрика для создания адаптеров файловых хранилищ (Abstract Factory)
 * @author Artem Ulyanov
 * @company STC Ltd.
 * @date May 2025
 *
 * @details
 * Класс AdapterFactory реализует паттерн «Абстрактная Фабрика» для создания
 * адаптеров различных типов хранилищ (локальное, SMB, FTP и др.) на основе
 * конфигурации SourceConfig.
 * Поддерживает динамическую регистрацию новых типов адаптеров во время
 * выполнения через метод registerAdapter().
 * Обеспечивает потокобезопасность при создании и регистрации адаптеров
 * с помощью std::mutex.
 */
#pragma once

#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <unordered_map>

#include "../include/FtpFileAdapter.hpp"
#include "../include/LocalStorageAdapter.hpp"
#include "../include/SmbFileAdapter.hpp"
#include "../include/filestorageinterface.hpp"
#include "../include/sourceconfig.hpp"

/**
 * @class AdapterFactory
 * @brief Singleton-фабрика для создания адаптеров файловых хранилищ
 *
 * @ingroup Core
 *
 * @details
 * AdapterFactory предоставляет единый интерфейс для получения адаптера
 * через метод createAdapter(), основанный на type из SourceConfig.
 * Реализует потокобезопасный реестр функций-производителей функций
 * CreatorFunction, сопоставляя строковый идентификатор типа адаптера
 * с функцией создания.
 *
 * @note
 * Динамическая регистрация поддерживает открытость/закрытость (OCP)
 * без изменения кода фабрики.
 * @warning
 * Не используйте registerAdapter() для ключей, уже зарегистрированных,
 * чтобы избежать перезаписи ранее заданного поведения.
 */
class AdapterFactory {
 public:
  /**
   * @typedef CreatorFunction
   * @brief Тип функции-фабрики для создания адаптеров
   *
   * @details
   * Функция принимает const SourceConfig& (in) и возвращает
   * std::unique_ptr<FileStorageInterface> (out).
   * Используется для регистрации и создания адаптера.
   */
  using CreatorFunction = std::function<std::unique_ptr<FileStorageInterface>(
      const SourceConfig &)>;

  /**
     * @brief Возвращает единственный экземпляр фабрики (Singleton)
     * @ingroup Getters
     *
     * @return AdapterFactory& Ссылка на глобальный экземпляр фабрики
     *
     * @details
     * Ленивая инициализация через локальную статическую переменную
     * в соответствии с C++11 thread-safe initialization.
     *
     * @code
     AdapterFactory &factory = AdapterFactory::instance();
     @endcode
     *
     * @note
     * Вызывать до регистрации пользовательских адаптеров.
     * @warning
     * Деструктор фабрики вызывается автоматически при завершении программы.
     */
  static AdapterFactory &instance();

  /**
     * @brief Создаёт адаптер указанного типа на основе конфигурации
     * @ingroup Creation
     *
     * @param[in] config Настройки источника данных (in)
     * @return std::unique_ptr<FileStorageInterface> Умный указатель на адаптер
     (out)
     * @throw std::invalid_argument Если config.type пуст или неподдерживаемый
     тип
     * @throw std::runtime_error При ошибках создания адаптера внутри
     CreatorFunction
     *
     * @details
     *  - Блокирует mutex_ для потокобезопасного доступа к реестру creators_
     *  - Пытается найти функцию-производитель по ключу config.type
     *  - При отсутствии функции генерирует std::invalid_argument
     *  - Вызывает функцию-производитель и возвращает полученный адаптер
     *
     * @code
     auto adapter = AdapterFactory::instance().createAdapter(sourceConfig);
     @endcode
     *
     * @note
     * Внутри метода логируются успешные и ошибочные попытки создания
     * через CompositeLogger::instance()
     * @warning
     * Не проверяет корректность config.params — это ответственность адаптера.
     */
  std::unique_ptr<FileStorageInterface> createAdapter(
      const SourceConfig &config);

  /**
     * @brief Регистрирует новый тип адаптера в фабрике
     * @ingroup Registration
     *
     * @param[in] type      Строковое обозначение типа хранилища (in)
     * @param[in] creator   Функция-производитель адаптера (in)
     * @throw std::invalid_argument Если type пустой или creator == nullptr
     *
     * @details
     *  - Блокирует mutex_
     *  - Проверяет корректность параметров
     *  - Добавляет пару (type, creator) в unordered_map creators_
     *
     * @code
     AdapterFactory::instance().registerAdapter("custom", [](const SourceConfig
     &cfg){ return std::make_unique<CustomAdapter>(cfg);
     });
     @endcode
     *
     * @note
     * Позволяет расширять поддержку хранилищ без модификации фабрики.
     * @warning
     * Повторная регистрация одного и того же типа перезапишет предыдущую
     функцию.
     */
  void registerAdapter(const std::string &type, CreatorFunction creator);

  /**
     * @brief Проверяет поддержку типа хранилища
     * @ingroup Query
     *
     * @param[in] type Строковой идентификатор типа (in)
     * @return bool true, если тип зарегистрирован, иначе false (out)
     *
     * @details
     *  - Блокирует mutex_
     *  - Ищет ключ type в creators_
     *
     * @code
     if (AdapterFactory::instance().isSupported("ftp")) {
         // Поддержка FTP доступна
     }
     @endcode
     *
     * @note noexcept, не бросает исключений
     * @warning Чувствительно к регистру символов в type.
     */
  bool isSupported(const std::string &type) const noexcept;

  /**
     * @brief Возвращает список всех поддерживаемых типов адаптеров
     * @ingroup Query
     *
     * @return std::vector<std::string> Вектор зарегистрированных типов (out)
     *
     * @details
     *  - Блокирует mutex_
     *  - Копирует все ключи creators_ в вектор
     *  - Сортирует вектор по алфавиту для удобства
     *
     * @code
     auto types = AdapterFactory::instance().getSupportedTypes();
     for (auto &t : types) { std::cout << t << "\n"; }
     @endcode
     *
     * @note noexcept, не бросает исключений
     * @warning Может быть дорогостоящей операцией при большом реестре.
     */
  std::vector<std::string> getSupportedTypes() const;

 private:
  /**
   * @brief Приватный конструктор Singleton
   *
   * @details
   * Регистрирует встроенные адаптеры через registerBuiltinAdapters().
   * Вызывается единожды при первом обращении к instance().
   */
  AdapterFactory();

  /** @brief Деструктор по умолчанию (пустой) */
  ~AdapterFactory() = default;

  // Запрещаем копирование и присваивание
  AdapterFactory(const AdapterFactory &) = delete;
  AdapterFactory &operator=(const AdapterFactory &) = delete;
  AdapterFactory(AdapterFactory &&) = delete;
  AdapterFactory &operator=(AdapterFactory &&) = delete;

  /**
   * @brief Регистрирует встроенные адаптеры: local, smb, ftp
   * @ingroup Registration
   *
   * @details
   * Автоматически вызывается из конструктора:
   *  - Регистрация LocalStorageAdapter
   *  - Регистрация SmbFileAdapter (валидация username)
   *  - Регистрация FtpFileAdapter (валидация username/password)
   *
   * @code
   * registerBuiltinAdapters();
   * @endcode
   */
  void registerBuiltinAdapters();

  /**
     * @brief Валидирует обязательные поля SourceConfig.params
     * @ingroup Validation
     *
     * @param[in] config          Конфигурация источника (in)
     * @param[in] required_fields Список ключей, обязательных в params (in)
     * @throw std::invalid_argument Если какой-либо ключ отсутствует или пуст
     *
     * @details
     * Проходит по каждому полю из required_fields и проверяет, что
     * config.params содержит непустую строку по этому ключу.
     *
     * @code
     validateRequiredFields(cfg, {"username", "password"});
     @endcode
     *
     * @note Используется при регистрации smb/ftp адаптеров
     * @warning Не вызывайте для local-адапторов.
     */
  void validateRequiredFields(
      const SourceConfig &config,
      const std::vector<std::string> &required_fields) const;

  /**
   * @brief Реестр функций-производителей адаптеров
   * @ingroup Internal
   *
   * @details
   * Хранит сопоставление строкового ключа типа адаптера
   * (например, "local", "ftp") и функции `CreatorFunction`,
   * создающей конкретный объект FileStorageInterface.
   *
   * @note Эта коллекция инициализируется вызовом registerBuiltinAdapters()
   *       в конструкторе Singleton.
   * @warning Неправильная регистрация (пустой ключ или nullptr-функция)
   *          может привести к ошибкам при вызове createAdapter().
   */
  std::unordered_map<std::string, CreatorFunction> creators_;

  /**
   * @brief Мьютекс для синхронизации доступа к реестру creators_
   * @ingroup Internal
   *
   * @details
   * Защищает операции чтения и записи в `creators_` от
   * одновременного доступа из разных потоков.
   *
   * @note Всегда использовать std::lock_guard или std::unique_lock
   *       при работе с creators_.
   * @warning Избегайте блокировки мьютекса во время длительных
   *          операций, чтобы не спровоцировать дедлоки.
   */
  mutable std::mutex mutex_;
};