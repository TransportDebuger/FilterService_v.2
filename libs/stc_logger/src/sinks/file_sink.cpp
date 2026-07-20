#include "stc/logger/sinks/file/file_sink.hpp"

#include <filesystem>
#include <iostream>
#include <stdexcept>

namespace fs = std::filesystem;

namespace stc::logger {

FileSink::FileSink(std::string file_path,
                   std::shared_ptr<ILogFormatter> formatter,
                   std::shared_ptr<ILogFilter> filter,
                   std::shared_ptr<IRotationPolicy> rotation_policy)
    : file_path_(std::move(file_path)),
      formatter_(std::move(formatter)),
      filter_(std::move(filter)),
      rotation_policy_(std::move(rotation_policy)) {
  if (!formatter_) {
    throw std::invalid_argument("FileSink: formatter cannot be null");
  }
  OpenFile();
}

FileSink::~FileSink() {
  std::lock_guard<std::mutex> lock(mutex_);
  if (file_stream_.is_open()) {
    file_stream_.flush();
    file_stream_.close();
  }
}

void FileSink::Write(const LogRecord& record,
                     std::string_view formatted_message) {
  std::lock_guard<std::mutex> lock(mutex_);

  // Самовосстановление (Self-Healing): если поток в состоянии ошибки, пробуем
  // переоткрыть
  if (!file_stream_.is_open() || !file_stream_.good()) {  // LCOV_EXCL_LINE
    file_stream_.clear();                                 // LCOV_EXCL_LINE
    if (file_stream_.is_open()) {                         // LCOV_EXCL_LINE
      file_stream_.close();                               // LCOV_EXCL_LINE
    }                                                     // LCOV_EXCL_LINE
    OpenFile();                                           // LCOV_EXCL_LINE
    if (!file_stream_.is_open()) {                        // LCOV_EXCL_LINE
      std::cerr
          << "[FileSink ERROR] Cannot write, file is not open: "  // LCOV_EXCL_LINE
          << file_path_ << "\n";  // LCOV_EXCL_LINE
      return;                     // LCOV_EXCL_LINE
    }                             // LCOV_EXCL_LINE
  }                               // LCOV_EXCL_LINE

  // Проверка необходимости ротации
  auto now = std::chrono::system_clock::now();
  RotateIfNeeded(now);

  // Физическая запись данных (используем binary для отсутствия накладных
  // расходов на конвертацию)
  file_stream_.write(formatted_message.data(),
                     static_cast<std::streamsize>(formatted_message.size()));

  // Инкремент размера в памяти
  current_size_ += formatted_message.size();
}

void FileSink::Flush() {
  std::lock_guard<std::mutex> lock(mutex_);
  if (file_stream_.is_open()) {
    file_stream_.flush();
  }
}

std::shared_ptr<ILogFormatter> FileSink::GetFormatter() const noexcept {
  return formatter_;
}

std::shared_ptr<ILogFilter> FileSink::GetFilter() const noexcept {
  return filter_;
}

void FileSink::OpenFile() {
  // Используем std::ios::app для дозаписи и std::ios::binary для отключения
  // трансляции символов (актуально при портировании на Windows) и ускорения
  // I/O.
  file_stream_.open(file_path_, std::ios::app | std::ios::binary);

  if (!file_stream_.is_open()) {
    std::cerr << "[FileSink ERROR] Failed to open file: " << file_path_ << "\n";
    return;
  }

  // Инициализируем текущий размер файла из файловой системы
  std::error_code ec;
  auto size = fs::file_size(file_path_, ec);
  if (ec) {                // LCOV_EXCL_LINE
    current_size_ = 0;     // LCOV_EXCL_LINE
  } else {                 // LCOV_EXCL_LINE
    current_size_ = size;  // LCOV_EXCL_LINE
  }                        // LCOV_EXCL_LINE
}

void FileSink::RotateIfNeeded(std::chrono::system_clock::time_point now) {
  if (!rotation_policy_) {
    return;
  }

  if (!rotation_policy_->ShouldRotate(current_size_, now)) {
    return;
  }

  // 1. Закрываем текущий файл
  file_stream_.flush();
  file_stream_.close();

  // 2. Переименование (архивация), если требует политика
  std::string rotated_path;
  if (rotation_policy_->RequiresArchiving()) {
    rotated_path = rotation_policy_->GenerateRotatedFileName(file_path_, now);

    std::error_code ec;
    fs::rename(file_path_, rotated_path, ec);
    if (ec) {
      std::cerr << "[FileSink ERROR] Failed to rename file for rotation: "
                << ec.message() << "\n";
    }
  }

  // 3. Открываем новый чистый файл
  OpenFile();

  // 4. Уведомляем политику о завершении ротации (для очистки старых архивов)
  rotation_policy_->OnRotationCompleted(file_path_, rotated_path);
}

}  // namespace stc::logger