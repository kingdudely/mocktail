#include "runtime/command_line.h"

#include <sstream>
#include <string>
#include <string_view>

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

bool ParseEqualsOption(const std::string& argument,
                       std::string_view prefix,
                       std::string* value,
                       std::string* error) {
  if (argument.rfind(prefix, 0) != 0) return false;
  const std::string_view raw(argument.data() + prefix.size(),
                             argument.size() - prefix.size());
  if (raw.empty()) {
    *error = std::string(prefix) + " requires a value";
    return true;
  }
  *value = std::string(raw);
  return true;
}

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
    } else if (argument == "--libroblox_so") {
      if (!ReadValue(argc, argv, &index, "--libroblox_so",
                     &result.options.roblox_library_path, &result.error)) {
        return result;
      }
    } else if (ParseEqualsOption(argument, "--libroblox_so=",
                                 &result.options.roblox_library_path,
                                 &result.error)) {
      if (!result.error.empty()) return result;
    } else if (argument == "--assets_dir") {
      if (!ReadValue(argc, argv, &index, "--assets_dir",
                     &result.options.asset_path, &result.error)) {
        return result;
      }
    } else if (ParseEqualsOption(argument, "--assets_dir=",
                                 &result.options.asset_path,
                                 &result.error)) {
      if (!result.error.empty()) return result;
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
      << " [options] [roblox:...|roblox-player:...]\n\n"
      << "Options:\n"
      << "  --libroblox_so=<path>    libroblox.so path (default: ./libroblox.so)\n"
      << "  --assets_dir=<path>      Roblox assets directory (default: ./assets)\n"
      << "  --headless               Run without creating an SDL window\n\n"
      << "Browser launch is the normal path. The Roblox website supplies the "
         "launch URI.\n"
      << "The URI may also be passed as the single positional argument.\n";
  return out.str();
}

}  // namespace mocktail::runtime
