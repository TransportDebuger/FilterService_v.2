/*!
  \file filterservice.cpp
  \author Artem Ulyanov
  \version 1
  \date March, 2024
  \brief Основной файл проекта filter_service.
  \details Основной файл проекта filter_service. Описывает функцию основной точки входа в программу.
*/

#include "../includes/service_controller.hpp"

int main(int argc, char** argv) {
    ServiceController controller;
    return controller.run(argc, argv);
}