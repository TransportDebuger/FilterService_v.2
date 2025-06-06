#include <filesystem>

#include "../include/AdapterFabric.hpp"
namespace fs = std::filesystem;

class LocalFileAdapter : public FileStorageInterface {
 public:
  std::vector<std::string> listFiles(const std::string& path) override;

  void downloadFile(const std::string& remotePath,
                    const std::string& localPath) override;

  bool isAvailable() override;  // Локальный диск всегда доступен
  void connect() = 0;
  void upload(const std::string& localPath) = 0;
};