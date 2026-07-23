/**
@file directory_monitor.cpp
@brief Реализация фабрики мониторов с эвристикой выбора стратегии.
@version 1.1.2
@date 2026-07-22
*/
#include "stc/fs/directory_monitor.hpp"

#include <sys/statfs.h>

#include <stdexcept>
#include <system_error>

#include "inotify_monitor.hpp"
#include "native_inotify_system_calls.hpp"
#include "polling_monitor.hpp"

namespace stc::fs {

std::unique_ptr<IDirectoryMonitor> DirectoryMonitor::Create(
    const std::string& path, IDirectoryMonitor::Callback callback,
    std::chrono::seconds polling_interval,
    std::shared_ptr<IFileSystemSystemCalls> sys_calls) {
  if (!sys_calls) {
    sys_calls = std::make_shared<NativeInotifySystemCalls>();  // LCOV_EXCL_LINE
  }

  struct statfs stat_buf {};
  if (sys_calls->StatFs(path, &stat_buf) == 0) {
    // Эвристика: сетевые и FUSE файловые системы не поддерживают inotify.
    // Магические числа захардкожены для независимости от версий
    // <linux/magic.h>. Явное приведение к long предотвращает предупреждения
    // компилятора о квалификаторах и знаковых/беззнаковых сравнениях.
    const long f_type_val = static_cast<long>(stat_buf.f_type);

    if (f_type_val == 0x517BL ||      // SMB_SUPER_MAGIC
        f_type_val == 0xFF534D42L ||  // CIFS_MAGIC_NUMBER
        f_type_val == 0x65735546L ||  // FUSE_SUPER_MAGIC
        f_type_val == 0x6969L)        // NFS_SUPER_MAGIC
    {
      return std::make_unique<PollingMonitor>(path, std::move(callback),
                                              polling_interval);
    }
  } else {
    throw std::system_error(
        errno, std::system_category(),
        "DirectoryMonitor: statfs failed for path: " + path);
  }

  return std::make_unique<InotifyMonitor>(path, std::move(callback),
                                          std::move(sys_calls));
}

}  // namespace stc::fs