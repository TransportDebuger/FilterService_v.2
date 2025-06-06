/**
 * @file FileStorageInterface.hpp
 * @brief Интерфейс для работы с файловыми хранилищами
 *
 * Определяет контракт для реализации адаптеров различных типов хранилищ.
 * Все методы являются чисто виртуальными, что делает класс абстрактным.
 */

#pragma once
#include <stdexcept>
#include <string>
#include <vector>

class FileStorageInterface {
 public:
  virtual ~FileStorageInterface() = default;

  /**
   * @brief Получает список файлов в указанной директории
   * @param path Путь к директории в хранилище
   * @return Вектор имен файлов
   * @throw std::runtime_error При ошибках доступа
   */
  virtual std::vector<std::string> listFiles(const std::string& path) = 0;

  /**
   * @brief Скачивает файл из хранилища
   * @param remotePath Путь к файлу в хранилище
   * @param localPath Локальный путь для сохранения
   * @throw std::ios_base::failure При ошибках ввода/вывода
   */
  virtual void downloadFile(const std::string& remotePath,
                            const std::string& localPath) = 0;

  /**
   * @brief Загружает файл в хранилище
   * @param localPath Локальный путь к файлу
   * @param remotePath Путь назначения в хранилище
   * @throw std::invalid_argument Если файл не существует
   */
  virtual void upload(const std::string& localPath,
                      const std::string& remotePath) = 0;

  /**
   * @brief Устанавливает соединение с хранилищем
   * @throw std::runtime_error При ошибках подключения
   * @note Должен вызываться перед другими операциями
   */
  virtual void connect() = 0;

  /**
   * @brief Проверяет доступность хранилища
   * @return true если соединение активно
   */
  virtual bool isAvailable() noexcept = 0;

 protected:
  /**
   * @brief Валидирует путь к файлу
   * @param path Проверяемый путь
   * @throw std::invalid_argument При невалидном пути
   */
  virtual void validatePath(const std::string& path) {
    if (path.empty() || path.find("..") != std::string::npos) {
      throw std::invalid_argument("Invalid file path: " + path);
    }
  }
};