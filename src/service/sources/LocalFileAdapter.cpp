#include "../includes/filestorageinterface.hpp"
#include <filesystem>
namespace fs = std::filesystem;

class LocalFileAdapter : public FileStorageInterface {
public:
    std::vector<std::string> listFiles(const std::string& path) override {
        std::vector<std::string> files;
        for (const auto& entry : fs::directory_iterator(path)) {
            if (entry.is_regular_file()) {
                files.push_back(entry.path().string());
            }
        }
        return files;
    }

    void downloadFile(const std::string& remotePath, const std::string& localPath) override {
        fs::copy(remotePath, localPath, fs::copy_options::overwrite_existing);
    }

    bool isAvailable() override { return true; }  // Локальный диск всегда доступен
};