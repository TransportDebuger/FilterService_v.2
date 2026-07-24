/**
@file localstorageadapter.cpp
@brief Реализация адаптера локальной файловой системы.
@version 2.1.0
@date 2026-07-24
*/
#include "localstorageadapter.hpp"
#include <algorithm>
#include <fstream>
#include <regex>
#include <stdexcept>

namespace fs = std::filesystem;

namespace stc {

LocalStorageAdapter::LocalStorageAdapter(const SourceConfig& config,
                                         std::shared_ptr<stc::logger::ILogger> logger)
    : config_(config) {
    logger_ = std::move(logger);
    validatePath(config_.path);
    if (config_.file_mask.empty()) {
        throw std::invalid_argument("File mask cannot be empty");
    }
    if (logger_) {
        logger_->Info("LocalStorageAdapter created for path: " + config_.path);
    }
}

LocalStorageAdapter::~LocalStorageAdapter() {
    try {
        stopMonitoring();
        disconnect();
        if (logger_) logger_->Debug("LocalStorageAdapter destroyed");
    } catch (...) {
        // Подавляем исключения в деструкторе
    }
}

std::vector<std::string> LocalStorageAdapter::listFiles(const std::string& path) {
    std::vector<std::string> files;
    try {
        if (!fs::exists(path) || !fs::is_directory(path)) {
            if (logger_) logger_->Warning("Directory does not exist or is not accessible: " + path);
            return files;
        }
        for (const auto& entry : fs::directory_iterator(path)) {
            if (entry.is_regular_file()) {
                std::string filename = entry.path().filename().string();
                if (matchesFileMask(filename)) {
                    files.push_back(entry.path().string());
                }
            }
        }
        if (logger_) logger_->Debug("Found " + std::to_string(files.size()) + " files in: " + path);
    } catch (const fs::filesystem_error& e) {
        if (logger_) logger_->Error("Filesystem error in listFiles: " + std::string(e.what()));
        throw std::runtime_error("Failed to list files: " + std::string(e.what()));
    }
    return files;
}

void LocalStorageAdapter::downloadFile(const std::string& remotePath, const std::string& localPath) {
    validatePath(remotePath);
    validatePath(localPath);
    try {
        if (!fs::exists(remotePath)) {
            throw std::invalid_argument("Source file does not exist: " + remotePath);
        }
        fs::path localDir = fs::path(localPath).parent_path();
        if (!localDir.empty()) fs::create_directories(localDir);
        fs::copy_file(remotePath, localPath, fs::copy_options::overwrite_existing);
        if (logger_) logger_->Info("File copied from " + remotePath + " to " + localPath);
    } catch (const fs::filesystem_error& e) {
        if (logger_) logger_->Error("Failed to copy file: " + std::string(e.what()));
        throw std::ios_base::failure("File copy failed: " + std::string(e.what()));
    }
}

void LocalStorageAdapter::upload(const std::string& localPath, const std::string& remotePath) {
    validatePath(localPath);
    validatePath(remotePath);
    try {
        if (!fs::exists(localPath)) {
            throw std::invalid_argument("Local file does not exist: " + localPath);
        }
        fs::path remoteDir = fs::path(remotePath).parent_path();
        if (!remoteDir.empty()) fs::create_directories(remoteDir);
        fs::copy_file(localPath, remotePath, fs::copy_options::overwrite_existing);
        if (logger_) logger_->Info("File uploaded from " + localPath + " to " + remotePath);
    } catch (const fs::filesystem_error& e) {
        if (logger_) logger_->Error("Failed to upload file: " + std::string(e.what()));
        throw std::runtime_error("File upload failed: " + std::string(e.what()));
    }
}

void LocalStorageAdapter::connect() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (connected_) {
        if (logger_) logger_->Warning("Already connected");
        return;
    }
    try {
        ensurePathExists();
        connected_ = true;
        if (logger_) logger_->Info("Connected to local storage: " + config_.path);
    } catch (const std::exception& e) {
        if (logger_) logger_->Error("Connection failed: " + std::string(e.what()));
        throw std::runtime_error("Failed to connect: " + std::string(e.what()));
    }
}

void LocalStorageAdapter::disconnect() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!connected_) return;
    stopMonitoring();
    connected_ = false;
    if (logger_) logger_->Info("Disconnected from local storage");
}

bool LocalStorageAdapter::isConnected() const noexcept { 
    return connected_.load(); 
}

void LocalStorageAdapter::startMonitoring() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!connected_) throw std::runtime_error("Cannot start monitoring: not connected");
    if (monitoring_) {
        if (logger_) logger_->Warning("Monitoring already started");
        return;
    }
    try {
        auto strategy = resolveStrategy();
        auto callback = [this](stc::fs::IDirectoryMonitor::Event event, const std::string& filePath) {
            handleFileEvent(event, filePath);
        };

        monitor_ = stc::fs::DirectoryMonitor::CreateWithStrategy(
            strategy,
            config_.path,
            callback,
            config_.check_interval
        );
        
        monitor_->Start();
        monitoring_ = true;
        if (logger_) logger_->Info("Started monitoring [" + config_.monitoring_strategy + "]: " + config_.path);
    } catch (const std::exception& e) {
        if (logger_) logger_->Error("Failed to start monitoring: " + std::string(e.what()));
        throw std::runtime_error("Monitoring start failed: " + std::string(e.what()));
    }
}

void LocalStorageAdapter::stopMonitoring() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!monitoring_) return;
    if (monitor_) {
        monitor_->Stop();
        monitor_.reset();
    }
    monitoring_ = false;
    if (logger_) logger_->Info("Stopped monitoring");
}

bool LocalStorageAdapter::isMonitoring() const noexcept { 
    return monitoring_.load(); 
}

void LocalStorageAdapter::setCallback(FileDetectedCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    onFileDetected_ = std::move(callback);
}

void LocalStorageAdapter::ensurePathExists() {
    if (!fs::exists(config_.path)) {
        try {
            fs::create_directories(config_.path);
            if (logger_) logger_->Info("Created directory: " + config_.path);
        } catch (const fs::filesystem_error& e) {
            throw std::runtime_error("Cannot create directory " + config_.path + ": " + e.what());
        }
    }
    if (!fs::is_directory(config_.path)) {
        throw std::runtime_error("Path is not a directory: " + config_.path);
    }
}

void LocalStorageAdapter::handleFileEvent(stc::fs::IDirectoryMonitor::Event event, const std::string& filePath) {
    try {
        if (event == stc::fs::IDirectoryMonitor::Event::Created) {
            std::string filename = fs::path(filePath).filename().string();
            if (matchesFileMask(filename)) {
                if (logger_) logger_->Debug("New file detected: " + filePath);
                if (onFileDetected_) onFileDetected_(filePath);
            }
        }
    } catch (const std::exception& e) {
        if (logger_) logger_->Error("Error handling file event: " + std::string(e.what()));
    }
}

bool LocalStorageAdapter::matchesFileMask(const std::string& filename) const {
    try {
        std::string pattern;
        pattern.reserve(config_.file_mask.size() * 2);
        const std::string special_chars = R"(\^$.|?*+()[]{})-)";
        for (size_t i = 0; i < config_.file_mask.size(); ++i) {
            char c = config_.file_mask[i];
            if (c == '*') pattern += ".*";
            else if (c == '?') pattern += '.';
            else if (special_chars.find(c) != std::string::npos) {
                pattern += '\\';
                pattern += c;
            } else {
                pattern += c;
            }
        }
        pattern = "^" + pattern + "$";
        std::regex mask_regex(pattern, std::regex_constants::icase);
        return std::regex_match(filename, mask_regex);
    } catch (const std::regex_error& e) {
        if (logger_) logger_->Warning("Invalid file mask regex: '" + config_.file_mask + "', error: " + e.what());
        return true;
    }
}

stc::fs::DirectoryMonitor::MonitoringStrategy LocalStorageAdapter::resolveStrategy() const {
    if (config_.monitoring_strategy == "inotify") {
        return stc::fs::DirectoryMonitor::MonitoringStrategy::Inotify;
    }
    if (config_.monitoring_strategy == "polling") {
        return stc::fs::DirectoryMonitor::MonitoringStrategy::Polling;
    }
    return stc::fs::DirectoryMonitor::MonitoringStrategy::Auto;
}

} // namespace stc