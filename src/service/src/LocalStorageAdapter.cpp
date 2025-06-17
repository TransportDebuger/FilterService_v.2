/**
 * @file LocalStorageAdapter.cpp
 * @brief Реализация адаптера локальной файловой системы
 */

#include "../include/LocalStorageAdapter.hpp"
#include "stc/compositelogger.hpp"
#include <algorithm>
#include <regex>
#include <fstream>

LocalStorageAdapter::LocalStorageAdapter(const SourceConfig& config) 
    : config_(config) {
    
    validatePath(config_.path);
    
    if (config_.file_mask.empty()) {
        throw std::invalid_argument("File mask cannot be empty");
    }
    
    stc::CompositeLogger::instance().info(
        "LocalStorageAdapter created for path: " + config_.path
    );
}

LocalStorageAdapter::~LocalStorageAdapter() {
    try {
        stopMonitoring();
        disconnect();
        stc::CompositeLogger::instance().debug("LocalStorageAdapter destroyed");
    } catch (...) {
        // Подавляем исключения в деструкторе
    }
}

std::vector<std::string> LocalStorageAdapter::listFiles(const std::string& path) {
    std::vector<std::string> files;
    
    try {
        if (!fs::exists(path) || !fs::is_directory(path)) {
            stc::CompositeLogger::instance().warning(
                "Directory does not exist or is not accessible: " + path
            );
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
        
        stc::CompositeLogger::instance().debug(
            "Found " + std::to_string(files.size()) + " files in: " + path
        );
        
    } catch (const fs::filesystem_error& e) {
        stc::CompositeLogger::instance().error(
            "Filesystem error in listFiles: " + std::string(e.what())
        );
        throw std::runtime_error("Failed to list files: " + std::string(e.what()));
    }
    
    return files;
}

void LocalStorageAdapter::downloadFile(const std::string& remotePath, 
                                      const std::string& localPath) {
    validatePath(remotePath);
    validatePath(localPath);
    
    try {
        // Для локального адаптера это обычное копирование файла
        if (!fs::exists(remotePath)) {
            throw std::invalid_argument("Source file does not exist: " + remotePath);
        }
        
        // Создаем директории если они не существуют
        fs::path localDir = fs::path(localPath).parent_path();
        if (!localDir.empty()) {
            fs::create_directories(localDir);
        }
        
        fs::copy_file(remotePath, localPath, fs::copy_options::overwrite_existing);
        
        stc::CompositeLogger::instance().info(
            "File copied from " + remotePath + " to " + localPath
        );
        
    } catch (const fs::filesystem_error& e) {
        stc::CompositeLogger::instance().error(
            "Failed to copy file: " + std::string(e.what())
        );
        throw std::ios_base::failure("File copy failed: " + std::string(e.what()));
    }
}

void LocalStorageAdapter::upload(const std::string& localPath,
                                const std::string& remotePath) {
    validatePath(localPath);
    validatePath(remotePath);
    
    try {
        if (!fs::exists(localPath)) {
            throw std::invalid_argument("Local file does not exist: " + localPath);
        }
        
        // Создаем директории назначения если они не существуют
        fs::path remoteDir = fs::path(remotePath).parent_path();
        if (!remoteDir.empty()) {
            fs::create_directories(remoteDir);
        }
        
        fs::copy_file(localPath, remotePath, fs::copy_options::overwrite_existing);
        
        stc::CompositeLogger::instance().info(
            "File uploaded from " + localPath + " to " + remotePath
        );
        
    } catch (const fs::filesystem_error& e) {
        stc::CompositeLogger::instance().error(
            "Failed to upload file: " + std::string(e.what())
        );
        throw std::runtime_error("File upload failed: " + std::string(e.what()));
    }
}

void LocalStorageAdapter::connect() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (connected_) {
        stc::CompositeLogger::instance().warning("Already connected");
        return;
    }
    
    try {
        ensurePathExists();
        connected_ = true;
        
        stc::CompositeLogger::instance().info(
            "Connected to local storage: " + config_.path
        );
        
    } catch (const std::exception& e) {
        stc::CompositeLogger::instance().error(
            "Connection failed: " + std::string(e.what())
        );
        throw std::runtime_error("Failed to connect: " + std::string(e.what()));
    }
}

void LocalStorageAdapter::disconnect() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!connected_) return;
    
    stopMonitoring();
    connected_ = false;
    
    stc::CompositeLogger::instance().info("Disconnected from local storage");
}

bool LocalStorageAdapter::isConnected() const noexcept {
    return connected_.load();
}

void LocalStorageAdapter::startMonitoring() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!connected_) {
        throw std::runtime_error("Cannot start monitoring: not connected");
    }
    
    if (monitoring_) {
        stc::CompositeLogger::instance().warning("Monitoring already started");
        return;
    }
    
    try {
        watcher_ = std::make_unique<FileWatcher>(
            config_.path,
            [this](FileWatcher::Event event, const std::string& filePath) {
                handleFileEvent(event, filePath);
            }
        );
        
        watcher_->start();
        monitoring_ = true;
        
        stc::CompositeLogger::instance().info(
            "Started monitoring: " + config_.path
        );
        
    } catch (const std::exception& e) {
        stc::CompositeLogger::instance().error(
            "Failed to start monitoring: " + std::string(e.what())
        );
        throw std::runtime_error("Monitoring start failed: " + std::string(e.what()));
    }
}

void LocalStorageAdapter::stopMonitoring() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!monitoring_) return;
    
    if (watcher_) {
        watcher_->stop();
        watcher_.reset();
    }
    
    monitoring_ = false;
    
    stc::CompositeLogger::instance().info("Stopped monitoring");
}

bool LocalStorageAdapter::isMonitoring() const noexcept {
    return monitoring_.load();
}

void LocalStorageAdapter::setCallback(FileDetectedCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    onFileDetected_ = callback;
}

void LocalStorageAdapter::ensurePathExists() {
    if (!fs::exists(config_.path)) {
        try {
            fs::create_directories(config_.path);
            stc::CompositeLogger::instance().info(
                "Created directory: " + config_.path
            );
        } catch (const fs::filesystem_error& e) {
            throw std::runtime_error(
                "Cannot create directory " + config_.path + ": " + e.what()
            );
        }
    }
    
    if (!fs::is_directory(config_.path)) {
        throw std::runtime_error("Path is not a directory: " + config_.path);
    }
}

void LocalStorageAdapter::handleFileEvent(FileWatcher::Event event, 
                                         const std::string& filePath) {
    try {
        if (event == FileWatcher::Event::Created) {
            fs::path path(filePath);
            std::string filename = path.filename().string();
            
            if (matchesFileMask(filename)) {
                stc::CompositeLogger::instance().debug(
                    "New file detected: " + filePath
                );
                
                if (onFileDetected_) {
                    onFileDetected_(filePath);
                }
            }
        }
    } catch (const std::exception& e) {
        stc::CompositeLogger::instance().error(
            "Error handling file event: " + std::string(e.what())
        );
    }
}

bool LocalStorageAdapter::matchesFileMask(const std::string& filename) const {
    try {
        // Преобразуем маску в регулярное выражение
        std::string pattern = config_.file_mask;
        
        // Заменяем * на .*
        std::replace(pattern.begin(), pattern.end(), '*', '.');
        size_t pos = 0;
        while ((pos = pattern.find(".", pos)) != std::string::npos) {
            if (pos + 1 < pattern.length() && pattern[pos + 1] != '*') {
                pattern.insert(pos, "\\");
                pos += 2;
            } else {
                pattern[pos] = '.';
                if (pos + 1 < pattern.length() && pattern[pos + 1] == '.') {
                    pattern[pos + 1] = '*';
                }
                pos += 2;
            }
        }
        
        // Заменяем ? на .
        for (size_t i = 0; i < pattern.length(); ++i) {
            if (pattern[i] == '?') {
                pattern[i] = '.';
            }
        }
        
        std::regex mask_regex(pattern, std::regex_constants::icase);
        return std::regex_match(filename, mask_regex);
        
    } catch (const std::regex_error& e) {
        stc::CompositeLogger::instance().warning(
            "Invalid file mask regex: " + config_.file_mask
        );
        return true; // В случае ошибки пропускаем все файлы
    }
}