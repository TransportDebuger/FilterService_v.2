/**
 * @file master.hpp
 * @brief Оркестратор рабочих потоков и менеджер жизненного цикла источников.
 * @version 4.0.0
 * @date 2026-07-24
 */
#pragma once

#include <atomic>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <vector>

#include "../domain/DTO/metrics_descriptors.hpp"
#include "../domain/FilterListManager.hpp"
#include "../domain/application_configuration.hpp"
#include "stc/logger/ilogger.hpp"
#include "stc/metrics/imetrics_registry.hpp"
#include "workercontainer.hpp"

namespace stc {

/**
 * @class Master
 * @brief Управляет созданием, перезагрузкой и мониторингом пула воркеров.
 */
class Master {
 public:
  /**
   * @brief Состояние жизненного цикла оркестратора.
   */
  enum class State { 
    STOPPED,    //< Оркестратор остановлен.
    STARTING,   //< Идет процесс запуска воркеров.
    RUNNING,    //< Оркестратор работает в штатном режиме.
    RELOADING,  //< Идет процесс транзакционной перезагрузки.
    FATAL       //< Состояние фатальной ошибки.
  };

  /**
   * @brief Конструктор мастера с инъекцией бизнес-зависимостей.
   *
   * @param[in] logger Диспетчер логирования.
   * @param[in] registry Реестр метрик.
   * @param[in] global_metrics Дескрипторы общих метрик сервиса.
   * @param[in] filter_list_manager Менеджер списков фильтрации.
   * @throw std::invalid_argument Если registry или filter_list_manager
   *        равны nullptr.
   */
  Master(std::shared_ptr<stc::logger::ILogger> logger,
         std::shared_ptr<stc::metrics::IMetricsRegistry> registry,
         GlobalMetricsDescriptors global_metrics,
         std::shared_ptr<FilterListManager> filter_list_manager);
  
  /// @brief Деструктор. Останавливает все воркеры.
  ~Master();

  /**
   * @brief Запускает воркеры на основе собранной конфигурации.
   *
   * @param[in] config Строгий DTO конфигурации.
   * @return bool Флаг успешности инициации запуска.
   * @retval true Запуск прошел успешно, состояние переведено в RUNNING.
   * @retval false Оркестратор уже запущен или произошла ошибка создания.
   */
  bool start(const ApplicationConfiguration& config);

  /// @brief Останавливает все воркеры и освобождает ресурсы.
  void stop() noexcept;

  /**
   * @brief Атомарно перезагружает воркеры с новой конфигурацией.
   *
   * @param[in] new_config Новый строгий DTO конфигурации.
   */
  void reload(const ApplicationConfiguration& new_config);

  /// @brief Проверяет здоровье воркеров и перезапускает упавшие.
  void healthCheck();

  /**
   * @brief Возвращает текущее состояние мастера.
   * @return State Текущее состояние оркестратора.
   */
  State getState() const noexcept;

  /** 
   * @brief Возвращает количество активных воркеров.
   * @return size_t Количество воркеров в контейнере.
   */
  size_t getWorkerCount() const;

  /// @brief Перезапускает мониторинг у всех активных воркеров.
  void restartAllMonitoring();

 private:
  /**
   * @brief Создает и запускает воркеры для указанных источников.
   *
   * @param[in] sources Вектор конфигураций источников.
   * @param[out] target Контейнер для сохранения новых воркеров.
   * @private
   */
  void spawnWorkers(const std::vector<SourceConfig>& sources,
                    WorkersContainer& target);

  /**
   * @brief Останавливает воркеры в указанном контейнере.
   *
   * @param[in] target Контейнер с воркерами для остановки.
   * @private
   */
  void terminateWorkers(WorkersContainer& target);

  /**
   * @brief Получает или создает дескрипторы метрик для источника.
   *
   * @param[in] source_name Имя источника.
   * @return SourceMetricsDescriptors Дескрипторы метрик источника.
   * @private
   */
  SourceMetricsDescriptors getOrCreateSourceMetrics(
      const std::string& source_name);

  /// @brief Контейнер активных воркеров.
  WorkersContainer workers_;
  /// @brief Диспетчер логирования.
  std::shared_ptr<stc::logger::ILogger> logger_;
  /// @brief Реестр метрик.
  std::shared_ptr<stc::metrics::IMetricsRegistry> metrics_registry_;
  /// @brief Дескрипторы общих метрик сервиса.
  GlobalMetricsDescriptors global_metrics_;
  /// @brief Менеджер списков фильтрации.
  std::shared_ptr<FilterListManager> filter_list_manager_;
  /// @brief Кэш дескрипторов детализированных метрик.
  std::unordered_map<std::string, SourceMetricsDescriptors>
      source_metrics_cache_;
  /// @brief Текущее состояние мастера.
  std::atomic<State> state_{State::STOPPED};
};

}  // namespace stc