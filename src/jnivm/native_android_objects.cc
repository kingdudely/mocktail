#include "jnivm/native_android_objects.h"

#include <string_view>

namespace jnivm {

AndroidContext::AndroidContext(std::shared_ptr<Class> klass)
    : NativeObject(std::move(klass)) {}

const char* AndroidContext::PackageName() const noexcept {
  return "com.roblox.client";
}

const char* AndroidContext::FilesDirectory() const noexcept {
  return "/data/user/0/com.roblox.client/files";
}

const char* AndroidContext::CacheDirectory() const noexcept {
  return "/data/user/0/com.roblox.client/cache";
}

const char* AndroidContext::ExternalFilesDirectory() const noexcept {
  return "/sdcard/Android/data/com.roblox.client/files";
}

AndroidSystemService AndroidContext::SystemService(
    std::string_view name) const noexcept {
  if (name == "window") return AndroidSystemService::kWindow;
  if (name == "display") return AndroidSystemService::kDisplay;
  if (name == "audio") return AndroidSystemService::kAudio;
  if (name == "input_method") return AndroidSystemService::kInputMethod;
  if (name == "sensor") return AndroidSystemService::kSensor;
  if (name == "connectivity") return AndroidSystemService::kConnectivity;
  if (name == "power") return AndroidSystemService::kPower;
  return AndroidSystemService::kUnknown;
}

AndroidPackageManager::AndroidPackageManager(std::shared_ptr<Class> klass)
    : NativeObject(std::move(klass)) {}

bool AndroidPackageManager::HasSystemFeature(std::string_view name,
                                             bool pc_hardware,
                                             bool touch_enabled) const
    noexcept {
  if (name == "android.hardware.type.pc") return pc_hardware;
  if (name.rfind("android.hardware.touchscreen", 0) == 0) {
    return touch_enabled;
  }
  return false;
}

std::unique_ptr<NativeObject> CreateNativeAndroidObject(
    const std::shared_ptr<Class>& klass) {
  if (klass == nullptr) return nullptr;

  const std::string& name = klass->GetName();
  if (name == "android/content/Context" ||
      name == "android/app/Application" ||
      name == "android/app/Activity" ||
      name == "com/roblox/client/startup/MainGameActivity") {
    return std::make_unique<AndroidContext>(klass);
  }
  if (name == "android/content/pm/PackageManager") {
    return std::make_unique<AndroidPackageManager>(klass);
  }
  return nullptr;
}

}  // namespace jnivm
