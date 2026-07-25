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
  
  // Проверка на наличие nullptr среди дочерних фильтров
  if (std::any_of(filters_.begin(), filters_.end(),
                  [](const auto& p) { return p == nullptr; })) {
    throw std::invalid_argument(
        "CompositeFilter: filters vector cannot contain nullptr");
  }
}

bool CompositeFilter::ShouldPass(const LogRecord& record) const {
  if (op_ == LogicOperator::kAnd) {
    for (const auto& filter : filters_) {
      if (!filter->ShouldPass(record)) {
        return false;
      }
    }
    return true;
  }
  
  // LogicOperator::kOr
  for (const auto& filter : filters_) {
    if (filter->ShouldPass(record)) {
      return true;
    }
  }
  return false;
}

}  // namespace stc::logger