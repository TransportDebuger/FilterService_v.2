#include "stc/logger/filters/source_filter.hpp"

namespace stc::logger {

SourceFilter::SourceFilter(std::string pattern, SourceMatchTarget target,
                           SourceMatchMode mode, bool invert)
    : pattern_(std::move(pattern)),
      target_(target),
      mode_(mode),
      invert_(invert) {}

bool SourceFilter::ShouldPass(const LogRecord& record) const {
  // Извлекаем нужную часть source_location как string_view (без аллокаций)
  std::string_view source_value;

  if (target_ == SourceMatchTarget::kFileName) {
    source_value = record.location.file_name();
  } else {
    source_value = record.location.function_name();
  }

  bool is_match = Match(source_value);

  // Если invert == true (Blacklist), мы пропускаем только те, которые НЕ
  // совпали. Если invert == false (Whitelist), мы пропускаем только те, которые
  // совпали.
  return invert_ ? !is_match : is_match;
}

bool SourceFilter::Match(std::string_view source_value) const {
  switch (mode_) {
    case SourceMatchMode::kExact:
      return source_value == pattern_;

    case SourceMatchMode::kContains:
      return source_value.find(pattern_) != std::string_view::npos;

    case SourceMatchMode::kStartsWith:
      // Безопасное сравнение начала строки
      if (source_value.size() < pattern_.size()) {
        return false;
      }
      return source_value.substr(0, pattern_.size()) == pattern_;

    default:
      return false;
  }
}

}  // namespace stc::logger