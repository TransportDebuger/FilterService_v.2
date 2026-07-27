#include <benchmark/benchmark.h>

#include <filesystem>
#include <memory>

#include "stc/logger/formatters/text_formatter.hpp"
#include "stc/logger/logger.hpp"
#include "stc/logger/sinks/file/async_file_sink.hpp"

namespace fs = std::filesystem;
using namespace stc::logger;

class LoggerFixture : public benchmark::Fixture {
 public:
  fs::path log_file_;
  void SetUp(const ::benchmark::State& state) {
    log_file_ = fs::temp_directory_path() /
                ("bench_" + std::to_string(state.thread_index()) + ".log");
  }
  void TearDown(const ::benchmark::State&) {
    std::error_code ec;
    fs::remove(log_file_, ec);
  }
};

// Базовый сценарий: очередь без ограничений (эмуляция текущего поведения)
BENCHMARK_DEFINE_F(LoggerFixture, AsyncSink_Unlimited)
(benchmark::State& state) {
  auto formatter = std::make_shared<TextFormatter>("%msg\n");
  auto sink = std::make_shared<AsyncFileSink>(
      log_file_.string(), formatter, nullptr, nullptr, 64 * 1024,
      std::chrono::milliseconds(100), 0,
      OverflowPolicy::kDrop);

  Logger logger("Bench");
  logger.AddSink(sink);

  for (auto _ : state) {
    logger.Info("Benchmark message for unlimited queue testing");
  }
  logger.Flush();
}
BENCHMARK_REGISTER_F(LoggerFixture, AsyncSink_Unlimited)
    ->Threads(4)
    ->Threads(16);

// Сценарий kDrop: искусственное ограничение очереди для измерения стоимости
// проверки и дропа
BENCHMARK_DEFINE_F(LoggerFixture, AsyncSink_DropPolicy)
(benchmark::State& state) {
  auto formatter = std::make_shared<TextFormatter>("%msg\n");
  auto sink = std::make_shared<AsyncFileSink>(
      log_file_.string(), formatter, nullptr, nullptr, 64 * 1024,
      std::chrono::milliseconds(5000),  // Длинный интервал для накопления
      100, OverflowPolicy::kDrop);

  Logger logger("Bench");
  logger.AddSink(sink);

  for (auto _ : state) {
    logger.Info("Benchmark message for drop policy");
  }
  logger.Flush();
}
BENCHMARK_REGISTER_F(LoggerFixture, AsyncSink_DropPolicy)
    ->Threads(4)
    ->Threads(16);

// Сценарий kBlock: измерение стоимости переключения контекста и ожидания
// мьютекса/CV
BENCHMARK_DEFINE_F(LoggerFixture, AsyncSink_BlockPolicy)
(benchmark::State& state) {
  auto formatter = std::make_shared<TextFormatter>("%msg\n");
  auto sink = std::make_shared<AsyncFileSink>(
      log_file_.string(), formatter, nullptr, nullptr, 64 * 1024,
      std::chrono::milliseconds(100), 100, OverflowPolicy::kBlock);

  Logger logger("Bench");
  logger.AddSink(sink);

  for (auto _ : state) {
    logger.Info("Benchmark message for block policy");
  }
  logger.Flush();
}
BENCHMARK_REGISTER_F(LoggerFixture, AsyncSink_BlockPolicy)
    ->Threads(4)
    ->Threads(16);

BENCHMARK_MAIN();