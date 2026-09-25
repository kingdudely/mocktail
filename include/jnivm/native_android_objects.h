#ifndef MOCKTAIL_JNIVM_NATIVE_ANDROID_OBJECTS_H_
#define MOCKTAIL_JNIVM_NATIVE_ANDROID_OBJECTS_H_

#include <memory>
#include <string>
#include <string_view>

#include "jnivm/jnivm.h"

namespace jnivm {

enum class AndroidSystemService {
  kWindow,
  kDisplay,
  kAudio,
  kInputMethod,
  kSensor,
  kConnectivity,
  kPower,
  kUnknown,
};

class AndroidContext final : public NativeObject {
public:
  explicit AndroidContext(std::shared_ptr<Class> klass);

  const char* PackageName() const noexcept;
  const char* FilesDirectory() const noexcept;
  const char* CacheDirectory() const noexcept;
  const char* ExternalFilesDirectory() const noexcept;
  AndroidSystemService SystemService(std::string_view name) const noexcept;
};

class AndroidPackageManager final : public NativeObject {
public:
  explicit AndroidPackageManager(std::shared_ptr<Class> klass);

  bool HasSystemFeature(std::string_view name) const noexcept;
};

// Returns a typed native C++ object for framework classes that have already
// been migrated. Unknown classes return nullptr and continue through the
// existing generic compatibility object path.
std::unique_ptr<NativeObject> CreateNativeAndroidObject(
    const std::shared_ptr<Class>& klass);

}  // namespace jnivm

#endif  // MOCKTAIL_JNIVM_NATIVE_ANDROID_OBJECTS_H_
