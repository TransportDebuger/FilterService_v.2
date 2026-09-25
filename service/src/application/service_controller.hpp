/**
@file service_controller.hpp
@brief Класс управления жизненным циклом сервиса (Composition Root).
@version 4.0.0
@date 2026-07-24
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
#include "argumentparser.hpp"
#include "master.hpp"
#include "stc/logger/ilogger.hpp"
#include "stc/metrics/imetrics_registry.hpp"
#include "stc/signals/signal_router.hpp"
#include "stc/signals/system_calls.hpp"

namespace stc {

/**
@class ServiceController
@brief Управляет запуском, конфигурацией и жизненным циклом сервиса.
*/
class ServiceController {
 public:
  /// @brief Основная точка входа сервиса.
  int Run(int argc, char** argv);

  /// @brief Формирует текстовый снимок метрик.
  [[nodiscard]] std::string GetMetricsPayload() const;

 private:
  /// @private Инициализирует SignalRouter и Master.
  void Initialize(const ApplicationConfiguration& config);
  /// @private Регистрирует общие метрики.
  void RegisterGlobalMetrics();
  /// @private Главный цикл работы сервиса.
  void MainLoop();
  /// @private Обработчик завершения работы.
  void HandleShutdown();
  /// @private Обработчик сигнала SIGHUP.
  void HandleSighup();
  /// @private Выводит справочную информацию.
  void PrintHelp();
  /// @private Выводит информацию о версии.
  void PrintVersion();

  /// @private Менеджер PID-файла.
  std::unique_ptr<PidFileManager> pid_file_mgr_;
  /// @private Диспетчер логирования.
  std::shared_ptr<stc::logger::ILogger> logger_;
  /// @private Оркестратор воркеров.
  std::unique_ptr<Master> master_;
  /// @private Маршрутизатор сигналов.
  std::unique_ptr<stc::signals::SignalRouter> signal_router_;
  /// @private Реестр метрик.
  std::shared_ptr<stc::metrics::IMetricsRegistry> metrics_registry_;
  /// @private Менеджер списков фильтрации.
  std::shared_ptr<stc::FilterListManager> filter_list_manager_;
  /// @private Сервис управления конфигурацией.
  std::unique_ptr<ConfigurationService> config_service_;

  /// @private Дескрипторы общих метрик.
  GlobalMetricsDescriptors global_metrics_;

  /// @private Флаг состояния основного цикла.
  std::atomic<bool> running_{false};
  /// @private Мьютекс для синхронизации.
  std::mutex mtx_;
  /// @private Условная переменная.
  std::condition_variable cv_;
  /// @private Флаг запроса завершения работы.
  std::atomic<bool> shutdown_requested_{false};
};

}  // namespace stc