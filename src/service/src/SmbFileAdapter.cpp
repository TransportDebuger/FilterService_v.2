/**
 * @file SmbFileAdapter.cpp
 * @brief Реализация адаптера SMB/CIFS файловой системы
 */

#include "../include/SmbFileAdapter.hpp"

#include <sys/mount.h>
#include <sys/stat.h>
#include <unistd.h>

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <regex>

#include "stc/compositelogger.hpp"

SmbFileAdapter::SmbFileAdapter(const SourceConfig &config) : config_(config) {
  validatePath(config_.path);
  validateSmbConfig();

  // Парсинг SMB URL из config_.path (формат: smb://server/share)
  std::regex smbRegex(R"(smb://([^/]+)/(.+))");
  std::smatch matches;
  if (std::regex_match(config_.path, matches, smbRegex)) {
    server_ = matches[1].str();
    share_ = matches[2].str();
    smbUrl_ = "//" + server_ + "/" + share_;
  } else {
    throw std::invalid_argument("Invalid SMB URL format: " + config_.path);
  }

  // Извлечение параметров подключения
  auto it = config_.params.find("username");
  if (it != config_.params.end()) username_ = it->second;

  it = config_.params.find("password");
  if (it != config_.params.end()) password_ = it->second;

  it = config_.params.find("domain");
  if (it != config_.params.end())
    domain_ = it->second;
  else
    domain_ = "WORKGROUP";  // Значение по умолчанию

  stc::CompositeLogger::instance().info("SmbFileAdapter created for: " +
                                        smbUrl_);
}

SmbFileAdapter::~SmbFileAdapter() {
  try {
    stopMonitoring();
    disconnect();
    stc::CompositeLogger::instance().debug("SmbFileAdapter destroyed");
  } catch (...) {
    // Подавляем исключения в деструкторе
  }
}

std::vector<std::string> SmbFileAdapter::listFiles(const std::string &path) {
  std::vector<std::string> files;

  if (!connected_) {
    stc::CompositeLogger::instance().warning("SMB adapter not connected");
    return files;
  }

  try {
    std::string searchPath =
        path.empty() ? mountPoint_ : (fs::path(mountPoint_) / path).string();

    if (!fs::exists(searchPath) || !fs::is_directory(searchPath)) {
      stc::CompositeLogger::instance().warning(
          "SMB directory does not exist: " + searchPath);
      return files;
    }

    for (const auto &entry : fs::directory_iterator(searchPath)) {
      if (entry.is_regular_file()) {
        std::string filename = entry.path().filename().string();
        if (matchesFileMask(filename)) {
          files.push_back(entry.path().string());
        }
      }
    }

    stc::CompositeLogger::instance().debug(
        "Found " + std::to_string(files.size()) + " files in SMB share");

  } catch (const fs::filesystem_error &e) {
    stc::CompositeLogger::instance().error(
        "SMB filesystem error in listFiles: " + std::string(e.what()));
    throw std::runtime_error("Failed to list SMB files: " +
                             std::string(e.what()));
  }

  return files;
}

void SmbFileAdapter::downloadFile(const std::string &remotePath,
                                  const std::string &localPath) {
  validatePath(remotePath);
  validatePath(localPath);

  if (!connected_) {
    throw std::runtime_error("SMB adapter not connected");
  }

  try {
    std::string sourcePath = (fs::path(mountPoint_) / remotePath).string();

    if (!fs::exists(sourcePath)) {
      throw std::invalid_argument("SMB file does not exist: " + sourcePath);
    }

    // Создаем директории назначения
    fs::path localDir = fs::path(localPath).parent_path();
    if (!localDir.empty()) {
      fs::create_directories(localDir);
    }

    fs::copy_file(sourcePath, localPath, fs::copy_options::overwrite_existing);

    stc::CompositeLogger::instance().info("SMB file downloaded from " +
                                          sourcePath + " to " + localPath);

  } catch (const fs::filesystem_error &e) {
    stc::CompositeLogger::instance().error("SMB download failed: " +
                                           std::string(e.what()));
    throw std::ios_base::failure("SMB file download failed: " +
                                 std::string(e.what()));
  }
}

void SmbFileAdapter::upload(const std::string &localPath,
                            const std::string &remotePath) {
  validatePath(localPath);
  validatePath(remotePath);

  if (!connected_) {
    throw std::runtime_error("SMB adapter not connected");
  }

  try {
    if (!fs::exists(localPath)) {
      throw std::invalid_argument("Local file does not exist: " + localPath);
    }

    std::string targetPath = (fs::path(mountPoint_) / remotePath).string();

    // Создаем директории назначения на SMB-ресурсе
    fs::path remoteDir = fs::path(targetPath).parent_path();
    if (!remoteDir.empty()) {
      fs::create_directories(remoteDir);
    }

    fs::copy_file(localPath, targetPath, fs::copy_options::overwrite_existing);

    stc::CompositeLogger::instance().info("File uploaded from " + localPath +
                                          " to SMB: " + targetPath);

  } catch (const fs::filesystem_error &e) {
    stc::CompositeLogger::instance().error("SMB upload failed: " +
                                           std::string(e.what()));
    throw std::runtime_error("SMB file upload failed: " +
                             std::string(e.what()));
  }
}

void SmbFileAdapter::connect() {
  std::lock_guard<std::mutex> lock(mutex_);

  if (connected_) {
    stc::CompositeLogger::instance().warning("SMB adapter already connected");
    return;
  }

  try {
    // Проверяем доступность сервера
    if (!checkServerAvailability()) {
      throw std::runtime_error("SMB server is not accessible: " + server_);
    }

    // Создаем точку монтирования
    mountPoint_ = createMountPoint();

    // Монтируем SMB-ресурс
    mountSmbResource();

    connected_ = true;
    mounted_ = true;

    stc::CompositeLogger::instance().info("Connected to SMB share: " + smbUrl_ +
                                          " at " + mountPoint_);

  } catch (const std::exception &e) {
    stc::CompositeLogger::instance().error("SMB connection failed: " +
                                           std::string(e.what()));
    throw std::runtime_error("Failed to connect to SMB: " +
                             std::string(e.what()));
  }
}

void SmbFileAdapter::disconnect() {
  std::lock_guard<std::mutex> lock(mutex_);

  if (!connected_) return;

  stopMonitoring();
  unmountSmbResource();

  connected_ = false;
  mounted_ = false;

  stc::CompositeLogger::instance().info("Disconnected from SMB share");
}

bool SmbFileAdapter::isConnected() const noexcept {
  return connected_.load() && mounted_.load();
}

void SmbFileAdapter::startMonitoring() {
  std::lock_guard<std::mutex> lock(mutex_);

  if (!connected_) {
    throw std::runtime_error("Cannot start SMB monitoring: not connected");
  }

  if (monitoring_) {
    stc::CompositeLogger::instance().warning("SMB monitoring already started");
    return;
  }

  try {
    watcher_ = std::make_unique<FileWatcher>(
        mountPoint_,
        [this](FileWatcher::Event event, const std::string &filePath) {
          handleFileEvent(event, filePath);
        });

    watcher_->start();
    monitoring_ = true;

    stc::CompositeLogger::instance().info("Started SMB monitoring: " +
                                          mountPoint_);

  } catch (const std::exception &e) {
    stc::CompositeLogger::instance().error("Failed to start SMB monitoring: " +
                                           std::string(e.what()));
    throw std::runtime_error("SMB monitoring start failed: " +
                             std::string(e.what()));
  }
}

void SmbFileAdapter::stopMonitoring() {
  std::lock_guard<std::mutex> lock(mutex_);

  if (!monitoring_) return;

  if (watcher_) {
    watcher_->stop();
    watcher_.reset();
  }

  monitoring_ = false;

  stc::CompositeLogger::instance().info("Stopped SMB monitoring");
}

bool SmbFileAdapter::isMonitoring() const noexcept {
  return monitoring_.load();
}

void SmbFileAdapter::setCallback(FileDetectedCallback callback) {
  std::lock_guard<std::mutex> lock(mutex_);
  onFileDetected_ = callback;
}

void SmbFileAdapter::mountSmbResource() {
  std::string mountCommand = buildMountCommand();

  stc::CompositeLogger::instance().debug("Executing: " + mountCommand);

  int result = std::system(mountCommand.c_str());
  if (result != 0) {
    throw std::runtime_error(
        "Failed to mount SMB resource. Command: " + mountCommand +
        ", Exit code: " + std::to_string(result));
  }

  // Проверяем, что монтирование прошло успешно
  if (!fs::exists(mountPoint_) || !fs::is_directory(mountPoint_)) {
    throw std::runtime_error("SMB mount point validation failed: " +
                             mountPoint_);
  }
}

void SmbFileAdapter::unmountSmbResource() {
  if (!mounted_ || mountPoint_.empty()) return;

  try {
    std::string umountCommand = "umount " + mountPoint_;
    int result = std::system(umountCommand.c_str());

    if (result == 0) {
      stc::CompositeLogger::instance().info(
          "SMB resource unmounted successfully");
    } else {
      stc::CompositeLogger::instance().warning(
          "SMB unmount failed with code: " + std::to_string(result));
    }

    // Удаляем временную точку монтирования
    if (fs::exists(mountPoint_)) {
      fs::remove(mountPoint_);
    }

  } catch (const std::exception &e) {
    stc::CompositeLogger::instance().error("Error during SMB unmount: " +
                                           std::string(e.what()));
  }
}

bool SmbFileAdapter::checkServerAvailability() const {
  std::string pingCommand = "ping -c 1 -W 3 " + server_ + " > /dev/null 2>&1";
  int result = std::system(pingCommand.c_str());
  return result == 0;
}

std::string SmbFileAdapter::createMountPoint() {
  std::string tempDir =
      "/tmp/smb_mount_" + std::to_string(getpid()) + "_" +
      std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(
                         std::chrono::system_clock::now().time_since_epoch())
                         .count());

  if (mkdir(tempDir.c_str(), 0755) != 0) {
    throw std::runtime_error("Failed to create mount point: " + tempDir);
  }

  return tempDir;
}

void SmbFileAdapter::handleFileEvent(FileWatcher::Event event,
                                     const std::string &filePath) {
  try {
    if (event == FileWatcher::Event::Created) {
      fs::path path(filePath);
      std::string filename = path.filename().string();

      if (matchesFileMask(filename)) {
        stc::CompositeLogger::instance().debug("New SMB file detected: " +
                                               filePath);

        if (onFileDetected_) {
          onFileDetected_(filePath);
        }
      }
    }
  } catch (const std::exception &e) {
    stc::CompositeLogger::instance().error("Error handling SMB file event: " +
                                           std::string(e.what()));
  }
}

bool SmbFileAdapter::matchesFileMask(const std::string &filename) const {
  try {
    // Преобразуем маску в регулярное выражение (аналогично LocalStorageAdapter)
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
    stc::CompositeLogger::instance().warning("Invalid SMB file mask regex: " +
                                             config_.file_mask);
    return true;
  }
}

std::string SmbFileAdapter::buildMountCommand() const {
  std::string command = "mount -t cifs " + smbUrl_ + " " + mountPoint_;

  // Добавляем опции монтирования
  command += " -o ";
  std::vector<std::string> options;

  if (!username_.empty()) {
    options.push_back("username=" + username_);
  }

  if (!password_.empty()) {
    options.push_back("password=" + password_);
  }

  if (!domain_.empty()) {
    options.push_back("domain=" + domain_);
  }

  // Добавляем стандартные опции для совместимости
  options.push_back("vers=3.0");
  options.push_back("uid=" + std::to_string(getuid()));
  options.push_back("gid=" + std::to_string(getgid()));
  options.push_back("file_mode=0644");
  options.push_back("dir_mode=0755");

  // Объединяем опции
  for (size_t i = 0; i < options.size(); ++i) {
    if (i > 0) command += ",";
    command += options[i];
  }

  return command;
}

void SmbFileAdapter::validateSmbConfig() const {
  if (config_.file_mask.empty()) {
    throw std::invalid_argument("SMB file mask cannot be empty");
  }

  // Проверяем обязательные параметры SMB
  std::vector<std::string> required_fields = {"username"};

  for (const auto &field : required_fields) {
    if (config_.params.find(field) == config_.params.end()) {
      throw std::invalid_argument("Missing required SMB field: " + field);
    }
  }
}