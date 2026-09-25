#ifndef MOCKTAIL_RUNTIME_COMMAND_LINE_H_
#define MOCKTAIL_RUNTIME_COMMAND_LINE_H_

#include <string>

namespace mocktail::runtime {

struct CommandLineOptions {
  std::string program_name = "roblox";
  std::string launch_uri;
  bool headless = false;
};

struct CommandLineParseResult {
  CommandLineOptions options;
  std::string error;
  explicit operator bool() const { return error.empty(); }
};

CommandLineParseResult ParseCommandLine(int argc, const char* const argv[]);
std::string CommandLineUsage(const std::string& program_name);

}  // namespace mocktail::runtime

#endif  // MOCKTAIL_RUNTIME_COMMAND_LINE_H_
