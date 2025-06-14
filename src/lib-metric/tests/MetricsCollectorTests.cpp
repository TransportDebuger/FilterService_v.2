#include <gtest/gtest.h>
#include "stc/MetricsCollector.hpp"
#include <thread>
#include <vector>
#include <atomic>

using namespace stc;

class MetricsCollectorTest : public ::testing::Test {
protected:
    void SetUp() override {
        collector_ = &MetricsCollector::instance();
        // Сброс состояния перед каждым тестом
        collector_->exportPrometheus(); 
    }

    MetricsCollector* collector_;
};

// 1. Тест базовой функциональности счетчиков
TEST_F(MetricsCollectorTest, CounterBasicOperations) {
    // Регистрация нового счетчика
    EXPECT_NO_THROW(collector_->registerCounter("requests", "Total requests count"));
    
    // Инкремент значения
    collector_->incrementCounter("requests");
    collector_->incrementCounter("requests", 4.5);
    
    // Проверка экспорта
    std::string metrics = collector_->exportPrometheus();
    ASSERT_NE(metrics.find("requests 5.5"), std::string::npos);
    ASSERT_NE(metrics.find("# HELP requests Total requests count"), std::string::npos);
}

// 2. Тест обработки ошибок
TEST_F(MetricsCollectorTest, ErrorHandling) {
    // Попытка регистрации дубликата
    collector_->registerCounter("errors");
    EXPECT_THROW(collector_->registerCounter("errors"), std::runtime_error);
    
    // Инкремент незарегистрированного счетчика
    EXPECT_NO_THROW(collector_->incrementCounter("unknown_metric"));
}

// 3. Тест работы с временем выполнения задач
TEST_F(MetricsCollectorTest, TaskTimeRecording) {
    // Запись времени выполнения
    collector_->recordTaskTime("db_query", std::chrono::milliseconds(150));
    collector_->recordTaskTime("db_query", std::chrono::milliseconds(350));
    
    // Проверка формата экспорта
    std::string metrics = collector_->exportPrometheus();
    ASSERT_NE(metrics.find("db_query_sum 500"), std::string::npos);
    ASSERT_NE(metrics.find("db_query_count 2"), std::string::npos);
}

// 4. Тест многопоточной работы
TEST_F(MetricsCollectorTest, ConcurrentAccess) {
    constexpr int THREADS = 4;
    constexpr int ITERATIONS = 10000;
    
    collector_->registerCounter("concurrent_counter");
    
    auto worker = [this]() {
        for (int i = 0; i < ITERATIONS; ++i) {
            collector_->incrementCounter("concurrent_counter");
        }
    };
    
    std::vector<std::thread> threads;
    for (int i = 0; i < THREADS; ++i) {
        threads.emplace_back(worker);
    }
    
    for (auto& t : threads) {
        t.join();
    }
    
    std::string metrics = collector_->exportPrometheus();
    ASSERT_NE(metrics.find("concurrent_counter " + 
        std::to_string(THREADS * ITERATIONS)), std::string::npos);
}

// 5. Тест граничных случаев
TEST_F(MetricsCollectorTest, EdgeCases) {
    // Пустые имена
    EXPECT_THROW(collector_->registerCounter(""), std::runtime_error);
    
    // Отрицательные значения
    collector_->registerCounter("negative_values");
    collector_->incrementCounter("negative_values", -10.5);
    std::string metrics = collector_->exportPrometheus();
    ASSERT_NE(metrics.find("negative_values -10.5"), std::string::npos);
    
    // Очень большие числа
    collector_->registerCounter("large_numbers");
    collector_->incrementCounter("large_numbers", 1e18);
    metrics = collector_->exportPrometheus();
    ASSERT_NE(metrics.find("large_numbers 1e+18"), std::string::npos);
}

// 6. Тест формата экспорта
TEST_F(MetricsCollectorTest, ExportFormatValidation) {
    collector_->registerCounter("format_test", "Test help text");
    collector_->incrementCounter("format_test", 3.14);
    
    std::string expected = 
        "# TYPE metrics_collector_counter counter\n"
        "# HELP metrics_collector_format_test Test help text\n"
        "metrics_collector_format_test 3.14\n";
    
    std::string actual = collector_->exportPrometheus();
    ASSERT_TRUE(actual.find(expected) != std::string::npos);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}