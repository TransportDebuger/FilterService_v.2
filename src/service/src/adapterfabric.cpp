#include "../include/adapterfabric.hpp"

#include "../include/LocalFileAdapter.hpp"
// #include "../includes/FtpFileAdapter.hpp"
#include <memory>

SourceType AdapterFabric::stringToType(const std::string& typeStr) {
  static const std::unordered_map<std::string, SourceType> mapping = {
      {"local", SourceType::LOCAL}
      // {"ftp",   SourceType::FTP},
      // {"sftp",  SourceType::SFTP},
      // {"smb",   SourceType::SMB}
  };

  auto it = mapping.find(typeStr);
  if (it == mapping.end()) {
    throw std::runtime_error("Unknown source type: " + typeStr);
  }
  return it->second;
}

std::unique_ptr<FileStorageInterface> AdapterFabric::createAdapter(
    SourceType type, const std::string& path, const std::string& username,
    const std::string& password) {
  switch (type) {
    case SourceType::LOCAL:
      return std::make_unique<LocalFileAdapter>(path);
    // case SourceType::FTP:   return std::make_unique<FtpFileAdapter>(path,
    // username, password); case SourceType::SFTP:  return
    // std::make_unique<SftpFileAdapter>(path, username, password); case
    // SourceType::SMB:   return std::make_unique<SmbFileAdapter>(path,
    // username, password);
    default:
      throw std::runtime_error("Unsupported type");
  }
}
