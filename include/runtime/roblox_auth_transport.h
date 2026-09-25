#ifndef MOCKTAIL_RUNTIME_ROBLOX_AUTH_TRANSPORT_H_
#define MOCKTAIL_RUNTIME_ROBLOX_AUTH_TRANSPORT_H_

#include <string>
#include <string_view>

#include "jnivm/jnivm.h"

namespace mocktail::runtime {

struct RobloxLaunchTicketAuthResult {
  std::string roblosecurity;
  jnivm::RobloxAuthIdentity identity;
  std::string error;

  explicit operator bool() const {
    return error.empty() && !roblosecurity.empty();
  }
};

RobloxLaunchTicketAuthResult RedeemRobloxLaunchTicket(
    std::string_view ticket, std::string_view asset_root);

}  // namespace mocktail::runtime

#endif  // MOCKTAIL_RUNTIME_ROBLOX_AUTH_TRANSPORT_H_
