/**
@file native_inotify_system_calls.cpp
@brief Реализация нативных системных вызовов Linux.
@version 1.0.0
@date 2026-07-22
*/
#include "native_inotify_system_calls.hpp"

#include <linux/magic.h>
#include <sys/inotify.h>
#include <sys/statfs.h>
#include <unistd.h>

namespace stc::fs {

int NativeInotifySystemCalls::Init() {
  return inotify_init1(IN_NONBLOCK | IN_CLOEXEC);
}

int NativeInotifySystemCalls::AddWatch(int fd, const std::string& path,
                                       uint32_t mask) {
  return inotify_add_watch(fd, path.c_str(), mask);
}

ssize_t NativeInotifySystemCalls::Read(int fd, void* buffer,
                                       std::size_t count) {
  return read(fd, buffer, count);
}

int NativeInotifySystemCalls::RemoveWatch(int fd, int wd) {
  return inotify_rm_watch(fd, wd);
}

int NativeInotifySystemCalls::Close(int fd) { return close(fd); }

int NativeInotifySystemCalls::StatFs(const std::string& path,
                                     struct statfs* buf) {
  return statfs(path.c_str(), buf);
}

NativeInotifySystemCalls::~NativeInotifySystemCalls() = default;

}  // namespace stc::fs