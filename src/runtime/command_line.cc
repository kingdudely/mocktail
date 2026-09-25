#include "runtime/command_line.h"

#include <charconv>
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

bool ParsePlaceId(std::string_view value, std::int64_t* output,
                  std::string* error) {
  if (output == nullptr || error == nullptr || value.empty() ||
      value.size() > 20 || value.front() == '-' || value.front() == '+') {
    if (error != nullptr) *error = "--place-id requires a positive integer";
    return false;
  }
  std::int64_t parsed = 0;
  const auto result =
      std::from_chars(value.data(), value.data() + value.size(), parsed);
  if (result.ec != std::errc() ||
      result.ptr != value.data() + value.size() || parsed <= 0) {
    *error = "--place-id requires a positive integer";
    return false;
  }
  *output = parsed;
  return true;
}

bool IsBoundedServerId(std::string_view value) {
  if (value.empty() || value.size() > 512) return false;
  for (unsigned char byte : value) {
    if (byte < 0x20 || byte == 0x7f) return false;
  }
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
    if (argument == "--ROBLOSECURITY") {
      if (!ReadValue(argc, argv, &index, "--ROBLOSECURITY",
                     &result.options.roblosecurity, &result.error)) return result;
    } else if (argument == "--assets_dir") {
      if (!ReadValue(argc, argv, &index, "--assets_dir",
                     &result.options.asset_path, &result.error)) return result;
    } else if (argument == "--libroblox_file") {
      if (!ReadValue(argc, argv, &index, "--libroblox_file",
                     &result.options.roblox_library_path, &result.error)) return result;
    } else if (argument == "--place-id") {
      std::string value;
      if (!ReadValue(argc, argv, &index, "--place-id", &value, &result.error)) return result;
      if (!ParsePlaceId(value, &result.options.place_id, &result.error)) return result;
    } else if (argument == "--server-id") {
      if (!ReadValue(argc, argv, &index, "--server-id",
                     &result.options.server_id, &result.error)) return result;
      if (!IsBoundedServerId(result.options.server_id)) {
        result.error = "--server-id is empty, oversized, or contains control bytes";
        return result;
      }
    } else if (argument == "--headless") {
      result.options.headless = true;
    } else if (!argument.empty() && argument.front() != '-' &&
               !positional_uri_seen) {
      result.options.launch_uri = argument;
      positional_uri_seen = true;
    } else {
      result.error = "unknown option or extra positional argument: " + argument;
      return result;
    }
  }

  if (!result.options.launch_uri.empty()) {
    const std::size_t colon = result.options.launch_uri.find(':');
    const std::string scheme =
        colon == std::string::npos ? "" : result.options.launch_uri.substr(0, colon);
    if (scheme != "roblox" && scheme != "roblox-player") {
      result.error = "launch URI must use roblox: or roblox-player:";
      return result;
    }
  }
  return result;
}

std::string CommandLineUsage(const std::string& program_name) {
  std::ostringstream out;
  out << "Usage: " << (program_name.empty() ? "mocktail" : program_name)
      << " [options] [roblox:...|roblox-player:...]\n\n"
      << "Options:\n"
      << "  --assets_dir <path>      Roblox assets directory (default: ./assets)\n"
      << "  --libroblox_file <path>  libroblox.so path (default: ./libroblox.so)\n"
      << "  --ROBLOSECURITY <value>  Roblox session cookie\n"
      << "  --place-id <id>          Place to launch\n"
      << "  --server-id <id>         Roblox server/game id\n"
      << "  --headless               Run without creating an SDL window\n"
      << "\nA Roblox URL may be passed as the single positional argument.\n";
  return out.str();
}

}  // namespace mocktail::runtime
