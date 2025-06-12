#include "../include/worker.hpp"

#include <chrono>
#include <filesystem>
#include <system_error>

#include "stc/compositelogger.hpp"

namespace fs = std::filesystem;

Worker::Worker(const SourceConfig& config) : config_(config), pid_(::getpid()) {
  // Инициализация компонентов
  //   monitor_ = std::make_unique<FileMonitor>(config_.path,
  //   config_.file_mask); processor_ =
  //   std::make_unique<XmlProcessor>(config_.comparison_list);

  stc::CompositeLogger::instance().info(
      "Worker created for source: " + config_.name +
      "Worker" + std::to_string(pid_));
}

Worker::~Worker() {
  stop();
  stc::CompositeLogger::instance().debug(
      "Worker destroyed"
      "Worker" +
      std::to_string(pid_));
}

void Worker::start() {
  if (running_) {
    stc::CompositeLogger::instance().warning(
        "Worker already running"
        "Worker" +
        std::to_string(pid_));
    return;
  }

  running_ = true;
  worker_thread_ = std::thread(&Worker::run, this);

  stc::CompositeLogger::instance().info(
      "Worker started monitoring: " + config_.path + "Worker" +
      std::to_string(pid_));
}

void Worker::stop() {
  if (!running_) return;

  {
    std::lock_guard<std::mutex> lock(state_mutex_);
    running_ = false;
    paused_ = false;
  }

  if (worker_thread_.joinable()) {
    worker_thread_.join();
  }

  stc::CompositeLogger::instance().info(
      "Worker stopped"
      "Worker" +
      std::to_string(pid_));
}

void Worker::pause() {
  std::lock_guard<std::mutex> lock(state_mutex_);
  paused_ = true;
  stc::CompositeLogger::instance().info(
      "Worker paused"
      "Worker" +
      std::to_string(pid_));
}

void Worker::resume() {
  std::lock_guard<std::mutex> lock(state_mutex_);
  paused_ = false;
  stc::CompositeLogger::instance().info(
      "Worker resumed"
      "Worker" +
      std::to_string(pid_));
}

void Worker::run() {
  const std::string worker_tag = "Worker" + std::to_string(pid_);

  try {
    while (running_) {
      // Проверка состояния паузы
      {
        std::unique_lock<std::mutex> lock(state_mutex_);
        if (paused_) {
          std::this_thread::sleep_for(std::chrono::seconds(1));
          continue;
        }
      }

      // Проверка новых файлов
      auto new_files = monitor_->checkNewFiles();
      if (!new_files.empty()) {
        stc::CompositeLogger::instance().debug(
            "Found " + std::to_string(new_files.size()) + " new files " +
            worker_tag);
      }

      // Обработка каждого файла
      for (const auto& file_path : new_files) {
        if (!running_) break;

        try {
          // Обработка XML
          std::string output_path =
              config_.processed_dir + "/" +
              config_.getFilteredFileName(
                  fs::path(file_path).filename().string());

          std::string excluded_path =
              config_.processed_dir + "/" +
              config_.getExcludedFileName(
                  fs::path(file_path).filename().string());

          if (config_.filtering_enabled) {
            processor_->filter(file_path, output_path, excluded_path);
            stc::CompositeLogger::instance().info(
                "Processed file: " + file_path, worker_tag);
          } else {
            fs::rename(file_path, output_path);
            stc::CompositeLogger::instance().info(
                "Moved file without filtering: " + file_path, worker_tag);
          }

          // Перемещение обработанного файла
          if (!config_.processed_dir.empty()) {
            fs::path processed_file = config_.processed_dir;
            processed_file /= fs::path(file_path).filename();
            fs::rename(file_path, processed_file);
          }

        } catch (const std::exception& e) {
          stc::CompositeLogger::instance().error(
              "Failed to process file " + file_path + ": " + e.what() + 
              worker_tag);
        }
      }

      // Пауза между проверками
      std::this_thread::sleep_for(std::chrono::seconds(config_.check_interval));
    }
  } catch (const std::exception& e) {
    stc::CompositeLogger::instance().error(
        "Worker crashed: " + std::string(e.what()) + worker_tag);
    running_ = false;
  }
}