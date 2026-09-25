#ifndef MOCKTAIL_RUNTIME_ROBLOX_LAUNCH_URI_H_
#define MOCKTAIL_RUNTIME_ROBLOX_LAUNCH_URI_H_

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

#include "mocktail/status.h"

namespace mocktail::runtime {

inline constexpr std::size_t kMaximumRobloxLaunchUriBytes = 64 * 1024;

struct RobloxLaunchRequest {
  std::int64_t place_id = 0;
  std::int64_t user_id = 0;
  std::int64_t conversation_id = 0;
  std::int64_t referred_by_player_id = 0;
  std::string game_instance_id;
  std::string reserved_server_access_code;
  std::string call_id;
  std::string referral_page;
  std::string access_code;
  std::string link_code;
  std::string launch_data;
  std::string event_id;
  std::string game_join_context;
  std::string join_attempt_id;
  std::string join_attempt_origin;
  std::string iso_context;
};

Status ParseRobloxLaunchUri(std::string_view uri, RobloxLaunchRequest* request);

}  // namespace mocktail::runtime

#endif  // MOCKTAIL_RUNTIME_ROBLOX_LAUNCH_URI_H_
