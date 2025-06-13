#include "../include/filewatcher.hpp"
#include <filesystem>
#include <iostream>

namespace fs = std::filesystem;

FileWatcher::FileWatcher(const std::string& path, Callback callback)
    : path_(fs::absolute(path).string()), callback_(callback) 
{
    if (!fs::exists(path_)) {
        throw std::runtime_error("Path does not exist: " + path_);
    }
}

FileWatcher::~FileWatcher() {
    stop();
}

void FileWatcher::start() {
    if (running_) return;
    running_ = true;

#ifdef _WIN32
    dirHandle_ = CreateFileA(
        path_.c_str(),
        FILE_LIST_DIRECTORY,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        NULL,
        OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED,
        NULL
    );
    if (dirHandle_ == INVALID_HANDLE_VALUE) {
        throw std::runtime_error("Failed to open directory handle");
    }
#else
    inotifyFd_ = inotify_init1(IN_NONBLOCK);
    if (inotifyFd_ < 0) {
        throw std::runtime_error("Failed to initialize inotify");
    }
    watchDescriptor_ = inotify_add_watch(inotifyFd_, path_.c_str(), 
        IN_CREATE | IN_DELETE | IN_MODIFY | IN_MOVED_FROM | IN_MOVED_TO);
    if (watchDescriptor_ < 0) {
        close(inotifyFd_);
        throw std::runtime_error("Failed to add watch");
    }
#endif

    thread_ = std::thread(&FileWatcher::run, this);
}

void FileWatcher::stop() {
    running_ = false;
    if (thread_.joinable()) {
        thread_.join();
    }

#ifdef _WIN32
    if (dirHandle_ != INVALID_HANDLE_VALUE) {
        CloseHandle(dirHandle_);
    }
#else
    if (watchDescriptor_ >= 0) {
        inotify_rm_watch(inotifyFd_, watchDescriptor_);
    }
    if (inotifyFd_ >= 0) {
        close(inotifyFd_);
    }
#endif
}

void FileWatcher::run() {
#ifdef _WIN32
    BYTE buffer[4096];
    DWORD bytesReturned;
    OVERLAPPED overlapped{};

    while (running_) {
        if (!ReadDirectoryChangesW(
            dirHandle_,
            buffer,
            sizeof(buffer),
            TRUE,
            FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_LAST_WRITE,
            &bytesReturned,
            &overlapped,
            NULL
        )) {
            break;
        }

        FILE_NOTIFY_INFORMATION* info = reinterpret_cast<FILE_NOTIFY_INFORMATION*>(buffer);
        do {
            std::wstring wname(info->FileName, info->FileNameLength / 2);
            std::string name(wname.begin(), wname.end());
            std::string fullPath = (fs::path(path_) / name).string();

            switch (info->Action) {
                case FILE_ACTION_ADDED:
                    callback_(Event::Created, fullPath);
                    break;
                case FILE_ACTION_REMOVED:
                    callback_(Event::Deleted, fullPath);
                    break;
                case FILE_ACTION_MODIFIED:
                    callback_(Event::Modified, fullPath);
                    break;
                case FILE_ACTION_RENAMED_OLD_NAME:
                    callback_(Event::Renamed, fullPath);
                    break;
                case FILE_ACTION_RENAMED_NEW_NAME:
                    callback_(Event::Renamed, fullPath);
                    break;
            }
            
            info = info->NextEntryOffset == 0 ? nullptr : 
                reinterpret_cast<FILE_NOTIFY_INFORMATION*>(reinterpret_cast<BYTE*>(info) + info->NextEntryOffset);
        } while (info != nullptr);
    }
#else
    char buffer[4096];
    while (running_) {
        int length = read(inotifyFd_, buffer, sizeof(buffer));
        if (length < 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }

        int i = 0;
        while (i < length) {
            inotify_event* event = reinterpret_cast<inotify_event*>(&buffer[i]);
            if (event->len) {
                std::string fullPath = (fs::path(path_) / event->name).string();
                
                if (event->mask & (IN_CREATE | IN_MOVED_TO)) {
                    callback_(Event::Created, fullPath);
                }
                if (event->mask & (IN_DELETE | IN_MOVED_FROM)) {
                    callback_(Event::Deleted, fullPath);
                }
                if (event->mask & IN_MODIFY) {
                    callback_(Event::Modified, fullPath);
                }
            }
            i += sizeof(inotify_event) + event->len;
        }
    }
#endif
}