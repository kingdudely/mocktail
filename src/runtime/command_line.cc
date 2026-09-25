#include "runtime/command_line.h"

#include <sstream>
#include <string>

namespace mocktail::runtime {
namespace {

bool IsRobloxScheme(const std::string& value) {
  const std::size_t colon = value.find(':');
  if (colon == std::string::npos || colon == 0) {
    return false;
  }
  std::string scheme = value.substr(0, colon);
  for (char& character : scheme) {
    if (character >= 'A' && character <= 'Z') {
      character = static_cast<char>(character - 'A' + 'a');
    }
  }
  return scheme == "roblox" || scheme == "roblox-player";
}

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
  bool positional_uri_seen = false;

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
    if (argument == "--headless") {
      result.options.headless = true;
    } else if (argument == "--launch-uri") {
      if (!ReadValue(argc, argv, &index, "--launch-uri",
                     &result.options.launch_uri, &result.error)) {
        return result;
      }
    } else if (!argument.empty() && argument.front() != '-' &&
               !positional_uri_seen) {
      result.options.launch_uri = argument;
      positional_uri_seen = true;
    } else {
      result.error = "unknown option or extra positional argument: " + argument;
      return result;
    }
  }

  if (!result.options.launch_uri.empty() &&
      !IsRobloxScheme(result.options.launch_uri)) {
    result.error = "launch URI must use roblox: or roblox-player:";
    return result;
  }

  return result;
}

std::string CommandLineUsage(const std::string& program_name) {
  std::ostringstream out;
  out << "Usage: " << (program_name.empty() ? "roblox" : program_name)
      << " [--headless] [roblox:...|roblox-player:...]\n\n"
      << "Browser launch is the normal path. The Roblox website supplies the "
         "launch URI.\n"
      << "The URI may also be passed as the single positional argument.\n";
  return out.str();
}

}  // namespace mocktail::runtime
