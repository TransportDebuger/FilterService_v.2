/**
@file FtpFileAdapter.cpp
@brief Реализация адаптера FTP файловой системы.
@version 2.0.0
@date 2026-07-17
*/
#include "../include/FtpFileAdapter.hpp"

#include <sys/stat.h>
#include <algorithm>
#include <fstream>
#include <regex>
#include <sstream>

namespace stc {

FtpFileAdapter::FtpFileAdapter(const SourceConfig &config, 
                               std::shared_ptr<stc::logger::ILogger> logger)
    : config_(config), port_(21), pollingInterval_(config.check_interval) {
    logger_ = std::move(logger); // Инициализация члена базового класса
    
    validatePath(config_.path);
    validateFtpConfig();

    std::regex ftpRegex(R"(ftp://([^:/]+)(?::(\d+))?(/.*)?$)");
    std::smatch matches;
    if (std::regex_match(config_.path, matches, ftpRegex)) {
        server_ = matches[1].str();
        if (matches[2].matched) {
            port_ = std::stoi(matches[2].str());
        }
        ftpUrl_ = "ftp://" + server_ + ":" + std::to_string(port_) + "/";
    } else {
        throw std::invalid_argument("Invalid FTP URL format: " + config_.path);
    }

    auto it = config_.params.find("username");
    if (it != config_.params.end()) username_ = it->second;
    it = config_.params.find("password");
    if (it != config_.params.end()) password_ = it->second;
    
    if (logger_) logger_->Info("FtpFileAdapter created for: " + ftpUrl_);
}

FtpFileAdapter::~FtpFileAdapter() {
    try {
        stopMonitoring();
        disconnect();
        if (logger_) logger_->Debug("FtpFileAdapter destroyed");
    } catch (...) {
        // Подавляем исключения в деструкторе
    }
}

std::vector<std::string> FtpFileAdapter::listFiles(const std::string &path) {
    std::vector<std::string> files;
    if (!connected_) {
        if (logger_) logger_->Warning("FTP adapter not connected");
        return files;
    }
    CURL *curl = curl_easy_init();
    if (!curl) {
        throw std::runtime_error("Failed to initialize CURL");
    }
    try {
        std::string url = buildFtpUrl(path);
        CurlResponse response;
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_USERPWD, (username_ + ":" + password_).c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
        curl_easy_setopt(curl, CURLOPT_DIRLISTONLY, 1L);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
        CURLcode res = curl_easy_perform(curl);
        if (res != CURLE_OK) {
            curl_easy_cleanup(curl);
            throw std::runtime_error("FTP LIST failed: " + std::string(curl_easy_strerror(res)));
        }
        files = parseFileList(response.data);
        files.erase(std::remove_if(files.begin(), files.end(),
            [this](const std::string &filename) {
                return !matchesFileMask(fs::path(filename).filename().string());
            }), files.end());
        if (logger_) logger_->Debug("Found " + std::to_string(files.size()) + " files on FTP server");
    } catch (...) {
        curl_easy_cleanup(curl);
        throw;
    }
    curl_easy_cleanup(curl);
    return files;
}

void FtpFileAdapter::downloadFile(const std::string &remotePath, const std::string &localPath) {
    validatePath(remotePath);
    validatePath(localPath);
    if (!connected_) {
        throw std::runtime_error("FTP adapter not connected");
    }
    CURL *curl = curl_easy_init();
    if (!curl) {
        throw std::runtime_error("Failed to initialize CURL");
    }
    try {
        std::string url = buildFtpUrl(remotePath);
        fs::path localDir = fs::path(localPath).parent_path();
        if (!localDir.empty()) fs::create_directories(localDir);
        FILE *file = fopen(localPath.c_str(), "wb");
        if (!file) {
            curl_easy_cleanup(curl);
            throw std::ios_base::failure("Cannot create local file: " + localPath);
        }
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_USERPWD, (username_ + ":" + password_).c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, file);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 300L);
        CURLcode res = curl_easy_perform(curl);
        fclose(file);
        if (res != CURLE_OK) {
            fs::remove(localPath);
            curl_easy_cleanup(curl);
            throw std::ios_base::failure("FTP download failed: " + std::string(curl_easy_strerror(res)));
        }
        if (logger_) logger_->Info("FTP file downloaded from " + url + " to " + localPath);
    } catch (...) {
        curl_easy_cleanup(curl);
        throw;
    }
    curl_easy_cleanup(curl);
}

void FtpFileAdapter::upload(const std::string &localPath, const std::string &remotePath) {
    validatePath(localPath);
    validatePath(remotePath);
    if (!connected_) {
        throw std::runtime_error("FTP adapter not connected");
    }
    if (!fs::exists(localPath)) {
        throw std::invalid_argument("Local file does not exist: " + localPath);
    }
    CURL *curl = curl_easy_init();
    if (!curl) {
        throw std::runtime_error("Failed to initialize CURL");
    }
    try {
        std::string url = buildFtpUrl(remotePath);
        FILE *file = fopen(localPath.c_str(), "rb");
        if (!file) {
            curl_easy_cleanup(curl);
            throw std::invalid_argument("Cannot open local file: " + localPath);
        }
        struct stat file_info;
        if (stat(localPath.c_str(), &file_info) != 0) {
            fclose(file);
            curl_easy_cleanup(curl);
            throw std::runtime_error("Cannot get file size: " + localPath);
        }
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_USERPWD, (username_ + ":" + password_).c_str());
        curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);
        curl_easy_setopt(curl, CURLOPT_READFUNCTION, readCallback);
        curl_easy_setopt(curl, CURLOPT_READDATA, file);
        curl_easy_setopt(curl, CURLOPT_INFILESIZE_LARGE, (curl_off_t)file_info.st_size);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 600L);
        CURLcode res = curl_easy_perform(curl);
        fclose(file);
        if (res != CURLE_OK) {
            curl_easy_cleanup(curl);
            throw std::runtime_error("FTP upload failed: " + std::string(curl_easy_strerror(res)));
        }
        if (logger_) logger_->Info("File uploaded from " + localPath + " to FTP: " + url);
    } catch (...) {
        curl_easy_cleanup(curl);
        throw;
    }
    curl_easy_cleanup(curl);
}

void FtpFileAdapter::connect() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (connected_) {
        if (logger_) logger_->Warning("FTP adapter already connected");
        return;
    }
    try {
        if (!checkServerAvailability()) {
            throw std::runtime_error("FTP server is not accessible: " + server_);
        }
        connected_ = true;
        if (logger_) logger_->Info("Connected to FTP server: " + ftpUrl_);
    } catch (const std::exception &e) {
        if (logger_) logger_->Error("FTP connection failed: " + std::string(e.what()));
        throw std::runtime_error("Failed to connect to FTP: " + std::string(e.what()));
    }
}

void FtpFileAdapter::disconnect() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!connected_) return;
    stopMonitoring();
    connected_ = false;
    if (logger_) logger_->Info("Disconnected from FTP server");
}

bool FtpFileAdapter::isConnected() const noexcept { return connected_.load(); }

void FtpFileAdapter::startMonitoring() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!connected_) {
        throw std::runtime_error("Cannot start FTP monitoring: not connected");
    }
    if (monitoring_) {
        if (logger_) logger_->Warning("FTP monitoring already started");
        return;
    }
    try {
        monitoring_ = true;
        monitoringThread_ = std::thread(&FtpFileAdapter::monitoringLoop, this);
        if (logger_) logger_->Info("Started FTP monitoring with " + std::to_string(pollingInterval_.count()) + "s interval");
    } catch (const std::exception &e) {
        monitoring_ = false;
        if (logger_) logger_->Error("Failed to start FTP monitoring: " + std::string(e.what()));
        throw std::runtime_error("FTP monitoring start failed: " + std::string(e.what()));
    }
}

void FtpFileAdapter::stopMonitoring() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!monitoring_) return;
    monitoring_ = false;
    if (monitoringThread_.joinable()) {
        monitoringThread_.join();
    }
    if (logger_) logger_->Info("Stopped FTP monitoring");
}

bool FtpFileAdapter::isMonitoring() const noexcept { return monitoring_.load(); }

void FtpFileAdapter::setCallback(FileDetectedCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    onFileDetected_ = std::move(callback);
}

size_t FtpFileAdapter::writeCallback(void *contents, size_t size, size_t nmemb, CurlResponse *response) {
    size_t totalSize = size * nmemb;
    response->data.append(static_cast<char *>(contents), totalSize);
    response->size += totalSize;
    return totalSize;
}

size_t FtpFileAdapter::readCallback(void *ptr, size_t size, size_t nmemb, FILE *stream) {
    return fread(ptr, size, nmemb, stream);
}

bool FtpFileAdapter::checkServerAvailability() const {
    CURL *curl = curl_easy_init();
    if (!curl) return false;
    bool available = false;
    try {
        CurlResponse response;
        curl_easy_setopt(curl, CURLOPT_URL, ftpUrl_.c_str());
        curl_easy_setopt(curl, CURLOPT_USERPWD, (username_ + ":" + password_).c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);
        curl_easy_setopt(curl, CURLOPT_DIRLISTONLY, 1L);
        CURLcode res = curl_easy_perform(curl);
        available = (res == CURLE_OK);
    } catch (...) {
        available = false;
    }
    curl_easy_cleanup(curl);
    return available;
}

void FtpFileAdapter::monitoringLoop() {
    while (monitoring_) {
        try {
            auto currentFiles = listFiles("");
            compareFilesList(currentFiles);
            lastFilesList_ = std::move(currentFiles);
        } catch (const std::exception &e) {
            if (logger_) logger_->Error("FTP monitoring error: " + std::string(e.what()));
        }
        auto start = std::chrono::steady_clock::now();
        while (monitoring_ && (std::chrono::steady_clock::now() - start) < pollingInterval_) {
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }
    }
}

std::vector<std::string> FtpFileAdapter::parseFileList(const std::string &listOutput) const {
    std::vector<std::string> files;
    std::istringstream stream(listOutput);
    std::string line;
    while (std::getline(stream, line)) {
        if (!line.empty() && line != "." && line != "..") {
            files.push_back(line);
        }
    }
    return files;
}

bool FtpFileAdapter::matchesFileMask(const std::string &filename) const {
    try {
        std::string pattern = config_.file_mask;
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
        for (size_t i = 0; i < pattern.length(); ++i) {
            if (pattern[i] == '?') pattern[i] = '.';
        }
        std::regex mask_regex(pattern, std::regex_constants::icase);
        return std::regex_match(filename, mask_regex);
    } catch (const std::regex_error &e) {
        if (logger_) logger_->Warning("Invalid FTP file mask regex: " + config_.file_mask);
        return true;
    }
}

std::string FtpFileAdapter::buildFtpUrl(const std::string &path) const {
    std::string url = ftpUrl_;
    if (!path.empty()) {
        std::string cleanPath = path;
        if (cleanPath.front() == '/') cleanPath = cleanPath.substr(1);
        url += cleanPath;
    }
    return url;
}

void FtpFileAdapter::validateFtpConfig() const {
    if (config_.file_mask.empty()) {
        throw std::invalid_argument("FTP file mask cannot be empty");
    }
    std::vector<std::string> required_fields = {"username", "password"};
    for (const auto &field : required_fields) {
        if (config_.params.find(field) == config_.params.end()) {
            throw std::invalid_argument("Missing required FTP field: " + field);
        }
    }
}

void FtpFileAdapter::compareFilesList(const std::vector<std::string> &currentFiles) {
    for (const auto &file : currentFiles) {
        if (std::find(lastFilesList_.begin(), lastFilesList_.end(), file) == lastFilesList_.end()) {
            if (logger_) logger_->Debug("New FTP file detected: " + file);
            if (onFileDetected_) onFileDetected_(file);
        }
    }
}

} // namespace stc