#pragma once

#include <unistd.h>
#include <fcntl.h>
#include <cstdlib>
#include <stdexcept>
	

class Daemonizer {
public:
    static void daemonize();
};