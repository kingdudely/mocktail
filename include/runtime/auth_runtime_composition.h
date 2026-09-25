#ifndef MOCKTAIL_RUNTIME_AUTH_RUNTIME_COMPOSITION_H_
#define MOCKTAIL_RUNTIME_AUTH_RUNTIME_COMPOSITION_H_

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "jnivm/jnivm.h"

namespace mocktail::runtime {

enum class AuthRuntimeStatus {
  kAuthenticated,
  kGuest,
  kInvalidCredentials,
  kUnavailable,
};

class SecureRobloxCredential final {
 public:
  SecureRobloxCredential() = default;
  explicit SecureRobloxCredential(std::string canonical_header);
  ~SecureRobloxCredential();

  SecureRobloxCredential(const SecureRobloxCredential&) = delete;
  SecureRobloxCredential& operator=(const SecureRobloxCredential&) = delete;
  SecureRobloxCredential(SecureRobloxCredential&& other) noexcept;
  SecureRobloxCredential& operator=(SecureRobloxCredential&& other) noexcept;

  bool empty() const { return bytes_.empty(); }
  size_t size() const { return bytes_.empty() ? 0 : bytes_.size() - 1; }
  const char* c_str() const { return bytes_.empty() ? "" : bytes_.data(); }
  std::string_view view() const { return {c_str(), size()}; }
  void Clear();

 private:
  std::vector<char> bytes_;
};

void SecurelyClearString(std::string* value);

class ScopedRobloxCredentialBinding final {
 public:
  ScopedRobloxCredentialBinding(jnivm::VM* jni_vm,
                                const SecureRobloxCredential& credential);
  ~ScopedRobloxCredentialBinding();

  ScopedRobloxCredentialBinding(const ScopedRobloxCredentialBinding&) = delete;
  ScopedRobloxCredentialBinding& operator=(
      const ScopedRobloxCredentialBinding&) = delete;
  ScopedRobloxCredentialBinding(ScopedRobloxCredentialBinding&&) = delete;
  ScopedRobloxCredentialBinding& operator=(
      ScopedRobloxCredentialBinding&&) = delete;

  bool bound() const { return jni_vm_ != nullptr; }

 private:
  static jnivm::RobloxCredentialView ProvideCredential(const void* context);
  jnivm::VM* jni_vm_ = nullptr;
};

struct AuthRuntimeComposition {
  AuthRuntimeStatus status = AuthRuntimeStatus::kUnavailable;
  std::shared_ptr<jnivm::VM> jni_vm;
  jnivm::RobloxAuthIdentity account_identity;
  SecureRobloxCredential credential;
  std::string error;

  explicit operator bool() const { return jni_vm != nullptr; }
};

// The minimal launcher does not validate or persist the cookie. It binds the
// supplied credential directly to the pseudo-JVM used by libroblox.so.
AuthRuntimeComposition ComposeAuthRuntime(std::string_view roblosecurity);

}  // namespace mocktail::runtime

#endif  // MOCKTAIL_RUNTIME_AUTH_RUNTIME_COMPOSITION_H_
