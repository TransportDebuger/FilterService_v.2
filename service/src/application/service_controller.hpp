/**
 * @file service_controller.hpp
 * @brief Корень композиции и контроллер жизненного цикла сервиса.
 * @version 4.1.0
 * @date 2026-07-24
 */
#pragma once

#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <string>

#include "../domain/DTO/metrics_descriptors.hpp"
#include "../domain/FilterListManager.hpp"
#include "../domain/application_configuration.hpp"
#include "../infrastructure/configuration/config_reload_transaction.hpp"
#include "../infrastructure/configuration/configuration_service.hpp"
#include "../infrastructure/system/pid_file_manager.hpp"
#include "../infrastructure/metrics/metrics_http_server.hpp"
#include "argumentparser.hpp"
#include "master.hpp"
#include "stc/logger/ilogger.hpp"
#include "stc/metrics/imetrics_registry.hpp"
#include "stc/signals/signal_router.hpp"
#include "stc/signals/system_calls.hpp"

namespace stc {

/**
 * @class ServiceController
 * @brief Управляет запуском, конфигурацией и жизненным циклом сервиса.
 */
class ServiceController {
 public:
  /**
   * @brief Основная точка входа сервиса.
   *
   * @param[in] argc Количество аргументов командной строки.
   * @param[in] argv Массив строк аргументов командной строки.
   * @return int Код завершения процесса (EXIT_SUCCESS или EXIT_FAILURE).
   */
  int Run(int argc, char** argv);

  /**
   * @brief Формирует текстовый снимок метрик в формате Prometheus.
   *
   * @return std::string Текстовый дамп метрик.
   */
  [[nodiscard]] std::string GetMetricsPayload() const;

 private:
  /**
   * @brief Инициализирует SignalRouter и Master.
   *
   * @param[in] config Строгий DTO конфигурации.
   * @private
   */
  void Initialize(const ApplicationConfiguration& config);

  /**
   * @brief Регистрирует общие метрики в реестре.
   * @private
   */
  void RegisterGlobalMetrics();
  
  /**
   * @brief Главный цикл работы сервиса.
   * @private
   */
  void MainLoop();

  /**
   * @brief Обработчик завершения работы (Graceful Shutdown).
   * @private
   */
  void HandleShutdown();

  /**
   * @brief Обработчик сигнала SIGHUP (транзакционная перезагрузка).
   * @private
   */
  void HandleSighup();

  /**
   * @brief Выводит справочную информацию в stdout.
   * @private
   */
  void PrintHelp();

  /**
   * @brief Выводит информацию о версии в stdout.
   * @private
   */
  void PrintVersion();

  /// Менеджер PID-файла.
  std::unique_ptr<PidFileManager> pid_file_mgr_;
  /// Диспетчер логирования.
  std::shared_ptr<stc::logger::ILogger> logger_;
  /// Оркестратор воркеров.
  std::unique_ptr<Master> master_;
  /// Маршрутизатор сигналов.
  std::unique_ptr<stc::signals::SignalRouter> signal_router_;
  /// Реестр метрик.
  std::shared_ptr<stc::metrics::IMetricsRegistry> metrics_registry_;
  /// Менеджер списков фильтрации.
  std::shared_ptr<stc::FilterListManager> filter_list_manager_;
  /// Сервис управления конфигурацией.
  std::unique_ptr<ConfigurationService> config_service_;
  /// HTTP-сервер для экспозиции метрик.
  std::unique_ptr<MetricsHttpServer> metrics_server_;
  /// Дескрипторы общих метрик.
  GlobalMetricsDescriptors global_metrics_;
  /// Флаг состояния основного цикла.
  std::atomic<bool> running_{false};
  /// Мьютекс для синхронизации.
  std::mutex mtx_;
  /// Условная переменная.
  std::condition_variable cv_;
  /// Флаг запроса завершения работы.
  std::atomic<bool> shutdown_requested_{false};
};

}  // namespace stc