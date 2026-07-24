/**
@file filterservice.cpp
@brief Основная точка входа в программу.
@version 2.0.0
@date 2026-07-17
*/
#include "../include/service_controller.hpp"

int main(int argc, char **argv) {
    stc::ServiceController controller;
    return controller.Run(argc, argv);
}