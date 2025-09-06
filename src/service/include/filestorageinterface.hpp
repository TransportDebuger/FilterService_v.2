/**
 * @file filestorgeinterface.hpp
 * @brief Абстрактный интерфейс для работы с файловыми хранилищами
 * @author Artem Ulyanov
 * @company STC Ltd.
 * @date May 2025
 *
 * @details
 * Класс FileStorageInterface определяет контракт для адаптеров различных типов
 * файловых хранилищ (локальные папки, SMB, FTP и др.). Реализует паттерн
 * «Адаптер» для унификации доступа к файловым ресурсам. Все методы должны быть
 * потокобезопасны в реализациях и корректно обрабатывать ошибки ввода-вывода.
 */

#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

/**
 * @class FileStorageInterface
 * @brief Интерфейс операций с файловым хранилищем
 *
 * @details
 * Определяет методы для перечисления, скачивания, загрузки, мониторинга
 * изменений и управления соединением с файловым хранилищем.
 * Поддерживает callback для уведомления о новых файлах.
 */
class FileStorageInterface {
 public:
  /**
   * @brief Тип callback-функции для обнаруженных файлов
   *
   * @details
   * Функция вызывается адаптером при появлении нового файла.
   * Принимает полный путь к обнаруженному файлу.
   *
   * @ingroup Types
   */
  using FileDetectedCallback = std::function<void(const std::string &)>;

  /// Виртуальный деструктор
  virtual ~FileStorageInterface() = default;

  /**
     * @brief Возвращает список файлов в заданной директории
     * @ingroup Listing
     *
     * @param[in] path Путь к директории в хранилище (in)
     * @return std::vector<std::string> Вектор полных путей к файлам
     *
     * @details
     * Должен рекурсивно обходить поддиректории, если это поддерживается
     * реализацией. Вектор может быть пустым, если файлов нет или доступ
     запрещён.
     *
     * @throw std::runtime_error При ошибках доступа или чтения каталога
     *
     * @note Метод используется для первичной индексации существующих файлов
     * @warning Некорректные пути или права доступа приводят к выбросу
     исключения
     *
     * @code
     auto files = adapter->listFiles("/data/incoming");
     for (auto &file : files) {
         process(file);
     }
     @endcode
     */
  virtual std::vector<std::string> listFiles(const std::string &path) = 0;

  /**
     * @brief Скачивает файл из хранилища на локальный диск
     * @ingroup Transfer
     *
     * @param[in] remotePath Путь к файлу в хранилище (in)
     * @param[in] localPath  Локальный путь для сохранения (in)
     *
     * @details
     * Должен перезаписывать локальный файл, если он существует.
     * Использует буферизированное чтение/запись для эффективности.
     *
     * @throw std::ios_base::failure При ошибках ввода-вывода
     * @throw std::invalid_argument   При пустых или некорректных путях
     *
     * @note Убедитесь, что директория localPath существует или создайте её
     заранее
     * @warning Ошибки сети или прав доступа приводят к исключениям
     *
     * @code
     adapter->downloadFile("/remote/file.xml", "/tmp/file.xml");
     @endcode
     */
  virtual void downloadFile(const std::string &remotePath,
                            const std::string &localPath) = 0;

  /**
     * @brief Загружает локальный файл в хранилище
     * @ingroup Transfer
     *
     * @param[in] localPath  Локальный путь к файлу (in)
     * @param[in] remotePath Путь назначения в хранилище (in)
     *
     * @details
     * Должен создавать недостающие директории в пути назначения при
     необходимости.
     * Выполняет проверку существования локального файла заранее.
     *
     * @throw std::invalid_argument При отсутствии локального файла
     * @throw std::runtime_error     При ошибках записи в хранилище
     *
     * @note Используется для отправки результатов обработки назад в удалённое
     хранилище
     * @warning Права на запись в удалённую директорию критичны для успешной
     загрузки
     *
     * @code
     adapter->upload("output.xml", "/remote/processed/output.xml");
     @endcode
     */
  virtual void upload(const std::string &localPath,
                      const std::string &remotePath) = 0;

  /**
     * @brief Устанавливает соединение с файловым хранилищем
     * @ingroup Connection
     *
     * @throw std::runtime_error При ошибках подключения или аутентификации
     *
     * @details
     * Должен выполнять все этапы установки соединения и аутентификацию.
     * Вызывается перед любыми операциями с хранилищем.
     *
     * @note Для некоторых хранилищ (FTP/SMB) выполняется handshake и проверка
     версии
     * @warning Повторный вызов без disconnect() может привести к утечке
     ресурсов
     *
     * @code
     adapter->connect();
     if (adapter->isConnected()) {
         // готов к работе
     }
     @endcode
     */
  virtual void connect() = 0;

  /**
     * @brief Разрывает соединение с хранилищем
     * @ingroup Connection
     *
     * @details
     * Должен освобождать все сетевые или системные ресурсы, закрывать
     дескрипторы.
     * Вызывается при завершении работы или при ошибках.
     *
     * @note Не бросает исключений, рекомендуется вызывать в деструкторе
     * @warning Некорректный вызов после connect() без обработки ошибок может
     *          оставить открытые дескрипторы
     *
     * @code
     adapter->disconnect();
     @endcode
     */
  virtual void disconnect() = 0;

  /**
     * @brief Проверяет текущее состояние соединения
     * @ingroup Connection
     *
     * @return true если соединение активно и работоспособно
     *
     * @details
     * Быстрая проверка статуса без выполнения тяжёлых операций.
     *
     * @note Используется для проверки до вызова операций чтения/записи
     * @warning Не гарантирует актуальность при сетевых разрывах между вызовами
     *
     * @code
     if (!adapter->isConnected()) {
         adapter->connect();
     }
     @endcode
     */
  virtual bool isConnected() const noexcept = 0;

  /**
     * @brief Запускает фоновый мониторинг изменений в хранилище
     * @ingroup Monitoring
     *
     * @throw std::runtime_error Если мониторинг невозможен или не
     поддерживается
     *
     * @details
     * Для событийных хранилищ (локальных, SMB) использует
     inotify/ReadDirectoryChanges.
     * Для не-событийных (FTP) выполняет периодический опрос.
     *
     * @note Вызывается единожды после connect()
     * @warning Невызов stopMonitoring() может привести к утечке потоков
     *
     * @code
     adapter->startMonitoring();
     adapter->setCallback(onFileDetected);
     @endcode
     */
  virtual void startMonitoring() = 0;

  /**
     * @brief Останавливает фоновый мониторинг изменений
     * @ingroup Monitoring
     *
     * @details
     * Должен безопасно завершить фоновые потоки или таймеры опроса.
     *
     * @note После вызова можно вновь вызвать startMonitoring()
     * @warning Убедитесь, что все ресурсы освобождены до завершения работы
     *
     * @code
     adapter->stopMonitoring();
     @endcode
     */
  virtual void stopMonitoring() = 0;

  /**
     * @brief Проверяет, активен ли мониторинг
     * @ingroup Monitoring
     *
     * @return true если мониторинг изменений запущен
     *
     * @details
     * Быстрая проверка состояния без блокирующих вызовов.
     *
     * @note Используется для выбора между периодическим опросом и событиями
     * @warning Может быть неактуальным сразу после stopMonitoring()
     *
     * @code
     if (!adapter->isMonitoring()) {
         adapter->startMonitoring();
     }
     @endcode
     */
  virtual bool isMonitoring() const noexcept = 0;

  /**
     * @brief Устанавливает коллбэk для уведомлений о новых файлах
     * @ingroup Monitoring
     *
     * @param[in] callback Функция-обработчик нового файла (in)
     *
     * @details
     * Коллбэк вызывается в потоке мониторинга при обнаружении нового файла.
     * Принимает один аргумент — полный путь к файлу.
     *
     * @note Должен вызываться до startMonitoring()
     * @warning Коллбэк обязан быть потокобезопасным
     *
     * @code
     adapter->setCallback([](const std::string &filePath){
         processFile(filePath);
     });
     @endcode
     */
  virtual void setCallback(FileDetectedCallback callback) = 0;

 protected:
  /**
     * @brief Базовая проверка корректности пути
     * @ingroup InternalValidation
     *
     * @param[in] path Строка с путём к файлу или директории
     * @throw std::invalid_argument При пустом, некорректном или с «..»
     вхождением пути
     *
     * @details
     * Проверяет, что:
     *  - строка не пуста
     *  - не содержит «..» для предотвращения выхода за корень хранилища
     *  - соответствует базовым требованиям формата (без спецсимволов)
     *
     * @note
     * Подклассы могут расширять логику, делая дополнительные проверки
     * специфичные для конкретного протокола[1].
     *
     * @warning
     * Не вызывает побочных эффектов — лишь валидация входного аргумента.
     *
     * @code
     try {
         validatePath("../etc/passwd"); // бросит std::invalid_argument
     } catch (const std::invalid_argument& e) {
         // Обработка некорректного пути
     }
     @endcode
     */
  virtual void validatePath(const std::string &path) {
    if (path.empty() || path.find("..") != std::string::npos) {
      throw std::invalid_argument("Invalid path: " + path);
    }
  }

  /**
   * @brief Коллбэк для уведомления о новых файлах
   * @ingroup Callbacks
   *
   * @details
   * Хранит функцию, вызываемую при обнаружении нового файла адаптером.
   * Сигнатура: `void(const std::string& fullPath)` — полный путь к файлу.
   *
   * @note
   * Следует устанавливать через публичный метод `setCallback()`
   * до вызова `startMonitoring()`.
   *
   * @warning
   * Коллбэк должен быть потокобезопасным и быстрым, так как вызывается
   * в потоке мониторинга изменений хранилища[2].
   */
  FileDetectedCallback onFileDetected_;
};