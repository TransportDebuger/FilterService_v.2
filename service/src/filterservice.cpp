/**
 * @file filterservice.cpp
 * @brief Точка входа в приложение XmlFilterService.
 * @version 2.0.0
 * @date 2026-07-17
 */
#include <libxml/parser.h>
#include <libxml/xmlmemory.h>

#include "application/service_controller.hpp"

/**
 * @brief Точка входа в приложение.
 *
 * @param[in] argc Количество аргументов командной строки.
 * @param[in] argv Массив строк аргументов командной строки.
 * @return int Код завершения процесса (EXIT_SUCCESS или EXIT_FAILURE).
 * @since 1.0
 */
int main(int argc, char **argv) {
  xmlInitParser();
  LIBXML_TEST_VERSION

  stc::ServiceController controller;
  int result = controller.Run(argc, argv);

  xmlCleanupParser();

  return result;
}