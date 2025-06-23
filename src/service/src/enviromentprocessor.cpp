#include "../include/enviromentprocessor.hpp"

#include <algorithm>
#include <cstdlib>

using namespace std;

void EnvironmentProcessor::process(nlohmann::json &config) const {
  walkJson(config, [this](string &value) { resolveVariable(value); });
}

void EnvironmentProcessor::walkJson(nlohmann::json &node,
                                    function<void(string &)> func) const {
  if (node.is_object()) {
    for (auto &[key, value] : node.items()) {
      walkJson(value, func);
    }
  } else if (node.is_array()) {
    for (auto &element : node) {
      walkJson(element, func);
    }
  } else if (node.is_string()) {
    string str = node.get<string>();
    func(str);
    node = str;
  }
}

void EnvironmentProcessor::resolveVariable(std::string &value) const {
  size_t start_pos = 0;
  const string prefix = "$ENV{";

  while ((start_pos = value.find(prefix, start_pos)) != string::npos) {
    size_t end_pos = value.find('}', start_pos + prefix.length());
    if (end_pos == string::npos)
      break;

    string var_name = value.substr(start_pos + prefix.length(),
                                   end_pos - start_pos - prefix.length());

    if (const char *env_val = getenv(var_name.c_str())) {
      value.replace(start_pos, end_pos - start_pos + 1, env_val);
      start_pos += strlen(env_val);
    } else {
      start_pos = end_pos + 1;
    }
  }
}