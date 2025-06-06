#include "../includes/filestorageinterface.hpp"
//#include <curl/curl.h>

class FtpFileAdapter : public FileStorageInterface {
  std::string ftpUrl;  // ftp://user:pass@server/path

 public:
  FtpFileAdapter(const std::string& url) : ftpUrl(url) {}

  std::vector<std::string> listFiles(const std::string& path) override {
    std::vector<std::string> files;
    // Реализация через libcurl (FTP LIST)
    // ...
    return files;
  }

  void downloadFile(const std::string& remotePath,
                    const std::string& localPath) override {
    // Скачивание файла через FTP
    // ...
  }

  bool isAvailable() override {
    // Проверка соединения с FTP
    // ...
  }
};