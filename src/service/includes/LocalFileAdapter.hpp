#include "../includes/AdapterFabric.hpp"
#include <filesystem>
namespace fs = std::filesystem;

class LocalFileAdapter : public FileStorageInterface {
    public:
        std::vector<std::string> listFiles(const std::string& path) override;
    
        void downloadFile(const std::string& remotePath, const std::string& localPath) override;
    
        bool isAvailable() override;  // Локальный диск всегда доступен
    };