#include "runtime/auth_runtime_composition.h"

#include <cstddef>
#include <string>
#include <string_view>
#include <utility>

namespace mocktail::runtime {
namespace {

void ClearSensitiveString(std::string* value) {
  if (value == nullptr) return;
  volatile char* bytes = value->empty() ? nullptr : value->data();
  for (std::size_t i = 0; i < value->size(); ++i) bytes[i] = '\0';
  value->clear();
}

std::string CanonicalCookieHeader(std::string_view value) {
  if (value.empty()) return {};
  constexpr std::string_view prefix = ".ROBLOSECURITY=";
  if (value.substr(0, prefix.size()) == prefix) return std::string(value);
  std::string result(prefix);
  result.append(value);
  return result;
}

}  // namespace

SecureRobloxCredential::SecureRobloxCredential(std::string canonical_header) {
  if (!canonical_header.empty()) {
    bytes_.assign(canonical_header.begin(), canonical_header.end());
    bytes_.push_back('\0');
  }
  ClearSensitiveString(&canonical_header);
}

SecureRobloxCredential::~SecureRobloxCredential() { Clear(); }

SecureRobloxCredential::SecureRobloxCredential(
    SecureRobloxCredential&& other) noexcept
    : bytes_(std::move(other.bytes_)) {}

SecureRobloxCredential& SecureRobloxCredential::operator=(
    SecureRobloxCredential&& other) noexcept {
  if (this != &other) {
    Clear();
    bytes_ = std::move(other.bytes_);
  }
  return *this;
}

void SecureRobloxCredential::Clear() {
  volatile char* bytes = bytes_.empty() ? nullptr : bytes_.data();
  for (std::size_t i = 0; i < bytes_.size(); ++i) bytes[i] = '\0';
  bytes_.clear();
}

void SecurelyClearString(std::string* value) { ClearSensitiveString(value); }

ScopedRobloxCredentialBinding::ScopedRobloxCredentialBinding(
    jnivm::VM* jni_vm, const SecureRobloxCredential& credential)
    : jni_vm_(jni_vm) {
  if (jni_vm_ != nullptr)
    jni_vm_->SetRobloxCredentialProvider(&credential, &ProvideCredential);
}

ScopedRobloxCredentialBinding::~ScopedRobloxCredentialBinding() {
  if (jni_vm_ != nullptr) jni_vm_->ClearRobloxCredentialProvider();
}

jnivm::RobloxCredentialView ScopedRobloxCredentialBinding::ProvideCredential(
    const void* context) {
  const auto* credential =
      static_cast<const SecureRobloxCredential*>(context);
  if (credential == nullptr) return {};
  return {credential->c_str(), credential->size()};
}

AuthRuntimeComposition ComposeAuthRuntime(
    std::string_view roblosecurity,
    const jnivm::RobloxAuthIdentity& identity) {
  AuthRuntimeComposition result;
  result.jni_vm = std::make_shared<jnivm::VM>();
  result.account_identity = identity;
  result.jni_vm->SetRobloxAuthIdentity(identity);

  std::string canonical = CanonicalCookieHeader(roblosecurity);
  if (canonical.empty()) {
    result.status = AuthRuntimeStatus::kGuest;
    return result;
  }

  result.status = AuthRuntimeStatus::kAuthenticated;
  result.credential = SecureRobloxCredential(std::move(canonical));
  return result;
}

}  // namespace mocktail::runtime
