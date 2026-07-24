/**
@file worker.cpp
@brief Реализация рабочего потока обработки файлов.
@version 3.1.0
@date 2026-07-21
*/
#include "../include/worker.hpp"

#include <openssl/evp.h>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <signal.h>

#include "../include/XMLProcessor.hpp"
#include "../include/signal_mask_guard.hpp"

namespace stc {

std::atomic<int> Worker::instanceCounter_{0};

Worker::Worker(const SourceConfig &config,
               std::shared_ptr<stc::logger::ILogger> logger,
               GlobalMetricsDescriptors global_metrics,
               SourceMetricsDescriptors source_metrics)
    : config_(config)
    , logger_(std::move(logger))
    , global_metrics_(std::move(global_metrics))
    , source_metrics_(std::move(source_metrics))
{
    int id = instanceCounter_.fetch_add(1, std::memory_order_relaxed);
    workerTag_ = config_.name + "#" + std::to_string(id);
    try {
        // Инъекция логгера в фабрику адаптеров
        adapter_ = AdapterFactory::instance().createAdapter(config_, logger_);
        if (!adapter_) {
            throw std::runtime_error("Failed to create adapter for type: " + config_.type);
        }
        
        // Событийная модель: адаптер сам находит файлы и вызывает этот callback
        adapter_->setCallback([this](const std::string &filePath) {
            if (running_ && !paused_) {
                processFile(filePath);
            }
        });
        logger_->Info("Worker created for source: " + config_.name + " (type: " + config_.type + "), " + workerTag_);
    } catch (const std::exception &e) {
        logger_->Error("Failed to create worker: " + std::string(e.what()) + ", " + workerTag_);
        throw;
    }
}

Worker::~Worker() {
    try {
        logger_->Debug("Worker destructor called for " + workerTag_);
        stopGracefully();
        logger_->Debug("Worker destroyed, " + workerTag_);
    } catch (...) {
        // Подавляем исключения в деструкторе
    }
}

void Worker::start() {
    std::lock_guard<std::mutex> lock(state_mutex_);
    if (running_) {
        logger_->Warning("Worker already running, " + workerTag_);
        return;
    }
    try {
        SignalMaskGuard guard({SIGINT, SIGTERM});
        validatePaths();
        adapter_->connect();
        if (!adapter_->isConnected()) {
            throw std::runtime_error("Failed to connect to storage");
        }
        adapter_->startMonitoring();
        running_ = true;
        paused_ = false;
        start_time_ = std::chrono::steady_clock::now();
        worker_thread_ = std::thread(&Worker::run, this);
        logger_->Info("Worker started monitoring: " + config_.path + ", " + workerTag_);
    } catch (const std::exception &e) {
        running_ = false;
        logger_->Error("Failed to start worker: " + std::string(e.what()) + ", " + workerTag_);
        throw;
    }
}

void Worker::stop() {
    std::lock_guard<std::mutex> lock(state_mutex_);
    if (!running_) return;
    running_ = false;
    paused_ = false;
    cv_.notify_all();
    if (adapter_) {
        adapter_->stopMonitoring();
        adapter_->disconnect();
    }
    if (worker_thread_.joinable()) {
        worker_thread_.join();
    }
    logger_->Info("Worker stopped, " + workerTag_);
}

void Worker::pause() {
    std::lock_guard<std::mutex> lock(state_mutex_);
    if (!running_ || paused_) return;
    paused_ = true;
    logger_->Info("Worker paused, " + workerTag_);
}

void Worker::resume() {
    std::lock_guard<std::mutex> lock(state_mutex_);
    if (!running_ || !paused_) return;
    paused_ = false;
    cv_.notify_all();
    logger_->Info("Worker resumed, " + workerTag_);
}

void Worker::restart() {
    logger_->Info("Restarting worker, " + workerTag_);
    stop();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    start();
}

void Worker::stopGracefully() {
    logger_->Debug("Worker::stopGracefully() ENTER " + workerTag_);
    if (!running_) return;
    while (processing_) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    stop();
    logger_->Debug("Worker::stopGracefully() EXIT " + workerTag_);
}

bool Worker::isAlive() const noexcept { return running_.load(std::memory_order_relaxed); }
bool Worker::isPaused() const noexcept { return paused_.load(std::memory_order_relaxed); }

void Worker::run() {
    try {
        while (running_) {
            std::unique_lock<std::mutex> lock(state_mutex_);
            if (paused_) {
                cv_.wait(lock, [this] { return !paused_ || !running_; });
                continue;
            }
            lock.unlock();
            
            // Цикл нужен только для проверки paused/running и периодического логирования.
            // Обработка файлов идет асинхронно через callback от adapter_.
            std::this_thread::sleep_for(std::chrono::seconds(config_.check_interval));
            
            if (std::chrono::steady_clock::now() - start_time_ > std::chrono::minutes(1)) {
                logger_->Debug("Worker stats - Avg duration: " + std::to_string(avg_duration_.load()) + "s, " + workerTag_);
            }
        }
    } catch (const std::exception &e) {
        logger_->Error("Worker crashed: " + std::string(e.what()) + ", " + workerTag_);
        running_ = false;
    }
}

void Worker::processFile(const std::string &filePath) {
    if (!running_ || paused_) return;
    processing_ = true;
    
    // 1. Фиксация начала обработки (worker_state = 1)
    if (source_metrics_.worker_state) source_metrics_.worker_state->Set(1.0);
    
    auto start_time = std::chrono::steady_clock::now();
    
    try {
        logger_->Debug("Processing file: " + filePath + ", " + workerTag_);
        if (!fs::exists(filePath)) {
            throw std::runtime_error("File not found: " + filePath);
        }

        std::string fileHash = getFileHash(filePath);
        fs::path inputFile(filePath);
        std::string filename = inputFile.filename().string();
        
        ProcessingResult result;

        if (config_.filtering_enabled) {
            XMLProcessor processor(config_, logger_);
            result = processor.process(filePath);
            
            if (!result.success) {
                // Ошибка обработки
                if (source_metrics_.files_failed) source_metrics_.files_failed->Increment(1.0);
                if (global_metrics_.files_failed) global_metrics_.files_failed->Increment(1.0);
                
                if (result.error_type == ProcessingResult::ErrorType::WRITE) {
                    if (source_metrics_.files_failed_write) source_metrics_.files_failed_write->Increment(1.0);
                } else {
                    if (source_metrics_.files_failed_parse) source_metrics_.files_failed_parse->Increment(1.0);
                }
                
                handleFileError(filePath, "XML processing failed");
                processing_ = false;
                if (source_metrics_.worker_state) source_metrics_.worker_state->Set(0.0);
                return;
            }
            
            // Успешная обработка
            if (source_metrics_.files_processed) source_metrics_.files_processed->Increment(1.0);
            if (global_metrics_.files_processed) global_metrics_.files_processed->Increment(1.0);
            
            if (source_metrics_.records_processed) source_metrics_.records_processed->Increment(static_cast<double>(result.records_processed));
            if (global_metrics_.records_processed) global_metrics_.records_processed->Increment(static_cast<double>(result.records_processed));
            
            if (source_metrics_.records_matched) source_metrics_.records_matched->Increment(static_cast<double>(result.records_matched));
            if (global_metrics_.records_matched) global_metrics_.records_matched->Increment(static_cast<double>(result.records_matched));
            
            if (source_metrics_.bytes_processed) source_metrics_.bytes_processed->Increment(static_cast<double>(result.bytes_processed));
            if (global_metrics_.bytes_processed) global_metrics_.bytes_processed->Increment(static_cast<double>(result.bytes_processed));

            // Перемещение файла
            std::string dst = (fs::path(config_.processed_dir) / config_.getFilteredFileName(filename)).string();
            moveToProcessed(filePath, dst);
            
        } else {
            // Если фильтрация отключена, просто перемещаем файл
            std::string dst = (fs::path(config_.processed_dir) / filename).string();
            moveToProcessed(filePath, dst);
            
            if (source_metrics_.files_processed) source_metrics_.files_processed->Increment(1.0);
            if (global_metrics_.files_processed) global_metrics_.files_processed->Increment(1.0);
            
            result.success = true;
            try { result.bytes_processed = fs::file_size(filePath); } catch (...) { result.bytes_processed = 0; }
        }

        auto end_time = std::chrono::steady_clock::now();
        double duration = std::chrono::duration<double>(end_time - start_time).count();

        // Обновление гистограммы и скользящего среднего
        if (source_metrics_.duration_hist) source_metrics_.duration_hist->Observe(duration);
        if (global_metrics_.duration_hist) global_metrics_.duration_hist->Observe(duration);
        updateAverageDuration(duration);

        // Обновление Unix-timestamp момента завершения
        auto now = std::chrono::system_clock::now();
        double unix_timestamp = static_cast<double>(
            std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count());
        if (source_metrics_.last_file_processed_timestamp) {
            source_metrics_.last_file_processed_timestamp->Set(unix_timestamp);
        }

        logger_->Info("Successfully processed file: " + filePath + " (hash: " + fileHash.substr(0, 8) + "...) in " + std::to_string(duration) + "s, " + workerTag_);

    } catch (const std::exception &e) {
        // Неожиданное исключение
        if (source_metrics_.files_failed) source_metrics_.files_failed->Increment(1.0);
        if (global_metrics_.files_failed) global_metrics_.files_failed->Increment(1.0);
        if (source_metrics_.files_failed_parse) source_metrics_.files_failed_parse->Increment(1.0);
        
        handleFileError(filePath, e.what());
        
        auto end_time = std::chrono::steady_clock::now();
        double duration = std::chrono::duration<double>(end_time - start_time).count();
        if (source_metrics_.duration_hist) source_metrics_.duration_hist->Observe(duration);
        if (global_metrics_.duration_hist) global_metrics_.duration_hist->Observe(duration);
        updateAverageDuration(duration);
    }
    
    processing_ = false;
    // 2. Фиксация завершения обработки (worker_state = 0)
    if (source_metrics_.worker_state) source_metrics_.worker_state->Set(0.0);
}

void Worker::validatePaths() const {
    std::vector<std::string> paths = {config_.processed_dir, config_.bad_dir, config_.excluded_dir};
    for (const auto &path : paths) {
        if (!path.empty() && !fs::exists(path)) {
            try {
                fs::create_directories(path);
                logger_->Info("Created directory: " + path + ", " + workerTag_);
            } catch (const fs::filesystem_error &e) {
                throw std::runtime_error("Cannot create directory " + path + ": " + e.what());
            }
        }
    }
}

std::string Worker::getFileHash(const std::string &filePath) const {
    std::ifstream file(filePath, std::ios::binary);
    if (!file) throw std::runtime_error("Cannot open file for hashing: " + filePath);
    EVP_MD_CTX *mdctx = EVP_MD_CTX_new();
    if (!mdctx) throw std::runtime_error("Failed to create EVP context");
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
    if (len != hash_size) throw std::runtime_error("Invalid SHA256 digest length");
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

void Worker::moveToProcessed(const std::string &filePath, const std::string &processedPath) {
    try {
        fs::path dir = fs::path(processedPath).parent_path();
        if (!fs::exists(dir)) {
            fs::create_directories(dir);
            logger_->Info("Created directory: " + dir.string());
        }
        if (fs::equivalent(fs::path(filePath).root_path(), dir.root_path())) {
            fs::rename(filePath, processedPath);
        } else {
            fs::copy_file(filePath, processedPath, fs::copy_options::overwrite_existing);
            fs::remove(filePath);
        }
        logger_->Debug("Moved file from " + filePath + " to " + processedPath + ", " + workerTag_);
    } catch (const fs::filesystem_error &e) {
        throw std::runtime_error("Failed to move file to processed directory: " + std::string(e.what()));
    }
}

void Worker::handleFileError(const std::string &filePath, const std::string &error) {
    try {
        if (!config_.bad_dir.empty()) {
            fs::path inputFile(filePath);
            std::string filename = inputFile.filename().string();
            std::string badPath = (fs::path(config_.bad_dir) / filename).string();
            if (!fs::exists(config_.bad_dir)) fs::create_directories(config_.bad_dir);
            if (fs::equivalent(fs::path(filePath).root_path(), fs::path(config_.bad_dir).root_path())) {
                fs::rename(filePath, badPath);
            } else {
                fs::copy_file(filePath, badPath, fs::copy_options::overwrite_existing);
                fs::remove(filePath);
            }
            logger_->Warning("Moved failed file to bad directory: " + badPath + ", " + workerTag_);
        }
        logger_->Error("Failed to process file " + filePath + ": " + error + ", " + workerTag_);
    } catch (const std::exception &e) {
        logger_->Error("Failed to handle file error: " + std::string(e.what()) + ", " + workerTag_);
    }
}

void Worker::restartMonitoring() {
    std::lock_guard<std::mutex> lock(state_mutex_);
    if (!adapter_) {
        logger_->Warning("Worker " + config_.name + " has no adapter to restart, " + workerTag_);
        return;
    }
    try {
        if (running_) {
            adapter_->stopMonitoring();
            logger_->Debug("Worker " + config_.name + " monitoring stopped, " + workerTag_);
            adapter_->startMonitoring();
            logger_->Info("Worker " + config_.name + " monitoring restarted, " + workerTag_);
        } else {
            logger_->Warning("Worker " + config_.name + " is not running, cannot restart monitoring, " + workerTag_);
        }
    } catch (const std::exception &e) {
        logger_->Error("Failed to restart monitoring for worker " + config_.name + ": " + std::string(e.what()) + ", " + workerTag_);
        throw;
    }
}

void Worker::updateAverageDuration(double duration) {
    uint64_t n = file_count_.fetch_add(1, std::memory_order_relaxed) + 1;
    double old_avg = avg_duration_.load(std::memory_order_relaxed);
    double new_avg = old_avg + (duration - old_avg) / static_cast<double>(n);
    avg_duration_.store(new_avg, std::memory_order_relaxed);

    if (global_metrics_.duration_avg) global_metrics_.duration_avg->Set(new_avg);
    if (source_metrics_.duration_avg) source_metrics_.duration_avg->Set(new_avg);
}

} // namespace stc