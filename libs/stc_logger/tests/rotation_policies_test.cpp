#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <thread>

#include "stc/logger/sinks/file/circular_rotation_policy.hpp"
#include "stc/logger/sinks/file/size_rotation_policy.hpp"
#include "stc/logger/sinks/file/time_rotation_policy.hpp"

namespace fs = std::filesystem;
namespace stc::logger::test {

// ============================================================================
// Фикстура теста
// ============================================================================

class RotationPoliciesTest : public ::testing::Test {
 protected:
  fs::path test_dir_;
  fs::path log_file_;

  void SetUp() override {
    // Генерируем уникальное имя директории на основе времени
    auto unique_id =
        std::chrono::steady_clock::now().time_since_epoch().count();
    test_dir_ = fs::temp_directory_path() /
                ("stc_logger_rot_test_" + std::to_string(unique_id));
    fs::create_directories(test_dir_);
    log_file_ = test_dir_ / "test.log";

    // Создаем пустой лог-файл для тестов
    std::ofstream(log_file_).close();
  }

  void TearDown() override {
    std::error_code ec;
    fs::remove_all(test_dir_, ec);
  }

  // Helper для создания файла с заданным размером
  void CreateFileWithSize(const fs::path& path, std::uint64_t size_bytes) {
    std::ofstream ofs(path, std::ios::binary);
    std::vector<char> buffer(size_bytes, 'A');
    ofs.write(buffer.data(), buffer.size());
  }
};

// ============================================================================
// 1. Тесты SizeRotationPolicy
// ============================================================================

TEST_F(RotationPoliciesTest, SizeRotationPolicy_ShouldRotate_Threshold) {
  SizeRotationPolicy policy(100);  // 100 bytes limit

  EXPECT_FALSE(policy.ShouldRotate(50, std::chrono::system_clock::now()));
  EXPECT_FALSE(policy.ShouldRotate(99, std::chrono::system_clock::now()));
  EXPECT_TRUE(policy.ShouldRotate(100, std::chrono::system_clock::now()));
  EXPECT_TRUE(policy.ShouldRotate(150, std::chrono::system_clock::now()));
}

TEST_F(RotationPoliciesTest, SizeRotationPolicy_ZeroSize_Throws) {
  EXPECT_THROW(SizeRotationPolicy(0), std::invalid_argument);
}

TEST_F(RotationPoliciesTest, SizeRotationPolicy_Naming_IncrementalIndex) {
  SizeRotationPolicy policy(100, 5);

  // Создаем фиктивные архивы
  CreateFileWithSize(test_dir_ / "test.log.1", 50);
  CreateFileWithSize(test_dir_ / "test.log.2", 50);
  // Посторонний файл, который не должен влиять на индексацию
  CreateFileWithSize(test_dir_ / "test.log.tmp", 10);

  std::string rotated_name = policy.GenerateRotatedFileName(
      log_file_.string(), std::chrono::system_clock::now());

  EXPECT_EQ(rotated_name, (test_dir_ / "test.log.3").string());
}

TEST_F(RotationPoliciesTest, SizeRotationPolicy_Cleanup_RemovesOldest) {
  SizeRotationPolicy policy(100, 2);  // Лимит 2 архива

  // Создаем 3 архива с разными временами модификации
  fs::path f1 = test_dir_ / "test.log.1";
  fs::path f2 = test_dir_ / "test.log.2";
  fs::path f3 = test_dir_ / "test.log.3";

  CreateFileWithSize(f1, 50);
  std::this_thread::sleep_for(
      std::chrono::milliseconds(50));  // Гарантируем разное время
  CreateFileWithSize(f2, 50);
  std::this_thread::sleep_for(std::chrono::milliseconds(50));
  CreateFileWithSize(f3, 50);

  // Имитируем завершение ротации (создание нового архива)
  policy.OnRotationCompleted(log_file_.string(),
                             (test_dir_ / "test.log.4").string());

  // Проверяем, что самый старый файл (f1) удален
  EXPECT_FALSE(fs::exists(f1)) << "Oldest archive should be removed";
  EXPECT_TRUE(fs::exists(f2));
  EXPECT_TRUE(fs::exists(f3));
}

TEST_F(RotationPoliciesTest,
       SizeRotationPolicy_OnRotationCompleted_InvalidDir_ReturnsEarly) {
  SizeRotationPolicy policy(100, 2);

  // Путь, родительская директория которого заведомо не существует
  std::string invalid_path = "/non_existent_directory_xyz_123/test.log";

  // Вызов не должен приводить к падениям, исключениям или попыткам сканирования
  EXPECT_NO_THROW(policy.OnRotationCompleted(invalid_path, ""));
}

// ============================================================================
// 2. Тесты TimeRotationPolicy
// ============================================================================

TEST_F(RotationPoliciesTest, TimeRotationPolicy_ShouldRotate_Threshold) {
  auto now = std::chrono::system_clock::now();
  TimeRotationPolicy policy(std::chrono::seconds(3600));  // 1 hour

  // Время еще не наступило
  EXPECT_FALSE(policy.ShouldRotate(0, now));

  // Время наступило (или прошло)
  auto future = now + std::chrono::seconds(7200);
  EXPECT_TRUE(policy.ShouldRotate(0, future));
}

TEST_F(RotationPoliciesTest, TimeRotationPolicy_InvalidInterval_Throws) {
  EXPECT_THROW(TimeRotationPolicy(std::chrono::seconds(0)),
               std::invalid_argument);
  EXPECT_THROW(TimeRotationPolicy(std::chrono::seconds(-10)),
               std::invalid_argument);
}

TEST_F(RotationPoliciesTest, TimeRotationPolicy_Naming_TimestampFormat) {
  TimeRotationPolicy policy(std::chrono::hours(24), "%Y%m%d");

  // Фиксированное время: 2023-10-27 15:30:00
  auto tp = std::chrono::system_clock::from_time_t(1698420600);

  std::string rotated_name =
      policy.GenerateRotatedFileName(log_file_.string(), tp);

  // Ожидаем суффикс с датой
  EXPECT_NE(rotated_name.find("20231027"), std::string::npos);
}

TEST_F(RotationPoliciesTest, TimeRotationPolicy_DriftPrevention) {
  // Критический тест: проверяем, что время следующей ротации
  // инкрементируется на ТОЧНЫЙ интервал, а не на текущее время.
  TimeRotationPolicy policy(std::chrono::seconds(100));

  auto now = std::chrono::system_clock::now();

  // Имитируем 3 цикла ротации
  policy.OnRotationCompleted(log_file_.string(), "dummy");
  policy.OnRotationCompleted(log_file_.string(), "dummy");
  policy.OnRotationCompleted(log_file_.string(), "dummy");

  // Проверяем, что ShouldRotate срабатывает строго через 3 * interval
  auto expected_trigger_time =
      now +
      std::chrono::seconds(
          400);  // 4 * 100s (1 от конструктора + 3 от OnRotationCompleted)

  // Чуть раньше не должно срабатывать (с учетом погрешности выполнения теста)
  EXPECT_FALSE(
      policy.ShouldRotate(0, expected_trigger_time - std::chrono::seconds(5)));

  // В ожидаемое время и позже - должно
  EXPECT_TRUE(
      policy.ShouldRotate(0, expected_trigger_time + std::chrono::seconds(5)));
}

TEST_F(RotationPoliciesTest, TimeRotationPolicy_RequiresArchiving_ReturnsTrue) {
  TimeRotationPolicy policy(std::chrono::seconds(10));
  // Метод должен возвращать true, так как временные метки уникальны
  // и файлы должны переименовываться, а не перезаписываться.
  EXPECT_TRUE(policy.RequiresArchiving());
}

TEST_F(RotationPoliciesTest, TimeRotationPolicy_EmptyFormat_Throws) {
  // Передаем валидный интервал, но пустую строку в качестве формата времени
  EXPECT_THROW(TimeRotationPolicy(std::chrono::seconds(10), ""),
               std::invalid_argument);
}

// ============================================================================
// 3. Тесты CircularRotationPolicy
// ============================================================================

TEST_F(RotationPoliciesTest, CircularRotationPolicy_ShouldRotate_Threshold) {
  CircularRotationPolicy policy(100, 3);

  EXPECT_FALSE(policy.ShouldRotate(50, std::chrono::system_clock::now()));
  EXPECT_TRUE(policy.ShouldRotate(100, std::chrono::system_clock::now()));
}

TEST_F(RotationPoliciesTest, CircularRotationPolicy_ZeroLimits_Throws) {
  EXPECT_THROW(CircularRotationPolicy(0, 3), std::invalid_argument);
  EXPECT_THROW(CircularRotationPolicy(100, 0), std::invalid_argument);
}

TEST_F(RotationPoliciesTest, CircularRotationPolicy_Modulo_Indexing) {
  CircularRotationPolicy policy(100, 3);  // 3 слота: 0, 1, 2

  auto now = std::chrono::system_clock::now();

  // Цикл 1: Индекс 0
  EXPECT_EQ(policy.GenerateRotatedFileName(log_file_.string(), now),
            (test_dir_ / "test.log.0").string());
  policy.OnRotationCompleted(log_file_.string(), "dummy");

  // Цикл 2: Индекс 1
  EXPECT_EQ(policy.GenerateRotatedFileName(log_file_.string(), now),
            (test_dir_ / "test.log.1").string());
  policy.OnRotationCompleted(log_file_.string(), "dummy");

  // Цикл 3: Индекс 2
  EXPECT_EQ(policy.GenerateRotatedFileName(log_file_.string(), now),
            (test_dir_ / "test.log.2").string());
  policy.OnRotationCompleted(log_file_.string(), "dummy");

  // Цикл 4: Возврат к Индексу 0 (перезапись)
  EXPECT_EQ(policy.GenerateRotatedFileName(log_file_.string(), now),
            (test_dir_ / "test.log.0").string());
}

TEST_F(RotationPoliciesTest,
       CircularRotationPolicy_RequiresArchiving_ReturnsTrue) {
  CircularRotationPolicy policy(100, 3);
  // CircularRotationPolicy всегда требует архивации (перезаписи слота),
  // чтобы гарантировать, что старые данные будут корректно затерты новыми.
  EXPECT_TRUE(policy.RequiresArchiving());
}

}  // namespace stc::logger::test