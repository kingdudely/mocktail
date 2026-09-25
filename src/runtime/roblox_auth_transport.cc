#include "runtime/roblox_auth_transport.h"

#include <netdb.h>
#include <netinet/in.h>
#include <openssl/ssl.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <cstddef>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <string_view>
#include <utility>

#include <nlohmann/json.hpp>

namespace mocktail::runtime {
namespace {

constexpr std::size_t kMaximumResponseBytes = 1024 * 1024;
constexpr std::string_view kRedeemHost = "auth.roblox.com";
constexpr std::string_view kRedeemPath =
    "/v1/authentication-ticket/redeem";
constexpr std::string_view kUserHost = "users.roblox.com";
constexpr std::string_view kUserPath = "/v1/users/authenticated";

void ClearSensitive(std::string* value) {
  if (value == nullptr) return;
  volatile char* bytes = value->empty() ? nullptr : value->data();
  for (std::size_t i = 0; i < value->size(); ++i) bytes[i] = '\0';
  value->clear();
}

bool ValidCookieValue(std::string_view value) {
  if (value.empty() || value.size() > 16384) return false;
  for (unsigned char byte : value) {
    if (byte < 0x21 || byte > 0x7e || byte == '"' || byte == ',' ||
        byte == ';' || byte == '\\')
      return false;
  }
  return true;
}

struct HttpResponse {
  int status = 0;
  std::string headers;
  std::string body;
  std::string error;
};

class TlsHttpClient final {
 public:
  explicit TlsHttpClient(std::string ca_file) : ca_file_(std::move(ca_file)) {}
  ~TlsHttpClient() { Close(); }

  HttpResponse Request(std::string_view host, std::string_view path,
                       std::string_view method, std::string_view body,
                       std::string_view cookie,
                       std::string_view extra_headers) {
    HttpResponse response;
    if (!Connect(host, &response.error)) return response;

    std::string request;
    request.reserve(512 + body.size() + cookie.size());
    request.append(method);
    request.append(" ");
    request.append(path);
    request.append(" HTTP/1.1\r\nHost: ");
    request.append(host);
    request.append("\r\nUser-Agent: Mocktail/1\r\n");
    request.append(extra_headers);
    if (!cookie.empty()) {
      request.append("Cookie: .ROBLOSECURITY=");
      request.append(cookie);
      request.append("\r\n");
    }
    request.append("Content-Type: application/json\r\n");
    request.append("Accept: application/json\r\n");
    request.append("Connection: close\r\nContent-Length: ");
    request.append(std::to_string(body.size()));
    request.append("\r\n\r\n");
    request.append(body);

    std::size_t offset = 0;
    while (offset < request.size()) {
      const int remaining = static_cast<int>(
          std::min<std::size_t>(request.size() - offset,
                                static_cast<std::size_t>(1 << 20)));
      const int written = SSL_write(ssl_, request.data() + offset, remaining);
      if (written <= 0) {
        response.error = "TLS write failed";
        return response;
      }
      offset += static_cast<std::size_t>(written);
    }

    std::string raw;
    raw.reserve(8192);
    char buffer[8192];
    while (raw.size() <= kMaximumResponseBytes + 8192) {
      const int count = SSL_read(ssl_, buffer, sizeof(buffer));
      if (count > 0) {
        raw.append(buffer, static_cast<std::size_t>(count));
        if (raw.size() > kMaximumResponseBytes + 8192) {
          response.error = "Roblox auth response is too large";
          return response;
        }
        continue;
      }
      const int ssl_error = SSL_get_error(ssl_, count);
      if (ssl_error == SSL_ERROR_ZERO_RETURN) break;
      if (ssl_error == SSL_ERROR_SYSCALL && errno == EINTR) continue;
      response.error = "TLS read failed";
      return response;
    }

    const std::size_t header_end = raw.find("\r\n\r\n");
    if (header_end == std::string::npos) {
      response.error = "Roblox auth response has no HTTP header";
      return response;
    }
    response.headers = raw.substr(0, header_end);
    response.body = raw.substr(header_end + 4);

    const std::size_t first_space = response.headers.find(' ');
    const std::size_t second_space =
        first_space == std::string::npos
            ? std::string::npos
            : response.headers.find(' ', first_space + 1);
    if (first_space == std::string::npos ||
        second_space == std::string::npos) {
      response.error = "Roblox auth response status is malformed";
      return response;
    }

    response.status = std::atoi(
        response.headers
            .substr(first_space + 1, second_space - first_space - 1)
            .c_str());
    return response;
  }

 private:
  bool Connect(std::string_view host, std::string* error) {
    Close();
    if (host != kRedeemHost && host != kUserHost) {
      if (error != nullptr) *error = "Roblox auth host is not permitted";
      return false;
    }

    addrinfo hints{};
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    addrinfo* results = nullptr;
    const int lookup =
        getaddrinfo(std::string(host).c_str(), "443", &hints, &results);
    if (lookup != 0) {
      if (error != nullptr) *error = "DNS lookup failed for Roblox auth host";
      return false;
    }

    int fd = -1;
    for (addrinfo* current = results; current != nullptr;
         current = current->ai_next) {
      fd = socket(current->ai_family, current->ai_socktype,
                  current->ai_protocol);
      if (fd < 0) continue;
      timeval timeout{10, 0};
      setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
      setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
      if (::connect(fd, current->ai_addr, current->ai_addrlen) == 0) break;
      close(fd);
      fd = -1;
    }
    freeaddrinfo(results);
    if (fd < 0) {
      if (error != nullptr) *error = "could not connect to Roblox auth host";
      return false;
    }
    fd_ = fd;

    ctx_ = SSL_CTX_new(TLS_client_method());
    if (ctx_ == nullptr) {
      if (error != nullptr) *error = "could not create TLS context";
      Close();
      return false;
    }
    SSL_CTX_set_verify(ctx_, SSL_VERIFY_PEER, nullptr);

    if (!ca_file_.empty() && std::filesystem::is_regular_file(ca_file_)) {
      if (SSL_CTX_load_verify_locations(ctx_, ca_file_.c_str(), nullptr) != 1) {
        if (error != nullptr) *error = "could not load Roblox CA bundle";
        Close();
        return false;
      }
    } else if (SSL_CTX_set_default_verify_paths(ctx_) != 1) {
      if (error != nullptr) *error = "could not load system CA paths";
      Close();
      return false;
    }

    ssl_ = SSL_new(ctx_);
    if (ssl_ == nullptr) {
      if (error != nullptr) *error = "could not create TLS session";
      Close();
      return false;
    }
    const std::string host_string(host);
    SSL_set_tlsext_host_name(ssl_, host_string.c_str());
    X509_VERIFY_PARAM* verify = SSL_get0_param(ssl_);
    X509_VERIFY_PARAM_set1_host(verify, host_string.c_str(), 0);
    SSL_set_fd(ssl_, fd_);

    if (SSL_connect(ssl_) != 1 ||
        SSL_get_verify_result(ssl_) != X509_V_OK) {
      if (error != nullptr)
        *error = "Roblox TLS certificate verification failed";
      Close();
      return false;
    }
    return true;
  }

  void Close() {
    if (ssl_ != nullptr) {
      SSL_shutdown(ssl_);
      SSL_free(ssl_);
      ssl_ = nullptr;
    }
    if (ctx_ != nullptr) {
      SSL_CTX_free(ctx_);
      ctx_ = nullptr;
    }
    if (fd_ >= 0) {
      close(fd_);
      fd_ = -1;
    }
  }

  int fd_ = -1;
  SSL_CTX* ctx_ = nullptr;
  SSL* ssl_ = nullptr;
  std::string ca_file_;
};

bool ExtractRoblosecurity(std::string_view headers, std::string* value) {
  if (value == nullptr) return false;
  value->clear();
  int matches = 0;
  std::size_t line_begin = 0;

  while (line_begin < headers.size()) {
    const std::size_t line_end = headers.find("\r\n", line_begin);
    const std::size_t end =
        line_end == std::string::npos ? headers.size() : line_end;
    const std::string_view line(headers.data() + line_begin,
                                end - line_begin);

    constexpr std::string_view prefix = "set-cookie:";
    bool set_cookie = line.size() >= prefix.size();
    for (std::size_t i = 0; set_cookie && i < prefix.size(); ++i) {
      if (std::tolower(static_cast<unsigned char>(line[i])) !=
          prefix[i]) {
        set_cookie = false;
      }
    }

    if (set_cookie) {
      std::size_t begin = prefix.size();
      while (begin < line.size() &&
             (line[begin] == ' ' || line[begin] == '\t')) {
        ++begin;
      }
      constexpr std::string_view cookie_name = ".ROBLOSECURITY=";
      const std::string_view cookie = line.substr(begin);
      if (cookie.size() >= cookie_name.size() &&
          cookie.substr(0, cookie_name.size()) == cookie_name) {
        const std::size_t semi = cookie.find(';', cookie_name.size());
        const std::string_view candidate =
            cookie.substr(cookie_name.size(),
                          semi == std::string_view::npos
                              ? cookie.size() - cookie_name.size()
                              : semi - cookie_name.size());
        if (!ValidCookieValue(candidate)) return false;
        *value = candidate;
        ++matches;
      }
    }

    if (line_end == std::string::npos) break;
    line_begin = line_end + 2;
  }
  return matches == 1 && !value->empty();
}

std::string AssetCaBundle(std::string_view asset_root) {
  if (asset_root.empty()) return {};
  const std::filesystem::path path =
      std::filesystem::path(asset_root) / "ssl" / "cacert.pem";
  return std::filesystem::is_regular_file(path) ? path.string() : std::string();
}

RobloxLaunchTicketAuthResult Failure(std::string error) {
  RobloxLaunchTicketAuthResult result;
  result.error = std::move(error);
  return result;
}

}  // namespace

RobloxLaunchTicketAuthResult RedeemRobloxLaunchTicket(
    std::string_view ticket, std::string_view asset_root) {
  if (ticket.empty() || ticket.size() > 16384)
    return Failure("Roblox launch ticket is invalid");
  for (unsigned char byte : ticket) {
    if (byte < 0x21 || byte == 0x7f)
      return Failure("Roblox launch ticket contains invalid bytes");
  }

  const std::string ca_file = AssetCaBundle(asset_root);
  TlsHttpClient client(ca_file);
  std::string body =
      nlohmann::json{{"authenticationTicket", std::string(ticket)}}.dump();

  HttpResponse redeemed = client.Request(
      kRedeemHost, kRedeemPath, "POST", body, "",
      "RBXAuthenticationNegotiation: 1\r\n");
  ClearSensitive(&body);

  if (!redeemed.error.empty())
    return Failure(redeemed.error);
  if (redeemed.status < 200 || redeemed.status >= 300)
    return Failure(
        "Roblox authentication-ticket redemption was rejected");

  std::string cookie;
  if (!ExtractRoblosecurity(redeemed.headers, &cookie))
    return Failure(
        "Roblox authentication-ticket redemption returned no unique session cookie");

  TlsHttpClient identity_client(ca_file);
  HttpResponse identity = identity_client.Request(
      kUserHost, kUserPath, "GET", "", cookie, "");
  ClearSensitive(&redeemed.headers);
  ClearSensitive(&redeemed.body);

  if (!identity.error.empty()) {
    ClearSensitive(&cookie);
    return Failure(identity.error);
  }
  if (identity.status < 200 || identity.status >= 300) {
    ClearSensitive(&cookie);
    return Failure("Roblox authenticated-user lookup was rejected");
  }

  const nlohmann::json document =
      nlohmann::json::parse(identity.body, nullptr, false);
  if (document.is_discarded() || !document.is_object() ||
      !document.contains("id") || !document["id"].is_number_integer()) {
    ClearSensitive(&cookie);
    ClearSensitive(&identity.body);
    return Failure("Roblox authenticated-user response is invalid");
  }

  RobloxLaunchTicketAuthResult result;
  result.roblosecurity = std::move(cookie);
  result.identity.user_id = document["id"].get<std::int64_t>();
  if (document.contains("name") && document["name"].is_string())
    result.identity.username = document["name"].get<std::string>();
  if (document.contains("displayName") &&
      document["displayName"].is_string())
    result.identity.display_name =
        document["displayName"].get<std::string>();
  ClearSensitive(&identity.body);
  return result;
}

}  // namespace mocktail::runtime
