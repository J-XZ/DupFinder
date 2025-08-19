#include "json/single_include/nlohmann/json.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>

auto main(int argc, char *argv[]) -> int {
  if (argc != 2) {
    std::cerr << "Usage: " << argv[0] << " <json_path>" << std::endl;
    return 1;
  }
  nlohmann::json input_file = nlohmann::json::parse(std::ifstream(argv[1]));
  const auto &file_list = input_file["items"];
  if (!file_list.is_array()) {
    std::cerr << "`items` is not an array\n";
    return 1;
  }

  for (const auto &item : file_list) {
    if (!item.is_string()) {
      std::cerr << "Item is not a string: " << item.dump() << std::endl;
      return 1;
    } else {
      std::filesystem::path file_path(item.get<std::string>());
      {
        try {
          std::filesystem::remove(file_path);
          std::cout << "Removing file " << file_path << std::endl;
        } catch (const std::filesystem::filesystem_error &e) {
          std::cerr << "Error removing file " << file_path << ": " << e.what()
                    << std::endl;
        }
      }
    }
  }
  return 0;
}