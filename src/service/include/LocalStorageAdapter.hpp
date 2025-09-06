/**
 * @file LocalStorageAdapter.hpp
 * @brief Адаптер для работы с локальной файловой системой и событийным
 * мониторингом
 * @author Artem Ulyanov
 * @company STC Ltd.
 * @date June 2025
 *
 * @details
 * Класс LocalStorageAdapter реализует интерфейс FileStorageInterface для
 * локальных директорий и использует FileWatcher для мониторинга появления,
 * изменения и удаления файлов в реальном времени. Адаптер обеспечивает
 * копирование и перемещение файлов между директориями, а также фильтрацию по
 * маске и безопасное создание необходимых каталогов.
 *
 * @note
 * Поддерживает как однократное сканирование всех существующих файлов через
 * listFiles(), так и событийный подход через
 * startMonitoring()/stopMonitoring().
 * @warning
 * При ошибках доступа к файловой системе генерирует исключения
 * std::runtime_error или std::ios_base::failure.
 */

#pragma once
#include <atomic>
#include <filesystem>
#include <memory>
#include <mutex>

#include "../include/filestorageinterface.hpp"
#include "../include/filewatcher.hpp"
#include "../include/sourceconfig.hpp"

namespace fs = std::filesystem;

/**
 * @class LocalStorageAdapter
 * @brief Адаптер для локальной файловой системы с поддержкой событийного
 * мониторинга
 *
 * @ingroup FileStorageAdapters
 *
 * @details
 * Обеспечивает операции перечисления, загрузки и выгрузки файлов в локальной
 * файловой системе. Для мониторинга изменений используется FileWatcher, который
 * генерирует callback при появлении новых файлов.
 */
class LocalStorageAdapter : public FileStorageInterface {
 public:
  /**
     * @brief Конструктор адаптера локальной директории
     *
     * @param[in] config Конфигурация источника с путями и шаблонами (in)
     * @throw std::invalid_argument Если конфигурация некорректна, например,
     пустая file_mask
     *
     * @details
     * Проверяет валидность пути и шаблона имени файла, регистрирует initial
     state
     * адаптера и пишет сообщение в лог через CompositeLogger.
     *
     * @note
     * Использует validatePath() для базовой проверки пути.
     *
     * @warning
     * Если config.file_mask пуст, выбрасывается исключение для предотвращения
     неопределённого поведения.
     *
     * @code
     SourceConfig cfg;
     cfg.path = "./input";
     cfg.file_mask = "*.xml";
     LocalStorageAdapter adapter(cfg);
     @endcode
     */
  explicit LocalStorageAdapter(const SourceConfig &config);

  /**
   * @brief Деструктор адаптера локальной файловой системы
   *
   * @details
   * Автоматически останавливает мониторинг и отключается от хранилища,
   * освобождая ресурсы FileWatcher и сбрасывая флаги состояния.
   *
   * @note
   * Подавляет все исключения в теле деструктора для безопасного завершения.
   */
  ~LocalStorageAdapter() override;

  /**
     * @brief Перечисляет все файлы в указанной директории
     *
     * @param[in] path Путь к директории для сканирования (in)
     * @return std::vector<std::string> Вектор полных путей к файлам (out)
     * @throw std::runtime_error При ошибках доступа к файловой системе
     *
     * @details
     * Использует std::filesystem::directory_iterator для перебора файлов.
     * Фильтрует имена в соответствии с config_.file_mask через
     matchesFileMask().
     *
     * @note
     * Не обходит поддиректории рекурсивно.
     *
     * @warning
     * В случае отсутствия директории возвращает пустой список и логирует
     warning.
     *
     * @code
     auto files = adapter.listFiles("/data/in");
     for (auto &f : files) {
         processFile(f);
     }
     @endcode
     */
  std::vector<std::string> listFiles(const std::string &path) override;

  /**
     * @brief Копирует файл из локального хранилища в заданный путь
     *
     * @param[in] remotePath Полный путь к исходному файлу (in)
     * @param[in] localPath  Полный путь для сохранения файла (in)
     * @throw std::invalid_argument   Если исходный файл не существует
     * @throw std::ios_base::failure   При ошибках копирования файловой системы
     *
     * @details
     * Проверяет существование remotePath, создаёт директорию localPath при
     необходимости
     * и выполняет copy_file() с опцией overwrite_existing.
     *
     * @note
     * При невозможности создать директорию выбрасывает std::runtime_error.
     *
     * @warning
     * Не обрабатывает сетевые ресурсы — только локальные файлы.
     *
     * @code
     adapter.downloadFile("/data/in/file.xml", "./out/file.xml");
     @endcode
     */
  void downloadFile(const std::string &remotePath,
                    const std::string &localPath) override;

  /**
     * @brief Загружает локальный файл в целевую директорию
     *
     * @param[in] localPath  Путь к исходному локальному файлу (in)
     * @param[in] remotePath Полный путь назначения в хранилище (in)
     * @throw std::invalid_argument При отсутствии локального файла
     * @throw std::runtime_error      При ошибках записи в файловую систему
     *
     * @details
     * Аналогично downloadFile, но в обратном направлении.
     * Создаёт директорию назначения и копирует файл.
     *
     * @note
     * Используется для сохранения результатов обработки XML.
     *
     * @warning
     * Размеры файлов могут влиять на время выполнения.
     *
     * @code
     adapter.upload("./out/result.xml", "/data/processed/result.xml");
     @endcode
     */
  void upload(const std::string &localPath,
              const std::string &remotePath) override;

  /**
     * @brief Устанавливает соединение с локальным хранилищем
     *
     * @throw std::runtime_error При ошибках проверки доступа или создания
     директорий
     *
     * @details
     * Вызывает ensurePathExists() для config_.path и устанавливает connected_ =
     true.
     * Логирует успешное подключение через CompositeLogger.
     *
     * @note
     * Для локального адаптера фактического сетевого соединения нет.
     *
     * @warning
     * Переопределяет статус предыдущего соединения без проверки.
     *
     * @code
     adapter.connect();
     @endcode
     */
  void connect() override;

  /**
     * @brief Разрывает «соединение» с локальным хранилищем
     *
     * @details
     * Останавливает мониторинг, сбрасывает connected_ = false
     * и логирует отключение.
     *
     * @note noexcept, не бросает исключений
     *
     * @code
     adapter.disconnect();
     @endcode
     */
  void disconnect() override;

  /**
     * @brief Проверяет состояние подключения
     *
     * @return bool true если подключение активно, иначе false
     *
     * @details
     * Возвращает текущее состояние atomic-флага connected_.
     *
     * @note noexcept
     *
     * @code
     if (adapter.isConnected()) { ... }
     @endcode
     */
  bool isConnected() const noexcept override;

  /**
     * @brief Запускает событийный мониторинг директории
     *
     * @throw std::runtime_error Если adapter не подключён или watcher_ не
     создан
     *
     * @details
     * Создаёт instance FileWatcher, устанавливает callback на
     handleFileEvent(),
     * запускает watcher_->start() и устанавливает monitoring_ = true.
     *
     * @note
     * Производит мониторинг в отдельном потоке FileWatcher.
     *
     * @warning
     * При повторном вызове без stopMonitoring() генерирует warning.
     *
     * @code
     adapter.startMonitoring();
     @endcode
     */
  void startMonitoring() override;

  /**
     * @brief Останавливает событийный мониторинг
     *
     * @details
     * Вызывает watcher_->stop(), сбрасывает monitoring_ = false
     * и освобождает ресурс watcher_.
     *
     * @note noexcept
     *
     * @code
     adapter.stopMonitoring();
     @endcode
     */
  void stopMonitoring() override;

  /**
     * @brief Проверяет, запущен ли мониторинг
     *
     * @return bool true если monitoring_ == true, иначе false
     *
     * @details
     * Возвращает текущее значение atomic-флага monitoring_.
     *
     * @note noexcept
     *
     * @code
     if (adapter.isMonitoring()) { ... }
     @endcode
     */
  bool isMonitoring() const noexcept override;

  /**
     * @brief Устанавливает callback для уведомлений о новых файлах
     *
     * @param[in] callback Функция-обработчик типа FileDetectedCallback (in)
     *
     * @details
     * Сохраняет callback в onFileDetected_ для обработки событий FileWatcher.
     *
     * @note Должно быть задано до startMonitoring().
     *
     * @warning Коллбэк должен быть потокобезопасным.
     *
     * @code
     adapter.setCallback([](auto path){ process(path); });
     @endcode
     */
  void setCallback(FileDetectedCallback callback) override;

 private:
  /**
   * @brief Проверяет доступность config_.path и создает директории
   *
   * @throw std::runtime_error При невозможности создать или получить доступ
   *
   * @details
   * Использует fs::exists() и fs::create_directories() для гарантии
   * существования директории.
   *
   * @note Вызывается в connect().
   *
   * @warning Нарушение прав доступа приводит к исключению.
   */
  void ensurePathExists();

  /**
   * @brief Обрабатывает события FileWatcher
   *
   * @param[in] event    Тип события FileWatcher::Event (in)
   * @param[in] filePath Полный путь к файлу, по которому произошло событие (in)
   *
   * @details
   * При событии Created проверяет маску файла через matchesFileMask()
   * и вызывает onFileDetected_().
   *
   * @note Вызывается из фонового потока FileWatcher.
   *
   * @warning Исключения внутри подавляются и логируются.
   */
  void handleFileEvent(FileWatcher::Event event, const std::string &filePath);

  /**
   * @brief Проверяет соответствие имени файла маске
   *
   * @param[in] filename Имя файла без пути (in)
   * @return bool true если filename соответствует config_.file_mask, иначе
   * false
   *
   * @details
   * Преобразует glob-маску в регулярное выражение и выполняет
   * std::regex_match() c флагом игнорирования регистра.
   *
   * @note Использует std::regex_constants::icase для нечувствительности.
   *
   * @warning В случае ошибки компиляции регулярного выражения возвращает true.
   */
  bool matchesFileMask(const std::string &filename) const;

  /**
   * @brief Конфигурация источника данных
   *
   * @details
   * Содержит все параметры источника: путь к директории мониторинга,
   * маску файлов для фильтрации, пути к директориям обработанных
   * и ошибочных файлов. Инициализируется в конструкторе и остается
   * неизменной на протяжении жизненного цикла адаптера.
   *
   * Включает следующие ключевые поля:
   * - `path`: абсолютный или относительный путь к мониторимой директории
   * - `file_mask`: glob-маска для фильтрации файлов (например, "*.xml")
   * - `processed_dir`: директория для успешно обработанных файлов
   * - `bad_dir`: директория для файлов с ошибками
   *
   * @note Валидация конфигурации выполняется в конструкторе через
   * validatePath()
   * @warning Изменение config_ после инициализации может привести к
   *          нарушению работы мониторинга
   */
  SourceConfig config_;

  /**
   * @brief Объект мониторинга файловой системы
   *
   * @details
   * Умный указатель на экземпляр FileWatcher, который обеспечивает
   * событийный мониторинг директории config_.path. Создается при вызове
   * startMonitoring() и уничтожается при stopMonitoring().
   *
   * FileWatcher использует платформенно-зависимые механизмы:
   * - Linux: inotify для получения событий файловой системы
   * - Windows: ReadDirectoryChangesW для мониторинга изменений
   *
   * При обнаружении событий (создание, удаление, изменение файлов)
   * вызывает callback-функцию handleFileEvent().
   *
   * @note Использует std::unique_ptr для автоматического управления памятью
   * @warning Доступ к watcher_ должен быть синхронизирован через mutex_
   *          из-за многопоточного использования
   */
  std::unique_ptr<FileWatcher> watcher_;

  /**
   * @brief Атомарный флаг состояния подключения
   *
   * @details
   * Указывает, установлено ли "соединение" с локальным хранилищем.
   * Для локального адаптера это означает успешную проверку доступности
   * и создание необходимых директорий.
   *
   * Устанавливается в true при успешном выполнении connect(),
   * сбрасывается в false при вызове disconnect().
   *
   * Состояния:
   * - `false`: адаптер не подключен, операции файлового доступа недоступны
   * - `true`: адаптер готов к работе, можно выполнять операции с файлами
   *
   * @note Использует std::atomic<bool> для потокобезопасного доступа
   *       без необходимости блокировки мьютекса
   * @warning Значение может изменяться асинхронно в разных потоках
   */
  std::atomic<bool> connected_{false};

  /**
   * @brief Атомарный флаг состояния мониторинга
   *
   * @details
   * Указывает, активен ли событийный мониторинг директории через FileWatcher.
   * Устанавливается в true при успешном запуске startMonitoring(),
   * сбрасывается в false при вызове stopMonitoring().
   *
   * Используется для:
   * - Предотвращения повторного запуска мониторинга
   * - Проверки состояния в методе isMonitoring()
   * - Корректного завершения в деструкторе
   *
   * Жизненный цикл:
   * 1. Инициализация: `false`
   * 2. startMonitoring(): проверка connected_, создание watcher_, установка
   * `true`
   * 3. stopMonitoring(): остановка watcher_, установка `false`
   *
   * @note Атомарность гарантирует безопасное чтение из разных потоков
   * @warning Изменение значения должно сопровождаться соответствующими
   *          операциями с watcher_
   */
  std::atomic<bool> monitoring_{false};

  /**
   * @brief Мьютекс для синхронизации многопоточного доступа
   *
   * @details
   * Обеспечивает потокобезопасность при выполнении критических операций:
   * - Создание и уничтожение watcher_ в startMonitoring()/stopMonitoring()
   * - Установка callback через setCallback()
   * - Изменение состояния подключения в connect()/disconnect()
   *
   * Защищаемые секции включают:
   * - Модификацию watcher_ и связанных ресурсов
   * - Установку onFileDetected_ callback
   * - Проверку и изменение флагов состояния при необходимости атомарности
   *
   * Использует RAII-идиому через std::lock_guard для автоматического
   * освобождения блокировки при выходе из области видимости.
   *
   * @note Помечен как mutable для использования в const-методах
   * @warning Порядок захвата мьютексов критичен для предотвращения deadlock
   *          при взаимодействии с другими синхронизированными объектами
   */
  mutable std::mutex mutex_;
};