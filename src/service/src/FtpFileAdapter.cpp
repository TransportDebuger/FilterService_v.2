/**
 * @file FtpFileAdapter.cpp
 * @brief Реализация адаптера FTP файловой системы
 */

#include "../include/FtpFileAdapter.hpp"
#include "stc/compositelogger.hpp"
#include <algorithm>
#include <fstream>
#include <regex>
#include <sstream>
#include <sys/stat.h>

FtpFileAdapter::FtpFileAdapter(const SourceConfig &config)
    : config_(config), port_(21), pollingInterval_(config.check_interval) {

  validatePath(config_.path);
  validateFtpConfig();

  // Парсинг FTP URL (формат: ftp://server:port/path)
  std::regex ftpRegex(R"(ftp://([^:/]+)(?::(\d+))?(/.*)?$)");
  std::smatch matches;
  if (std::regex_match(config_.path, matches, ftpRegex)) {
    server_ = matches[1].str();
    if (matches[2].matched) {
      port_ = std::stoi(matches[2].str());
    }
    // Базовый URL без пути
    ftpUrl_ = "ftp://" + server_ + ":" + std::to_string(port_) + "/";
  } else {
    throw std::invalid_argument("Invalid FTP URL format: " + config_.path);
  }

  // Извлечение параметров подключения[18][28]
  auto it = config_.params.find("username");
  if (it != config_.params.end())
    username_ = it->second;

  it = config_.params.find("password");
  if (it != config_.params.end())
    password_ = it->second;

  stc::CompositeLogger::instance().info("FtpFileAdapter created for: " +
                                        ftpUrl_);
}

FtpFileAdapter::~FtpFileAdapter() {
  try {
    stopMonitoring();
    disconnect();
    stc::CompositeLogger::instance().debug("FtpFileAdapter destroyed");
  } catch (...) {
    // Подавляем исключения в деструкторе
  }
}

std::vector<std::string> FtpFileAdapter::listFiles(const std::string &path) {
  std::vector<std::string> files;

  if (!connected_) {
    stc::CompositeLogger::instance().warning("FTP adapter not connected");
    return files;
  }

  CURL *curl = curl_easy_init();
  if (!curl) {
    throw std::runtime_error("Failed to initialize CURL");
  }

  try {
    std::string url = buildFtpUrl(path);
    CurlResponse response;

    // Настройка libcurl для получения списка файлов[23][24]
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_USERPWD,
                     (username_ + ":" + password_).c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_DIRLISTONLY, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);

    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
      curl_easy_cleanup(curl);
      throw std::runtime_error("FTP LIST failed: " +
                               std::string(curl_easy_strerror(res)));
    }

    files = parseFileList(response.data);

    // Фильтрация по маске файлов
    files.erase(std::remove_if(files.begin(), files.end(),
                               [this](const std::string &filename) {
                                 return !matchesFileMask(
                                     fs::path(filename).filename().string());
                               }),
                files.end());

    stc::CompositeLogger::instance().debug(
        "Found " + std::to_string(files.size()) + " files on FTP server");

  } catch (...) {
    curl_easy_cleanup(curl);
    throw;
  }

  curl_easy_cleanup(curl);
  return files;
}

void FtpFileAdapter::downloadFile(const std::string &remotePath,
                                  const std::string &localPath) {
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

    // Создаем директории назначения
    fs::path localDir = fs::path(localPath).parent_path();
    if (!localDir.empty()) {
      fs::create_directories(localDir);
    }

    FILE *file = fopen(localPath.c_str(), "wb");
    if (!file) {
      curl_easy_cleanup(curl);
      throw std::ios_base::failure("Cannot create local file: " + localPath);
    }

    // Настройка libcurl для скачивания файла[26][28]
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_USERPWD,
                     (username_ + ":" + password_).c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, file);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 300L);

    CURLcode res = curl_easy_perform(curl);
    fclose(file);

    if (res != CURLE_OK) {
      fs::remove(localPath); // Удаляем неполный файл
      curl_easy_cleanup(curl);
      throw std::ios_base::failure("FTP download failed: " +
                                   std::string(curl_easy_strerror(res)));
    }

    stc::CompositeLogger::instance().info("FTP file downloaded from " + url +
                                          " to " + localPath);

  } catch (...) {
    curl_easy_cleanup(curl);
    throw;
  }

  curl_easy_cleanup(curl);
}

void FtpFileAdapter::upload(const std::string &localPath,
                            const std::string &remotePath) {
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

    // Получаем размер файла через stat
    struct stat file_info;
    if (stat(localPath.c_str(), &file_info) != 0) {
      fclose(file);
      curl_easy_cleanup(curl);
      throw std::runtime_error("Cannot get file size: " + localPath);
    }

    // Настройка libcurl для загрузки файла
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_USERPWD,
                     (username_ + ":" + password_).c_str());
    curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);
    curl_easy_setopt(curl, CURLOPT_READFUNCTION, readCallback);
    curl_easy_setopt(curl, CURLOPT_READDATA, file);
    curl_easy_setopt(curl, CURLOPT_INFILESIZE_LARGE,
                     (curl_off_t)file_info.st_size);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 600L);

    CURLcode res = curl_easy_perform(curl);
    fclose(file);

    if (res != CURLE_OK) {
      curl_easy_cleanup(curl);
      throw std::runtime_error("FTP upload failed: " +
                               std::string(curl_easy_strerror(res)));
    }

    stc::CompositeLogger::instance().info("File uploaded from " + localPath +
                                          " to FTP: " + url);

  } catch (...) {
    curl_easy_cleanup(curl);
    throw;
  }

  curl_easy_cleanup(curl);
}

void FtpFileAdapter::connect() {
  std::lock_guard<std::mutex> lock(mutex_);

  if (connected_) {
    stc::CompositeLogger::instance().warning("FTP adapter already connected");
    return;
  }

  try {
    // Проверяем доступность сервера
    if (!checkServerAvailability()) {
      throw std::runtime_error("FTP server is not accessible: " + server_);
    }

    connected_ = true;

    stc::CompositeLogger::instance().info("Connected to FTP server: " +
                                          ftpUrl_);

  } catch (const std::exception &e) {
    stc::CompositeLogger::instance().error("FTP connection failed: " +
                                           std::string(e.what()));
    throw std::runtime_error("Failed to connect to FTP: " +
                             std::string(e.what()));
  }
}

void FtpFileAdapter::disconnect() {
  std::lock_guard<std::mutex> lock(mutex_);

  if (!connected_)
    return;

  stopMonitoring();
  connected_ = false;

  stc::CompositeLogger::instance().info("Disconnected from FTP server");
}

bool FtpFileAdapter::isConnected() const noexcept { return connected_.load(); }

void FtpFileAdapter::startMonitoring() {
  std::lock_guard<std::mutex> lock(mutex_);

  if (!connected_) {
    throw std::runtime_error("Cannot start FTP monitoring: not connected");
  }

  if (monitoring_) {
    stc::CompositeLogger::instance().warning("FTP monitoring already started");
    return;
  }

  try {
    monitoring_ = true;
    monitoringThread_ = std::thread(&FtpFileAdapter::monitoringLoop, this);

    stc::CompositeLogger::instance().info(
        "Started FTP monitoring with " +
        std::to_string(pollingInterval_.count()) + "s interval");

  } catch (const std::exception &e) {
    monitoring_ = false;
    stc::CompositeLogger::instance().error("Failed to start FTP monitoring: " +
                                           std::string(e.what()));
    throw std::runtime_error("FTP monitoring start failed: " +
                             std::string(e.what()));
  }
}

void FtpFileAdapter::stopMonitoring() {
  std::lock_guard<std::mutex> lock(mutex_);

  if (!monitoring_)
    return;

  monitoring_ = false;

  if (monitoringThread_.joinable()) {
    monitoringThread_.join();
  }

  stc::CompositeLogger::instance().info("Stopped FTP monitoring");
}

bool FtpFileAdapter::isMonitoring() const noexcept {
  return monitoring_.load();
}

void FtpFileAdapter::setCallback(FileDetectedCallback callback) {
  std::lock_guard<std::mutex> lock(mutex_);
  onFileDetected_ = callback;
}

size_t FtpFileAdapter::writeCallback(void *contents, size_t size, size_t nmemb,
                                     CurlResponse *response) {
  size_t totalSize = size * nmemb;
  response->data.append(static_cast<char *>(contents), totalSize);
  response->size += totalSize;
  return totalSize;
}

size_t FtpFileAdapter::readCallback(void *ptr, size_t size, size_t nmemb,
                                    FILE *stream) {
  size_t retcode = fread(ptr, size, nmemb, stream);
  return retcode;
}

bool FtpFileAdapter::checkServerAvailability() const {
  CURL *curl = curl_easy_init();
  if (!curl)
    return false;

  bool available = false;
  try {
    CurlResponse response;

    curl_easy_setopt(curl, CURLOPT_URL, ftpUrl_.c_str());
    curl_easy_setopt(curl, CURLOPT_USERPWD,
                     (username_ + ":" + password_).c_str());
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
      stc::CompositeLogger::instance().error("FTP monitoring error: " +
                                             std::string(e.what()));
    }

    // Ожидание следующего цикла опроса[21][32]
    auto start = std::chrono::steady_clock::now();
    while (monitoring_ &&
           (std::chrono::steady_clock::now() - start) < pollingInterval_) {
      std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
  }
}

std::vector<std::string>
FtpFileAdapter::parseFileList(const std::string &listOutput) const {
  std::vector<std::string> files;
  std::istringstream stream(listOutput);
  std::string line;

  // Парсинг вывода команды NLST (только имена файлов)[24]
  while (std::getline(stream, line)) {
    if (!line.empty() && line != "." && line != "..") {
      files.push_back(line);
    }
  }

  return files;
}

bool FtpFileAdapter::matchesFileMask(const std::string &filename) const {
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

  } catch (const std::regex_error &e) {
    stc::CompositeLogger::instance().warning("Invalid FTP file mask regex: " +
                                             config_.file_mask);
    return true;
  }
}

std::string FtpFileAdapter::buildFtpUrl(const std::string &path) const {
  std::string url = ftpUrl_;
  if (!path.empty()) {
    // Убираем начальный слэш если есть
    std::string cleanPath = path;
    if (cleanPath.front() == '/') {
      cleanPath = cleanPath.substr(1);
    }
    url += cleanPath;
  }
  return url;
}

void FtpFileAdapter::validateFtpConfig() const {
  if (config_.file_mask.empty()) {
    throw std::invalid_argument("FTP file mask cannot be empty");
  }

  // Проверяем обязательные параметры FTP
  std::vector<std::string> required_fields = {"username", "password"};

  for (const auto &field : required_fields) {
    if (config_.params.find(field) == config_.params.end()) {
      throw std::invalid_argument("Missing required FTP field: " + field);
    }
  }
}

void FtpFileAdapter::compareFilesList(
    const std::vector<std::string> &currentFiles) {
  for (const auto &file : currentFiles) {
    // Проверяем, является ли файл новым
    if (std::find(lastFilesList_.begin(), lastFilesList_.end(), file) ==
        lastFilesList_.end()) {
      stc::CompositeLogger::instance().debug("New FTP file detected: " + file);

      if (onFileDetected_) {
        onFileDetected_(file);
      }
    }
  }
}