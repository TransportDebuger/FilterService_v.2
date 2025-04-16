#pragma once

#include <memory>
#include <vector>
#include <string>
#include <stdexcept>
#include <unordered_map>

#include "FileStorageInterface.hpp"

// Типы хранилищ
enum class SourceType { 
    LOCAL,  // Локальная файловая система
    FTP,    // FTP-сервер
    SFTP,   // SFTP (SSH)
    SMB     // SMB/CIFS 
};

class AdapterFabric {
    public:
        static SourceType stringToType(const std::string& typeStr);
        static std::unique_ptr<FileStorageInterface> createAdapter(
            SourceType type,
            const std::string& path,
            const std::string& username = "",
            const std::string& password = ""
        );
};