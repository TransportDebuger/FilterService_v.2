/**
@file filterservice.cpp
@brief Основная точка входа в программу.
@version 2.0.0
@date 2026-07-17
*/
#include <libxml/parser.h>
#include <libxml/xmlmemory.h>

#include "application/service_controller.hpp"

int main(int argc, char **argv) {
  xmlInitParser();
  LIBXML_TEST_VERSION

  stc::ServiceController controller;
  int result = controller.Run(argc, argv);

  // Очистка глобальных структур libxml2
  xmlCleanupParser();

  return result;
}