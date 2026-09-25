#ifndef MOCKTAIL_RUNTIME_BROWSER_AUTH_H_
#define MOCKTAIL_RUNTIME_BROWSER_AUTH_H_

#include <string>
#include <string_view>

#include "mocktail/status.h"

namespace mocktail::runtime {

// Redeems the one-use gameinfo ticket produced by Roblox's website into the
// .ROBLOSECURITY session cookie used by the native runtime. The ticket itself
// is never returned to the caller after a successful redemption.
Status RedeemRobloxLaunchTicket(std::string_view authentication_ticket,
                                std::string* roblosecurity);

}  // namespace mocktail::runtime

#endif  // MOCKTAIL_RUNTIME_BROWSER_AUTH_H_
