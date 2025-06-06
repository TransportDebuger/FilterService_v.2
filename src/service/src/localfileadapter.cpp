#include "../include/LocalFileAdapter.hpp"

#include <filesystem>
namespace fs = std::filesystem;

std::vector<std::string> LocalFileAdapter::listFiles(const std::string& path) {
  std::vector<std::string> files;
  for (const auto& entry : fs::directory_iterator(path)) {
    if (entry.is_regular_file()) {
      files.push_back(entry.path().string());
    }
  }
  return files;
}

void LocalFileAdapter::downloadFile(const std::string& remotePath,
                                    const std::string& localPath) {
  try {
    fs::copy(remotePath, localPath, fs::copy_options::overwrite_existing);
  } catch (const fs::filesystem_error& e) {
    throw std::runtime_error("File copy failed: " + std::string(e.what()));
  }
}

bool LocalFileAdapter::isAvailable() {
  return true;
}  // Локальный диск всегда доступен