#include "stc/logger/filters/composite_filter.hpp"

#include <stdexcept>

namespace stc::logger {

CompositeFilter::CompositeFilter(
    std::vector<std::shared_ptr<ILogFilter>> filters, LogicOperator op)
    : filters_(std::move(filters)), op_(op) {
  if (filters_.empty()) {
    throw std::invalid_argument(
        "CompositeFilter: filters vector cannot be empty");
  }
}

bool CompositeFilter::ShouldPass(const LogRecord& record) const {
  if (op_ == LogicOperator::kAnd) {
    // Short-circuit AND: return false on the first failure
    for (const auto& filter : filters_) {
      if (!filter->ShouldPass(record)) {
        return false;
      }
    }
    return true;
  }

  // LogicOperator::kOr
  // Short-circuit OR: return true on the first success
  for (const auto& filter : filters_) {
    if (filter->ShouldPass(record)) {
      return true;
    }
  }
  return false;
}

}  // namespace stc::logger