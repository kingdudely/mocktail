#include "runtime/command_line.h"

#include <sstream>
#include <string>

namespace mocktail::runtime {
namespace {

bool ReadValue(int argc, const char* const argv[], int* index,
               const char* option, std::string* value, std::string* error) {
  if (index == nullptr || value == nullptr || error == nullptr ||
      *index + 1 >= argc || argv[*index + 1] == nullptr ||
      argv[*index + 1][0] == '\0') {
    if (error != nullptr) *error = std::string("missing value for ") + option;
    return false;
  }
  *value = argv[++(*index)];
  return true;
}

}  // namespace

CommandLineParseResult ParseCommandLine(int argc, const char* const argv[]) {
  CommandLineParseResult result;
  if (argc > 0 && argv != nullptr && argv[0] != nullptr &&
      argv[0][0] != '\0') {
    result.options.program_name = argv[0];
  }

  for (int index = 1; index < argc; ++index) {
    if (argv == nullptr || argv[index] == nullptr) {
      result.error = "invalid null command-line argument";
      return result;
    }

    const std::string argument = argv[index];
    if (argument == "--ROBLOSECURITY") {
      if (!ReadValue(argc, argv, &index, "--ROBLOSECURITY",
                     &result.options.roblosecurity, &result.error)) {
        return result;
      }
    } else if (argument == "--assets_dir") {
      if (!ReadValue(argc, argv, &index, "--assets_dir",
                     &result.options.asset_path, &result.error)) {
        return result;
      }
    } else if (argument == "--libroblox_file") {
      if (!ReadValue(argc, argv, &index, "--libroblox_file",
                     &result.options.roblox_library_path, &result.error)) {
        return result;
      }
    } else {
      result.error = "unknown option: " + argument;
      return result;
    }
  }

  return result;
}

std::string CommandLineUsage(const std::string& program_name) {
  std::ostringstream out;
  out << "Usage: " << (program_name.empty() ? "mocktail" : program_name)
      << " [options]\n\n"
      << "Options:\n"
      << "  --ROBLOSECURITY <value>  Roblox authentication cookie\n"
      << "  --assets_dir <path>      Roblox assets directory (default: ./assets)\n"
      << "  --libroblox_file <path>  libroblox.so path (default: ./libroblox.so)\n";
  return out.str();
}

}  // namespace mocktail::runtime
