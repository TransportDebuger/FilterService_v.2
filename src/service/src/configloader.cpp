#include "../include/configloader.hpp"

#include <fstream>
#include <sstream>

nlohmann::json ConfigLoader::loadFromFile(const std::string &filename) {
  lastLoadedFile = filename;
  return readFileContents(filename);
}

nlohmann::json ConfigLoader::reload(const std::string &currentFile) {
  if (currentFile.empty()) {
    throw std::runtime_error("ConfigLoader: no file specified for reload");
  }
  return readFileContents(currentFile);
}

nlohmann::json
ConfigLoader::readFileContents(const std::string &filename) const {
  std::ifstream file(filename);

  if (!file.is_open()) {
    throw std::runtime_error("ConfigLoader: Failed to open file " + filename);
  }

  try {
    nlohmann::json config;
    file >> config;
    return config;
  } catch (const nlohmann::json::parse_error &e) {
    std::stringstream ss;
    ss << "ConfigLoader: JSON parse error: " << e.what() << " at byte "
       << e.byte;
    throw std::runtime_error(ss.str());
  }
}