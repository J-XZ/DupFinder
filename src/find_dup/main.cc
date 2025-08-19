#include "search.h"
#include <filesystem>
#include <fstream>
#include <iostream>

auto main(int argc, char *argv[]) -> int {
  if (argc < 3) {
    std::cerr << "Usage: " << argv[0] << " <file_path> <output_json_path>"
              << std::endl;
    return 1;
  } else {
    // std::cout << "arg 1 " << argv[1] << std::endl;
    // std::cout << "arg 2 " << argv[2] << std::endl;
  }
  std::string search_dir = argv[1];

  std::filesystem::path dir(search_dir);

  if (!std::filesystem::exists(dir)) {
    std::cerr << "Directory does not exist: " << search_dir << std::endl;
    return 1;
  } else {
    if (!std::filesystem::is_directory(dir)) {
      std::cerr << "Not a directory: " << search_dir << std::endl;
      return 1;
    }
  }

  std::string output_json_path = argv[2];
  std::filesystem::path output = output_json_path;

  if (std::filesystem::exists(output_json_path)) {
    std::cerr << "Output file already exists: " << output_json_path
              << std::endl;
    return 1;
  } else {
    if (!output.has_extension()) {
      output = output.replace_extension(".json");
    } else {
      if (output.extension().string() != ".json") {
        std::cerr << "Output file must have a .json extension: "
                  << output_json_path << std::endl;
        return 1;
      }
    }
  }

  auto output_fd = std::ofstream(output);
  if (!output_fd.is_open()) {
    std::cerr << "Failed to open output file: " << output_json_path
              << std::endl;
    return 1;
  }

  std::cout << "Searching for duplicates in: " << search_dir << std::endl;
  std::cout << "Output JSON will be saved to: " << output_json_path
            << std::endl;

  DuplicateFinder d(dir, output_fd);
  d.process();

  output_fd.flush();
  output_fd.close();
}