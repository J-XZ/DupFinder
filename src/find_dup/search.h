#pragma once

#include "json/single_include/nlohmann/json.hpp"
#include <filesystem>
#include <fstream>

class DuplicateFinder {
public:
  DuplicateFinder(std::filesystem::path dir, std::ofstream &output_json_path);
  void process();

private:
  std::ofstream &output_;
  std::filesystem::path search_dir_;
  nlohmann::json oj_;

  void process(nlohmann::json &output);
};