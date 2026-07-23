/**
@file mock_system_calls.hpp
@brief Мок для IFileSystemSystemCalls, используемый в unit-тестах фабрики.
@version 1.0.0
@date 2026-07-22
*/
#pragma once

#include <gmock/gmock.h>
#include <sys/statfs.h>

#include "stc/fs/i_file_system_system_calls.hpp"

namespace stc::fs::testing {

class MockFileSystemSystemCalls : public IFileSystemSystemCalls {
 public:
  MOCK_METHOD(int, Init, (), (override));
  MOCK_METHOD(int, AddWatch, (int fd, const std::string& path, uint32_t mask),
              (override));
  MOCK_METHOD(ssize_t, Read, (int fd, void* buffer, std::size_t count),
              (override));
  MOCK_METHOD(int, RemoveWatch, (int fd, int wd), (override));
  MOCK_METHOD(int, Close, (int fd), (override));
  MOCK_METHOD(int, StatFs, (const std::string& path, struct statfs* buf),
              (override));
};

}  // namespace stc::fs::testing