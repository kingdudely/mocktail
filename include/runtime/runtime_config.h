#ifndef MOCKTAIL_RUNTIME_RUNTIME_CONFIG_H_
#define MOCKTAIL_RUNTIME_RUNTIME_CONFIG_H_

#include <filesystem>
#include <string>

#include "runtime/environment.h"

namespace mocktail::runtime {

struct WindowConfig {
  int width = 1280;
  int height = 720;
  std::string title = "Roblox";
};

struct InputCapabilityConfig {
  bool touch_enabled = false;
  bool mouse_enabled = true;
  bool keyboard_enabled = true;
};

class RuntimeConfig {
 public:
  static RuntimeConfig FromEnvironment(const Environment& environment);

  bool headless() const { return headless_; }
  const std::filesystem::path& roblox_library_path() const {
    return roblox_library_path_;
  }
  const WindowConfig& window() const { return window_; }
  const std::string& theme_mode() const { return theme_mode_; }
  bool theme_mode_valid() const {
    return theme_mode_ == "roblox" || theme_mode_ == "system" ||
           theme_mode_ == "light" || theme_mode_ == "dark";
  }
  const InputCapabilityConfig& input_capabilities() const {
    return input_capabilities_;
  }
  bool microphone_enabled() const { return microphone_enabled_; }
  bool has_unsafe_detached_thread_overrides() const { return false; }

 private:
  std::filesystem::path roblox_library_path_ = "./libroblox.so";
  WindowConfig window_;
  std::string theme_mode_ = "roblox";
  InputCapabilityConfig input_capabilities_;
  bool microphone_enabled_ = true;
  bool headless_ = false;
};

}  // namespace mocktail::runtime

#endif  // MOCKTAIL_RUNTIME_RUNTIME_CONFIG_H_
