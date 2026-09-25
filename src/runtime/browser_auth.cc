#include "runtime/browser_auth.h"

#include <algorithm>
#include <cctype>
#include <mutex>
#include <string>
#include <string_view>

#include <curl/curl.h>

namespace mocktail::runtime {
namespace {

constexpr char kRedeemUrl[] =
    "https://auth.roblox.com/v1/authentication-ticket/redeem";

Status Invalid(std::string message) {
  return Status::Error(StatusCode::kInvalidArgument, std::move(message));
}

Status Unavailable(std::string message) {
  return Status::Error(StatusCode::kUnavailable, std::move(message));
}

bool TicketIsSafe(std::string_view ticket) {
  if (ticket.empty() || ticket.size() > 2048) {
    return false;
  }
  return std::all_of(ticket.begin(), ticket.end(), [](unsigned char byte) {
    return std::isalnum(byte) || byte == '-' || byte == '.' ||
           byte == '_' || byte == '~';
  });
}

bool CookieValueIsSafe(std::string_view value) {
  if (value.empty() || value.size() > 16384) {
    return false;
  }
  return std::all_of(value.begin(), value.end(), [](unsigned char byte) {
    return byte >= 0x21 && byte <= 0x7e &&
           byte != '"' && byte != ',' && byte != ';' && byte != '\';
  });
}

struct ResponseState {
  std::string roblosecurity;
  int roblosecurity_cookies = 0;
  bool malformed = false;
};

size_t DiscardBody(char*, size_t size, size_t count, void*) {
  return size * count;
}

size_t CaptureHeaders(char* buffer, size_t size, size_t count, void* userdata) {
  auto* state = static_cast<ResponseState*>(userdata);
  if (state == nullptr) {
    return 0;
  }

  const size_t length = size * count;
  std::string_view line(buffer, length);
  constexpr std::string_view kPrefix = "set-cookie:";
  if (line.size() < kPrefix.size()) {
    return length;
  }

  std::string lowered;
  lowered.reserve(kPrefix.size());
  for (std::size_t i = 0; i < kPrefix.size(); ++i) {
    char ch = line[i];
    if (ch >= 'A' && ch <= 'Z') ch = static_cast<char>(ch - 'A' + 'a');
    lowered.push_back(ch);
  }
  if (lowered != kPrefix) {
    return length;
  }

  line.remove_prefix(kPrefix.size());
  while (!line.empty() && std::isspace(static_cast<unsigned char>(line.front()))) {
    line.remove_prefix(1);
  }
  const std::size_t semicolon = line.find(';');
  std::string_view pair = line.substr(
      0, semicolon == std::string_view::npos ? line.size() : semicolon);

  constexpr std::string_view kCookieName = ".ROBLOSECURITY=";
  if (pair.substr(0, kCookieName.size()) != kCookieName) {
    return length;
  }

  std::string_view value = pair.substr(kCookieName.size());
  while (!value.empty() &&
         std::isspace(static_cast<unsigned char>(value.back()))) {
    value.remove_suffix(1);
  }

  ++state->roblosecurity_cookies;
  if (!CookieValueIsSafe(value) || state->roblosecurity_cookies > 1) {
    state->malformed = true;
    return length;
  }
  state->roblosecurity.assign(value);
  return length;
}

struct CurlGlobalInit {
  CurlGlobalInit() : status(curl_global_init(CURL_GLOBAL_DEFAULT)) {}
  ~CurlGlobalInit() {
    if (status == CURLE_OK) {
      curl_global_cleanup();
    }
  }
  CURLcode status;
};

CurlGlobalInit& CurlState() {
  static CurlGlobalInit state;
  return state;
}

}  // namespace

Status RedeemRobloxLaunchTicket(std::string_view authentication_ticket,
                                std::string* roblosecurity) {
  if (roblosecurity == nullptr) {
    return Invalid("Roblox session output is null");
  }
  roblosecurity->clear();

  if (!TicketIsSafe(authentication_ticket)) {
    return Invalid("Roblox browser launch ticket is invalid");
  }

  CurlGlobalInit& curl_state = CurlState();
  if (curl_state.status != CURLE_OK) {
    return Unavailable("could not initialize the HTTP client");
  }

  CURL* curl = curl_easy_init();
  if (curl == nullptr) {
    return Unavailable("could not initialize the HTTP request");
  }

  ResponseState response;
  std::string json = "{"authenticationTicket":"";
  json.append(authentication_ticket);
  json.append(""}");

  struct curl_slist* headers = nullptr;
  headers = curl_slist_append(headers, "Content-Type: application/json");
  headers = curl_slist_append(headers, "RBXAuthenticationNegotiation: 1");
  if (headers == nullptr) {
    curl_easy_cleanup(curl);
    return Unavailable("could not initialize the HTTP headers");
  }

  curl_easy_setopt(curl, CURLOPT_URL, kRedeemUrl);
  curl_easy_setopt(curl, CURLOPT_POST, 1L);
  curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json.data());
  curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE,
                  static_cast<long>(json.size()));
  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
  curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, &CaptureHeaders);
  curl_easy_setopt(curl, CURLOPT_HEADERDATA, &response);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, &DiscardBody);
  curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT_MS, 10000L);
  curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, 15000L);
  curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 0L);
  curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);

  const CURLcode result = curl_easy_perform(curl);
  long response_code = 0;
  if (result == CURLE_OK) {
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
  }

  curl_slist_free_all(headers);
  curl_easy_cleanup(curl);

  if (result != CURLE_OK || response_code < 200 || response_code >= 300) {
    return Unavailable("Roblox did not redeem the browser launch ticket");
  }
  if (response.malformed || response.roblosecurity_cookies != 1) {
    return Unavailable("Roblox did not return one valid .ROBLOSECURITY session");
  }

  *roblosecurity = std::move(response.roblosecurity);
  return Status::Ok();
}

}  // namespace mocktail::runtime
