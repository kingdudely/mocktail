#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <memory>
#include <string>
#include <unistd.h>
#include <utility>

#include "legacy/legacy_runtime.h"
#include "mocktail/audio/fmod_jni_audio_bridge.h"
#include "mocktail/audio/webrtc_jni_audio_bridge.h"
#include "runtime/auth_runtime_composition.h"
#include "runtime/browser_auth.h"
#include "runtime/command_line.h"
#include "runtime/roblox_launch_uri.h"
#include "window/window.h"

namespace {

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

void ScrubArgv(int argc, char* argv[]) {
  for (int i = 1; i < argc; ++i) {
    if (argv[i] == nullptr) continue;
    std::fill(argv[i], argv[i] + std::strlen(argv[i]), '\0');
  }
}

std::filesystem::path ExecutableDirectory() {
  char buffer[4096] = {};
  const ssize_t length = readlink("/proc/self/exe", buffer, sizeof(buffer) - 1);
  if (length > 0) {
    buffer[length] = '\0';
    return std::filesystem::path(buffer).parent_path();
  }
  return std::filesystem::current_path();
}

bool SetDefaultPayloadEnvironment() {
  const std::filesystem::path root = ExecutableDirectory();
  const std::filesystem::path library = root / "libroblox.so";
  const std::filesystem::path assets = root / "assets";

  auto set_default = [](const char* name, const std::string& value) {
    return std::getenv(name) != nullptr ||
           setenv(name, value.c_str(), 1) == 0;
  };

  return set_default("ROBLOX_LIB_PATH", library.string()) &&
         set_default("MOCKTAIL_ASSET_PATH", assets.string()) &&
         set_default("MOCKTAIL_ASSET_ROOT", assets.string());
}

int JoinRequestType(const mocktail::runtime::RobloxLaunchRequest& request) {
  if (request.conversation_id > 0) return 6;
  if (request.user_id > 0) return 1;
  if (!request.access_code.empty() || !request.link_code.empty()) return 2;
  if (!request.game_instance_id.empty()) return 3;
  if (!request.reserved_server_access_code.empty()) return 8;
  return 0;
}

void ExportLaunchRequest(
    const mocktail::runtime::RobloxLaunchRequest& request) {
  if (request.place_id > 0) {
    setenv("MOCKTAIL_PLACE_ID",
           std::to_string(request.place_id).c_str(), 1);
  }

  if (!request.game_instance_id.empty()) {
    setenv("MOCKTAIL_GAME_ID", request.game_instance_id.c_str(), 1);
  }

  setenv("MOCKTAIL_GAME_JOIN_REQUEST_TYPE",
         std::to_string(JoinRequestType(request)).c_str(), 1);

  if (!request.access_code.empty()) {
    setenv("MOCKTAIL_GAME_ACCESS_CODE", request.access_code.c_str(), 1);
  }
  if (!request.link_code.empty()) {
    setenv("MOCKTAIL_GAME_LINK_CODE", request.link_code.c_str(), 1);
  }
  if (!request.reserved_server_access_code.empty()) {
    setenv("MOCKTAIL_GAME_RESERVED_SERVER_ACCESS_CODE",
           request.reserved_server_access_code.c_str(), 1);
  }
  if (request.user_id > 0) {
    setenv("MOCKTAIL_GAME_JOIN_USER_ID",
           std::to_string(request.user_id).c_str(), 1);
  }
  if (request.conversation_id > 0) {
    setenv("MOCKTAIL_GAME_CONVERSATION_ID",
           std::to_string(request.conversation_id).c_str(), 1);
  }
  if (request.referred_by_player_id > 0) {
    setenv("MOCKTAIL_REFERRED_BY_PLAYER_ID",
           std::to_string(request.referred_by_player_id).c_str(), 1);
  }
  if (!request.launch_data.empty()) {
    setenv("MOCKTAIL_GAME_PARAMS_JSON", request.launch_data.c_str(), 1);
  }
  if (!request.call_id.empty()) {
    setenv("MOCKTAIL_GAME_CALL_ID", request.call_id.c_str(), 1);
  }
  if (!request.event_id.empty()) {
    setenv("MOCKTAIL_GAME_EVENT_ID", request.event_id.c_str(), 1);
  }
  if (!request.join_attempt_id.empty()) {
    setenv("MOCKTAIL_GAME_JOIN_ATTEMPT_ID",
           request.join_attempt_id.c_str(), 1);
  }
  if (!request.join_attempt_origin.empty()) {
    setenv("MOCKTAIL_GAME_JOIN_ATTEMPT_ORIGIN",
           request.join_attempt_origin.c_str(), 1);
  }
  if (!request.iso_context.empty()) {
    setenv("MOCKTAIL_GAME_ISO_CONTEXT", request.iso_context.c_str(), 1);
  }
  if (!request.referral_page.empty()) {
    setenv("MOCKTAIL_REFERRAL_PAGE", request.referral_page.c_str(), 1);
  }
  if (!request.game_join_context.empty()) {
    setenv("MOCKTAIL_GAME_JOIN_CONTEXT",
           request.game_join_context.c_str(), 1);
  }
}

}  // namespace

int main(int argc, char* argv[]) {
  mocktail::runtime::CommandLineParseResult command_line =
      mocktail::runtime::ParseCommandLine(argc, argv);
  if (!command_line) {
    std::cerr << command_line.error << "\n\n"
              << mocktail::runtime::CommandLineUsage(
                     command_line.options.program_name);
    return EXIT_FAILURE;
  }

  mocktail::runtime::CommandLineOptions options =
      std::move(command_line.options);
  mocktail::runtime::RobloxLaunchRequest launch_request;
  std::string browser_ticket;

  if (!options.launch_uri.empty()) {
    const mocktail::Status status =
        mocktail::runtime::ParseRobloxLaunchUri(
            options.launch_uri, &launch_request, &browser_ticket);
    if (!status.ok()) {
      std::cerr << "[launch] invalid Roblox URI: " << status.message() << '\n';
      return EXIT_FAILURE;
    }

    mocktail::runtime::SecurelyClearString(&options.launch_uri);
  }

  ScrubArgv(argc, argv);

  std::string roblosecurity;
  if (!browser_ticket.empty()) {
    const mocktail::Status status =
        mocktail::runtime::RedeemRobloxLaunchTicket(
            browser_ticket, &roblosecurity);
    mocktail::runtime::SecurelyClearString(&browser_ticket);
    if (!status.ok()) {
      std::cerr << "[auth] could not authenticate the browser launch: "
                << status.message() << '\n';
      return EXIT_FAILURE;
    }
  }

  ExportLaunchRequest(launch_request);

  if (!SetDefaultPayloadEnvironment()) {
    std::cerr << "could not set the Roblox payload environment\n";
    mocktail::runtime::SecurelyClearString(&roblosecurity);
    return EXIT_FAILURE;
  }

  setenv("MOCKTAIL_HEADLESS", options.headless ? "1" : "0", 1);
  if (options.headless) {
    setenv("MOCKTAIL_KEEPALIVE", "1", 1);
    setenv("MOCKTAIL_MAIN_THREAD_MESSAGE_PUMP", "1", 1);
  }

  mocktail::runtime::AuthRuntimeComposition composition =
      mocktail::runtime::ComposeAuthRuntime(roblosecurity);
  mocktail::runtime::SecurelyClearString(&roblosecurity);

  if (!composition) {
    std::cerr << "[auth] could not create the Roblox pseudo-JVM\n";
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
  return mocktail::legacy::Run(options, std::move(dependencies));
}
