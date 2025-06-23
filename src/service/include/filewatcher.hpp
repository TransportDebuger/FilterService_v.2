#pragma once
#include <atomic>
#include <functional>
#include <string>
#include <thread>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/inotify.h>
#include <unistd.h>
#endif

class FileWatcher {
public:
  enum class Event { Created, Deleted, Modified, Renamed };
  using Callback = std::function<void(Event, const std::string &)>;

  FileWatcher(const std::string &path, Callback callback);
  ~FileWatcher();

  void start();
  void stop();

private:
  void run();

  std::string path_;
  Callback callback_;
  std::atomic<bool> running_{false};
  std::thread thread_;

#ifdef _WIN32
  HANDLE dirHandle_ = INVALID_HANDLE_VALUE;
#else
  int inotifyFd_ = -1;
  int watchDescriptor_ = -1;
#endif
};