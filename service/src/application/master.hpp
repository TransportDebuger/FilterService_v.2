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
  enum class State { STOPPED, STARTING, RUNNING, RELOADING, FATAL };

  /**
   * @brief Конструктор мастера с инъекцией бизнес-зависимостей.
   * @param[in] logger Диспетчер логирования.
   * @param[in] registry Реестр метрик.
   * @param[in] global_metrics Дескрипторы общих метрик сервиса.
   * @param[in] filter_list_manager Менеджер списков фильтрации.
   */
  Master(std::shared_ptr<stc::logger::ILogger> logger,
         std::shared_ptr<stc::metrics::IMetricsRegistry> registry,
         GlobalMetricsDescriptors global_metrics,
         std::shared_ptr<FilterListManager> filter_list_manager);

  ~Master();

  /**
   * @brief Запускает воркеры на основе собранной конфигурации.
   * @param[in] config Строгий DTO конфигурации.
   * @return true Если запуск прошел успешно.
   */
  bool start(const ApplicationConfiguration& config);

  /// @brief Останавливает все воркеры и освобождает ресурсы.
  void stop() noexcept;

  /**
   * @brief Атомарно перезагружает воркеры с новой конфигурацией.
   * @param[in] new_config Новый строгий DTO конфигурации.
   */
  void reload(const ApplicationConfiguration& new_config);

  /// @brief Проверяет здоровье воркеров и перезапускает упавшие.
  void healthCheck();

  /// @brief Возвращает текущее состояние мастера.
  State getState() const noexcept;

  /// @brief Возвращает количество активных воркеров.
  size_t getWorkerCount() const;

  /// @brief Перезапускает мониторинг у всех активных воркеров.
  void restartAllMonitoring();

 private:
  /**
   * @private
   * @brief Создает и запускает воркеры для указанных источников.
   * @param[in] sources Вектор конфигураций источников.
   * @param[out] target Контейнер для сохранения новых воркеров.
   */
  void spawnWorkers(const std::vector<SourceConfig>& sources,
                    WorkersContainer& target);

  /**
   * @private
   * @brief Останавливает воркеры в указанном контейнере.
   * @param[in] target Контейнер с воркерами для остановки.
   */
  void terminateWorkers(WorkersContainer& target);

  /**
   * @private
   * @brief Получает или создает дескрипторы метрик для источника.
   * @param[in] source_name Имя источника.
   * @return SourceMetricsDescriptors Дескрипторы метрик.
   */
  SourceMetricsDescriptors getOrCreateSourceMetrics(
      const std::string& source_name);

  /** @private
   * @brief Контейнер активных воркеров.*/
  WorkersContainer workers_;
  /** @private
   * @brief Диспетчер логирования.*/
  std::shared_ptr<stc::logger::ILogger> logger_;
  /** @private
   * @brief Реестр метрик.*/
  std::shared_ptr<stc::metrics::IMetricsRegistry> metrics_registry_;
  /** @private
   * @brief Дескрипторы общих метрик сервиса.*/
  GlobalMetricsDescriptors global_metrics_;
  /** @private
   * @brief Менеджер списков фильтрации.*/
  std::shared_ptr<FilterListManager> filter_list_manager_;
  /** @private
   * @brief Кэш дескрипторов детализированных метрик.*/
  std::unordered_map<std::string, SourceMetricsDescriptors>
      source_metrics_cache_;
  /** @private
   * @brief Текущее состояние мастера.*/
  std::atomic<State> state_{State::STOPPED};
};

}  // namespace stc