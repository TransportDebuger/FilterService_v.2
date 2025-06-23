/**
 * @file FileStorageInterface.hpp
 * @brief Интерфейс для работы с файловыми хранилищами
 *
 * @details Определяет контракт для реализации адаптеров различных типов
 * хранилищ. Все методы являются чисто виртуальными, что делает класс
 * абстрактным.
 *
 * @note Реализует паттерн "Адаптер" для унификации работы с разными типами
 * хранилищ
 */

#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

/**
 * @class FileStorageInterface
 * @brief Базовый интерфейс для работы с файловыми хранилищами
 *
 * @warning Все методы должны быть потокобезопасными в реализациях
 */
class FileStorageInterface {
 public:
  /// Тип коллбэка для обработки обнаруженных файлов
  using FileDetectedCallback = std::function<void(const std::string &)>;

  virtual ~FileStorageInterface() = default;

  /**
   * @brief Получает список файлов в указанной директории
   * @param path Путь к директории в хранилище
   * @return Вектор полных путей к файлам
   * @throw std::runtime_error При ошибках доступа
   *
   * @note Реализация должна рекурсивно обходить поддиректории, если это
   * поддерживается хранилищем
   */
  virtual std::vector<std::string> listFiles(const std::string &path) = 0;

  /**
   * @brief Скачивает файл из хранилища
   * @param remotePath Путь к файлу в хранилище
   * @param localPath Локальный путь для сохранения
   * @throw std::ios_base::failure При ошибках ввода/вывода
   * @throw std::invalid_argument При невалидных путях
   *
   * @note Должен перезаписывать существующие файлы
   */
  virtual void downloadFile(const std::string &remotePath,
                            const std::string &localPath) = 0;

  /**
   * @brief Загружает файл в хранилище
   * @param localPath Локальный путь к файлу
   * @param remotePath Путь назначения в хранилище
   * @throw std::invalid_argument Если локальный файл не существует
   * @throw std::runtime_error При ошибках загрузки
   *
   * @note Должен создавать недостающие директории в пути назначения
   */
  virtual void upload(const std::string &localPath,
                      const std::string &remotePath) = 0;

  /**
   * @brief Устанавливает соединение с хранилищем
   * @throw std::runtime_error При ошибках подключения
   *
   * @note Должен вызываться перед другими операциями. Может выполнять
   * аутентификацию.
   */
  virtual void connect() = 0;

  /**
   * @brief Разрывает соединение с хранилищем
   * @note Должен освобождать все выделенные ресурсы
   */
  virtual void disconnect() = 0;

  /**
   * @brief Проверяет активность соединения
   * @return true если соединение активно и работоспособно
   *
   * @note Не должен выполнять тяжелых операций, только быструю проверку
   */
  virtual bool isConnected() const noexcept = 0;

  /**
   * @brief Запускает фоновый мониторинг изменений
   * @throw std::runtime_error Если мониторинг невозможен
   *
   * @note Для событийных хранилищ (локальные/SMB) использует механизмы
   * inotify/ReadDirectoryChanges. Для протоколов без событий (FTP) реализует
   * периодический опрос.
   */
  virtual void startMonitoring() = 0;

  /**
   * @brief Останавливает мониторинг изменений
   * @note Должен гарантировать безопасное завершение фоновых потоков
   */
  virtual void stopMonitoring() = 0;

  /**
   * @brief Проверяет статус мониторинга
   * @return true если мониторинг активен
   */
  virtual bool isMonitoring() const noexcept = 0;

  /**
   * @brief Устанавливает обработчик обнаружения новых файлов
   * @param callback Функция-обработчик
   *
   * @note Коллбэк должен вызываться в потоке мониторинга. Реализации должны
   * гарантировать потокобезопасность при вызове.
   */
  virtual void setCallback(FileDetectedCallback callback) = 0;

 protected:
  /**
   * @brief Валидирует путь к файлу
   * @param path Проверяемый путь
   * @throw std::invalid_argument При невалидном пути
   *
   * @note Базовая реализация проверяет:
   * - Непустой путь
   * - Отсутствие переходов наверх (..)
   * - Корректность формата для конкретного протокола
   */
  virtual void validatePath(const std::string &path) {
    if (path.empty() || path.find("..") != std::string::npos) {
      throw std::invalid_argument("Invalid path: " + path);
    }
  }

  FileDetectedCallback
      onFileDetected_;  ///< Коллбэк для уведомлений о новых файлах
};