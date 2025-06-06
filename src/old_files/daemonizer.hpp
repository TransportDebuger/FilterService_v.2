#pragma once

#include <fcntl.h>
#include <unistd.h>

#include <cstdlib>
#include <stdexcept>

class Daemonizer {
 public:
  static void daemonize();
};