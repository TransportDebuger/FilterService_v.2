/**
 * @file Worker.cpp
 * @brief Реализация класса Worker для обработки файлов
 */

#include "../include/worker.hpp"

#include <openssl/evp.h>

#include <fstream>
#include <iomanip>
#include <sstream>

#include "../include/XMLProcessor.hpp"

std::atomic<int> Worker::instanceCounter_{0};

Worker::Worker(const SourceConfig &config) : config_(config), pid_(::getpid()) {
  int id = instanceCounter_.fetch_add(1, std::memory_order_relaxed);
  workerTag_ = config_.name + "#" + std::to_string(id);

  try {
    // Создание адаптера через фабрику
    adapter_ = AdapterFactory::instance().createAdapter(config_);

    if (!adapter_) {
      throw std::runtime_error("Failed to create adapter for type: " +
                               config_.type);
    }

    // Установка callback для обработки новых файлов
    adapter_->setCallback([this](const std::string &filePath) {
      if (running_ && !paused_) {
        processFile(filePath);
      }
    });

    stc::CompositeLogger::instance().info(
        "Worker created for source: " + config_.name +
        " (type: " + config_.type + "), " + workerTag_);

  } catch (const std::exception &e) {
    stc::CompositeLogger::instance().error(
        "Failed to create worker: " + std::string(e.what()) + ", " +
        workerTag_);
    throw;
  }
}

Worker::~Worker() {
  try {
    stopGracefully();
    stc::CompositeLogger::instance().debug("Worker destroyed, " + workerTag_);
  } catch (...) {
    // Подавляем исключения в деструкторе
  }
}

void Worker::start() {
  std::lock_guard<std::mutex> lock(state_mutex_);

  if (running_) {
    stc::CompositeLogger::instance().warning("Worker already running, " +
                                             workerTag_);
    return;
  }

  try {
    // Валидация путей
    validatePaths();

    // Подключение к хранилищу
    adapter_->connect();

    if (!adapter_->isConnected()) {
      throw std::runtime_error("Failed to connect to storage");
    }

    // Запуск мониторинга
    adapter_->startMonitoring();

    // Запуск рабочего потока
    running_ = true;
    paused_ = false;
    start_time_ = std::chrono::steady_clock::now();
    worker_thread_ = std::thread(&Worker::run, this);

    stc::CompositeLogger::instance().info(
        "Worker started monitoring: " + config_.path + ", " + workerTag_);

    stc::MetricsCollector::instance().incrementCounter("worker_started");

  } catch (const std::exception &e) {
    running_ = false;
    stc::CompositeLogger::instance().error(
        "Failed to start worker: " + std::string(e.what()) + ", " + workerTag_);
    throw;
  }
}

void Worker::stop() {
  std::lock_guard<std::mutex> lock(state_mutex_);

  if (!running_) return;

  running_ = false;
  paused_ = false;
  cv_.notify_all();

  // Останавливаем мониторинг
  if (adapter_) {
    adapter_->stopMonitoring();
    adapter_->disconnect();
  }

  // Ожидаем завершения потока
  if (worker_thread_.joinable()) {
    worker_thread_.join();
  }

  stc::CompositeLogger::instance().info("Worker stopped, " + workerTag_);
}

void Worker::pause() {
  std::lock_guard<std::mutex> lock(state_mutex_);

  if (!running_ || paused_) return;

  paused_ = true;
  stc::CompositeLogger::instance().info("Worker paused, " + workerTag_);
}

void Worker::resume() {
  std::lock_guard<std::mutex> lock(state_mutex_);

  if (!running_ || !paused_) return;

  paused_ = false;
  cv_.notify_all();

  stc::CompositeLogger::instance().info("Worker resumed, " + workerTag_);
}

void Worker::restart() {
  stc::CompositeLogger::instance().info("Restarting worker, " + workerTag_);

  stop();
  std::this_thread::sleep_for(std::chrono::milliseconds(100));
  start();
}

void Worker::stopGracefully() {
  if (!running_) return;

  // Ожидаем завершения текущей обработки
  while (processing_) {
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }

  stop();
}

bool Worker::isAlive() const noexcept {
  return running_.load(std::memory_order_relaxed);
}

bool Worker::isRunning() const noexcept {
  return running_.load(std::memory_order_relaxed);
}

bool Worker::isPaused() const noexcept {
  return paused_.load(std::memory_order_relaxed);
}

void Worker::run() {
  try {
    while (running_) {
      // Проверка состояния паузы
      std::unique_lock<std::mutex> lock(state_mutex_);
      if (paused_) {
        cv_.wait(lock, [this] { return !paused_ || !running_; });
        continue;
      }
      lock.unlock();

      // Основной цикл уже управляется через callback от адаптера
      std::this_thread::sleep_for(std::chrono::seconds(config_.check_interval));

      // Проверка статистики
      if (std::chrono::steady_clock::now() - start_time_ >
          std::chrono::minutes(1)) {
        stc::CompositeLogger::instance().debug(
            "Worker stats - Processed: " + std::to_string(files_processed_) +
            ", Failed: " + std::to_string(files_failed_) + ", " + workerTag_);
      }
    }
  } catch (const std::exception &e) {
    stc::CompositeLogger::instance().error(
        "Worker crashed: " + std::string(e.what()) + ", " + workerTag_);
    running_ = false;
  }
}

void Worker::processFile(const std::string &filePath) {
  if (!running_ || paused_)
    return;  // Если воркер остановлен или на паузе, ничего не делаем

  processing_ = true;
  auto start_time = std::chrono::steady_clock::now();

  try {
    stc::CompositeLogger::instance().debug(
        "Processing file: " + filePath + ", " +
        workerTag_  // Логируем начало обработки
    );

    // Проверка существования файла
    if (!fs::exists(filePath)) {
      throw std::runtime_error("File not found: " + filePath);
    }

    // Вычисление хеша для возможности дедупликации
    std::string fileHash = getFileHash(filePath);
    // Определяем имя и путь назначения
    fs::path inputFile(filePath);
    std::string filename = inputFile.filename().string();
    std::string processedPath =
        (fs::path(config_.processed_dir) / filename).string();

    if (config_.filtering_enabled) {
      // Обработка с фильтрацией
      std::string filteredPath = getFilteredFilePath(filePath);
      std::string excludedPath = (fs::path(config_.excluded_dir) /
                                  config_.getExcludedFileName(filename))
                                     .string();

      // Здесь должна быть логика фильтрации через процессор
      XMLProcessor processor(config_);
      bool result = processor.process(filePath);
      if (!result) {
        // Если парсинг или фильтрация не удалась — перемещаем в bad_dir
        files_failed_++;
        stc::MetricsCollector::instance().incrementCounter("files_failed");
        handleFileError(filePath, "XML processing failed");
        processing_ = false;
        return;
      }

      fs::remove(filePath);
    } else {
      // Простое перемещение без фильтрации
      std::string dst =
          (fs::path(config_.processed_dir) / fs::path(filePath).filename())
              .string();
      moveToProcessed(filePath, dst);
    }

    // Записываем метрики времени обработки
    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);

    stc::MetricsCollector::instance().recordTaskTime("file_processing_time",
                                                     duration);

    files_processed_++;

    stc::CompositeLogger::instance().info(
        "Successfully processed file: " + filePath +
        " (hash: " + fileHash.substr(0, 8) + "...) in " +
        std::to_string(duration.count()) + "ms, " + workerTag_);

  } catch (const std::exception &e) {
    files_failed_++;
    handleFileError(filePath, e.what());

    stc::MetricsCollector::instance().incrementCounter("files_failed");
  }

  processing_ = false;
}

void Worker::validatePaths() const {
  std::vector<std::string> paths = {config_.processed_dir, config_.bad_dir,
                                    config_.excluded_dir};

  for (const auto &path : paths) {
    if (!path.empty() && !fs::exists(path)) {
      try {
        fs::create_directories(path);
        stc::CompositeLogger::instance().info("Created directory: " + path +
                                              ", " + workerTag_);
      } catch (const fs::filesystem_error &e) {
        throw std::runtime_error("Cannot create directory " + path + ": " +
                                 e.what());
      }
    }
  }
}

std::string Worker::getFileHash(const std::string &filePath) const {
  std::ifstream file(filePath, std::ios::binary);
  if (!file) {
    throw std::runtime_error("Cannot open file for hashing: " + filePath);
  }

  EVP_MD_CTX *mdctx = EVP_MD_CTX_new();
  if (!mdctx) {
    throw std::runtime_error("Failed to create EVP context");
  }

  // Используем EVP для получения размера хеша
  const EVP_MD *md = EVP_sha256();
  const size_t hash_size = EVP_MD_size(md);

  if (EVP_DigestInit_ex(mdctx, md, nullptr) != 1) {
    EVP_MD_CTX_free(mdctx);
    throw std::runtime_error("Failed to initialize SHA256 digest");
  }

  char buffer[8192];
  while (file.read(buffer, sizeof(buffer)) || file.gcount() > 0) {
    if (EVP_DigestUpdate(mdctx, buffer, file.gcount()) != 1) {
      EVP_MD_CTX_free(mdctx);
      throw std::runtime_error("Failed to update SHA256 digest");
    }
  }

  unsigned char hash[EVP_MAX_MD_SIZE];
  unsigned int len = 0;
  if (EVP_DigestFinal_ex(mdctx, hash, &len) != 1) {
    EVP_MD_CTX_free(mdctx);
    throw std::runtime_error("Failed to finalize SHA256 digest");
  }

  EVP_MD_CTX_free(mdctx);

  // Проверяем размер через EVP
  if (len != hash_size) {
    throw std::runtime_error("Invalid SHA256 digest length");
  }

  std::stringstream ss;
  ss << std::hex << std::setfill('0');
  for (unsigned int i = 0; i < len; i++) {
    ss << std::setw(2) << static_cast<unsigned>(hash[i]);
  }

  return ss.str();
}

std::string Worker::getFilteredFilePath(const std::string &originalPath) const {
  fs::path inputFile(originalPath);
  std::string filename = inputFile.filename().string();
  std::string filteredName = config_.getFilteredFileName(filename);
  return (fs::path(config_.processed_dir) / filteredName).string();
}

void Worker::moveToProcessed(const std::string &filePath,
                             const std::string &processedPath) {
  try {
    // Создаем директорию если не существует
    fs::path dir = fs::path(processedPath).parent_path();
    if (!fs::exists(dir)) {
      fs::create_directories(dir);
      stc::CompositeLogger::instance().info("Created directory: " +
                                            dir.string());
    }

    // Перемещаем файл
    if (fs::equivalent(fs::path(filePath).root_path(), dir.root_path())) {
      fs::rename(filePath, processedPath);
    } else {
      fs::copy_file(filePath, processedPath,
                    fs::copy_options::overwrite_existing);
      fs::remove(filePath);
    }

    stc::CompositeLogger::instance().debug("Moved file from " + filePath +
                                           " to " + processedPath + ", " +
                                           workerTag_);

  } catch (const fs::filesystem_error &e) {
    throw std::runtime_error("Failed to move file to processed directory: " +
                             std::string(e.what()));
  }
}

void Worker::handleFileError(const std::string &filePath,
                             const std::string &error) {
  try {
    if (!config_.bad_dir.empty()) {
      fs::path inputFile(filePath);
      std::string filename = inputFile.filename().string();
      std::string badPath = (fs::path(config_.bad_dir) / filename).string();

      // Создаем директорию для ошибочных файлов
      if (!fs::exists(config_.bad_dir)) {
        fs::create_directories(config_.bad_dir);
      }

      // Перемещаем файл в bad_dir
      if (fs::equivalent(fs::path(filePath).root_path(),
                         fs::path(config_.bad_dir).root_path())) {
        fs::rename(filePath, badPath);
      } else {
        fs::copy_file(filePath, badPath, fs::copy_options::overwrite_existing);
        fs::remove(filePath);
      }

      stc::CompositeLogger::instance().warning(
          "Moved failed file to bad directory: " + badPath + ", " + workerTag_);
    }

    stc::CompositeLogger::instance().error("Failed to process file " +
                                           filePath + ": " + error + ", " +
                                           workerTag_);

  } catch (const std::exception &e) {
    stc::CompositeLogger::instance().error(
        "Failed to handle file error: " + std::string(e.what()) + ", " +
        workerTag_);
  }
}