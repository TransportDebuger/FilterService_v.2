#include "stc/logger/sinks/file/size_rotation_policy.hpp"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace stc::logger {

SizeRotationPolicy::SizeRotationPolicy(std::uint64_t max_size_bytes,
                                       std::size_t max_archives)
    : max_size_bytes_(max_size_bytes), max_archives_(max_archives) {
  if (max_size_bytes_ == 0) {
    throw std::invalid_argument(
        "SizeRotationPolicy: max_size_bytes must be greater than 0");
  }
}

bool SizeRotationPolicy::ShouldRotate(
    std::uint64_t current_file_size_bytes,
    std::chrono::system_clock::time_point /*current_time*/) const {
  return current_file_size_bytes >= max_size_bytes_;
}

std::string SizeRotationPolicy::GenerateRotatedFileName(
    const std::string& original_file_path,
    std::chrono::system_clock::time_point /*rotation_time*/) const {
  fs::path orig_path(original_file_path);
  fs::path dir = orig_path.parent_path();
  std::string stem = orig_path.filename().string();

  std::size_t max_index = 0;
  std::error_code ec;

  // Сканируем директорию для поиска существующих архивов
  if (fs::exists(dir, ec) && fs::is_directory(dir, ec)) {
    for (const auto& entry : fs::directory_iterator(dir, ec)) {
      if (!entry.is_regular_file(ec)) continue;

      std::string name = entry.path().filename().string();
      // Проверяем, начинается ли имя файла с базового имени + "."
      if (name.starts_with(stem + ".")) {
        std::string suffix = name.substr(stem.length() + 1);
        try {
          // Пытаемся преобразовать суффикс в число
          std::size_t idx = std::stoul(suffix);
          if (idx > max_index) {
            max_index = idx;
          }
        } catch (const std::exception&) {
          // Игнорируем файлы с нечисловыми суффиксами (например, app.log.tmp)
        }
      }
    }
  }

  // Возвращаем путь с следующим по порядку индексом
  return original_file_path + "." + std::to_string(max_index + 1);
}

void SizeRotationPolicy::OnRotationCompleted(
    const std::string& original_file_path,
    const std::string& /*rotated_file_path*/) {
  fs::path orig_path(original_file_path);
  fs::path dir = orig_path.parent_path();
  std::string stem = orig_path.filename().string();

  std::vector<fs::path> archives;
  std::error_code ec;

  if (!fs::exists(dir, ec) || !fs::is_directory(dir, ec)) {
    return;  // Директория недоступна, очистку не производим
  }

  // Собираем все валидные архивы
  for (const auto& entry : fs::directory_iterator(dir, ec)) {
    if (!entry.is_regular_file(ec)) continue;

    std::string name = entry.path().filename().string();
    if (name.starts_with(stem + ".")) {
      std::string suffix = name.substr(stem.length() + 1);

      // Убеждаемся, что суффикс строго числовой
      bool is_numeric =
          !suffix.empty() &&
          std::all_of(suffix.begin(), suffix.end(),
                      [](unsigned char c) { return std::isdigit(c); });
      if (is_numeric) {
        archives.push_back(entry.path());
      }
    }
  }

  // Если архивов меньше или равно лимиту, ничего не удаляем
  if (archives.size() <= max_archives_) {
    return;
  }

  // Сортируем архивы по времени последней модификации (от старых к новым)
  std::sort(archives.begin(), archives.end(),
            [](const fs::path& a, const fs::path& b) {
              std::error_code ec_a, ec_b;
              auto time_a = fs::last_write_time(a, ec_a);
              auto time_b = fs::last_write_time(b, ec_b);
              // Если время не удалось получить, считаем файл "старым"
              if (ec_a) return true;
              if (ec_b) return false;
              return time_a < time_b;
            });

  // Удаляем самые старые файлы, пока не останемся в рамках лимита
  std::size_t files_to_delete = archives.size() - max_archives_;
  for (std::size_t i = 0; i < files_to_delete; ++i) {
    fs::remove(
        archives[i],
        ec);  // Игнорируем ошибки удаления (например, если файл уже удален)
  }
}

bool SizeRotationPolicy::RequiresArchiving() const noexcept { return true; }

}  // namespace stc::logger