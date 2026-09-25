#include "runtime/runtime_config.h"

namespace mocktail::runtime {

RuntimeConfig RuntimeConfig::FromEnvironment(const Environment& environment) {
  RuntimeConfig config;
  config.roblox_library_path_ =
      environment.GetOr("ROBLOX_LIB_PATH", "./libroblox.so");
  config.window_.title = environment.GetOr("MOCKTAIL_WIN_TITLE", "Roblox");
  config.theme_mode_ = environment.GetOr("MOCKTAIL_THEME", "roblox");
  return config;
}

}  // namespace mocktail::runtime
