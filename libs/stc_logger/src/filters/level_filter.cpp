#include "stc/logger/filters/level_filter.hpp"

namespace stc::logger {

bool LevelFilter::ShouldPass(const LogRecord& record) const {
  // Сравнение enum class работает напрямую, так как уровни
  // упорядочены по возрастанию критичности (Trace=0 ... Critical=5).
  return record.level >= min_level_;
}

}  // namespace stc::logger