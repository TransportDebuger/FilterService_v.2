#include <gtest/gtest.h>
#include "stc/ilogger.hpp"

TEST(TimeFormatterTest, BasicFormat) {
    auto now = std::chrono::system_clock::now();
    std::string formatted = stc::TimeFormatter::format(now);
    EXPECT_EQ(formatted.size(), 8);
}