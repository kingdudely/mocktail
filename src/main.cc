#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <memory>
#include <string>
#include <utility>

#include "legacy/legacy_runtime.h"
#include "mocktail/audio/fmod_jni_audio_bridge.h"
#include "mocktail/audio/webrtc_jni_audio_bridge.h"
#include "runtime/auth_runtime_composition.h"
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

int JoinRequestType(const mocktail::runtime::RobloxLaunchRequest& request) {
  if (request.conversation_id > 0) return 6;
  if (request.user_id > 0) return 1;
  if (!request.access_code.empty() || !request.link_code.empty()) return 2;
  if (!request.game_instance_id.empty()) return 3;
  if (!request.reserved_server_access_code.empty()) return 8;
  return 0;
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
  std::string roblosecurity = std::move(options.roblosecurity);
  mocktail::runtime::RobloxLaunchRequest launch_request;

  if (!options.launch_uri.empty()) {
    const mocktail::Status status =
        mocktail::runtime::ParseRobloxLaunchUri(options.launch_uri,
                                                &launch_request);
    if (!status.ok()) {
      std::cerr << "[launch] invalid Roblox URI: " << status.message() << '\n';
      return EXIT_FAILURE;
    }
  }

  std::fill(options.launch_uri.begin(), options.launch_uri.end(), '\0');
  options.launch_uri.clear();
  ScrubArgv(argc, argv);

  if (options.place_id.has_value())
    launch_request.place_id = *options.place_id;
  if (!options.server_id.empty())
    launch_request.game_instance_id = options.server_id;

  if (launch_request.place_id > 0) {
    setenv("MOCKTAIL_PLACE_ID",
           std::to_string(launch_request.place_id).c_str(), 1);
  } else if (options.place_id.has_value()) {
    std::cerr << "[launch] --place-id must be positive\n";
    return EXIT_FAILURE;
  }

  if (!launch_request.game_instance_id.empty())
    setenv("MOCKTAIL_GAME_ID", launch_request.game_instance_id.c_str(), 1);

  setenv("MOCKTAIL_GAME_JOIN_REQUEST_TYPE",
         std::to_string(JoinRequestType(launch_request)).c_str(), 1);

  if (!launch_request.access_code.empty())
    setenv("MOCKTAIL_GAME_ACCESS_CODE",
           launch_request.access_code.c_str(), 1);
  if (!launch_request.link_code.empty())
    setenv("MOCKTAIL_GAME_LINK_CODE",
           launch_request.link_code.c_str(), 1);
  if (!launch_request.reserved_server_access_code.empty())
    setenv("MOCKTAIL_GAME_RESERVED_SERVER_ACCESS_CODE",
           launch_request.reserved_server_access_code.c_str(), 1);
  if (launch_request.user_id > 0)
    setenv("MOCKTAIL_GAME_JOIN_USER_ID",
           std::to_string(launch_request.user_id).c_str(), 1);
  if (launch_request.conversation_id > 0)
    setenv("MOCKTAIL_GAME_CONVERSATION_ID",
           std::to_string(launch_request.conversation_id).c_str(), 1);
  if (launch_request.referred_by_player_id > 0)
    setenv("MOCKTAIL_REFERRED_BY_PLAYER_ID",
           std::to_string(launch_request.referred_by_player_id).c_str(), 1);
  if (!launch_request.launch_data.empty())
    setenv("MOCKTAIL_GAME_PARAMS_JSON",
           launch_request.launch_data.c_str(), 1);
  if (!launch_request.call_id.empty())
    setenv("MOCKTAIL_GAME_CALL_ID", launch_request.call_id.c_str(), 1);
  if (!launch_request.event_id.empty())
    setenv("MOCKTAIL_GAME_EVENT_ID", launch_request.event_id.c_str(), 1);
  if (!launch_request.join_attempt_id.empty())
    setenv("MOCKTAIL_GAME_JOIN_ATTEMPT_ID",
           launch_request.join_attempt_id.c_str(), 1);
  if (!launch_request.join_attempt_origin.empty())
    setenv("MOCKTAIL_GAME_JOIN_ATTEMPT_ORIGIN",
           launch_request.join_attempt_origin.c_str(), 1);
  if (!launch_request.iso_context.empty())
    setenv("MOCKTAIL_GAME_ISO_CONTEXT",
           launch_request.iso_context.c_str(), 1);
  if (!launch_request.referral_page.empty())
    setenv("MOCKTAIL_REFERRAL_PAGE",
           launch_request.referral_page.c_str(), 1);
  if (!launch_request.game_join_context.empty())
    setenv("MOCKTAIL_GAME_JOIN_CONTEXT",
           launch_request.game_join_context.c_str(), 1);

  if (setenv("ROBLOX_LIB_PATH", options.roblox_library_path.c_str(), 1) != 0 ||
      setenv("MOCKTAIL_ASSET_PATH", options.asset_path.c_str(), 1) != 0 ||
      setenv("MOCKTAIL_ASSET_ROOT", options.asset_path.c_str(), 1) != 0 ||
      setenv("MOCKTAIL_HEADLESS", options.headless ? "1" : "0", 1) != 0) {
    std::cerr << "could not set Roblox payload environment\n";
    return EXIT_FAILURE;
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
