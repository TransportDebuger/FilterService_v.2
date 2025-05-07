#pragma once

#include <chrono>
#include <string>

namespace stc {

enum class RotationType { NONE, SIZE, TIME };

struct RotationConfig {
  bool enabled = false;
  RotationType type = RotationType::NONE;
  size_t maxFileSizeBytes = 0;  //Максимальный размер лог-файла по достижению
                                //которого происходит ротация лога.
  std::chrono::seconds rotationInterval{0};  // например, 24h = 86400s
  std::chrono::system_clock::time_point lastRotationTime;
  std::string filenamePattern;

  RotationConfig() = default;
};

class IRotatableLogger {
    public:
        virtual void setRotationConfig(const RotationConfig& config) = 0;
        virtual RotationConfig getRotationConfig() const = 0;
        virtual ~IRotatableLogger() = default;
};

}  // namespace stc