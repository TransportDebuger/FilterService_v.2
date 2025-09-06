/**
 * @file filewatcher.hpp
 * @brief Класс FileWatcher для мониторинга изменений в файловой системе
 * @author Artem Ulyanov
 * @company STC Ltd.
 * @date June 2025
 *
 * @details
 * FileWatcher реализует кроссплатформенный мониторинг каталога:
 * на Windows используется ReadDirectoryChangesW, на Linux – inotify.
 * При потере подключения (например, SMB) выполняется автоматическая попытка
 * переподключения с логированием событий восстановления или неудачи.
 *
 * @note
 * Для Windows требуется заголовок <windows.h>, для Linux – <sys/inotify.h>
 * @warning
 * Необходимо убедиться в наличии прав доступа к каталогу, иначе возникнут
 * исключения при инициализации inotify или CreateFileA.
 */
#pragma once

#include <atomic>
#include <functional>
#include <string>
#include <thread>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/inotify.h>
#include <unistd.h>
#endif

/**
 * @class FileWatcher
 * @brief Монитор каталога с уведомлениями о событиях файловой системы
 *
 * @ingroup Core
 *
 * @details
 * FileWatcher отслеживает создание, удаление, модификацию и переименование
 * файлов в указанной директории. При любом событии вызывает пользовательский
 * Callback с типом Event и полным путём к файлу.
 */
class FileWatcher {
 public:
  /**
   * @enum Event
   * @brief Типы событий файловой системы
   *
   * @details
   * Перечисление сигнализирует о четырёх видах изменений:
   * - Created — файл создан
   * - Deleted — файл удалён
   * - Modified — файл изменён
   * - Renamed  — файл переименован
   *
   * @ingroup Core
   */
  enum class Event { Created, Deleted, Modified, Renamed };

  /**
   * @typedef Callback
   * @brief Тип функции-обработчика событий
   *
   * @details
   * Функция принимает два параметра:
   * - Event event — тип произошедшего события (in)
   * - const std::string &path — полный путь к затронутому файлу (in)
   *
   * @ingroup Core
   */
  using Callback = std::function<void(Event, const std::string &)>;

  /**
     * @brief Конструктор FileWatcher
     *
     * @param[in] path Путь к директории для мониторинга (in)
     * @param[in] callback Функция-обработчик событий (in)
     *
     * @details
     * Сохраняет абсолютный путь к каталогу и callback. Не начинает мониторинг
     * до вызова start().
     *
     * @throw std::runtime_error Если каталог не существует или недоступен
     *
     * @note Используйте std::filesystem::absolute() для нормализации пути
     * @warning Параметр path должен указывать на существующую директорию
     *
     * @code
     FileWatcher watcher("/var/log", [](FileWatcher::Event ev, const
     std::string& p){ std::cout << "Event: " << static_cast<int>(ev) << " on "
     << p << std::endl;
     });
     @endcode
     */
  FileWatcher(const std::string &path, Callback callback);

  /**
   * @brief Деструктор FileWatcher
   *
   * @details
   * Вызывает stop() для корректного завершения потока мониторинга и
   * освобождения ресурсов.
   *
   * @note Не бросает исключений
   */
  ~FileWatcher();

  /**
     * @brief Запускает мониторинг директории
     *
     * @details
     * Инициализирует inotify (Linux) или CreateFileA (Windows), создаёт
     * фоновый поток run(), помечает running_ = true.
     *
     * @throw std::runtime_error При ошибках инициализации inotify или
     CreateFileA
     *
     * @note Вызов start() при running_ == true игнорируется
     * @warning Запуск без наличия прав может привести к исключению
     *
     * @code
     watcher.start();
     @endcode
     */
  void start();

  /**
     * @brief Останавливает мониторинг
     *
     * @details
     * Устанавливает running_ = false, дожидается завершения потока join(),
     * удаляет watchDescriptor_ (Linux) или закрывает dirHandle_ (Windows).
     *
     * @note noexcept, не бросает исключений
     * @warning После stop() можно повторно вызвать start() для возобновления
     *
     * @code
     watcher.stop();
     @endcode
     */
  void stop();

 private:
  /**
   * @brief Функция фонового потока для обработки событий
   *
   * @details
   * В цикле while(running_) читает события через ReadDirectoryChangesW
   * или read(inotifyFd_), обрабатывает ошибки подключения и выполняет
   * переподключение с задержками при необходимости.
   *
   * @note Вызывается через std::thread(&FileWatcher::run, this)
   * @warning Все исключения внутри ловятся, чтобы поток не упал
   */
  void run();

  /**
   * @brief Абсолютный путь к мониторимой директории
   *
   * @details
   * Сохраняет преобразованный через std::filesystem::absolute() путь,
   * на который настроен мониторинг. Используется для определения корня
   * отслеживаемого файлового дерева и формирования полного пути для callback.
   *
   * @note Значение инициализируется в конструкторе и не изменяется
   * впоследствии.
   * @warning Неверный или несуществующий путь приводит к исключению в
   * конструкторе.
   */
  std::string path_;

  /**
   * @brief Функция-обработчик событий файловой системы
   *
   * @details
   * Пользовательский callback типа std::function<void(Event, const
   * std::string&)>, который вызывается при каждом событии (Created, Deleted,
   * Modified, Renamed). Передаёт тип события и полный путь к файлу.
   *
   * @note Должна быть задана до вызова start() и оставаться валидной на
   * протяжении всего времени жизни FileWatcher.
   * @warning Callback обязан быть потокобезопасным и работать быстро, чтобы не
   * блокировать run().
   */
  Callback callback_;

  /**
   * @brief Флаг, указывающий на активность мониторинга
   *
   * @details
   * std::atomic<bool>, устанавливается в true при запуске start() и
   * сбрасывается в stop(). Используется в цикле run() для определения, следует
   * ли продолжать мониторинг.
   *
   * @note Гарантирует безопасный доступ из разных потоков без дополнительной
   * синхронизации.
   */
  std::atomic<bool> running_{false};

  /**
   * @brief Фоновый поток для выполнения функции run()
   *
   * @details
   * При старте FileWatcher создаёт thread_, который исполняет
   * метод run() до тех пор, пока running_ == true. После stop()
   * происходит корректный join() этого потока.
   *
   * @warning Должен быть join() перед разрушением FileWatcher, чтобы избежать
   *          незавершённых потоков.
   */
  std::thread thread_;

#ifdef _WIN32

  /**
   * @brief Дескриптор директории для Windows API
   *
   * @details
   * HANDLE, получаемый через CreateFileA() с флагами FILE_LIST_DIRECTORY и
   * OVERLAPPED. Используется ReadDirectoryChangesW для получения уведомлений об
   * изменениях.
   *
   * @note Инициализируется в start() и закрывается в stop() через
   * CloseHandle().
   * @warning INVALID_HANDLE_VALUE означает сбой при открытии директории.
   */
  HANDLE dirHandle_ = INVALID_HANDLE_VALUE;
#else
  /**
   * @brief Файловый дескриптор inotify
   *
   * @details
   * int, получаемый в start() через inotify_init1(IN_NONBLOCK).
   * Используется для чтения событий через read().
   *
   * @note Должен быть >= 0 при успешной инициализации.
   * @warning Отрицательное значение указывает на ошибку inotify_init1().
   */
  int inotifyFd_ = -1;

  /**
   * @brief Дескриптор watch для inotify_add_watch
   *
   * @details
   * int, идентификатор наблюдения за path_.
   * Используется для удаления наблюдения в stop() через inotify_rm_watch().
   *
   * @note Должен быть >= 0 при успешном добавлении watch.
   * @warning Отрицательное значение указывает на ошибку inotify_add_watch().
   */
  int watchDescriptor_ = -1;
#endif
};