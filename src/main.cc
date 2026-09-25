#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>

#include "legacy/legacy_runtime.h"
#include "mocktail/audio/fmod_jni_audio_bridge.h"
#include "mocktail/audio/webrtc_jni_audio_bridge.h"
#include "runtime/auth_runtime_composition.h"
#include "runtime/command_line.h"
#include "runtime/environment.h"
#include "runtime/runtime_paths.h"
#include "services/auth_service.h"
#include "services/http_client.h"
#include "jnivm/jnivm.h"
#include "window/window.h"

namespace {

bool SetEnv(const char* name, const std::string& value) {
  return setenv(name, value.c_str(), 1) == 0;
}

mocktail::Status ShutdownAudio(jnivm::VM* vm) {
  if (vm == nullptr) {
    return mocktail::Status::Error(
        mocktail::StatusCode::kInvalidArgument,
        "audio shutdown requires a VM");
  }

  const mocktail::Status voice =
      mocktail::audio::ShutdownWebRtcJniAudioBridge(vm);
  const mocktail::Status fmod =
      mocktail::audio::ShutdownFmodJniAudioBridge(vm);
  return !voice.ok() ? voice : fmod;
}

}  // namespace

int main(int argc, char* argv[]) {
  const mocktail::runtime::CommandLineParseResult command_line =
      mocktail::runtime::ParseCommandLine(argc, argv);
  if (!command_line) {
    std::cerr << command_line.error << "\n\n"
              << mocktail::runtime::CommandLineUsage(
                     command_line.options.program_name);
    return EXIT_FAILURE;
  }

  if (!SetEnv("ROBLOX_LIB_PATH", command_line.options.roblox_library_path) ||
      !SetEnv("MOCKTAIL_ASSET_PATH", command_line.options.asset_path) ||
      !SetEnv("MOCKTAIL_ASSET_ROOT", command_line.options.asset_path)) {
    std::cerr << "could not set Roblox payload environment\n";
    return EXIT_FAILURE;
  }

  if (command_line.options.roblosecurity.empty()) {
    if (!SetEnv("MOCKTAIL_ALLOW_NO_COOKIE_LUA_APP", "1")) {
      std::cerr << "could not configure guest startup\n";
      return EXIT_FAILURE;
    }
  } else {
    std::string cookie = command_line.options.roblosecurity;
    if (cookie.rfind(".ROBLOSECURITY=", 0) != 0) {
      cookie.insert(0, ".ROBLOSECURITY=");
    }
    if (!SetEnv("MOCKTAIL_ROBLOX_COOKIES", cookie) ||
        !SetEnv("MOCKTAIL_ALLOW_NO_COOKIE_LUA_APP", "0")) {
      std::cerr << "could not configure Roblox authentication\n";
      return EXIT_FAILURE;
    }
  }

  const mocktail::runtime::ProcessEnvironment environment;
  const mocktail::runtime::RuntimePaths paths =
      mocktail::runtime::RuntimePaths::FromEnvironment(environment);
  auto http_client =
      std::make_shared<mocktail::services::CurlHttpClient>();
  mocktail::services::AuthService auth_service(*http_client);
  mocktail::runtime::AuthRuntimeComposition composition =
      mocktail::runtime::ComposeAuthRuntime(
          environment, paths, auth_service, http_client);

  if (!composition) {
    std::cerr << "[auth] " << composition.error;
    if (composition.http_status != 0) {
      std::cerr << " (HTTP " << composition.http_status << ')';
    }
    std::cerr << '\n';
    return EXIT_FAILURE;
  }

  auto android_window_context = std::make_shared<int>(0);
  jnivm::AndroidWindowCallbacks callbacks;
  callbacks.set_flags = [](void*, int flags, int mask) {
    return mocktail::window::RequestFullscreenFromAndroidWindowFlags(
        flags, mask);
  };
  composition.jni_vm->SetAndroidWindowCallbacks(
      std::move(android_window_context), callbacks);

  const mocktail::Status fmod_status =
      mocktail::audio::InstallFmodJniAudioBridge(composition.jni_vm.get());
  if (!fmod_status.ok()) {
    std::cerr << "[audio] " << fmod_status.message() << '\n';
    return EXIT_FAILURE;
  }

  const mocktail::Status voice_status =
      mocktail::audio::InstallWebRtcJniAudioBridge(composition.jni_vm.get());
  if (!voice_status.ok()) {
    (void)mocktail::audio::ShutdownFmodJniAudioBridge(
        composition.jni_vm.get());
    std::cerr << "[audio] " << voice_status.message() << '\n';
    return EXIT_FAILURE;
  }

  mocktail::legacy::RuntimeDependencies dependencies(
      std::move(composition), &ShutdownAudio);

  return mocktail::legacy::Run(command_line.options,
                               std::move(dependencies));
}
