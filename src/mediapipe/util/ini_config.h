// Lightweight INI config file parser for face_mesh
// No external dependencies — pure C++11 with STL only.
//
// Format:
//   [section]
//   key = value
//   # comment
//   ; comment

#ifndef MEDIAPIPE_UTIL_INI_CONFIG_H_
#define MEDIAPIPE_UTIL_INI_CONFIG_H_

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include <unordered_map>

namespace mediapipe {

class IniConfig {
 public:
  using Section = std::unordered_map<std::string, std::string>;
  using Data = std::unordered_map<std::string, Section>;

  // Parse an INI file from disk. Returns true on success.
  bool LoadFromFile(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
      fprintf(stderr, "INI: Cannot open config file: %s\n", path.c_str());
      return false;
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    Parse(buffer.str());
    fprintf(stderr, "INI: Loaded config from %s (%d sections)\n",
            path.c_str(), (int)data_.size());
    return true;
  }

  // Get a value from a section/key, with an optional default.
  std::string Get(const std::string& section, const std::string& key,
                  const std::string& default_value = "") const {
    auto sec = data_.find(section);
    if (sec != data_.end()) {
      auto kv = sec->second.find(key);
      if (kv != sec->second.end()) {
        return kv->second;
      }
    }
    return default_value;
  }

  // Get a boolean value
  bool GetBool(const std::string& section, const std::string& key,
               bool default_value = false) const {
    std::string v = Get(section, key, "");
    if (v.empty()) return default_value;
    std::transform(v.begin(), v.end(), v.begin(), ::tolower);
    return v == "true" || v == "yes" || v == "1" || v == "on";
  }

  // Get an integer value
  int GetInt(const std::string& section, const std::string& key,
             int default_value = 0) const {
    std::string v = Get(section, key, "");
    if (v.empty()) return default_value;
    try {
      return std::stoi(v);
    } catch (...) {
      return default_value;
    }
  }

  // Check if a section exists
  bool HasSection(const std::string& section) const {
    return data_.find(section) != data_.end();
  }

  // Check if a key exists in a section
  bool HasKey(const std::string& section, const std::string& key) const {
    auto sec = data_.find(section);
    if (sec != data_.end()) {
      return sec->second.find(key) != sec->second.end();
    }
    return false;
  }

 private:
  Data data_;

  void Parse(const std::string& content) {
    std::istringstream stream(content);
    std::string line;
    std::string current_section;

    while (std::getline(stream, line)) {
      // Trim leading/trailing whitespace
      line = Trim(line);

      // Skip empty lines and comments
      if (line.empty() || line[0] == '#' || line[0] == ';') {
        continue;
      }

      // Section header: [section]
      if (line[0] == '[' && line.back() == ']') {
        current_section = line.substr(1, line.size() - 2);
        // Ensure section exists
        data_.emplace(current_section, Section());
        continue;
      }

      // Key = Value
      size_t eq = line.find('=');
      if (eq != std::string::npos) {
        std::string key = Trim(line.substr(0, eq));
        std::string value = Trim(line.substr(eq + 1));
        // Strip quotes if present
        if (value.size() >= 2 &&
            ((value.front() == '"' && value.back() == '"') ||
             (value.front() == '\'' && value.back() == '\''))) {
          value = value.substr(1, value.size() - 2);
        }
        if (!current_section.empty() && !key.empty()) {
          data_[current_section][key] = value;
        }
      }
    }
  }

  static std::string Trim(const std::string& s) {
    auto start = s.begin();
    while (start != s.end() && std::isspace(static_cast<unsigned char>(*start))) {
      ++start;
    }
    auto end = s.end();
    while (end != start && std::isspace(static_cast<unsigned char>(*(end - 1)))) {
      --end;
    }
    return std::string(start, end);
  }
};

}  // namespace mediapipe

#endif  // MEDIAPIPE_UTIL_INI_CONFIG_H_
