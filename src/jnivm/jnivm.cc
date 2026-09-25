#include "jnivm/jnivm.h"

#include <algorithm>
#include <atomic>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <unordered_map>
#include <utility>
#include <vector>

#include "runtime/display_size.h"

namespace jnivm {
namespace {

std::mutex g_vm_mutex;
std::vector<VM*> g_vms;

template <typename T>
std::shared_ptr<T> ObjectFromJni(JNIEnv* env, jobject object) {
  if (env == nullptr || object == nullptr) {
    return nullptr;
  }
  auto value = mocktail_libjnivm::JNITypes<
      std::shared_ptr<Object>>::JNICast(
      mocktail_libjnivm::ENV::FromJNIEnv(env), object);
  return std::dynamic_pointer_cast<T>(value);
}

std::shared_ptr<Object> GenericObject(ENV* env, const char* class_name) {
  if (env == nullptr || class_name == nullptr) {
    return nullptr;
  }
  auto value = std::make_shared<Object>();
  value->clazz = env->GetClass(class_name);
  return value;
}

jstring NewString(JNIEnv* env, const char* value) {
  return env != nullptr && value != nullptr ? env->NewStringUTF(value)
                                             : nullptr;
}

std::string StringValue(JNIEnv* env, jstring value) {
  if (env == nullptr || value == nullptr) {
    return {};
  }
  const char* text = env->GetStringUTFChars(value, nullptr);
  if (text == nullptr) {
    return {};
  }
  std::string result(text);
  env->ReleaseStringUTFChars(value, text);
  return result;
}

PlatformIdentity Platform(const VM* vm) {
  return vm != nullptr ? vm->GetPlatformIdentitySnapshot() : PlatformIdentity{};
}

class AndroidFile final : public Object {
 public:
  std::string path;
};

class AndroidContext : public Object {
 public:
  virtual ~AndroidContext() = default;
};

class AndroidApplication final : public AndroidContext {};
class AndroidActivity : public AndroidApplication {};
class MainGameActivity final : public AndroidActivity {};

class AndroidPackageManager final : public Object {};
class AndroidConfiguration final : public Object {
 public:
  jint color_mode = 0;
  jint density_dpi = 160;
  jint font_weight_adjustment = 0;
  jint hard_keyboard_hidden = 1;
  jint keyboard = 2;
  jint keyboard_hidden = 1;
  jint mcc = 0;
  jint mnc = 0;
  jint navigation = 1;
  jint navigation_hidden = 1;
  jint orientation = 2;
  jint screen_height_dp = 0;
  jint screen_layout = 0;
  jint screen_width_dp = 0;
  jint smallest_screen_width_dp = 0;
  jint touchscreen = 1;
  jint ui_mode = 0;
};

class NativeTextBoxInfoObject final : public Object {
 public:
  float x = 0.0f;
  float y = 0.0f;
  float width = 0.0f;
  float height = 0.0f;
  float font_size = 0.0f;
  jboolean multiline = JNI_FALSE;
  jint x_alignment = 0;
  jint y_alignment = 0;
  jint text_color = 0;
  jint font = 0;
  jint text_input_type = 0;
  jint return_key_type = 0;
  jboolean manual_focus_release = JNI_FALSE;
  jboolean text_wrapped = JNI_FALSE;
};

class WebRtcAudioManagerObject final : public Object {
 public:
  jlong native_audio_manager = 0;
  bool parameters_cached = false;
  bool initialized = false;
  WebRtcAudioManagerParameters parameters;
};

class WebRtcAudioRecordObject final : public Object {
 public:
  jlong native_audio_record = 0;
};

class WebRtcAudioTrackObject final : public Object {
 public:
  jlong native_audio_track = 0;
};

std::shared_ptr<Object> FileObject(ENV* env, std::string path) {
  auto object = std::make_shared<AndroidFile>();
  object->clazz = env->GetClass<AndroidFile>("java/io/File");
  object->path = std::move(path);
  return object;
}

std::shared_ptr<Object> ServiceObject(ENV* env, const char* class_name) {
  return GenericObject(env, class_name);
}

std::string CookieHeaderFromJString(JNIEnv* env, jstring cookie) {
  const std::string value = StringValue(env, cookie);
  if (value.empty()) {
    return {};
  }
  constexpr const char* prefix = ".ROBLOSECURITY=";
  if (value.rfind(prefix, 0) == 0) {
    return value;
  }
  return std::string(prefix) + value;
}

template <typename Binding>
void ClearBinding(Binding* binding) {
  if (binding != nullptr) {
    *binding = {};
  }
}

template <typename T>
T ClampJlong(jlong value) {
  if (value < 0 || static_cast<unsigned long long>(value) >
                       static_cast<unsigned long long>(
                           std::numeric_limits<T>::max())) {
    return 0;
  }
  return static_cast<T>(value);
}

VM* VmFromEnv(JNIEnv* env) {
  if (env == nullptr) {
    return nullptr;
  }
  auto* base = mocktail_libjnivm::ENV::FromJNIEnv(env)->GetVM();
  return dynamic_cast<VM*>(base);
}

void HookContext(ENV* env, std::shared_ptr<Class> clazz) {
  if (!env || !clazz) return;

  clazz->HookInstanceFunction(env, "getPackageName",
      [](JNIEnv* e, Object*) { return NewString(e, "com.roblox.client"); });

  clazz->HookInstanceFunction(env, "getFilesDir",
      [](JNIEnv* e, Object*) {
        return FileObject(e, "/data/user/0/com.roblox.client/files");
      });

  clazz->HookInstanceFunction(env, "getCacheDir",
      [](JNIEnv* e, Object*) {
        return FileObject(e, "/data/user/0/com.roblox.client/cache");
      });

  clazz->HookInstanceFunction(env, "getExternalFilesDir",
      [](JNIEnv* e, Object*, jstring) {
        return FileObject(e, "/sdcard/Android/data/com.roblox.client/files");
      });

  clazz->HookInstanceFunction(env, "getApplicationContext",
      [](JNIEnv* e, Object*) {
        auto* current = mocktail_libjnivm::ENV::FromJNIEnv(e);
        return current->GetClass<AndroidApplication>(
            "android/app/Application")->Instantiate(current);
      });

  clazz->HookInstanceFunction(env, "getBaseContext",
      [](JNIEnv* e, Object*) {
        auto* current = mocktail_libjnivm::ENV::FromJNIEnv(e);
        return current->GetClass<AndroidContext>(
            "android/content/Context")->Instantiate(current);
      });

  clazz->HookInstanceFunction(env, "getContext",
      [](JNIEnv* e, Object*) {
        auto* current = mocktail_libjnivm::ENV::FromJNIEnv(e);
        return current->GetClass<AndroidContext>(
            "android/content/Context")->Instantiate(current);
      });

  clazz->HookInstanceFunction(env, "getAssets",
      [](JNIEnv* e, Object*) {
        return GenericObject(mocktail_libjnivm::ENV::FromJNIEnv(e),
                             "android/content/res/AssetManager");
      });

  clazz->HookInstanceFunction(env, "getAssetManager",
      [](JNIEnv* e, Object*) {
        return GenericObject(mocktail_libjnivm::ENV::FromJNIEnv(e),
                             "android/content/res/AssetManager");
      });

  clazz->HookInstanceFunction(env, "getResources",
      [](JNIEnv* e, Object*) {
        return GenericObject(mocktail_libjnivm::ENV::FromJNIEnv(e),
                             "android/content/res/Resources");
      });

  clazz->HookInstanceFunction(env, "getClassLoader",
      [](JNIEnv* e, Object*) {
        return GenericObject(mocktail_libjnivm::ENV::FromJNIEnv(e),
                             "java/lang/ClassLoader");
      });

  clazz->HookInstanceFunction(env, "getSharedPreferences",
      [](JNIEnv* e, Object*, jstring, jint) {
        return GenericObject(mocktail_libjnivm::ENV::FromJNIEnv(e),
                             "android/content/SharedPreferences");
      });

  clazz->HookInstanceFunction(env, "getPackageManager",
      [](JNIEnv* e, Object*) {
        auto* current = mocktail_libjnivm::ENV::FromJNIEnv(e);
        return current->GetClass<AndroidPackageManager>(
            "android/content/pm/PackageManager")->Instantiate(current);
      });

  clazz->HookInstanceFunction(env, "getSystemService",
      [](JNIEnv* e, Object*, jstring service) -> std::shared_ptr<Object> {
        auto* current = mocktail_libjnivm::ENV::FromJNIEnv(e);
        const std::string name = StringValue(e, service);
        if (name == "window") return ServiceObject(current, "android/view/WindowManager");
        if (name == "display") return ServiceObject(current, "android/hardware/display/DisplayManager");
        if (name == "audio") return ServiceObject(current, "android/media/AudioManager");
        if (name == "input_method") {
          return ServiceObject(current, "android/view/inputmethod/InputMethodManager");
        }
        if (name == "sensor") return ServiceObject(current, "android/hardware/SensorManager");
        if (name == "connectivity") return ServiceObject(current, "android/net/ConnectivityManager");
        if (name == "power") return ServiceObject(current, "android/os/PowerManager");
        return GenericObject(current, "java/lang/Object");
      });
}

void HookFiles(ENV* env) {
  auto clazz = env->GetClass<AndroidFile>("java/io/File");
  clazz->HookInstanceFunction(env, "getPath",
      [](JNIEnv* e, AndroidFile* file) { return NewString(e, file->path.c_str()); });
  clazz->HookInstanceFunction(env, "getAbsolutePath",
      [](JNIEnv* e, AndroidFile* file) { return NewString(e, file->path.c_str()); });
  clazz->HookInstanceFunction(env, "getCanonicalPath",
      [](JNIEnv* e, AndroidFile* file) { return NewString(e, file->path.c_str()); });
  clazz->HookInstanceFunction(env, "toString",
      [](JNIEnv* e, AndroidFile* file) { return NewString(e, file->path.c_str()); });
}

void HookPackageManager(ENV* env, VM* vm) {
  auto clazz = env->GetClass<AndroidPackageManager>(
      "android/content/pm/PackageManager");
  clazz->HookInstanceFunction(env, "hasSystemFeature",
      [vm](JNIEnv* e, AndroidPackageManager*, jstring feature) -> jboolean {
        const std::string name = StringValue(e, feature);
        const PlatformIdentity identity = Platform(vm);
        if (name == "android.hardware.type.pc") {
          return identity.pc_hardware ? JNI_TRUE : JNI_FALSE;
        }
        if (name.rfind("android.hardware.touchscreen", 0) == 0) {
          return identity.touch_enabled ? JNI_TRUE : JNI_FALSE;
        }
        return JNI_FALSE;
      });
}

void HookConfiguration(ENV* env) {
  auto clazz = env->GetClass<AndroidConfiguration>(
      "android/content/res/Configuration");
  clazz->HookInstanceGetterFunction(env, "colorMode",
      [](AndroidConfiguration* o) { return o->color_mode; });
  clazz->HookInstanceGetterFunction(env, "densityDpi",
      [](AndroidConfiguration* o) { return o->density_dpi; });
  clazz->HookInstanceGetterFunction(env, "fontWeightAdjustment",
      [](AndroidConfiguration* o) { return o->font_weight_adjustment; });
  clazz->HookInstanceGetterFunction(env, "hardKeyboardHidden",
      [](AndroidConfiguration* o) { return o->hard_keyboard_hidden; });
  clazz->HookInstanceGetterFunction(env, "keyboard",
      [](AndroidConfiguration* o) { return o->keyboard; });
  clazz->HookInstanceGetterFunction(env, "keyboardHidden",
      [](AndroidConfiguration* o) { return o->keyboard_hidden; });
  clazz->HookInstanceGetterFunction(env, "mcc",
      [](AndroidConfiguration* o) { return o->mcc; });
  clazz->HookInstanceGetterFunction(env, "mnc",
      [](AndroidConfiguration* o) { return o->mnc; });
  clazz->HookInstanceGetterFunction(env, "navigation",
      [](AndroidConfiguration* o) { return o->navigation; });
  clazz->HookInstanceGetterFunction(env, "navigationHidden",
      [](AndroidConfiguration* o) { return o->navigation_hidden; });
  clazz->HookInstanceGetterFunction(env, "orientation",
      [](AndroidConfiguration* o) { return o->orientation; });
  clazz->HookInstanceGetterFunction(env, "screenHeightDp",
      [](AndroidConfiguration* o) { return o->screen_height_dp; });
  clazz->HookInstanceGetterFunction(env, "screenLayout",
      [](AndroidConfiguration* o) { return o->screen_layout; });
  clazz->HookInstanceGetterFunction(env, "screenWidthDp",
      [](AndroidConfiguration* o) { return o->screen_width_dp; });
  clazz->HookInstanceGetterFunction(env, "smallestScreenWidthDp",
      [](AndroidConfiguration* o) { return o->smallest_screen_width_dp; });
  clazz->HookInstanceGetterFunction(env, "touchscreen",
      [](AndroidConfiguration* o) { return o->touchscreen; });
  clazz->HookInstanceGetterFunction(env, "uiMode",
      [](AndroidConfiguration* o) { return o->ui_mode; });
}

void HookTextBoxInfo(ENV* env) {
  auto clazz = env->GetClass<NativeTextBoxInfoObject>(
      "com/roblox/engine/jni/model/NativeTextBoxInfo");
  clazz->HookInstanceGetterFunction(env, "x",
      [](NativeTextBoxInfoObject* o) { return o->x; });
  clazz->HookInstanceGetterFunction(env, "y",
      [](NativeTextBoxInfoObject* o) { return o->y; });
  clazz->HookInstanceGetterFunction(env, "width",
      [](NativeTextBoxInfoObject* o) { return o->width; });
  clazz->HookInstanceGetterFunction(env, "height",
      [](NativeTextBoxInfoObject* o) { return o->height; });
  clazz->HookInstanceGetterFunction(env, "fontSize",
      [](NativeTextBoxInfoObject* o) { return o->font_size; });
  clazz->HookInstanceGetterFunction(env, "multiline",
      [](NativeTextBoxInfoObject* o) { return o->multiline; });
  clazz->HookInstanceGetterFunction(env, "xAlignment",
      [](NativeTextBoxInfoObject* o) { return o->x_alignment; });
  clazz->HookInstanceGetterFunction(env, "yAlignment",
      [](NativeTextBoxInfoObject* o) { return o->y_alignment; });
  clazz->HookInstanceGetterFunction(env, "textColor",
      [](NativeTextBoxInfoObject* o) { return o->text_color; });
  clazz->HookInstanceGetterFunction(env, "font",
      [](NativeTextBoxInfoObject* o) { return o->font; });
  clazz->HookInstanceGetterFunction(env, "textInputType",
      [](NativeTextBoxInfoObject* o) { return o->text_input_type; });
  clazz->HookInstanceGetterFunction(env, "returnKeyType",
      [](NativeTextBoxInfoObject* o) { return o->return_key_type; });
  clazz->HookInstanceGetterFunction(env, "manualFocusRelease",
      [](NativeTextBoxInfoObject* o) { return o->manual_focus_release; });
  clazz->HookInstanceGetterFunction(env, "textWrapped",
      [](NativeTextBoxInfoObject* o) { return o->text_wrapped; });
}

void HookWebRtc(ENV* env, VM* vm) {
  auto manager = env->GetClass<WebRtcAudioManagerObject>(
      "org/webrtc/voiceengine/WebRtcAudioManager");
  manager->HookInstanceFunction(env, "<init>",
      [vm](JNIEnv*, WebRtcAudioManagerObject* o, jlong native_handle) {
        o->native_audio_manager = native_handle;
      });
  manager->HookInstanceFunction(env, "init",
      [vm](JNIEnv*, WebRtcAudioManagerObject* o) {
        o->initialized = vm->DispatchWebRtcAudioManagerInit(
            reinterpret_cast<jobject>(o));
        return o->initialized ? JNI_TRUE : JNI_FALSE;
      });
  manager->HookInstanceFunction(env, "dispose",
      [vm](JNIEnv*, WebRtcAudioManagerObject* o) {
        vm->DispatchWebRtcAudioManagerDispose(reinterpret_cast<jobject>(o));
      });
  manager->HookInstanceFunction(env, "setMicrophoneMute",
      [vm](JNIEnv*, WebRtcAudioManagerObject* o, jboolean muted) {
        vm->DispatchWebRtcAudioManagerMicrophoneMute(
            reinterpret_cast<jobject>(o), muted != JNI_FALSE);
      });
  manager->HookInstanceFunction(env, "isCommunicationModeEnabled",
      [](JNIEnv*, WebRtcAudioManagerObject* o) { return o->initialized ? JNI_TRUE : JNI_FALSE; });
  manager->HookInstanceFunction(env, "isLowLatencyOutputSupported",
      [](JNIEnv*, WebRtcAudioManagerObject*) { return JNI_FALSE; });
  manager->HookInstanceFunction(env, "isLowLatencyInputSupported",
      [](JNIEnv*, WebRtcAudioManagerObject*) { return JNI_FALSE; });
  manager->HookInstanceFunction(env, "isProAudioSupported",
      [](JNIEnv*, WebRtcAudioManagerObject*) { return JNI_FALSE; });
  manager->HookInstanceFunction(env, "isAAudioSupported",
      [](JNIEnv*, WebRtcAudioManagerObject*) { return JNI_FALSE; });
  manager->HookInstanceFunction(env, "isDeviceBlacklistedForOpenSLESUsage",
      [](JNIEnv*, WebRtcAudioManagerObject*) { return JNI_TRUE; });

  auto record = env->GetClass<WebRtcAudioRecordObject>(
      "org/webrtc/voiceengine/WebRtcAudioRecord");
  record->HookInstanceFunction(env, "initRecording",
      [vm](JNIEnv*, WebRtcAudioRecordObject* o, jint rate, jint channels) {
        void* buffer = nullptr;
        std::size_t capacity = 0;
        return vm->DispatchWebRtcAudioRecordInit(
            reinterpret_cast<jobject>(o), rate, channels, &buffer, &capacity);
      });
  record->HookInstanceFunction(env, "startRecording",
      [vm](JNIEnv*, WebRtcAudioRecordObject* o) {
        return vm->DispatchWebRtcAudioRecordStart(reinterpret_cast<jobject>(o))
                   ? JNI_TRUE
                   : JNI_FALSE;
      });
  record->HookInstanceFunction(env, "stopRecording",
      [vm](JNIEnv*, WebRtcAudioRecordObject* o) {
        return vm->DispatchWebRtcAudioRecordStop(reinterpret_cast<jobject>(o))
                   ? JNI_TRUE
                   : JNI_FALSE;
      });
  record->HookInstanceFunction(env, "enableBuiltInAEC",
      [](JNIEnv*, WebRtcAudioRecordObject*, jboolean) { return JNI_TRUE; });
  record->HookInstanceFunction(env, "enableBuiltInNS",
      [](JNIEnv*, WebRtcAudioRecordObject*, jboolean) { return JNI_TRUE; });
  record->HookInstanceFunction(env, "enableBuiltInAGC",
      [](JNIEnv*, WebRtcAudioRecordObject*, jboolean) { return JNI_TRUE; });
  record->HookInstanceFunction(env, "isAudioConfigVerified",
      [](JNIEnv*, WebRtcAudioRecordObject*) { return JNI_TRUE; });
  record->HookInstanceFunction(env, "isAudioSourceMatchingRecordingSession",
      [](JNIEnv*, WebRtcAudioRecordObject*) { return JNI_TRUE; });

  auto track = env->GetClass<WebRtcAudioTrackObject>(
      "org/webrtc/voiceengine/WebRtcAudioTrack");
  track->HookInstanceFunction(env, "initPlayout",
      [vm](JNIEnv*, WebRtcAudioTrackObject* o, jint rate, jint channels,
           jdouble factor) {
        void* buffer = nullptr;
        std::size_t capacity = 0;
        return vm->DispatchWebRtcAudioTrackInit(
            reinterpret_cast<jobject>(o), rate, channels, factor, &buffer,
            &capacity);
      });
  track->HookInstanceFunction(env, "getBufferSizeInFrames",
      [vm](JNIEnv*, WebRtcAudioTrackObject* o) {
        return vm->DispatchWebRtcAudioTrackBufferSizeFrames(
            reinterpret_cast<jobject>(o));
      });
  track->HookInstanceFunction(env, "startPlayout",
      [vm](JNIEnv*, WebRtcAudioTrackObject* o) {
        return vm->DispatchWebRtcAudioTrackStart(reinterpret_cast<jobject>(o))
                   ? JNI_TRUE
                   : JNI_FALSE;
      });
  track->HookInstanceFunction(env, "stopPlayout",
      [vm](JNIEnv*, WebRtcAudioTrackObject* o) {
        return vm->DispatchWebRtcAudioTrackStop(reinterpret_cast<jobject>(o))
                   ? JNI_TRUE
                   : JNI_FALSE;
      });
  track->HookInstanceFunction(env, "setStreamVolume",
      [](JNIEnv*, WebRtcAudioTrackObject*, jint) { return JNI_TRUE; });
  track->HookInstanceFunction(env, "getStreamVolume",
      [](JNIEnv*, WebRtcAudioTrackObject*) { return 100; });
  track->HookInstanceFunction(env, "getStreamMaxVolume",
      [](JNIEnv*, WebRtcAudioTrackObject*) { return 100; });
}

void HookNativeGl(ENV* env, VM* vm) {
  auto clazz = env->GetClass("com/roblox/engine/jni/NativeGLJavaInterface");
  clazz->Hook(env, "setAppBridgeNotificationListener",
      [vm](JNIEnv*, Class*, jobject) {});
  clazz->Hook(env, "setImplementation",
      [vm](JNIEnv*, Class*, jobject) {});
  clazz->Hook(env, "onAppBridgeNotification",
      [vm](JNIEnv* e, Class*, jstring type, jstring data) {
        vm->DispatchAndroidWindowFlags(0, 0);
        return;
      });
  clazz->Hook(env, "openNativeOverlay",
      [vm](JNIEnv* e, Class*, jstring title, jstring url) {
        (void)e; (void)title; (void)url;
        return;
      });
  clazz->Hook(env, "onDataModelNotificationCallback",
      [](JNIEnv*, Class*, jstring, jstring) {});
  clazz->Hook(env, "gameDidLeave",
      [vm](JNIEnv*, Class*) {});
}

void HookRobloxObjects(ENV* env, VM* vm) {
  const char* context_classes[] = {
      "android/content/Context",
      "android/app/Application",
      "android/app/Activity",
      "com/roblox/client/RobloxActivity",
      "com/roblox/client/startup/MainGameActivity",
      "com/google/androidgamesdk/GameActivity",
  };
  env->GetClass<AndroidContext>("android/content/Context");
  env->GetClass<AndroidApplication>("android/app/Application");
  env->GetClass<AndroidActivity>("android/app/Activity");
  env->GetClass<MainGameActivity>("com/roblox/client/startup/MainGameActivity");
  for (const char* name : context_classes) {
    HookContext(env, env->GetClass(name));
  }

  env->GetClass<AndroidPackageManager>("android/content/pm/PackageManager");
  HookPackageManager(env, vm);
  HookFiles(env);
  HookConfiguration(env);
  HookTextBoxInfo(env);
  env->GetClass<WebRtcAudioManagerObject>("org/webrtc/voiceengine/WebRtcAudioManager");
  env->GetClass<WebRtcAudioRecordObject>("org/webrtc/voiceengine/WebRtcAudioRecord");
  env->GetClass<WebRtcAudioTrackObject>("org/webrtc/voiceengine/WebRtcAudioTrack");
  HookWebRtc(env, vm);
  HookNativeGl(env, vm);
}

}  // namespace

VM::VM() : mocktail_libjnivm::VM() {
  std::lock_guard<std::mutex> lock(g_vm_mutex);
  g_vms.push_back(this);
  InstallHooks();
}

VM::~VM() {
  {
    std::lock_guard<std::mutex> lock(g_vm_mutex);
    g_vms.erase(std::remove(g_vms.begin(), g_vms.end(), this), g_vms.end());
  }
  ClearFmodAudioDeviceCallbacks();
  ClearWebRtcAudioManagerCallbacks();
  ClearWebRtcAudioRecordCallbacks();
  ClearWebRtcAudioTrackCallbacks();
  ClearAndroidWindowCallbacks();
  ClearRobloxTextInputCallbacks();
  ClearRobloxCredentialSink();
  ClearRobloxCredentialProvider();
}

VM* VM::FromJavaVM(JavaVM* vm) {
  std::lock_guard<std::mutex> lock(g_vm_mutex);
  for (VM* current : g_vms) {
    if (current->GetJavaVM() == vm) return current;
  }
  return nullptr;
}

std::shared_ptr<Class> VM::RegisterClass(const char* name) {
  return GetEnv()->GetClass(name);
}

void VM::RegisterMethod(const std::shared_ptr<Class>& clazz, const char* name,
                        const char*, void (*callback)(JNIEnv*, jobject)) {
  if (clazz && name && callback) {
    clazz->HookInstance(GetEnv().get(), name, callback);
  }
}

std::size_t VM::GetClassCount() const { return classes.size(); }

void VM::RestoreFunctions() {
  // libjnivm owns the JNI function table; Roblox cannot permanently replace it.
}

void VM::SetRobloxAuthIdentity(const RobloxAuthIdentity& identity) {
  std::lock_guard<std::mutex> lock(auth_mutex_);
  auth_identity_ = identity;
  if (auth_identity_.user_id <= 0) {
    auth_identity_ = {};
  }
}

void VM::ClearRobloxAuthIdentity() {
  std::lock_guard<std::mutex> lock(auth_mutex_);
  auth_identity_ = {};
}

RobloxAuthIdentity VM::GetRobloxAuthIdentitySnapshot() const {
  std::lock_guard<std::mutex> lock(auth_mutex_);
  return auth_identity_;
}

void VM::SetPlatformIdentity(const PlatformIdentity& identity) {
  std::lock_guard<std::mutex> lock(platform_mutex_);
  platform_identity_ = identity;
}

PlatformIdentity VM::GetPlatformIdentitySnapshot() const {
  std::lock_guard<std::mutex> lock(platform_mutex_);
  return platform_identity_;
}

void VM::SetRobloxCredentialProvider(const void* context,
                                     RobloxCredentialProvider provider) {
  std::lock_guard<std::mutex> lock(credential_mutex_);
  credential_context_ = context;
  credential_provider_ = provider;
  credential_override_.clear();
}

void VM::ClearRobloxCredentialProvider() {
  std::lock_guard<std::mutex> lock(credential_mutex_);
  credential_context_ = nullptr;
  credential_provider_ = nullptr;
  credential_override_.clear();
}

bool VM::CopyRobloxCredentialFromProvider(std::string* credential) const {
  std::lock_guard<std::mutex> lock(credential_mutex_);
  if (credential_override_.empty() && credential_provider_ == nullptr) {
    return false;
  }
  if (credential != nullptr) {
    credential->assign(credential_override_);
    if (credential->empty() && credential_provider_ != nullptr) {
      const RobloxCredentialView value =
          credential_provider_(credential_context_);
      if (value.data != nullptr && value.size != 0) {
        credential->assign(value.data, value.size);
      }
    }
  }
  return true;
}

void VM::SetRobloxCookieGetter(RobloxCookieGetter getter) {
  std::lock_guard<std::mutex> lock(cookie_mutex_);
  cookie_getter_ = getter;
}

bool VM::RefreshRobloxCredentialFromEngine(JNIEnv* env) {
  RobloxCookieGetter getter = nullptr;
  {
    std::lock_guard<std::mutex> lock(cookie_mutex_);
    getter = cookie_getter_;
  }
  if (getter == nullptr || env == nullptr) {
    return false;
  }
  jclass clazz = env->FindClass("com/roblox/universalapp/cookie/JNICookieManager");
  if (clazz == nullptr) return false;
  jstring cookie = getter(env, clazz, NewString(env, "roblox.com"));
  if (cookie == nullptr) {
    return false;
  }
  const std::string value = CookieHeaderFromJString(env, cookie);
  if (value.empty()) return false;
  return DispatchRobloxCredential(value.data(), value.size());
}

void VM::SetRobloxCredentialSink(
    std::shared_ptr<void> context,
    const RobloxCredentialSinkCallbacks& callbacks) {
  std::lock_guard<std::mutex> lock(credential_sink_mutex_);
  credential_sink_ = {std::move(context), callbacks};
}

void VM::ClearRobloxCredentialSink() {
  std::lock_guard<std::mutex> lock(credential_sink_mutex_);
  credential_sink_ = {};
}

bool VM::DispatchRobloxCredential(const char* data, std::size_t size) {
  if (!data || size == 0) return false;
  CredentialSinkBinding binding;
  {
    std::lock_guard<std::mutex> lock(credential_sink_mutex_);
    binding = credential_sink_;
  }
  if (binding.context && binding.callbacks.store) {
    return binding.callbacks.store(binding.context.get(), data, size);
  }
  std::lock_guard<std::mutex> lock(credential_mutex_);
  credential_override_.assign(data, size);
  return true;
}

void VM::SetFmodAudioDeviceCallbacks(
    std::shared_ptr<void> context, const FmodAudioDeviceCallbacks& callbacks) {
  FmodBinding old;
  {
    std::lock_guard<std::mutex> lock(fmod_mutex_);
    old = std::move(fmod_);
    fmod_ = {std::move(context), callbacks};
  }
  if (old.context && old.callbacks.shutdown) old.callbacks.shutdown(old.context.get());
}

void VM::ClearFmodAudioDeviceCallbacks() {
  FmodBinding old;
  {
    std::lock_guard<std::mutex> lock(fmod_mutex_);
    old = std::move(fmod_);
    fmod_ = {};
  }
  if (old.context && old.callbacks.shutdown) old.callbacks.shutdown(old.context.get());
}

bool VM::DispatchFmodAudioDeviceInit(const void* identity, int channels,
                                     int sample_rate_hz, int block_size_frames,
                                     int block_count) {
  FmodBinding b;
  { std::lock_guard<std::mutex> lock(fmod_mutex_); b = fmod_; }
  return b.context && b.callbacks.init &&
         b.callbacks.init(b.context.get(), identity, channels, sample_rate_hz,
                          block_size_frames, block_count);
}

bool VM::DispatchFmodAudioDeviceWrite(const void* identity,
                                      const std::uint8_t* data, std::size_t size) {
  FmodBinding b;
  { std::lock_guard<std::mutex> lock(fmod_mutex_); b = fmod_; }
  return b.context && b.callbacks.write &&
         b.callbacks.write(b.context.get(), identity, data, size);
}

bool VM::DispatchFmodAudioDeviceClose(const void* identity) {
  FmodBinding b;
  { std::lock_guard<std::mutex> lock(fmod_mutex_); b = fmod_; }
  return b.context && b.callbacks.close &&
         b.callbacks.close(b.context.get(), identity);
}

void VM::SetWebRtcAudioManagerCallbacks(
    std::shared_ptr<void> context, const WebRtcAudioManagerCallbacks& callbacks) {
  std::lock_guard<std::mutex> lock(webrtc_manager_mutex_);
  webrtc_manager_ = {std::move(context), callbacks};
}

void VM::ClearWebRtcAudioManagerCallbacks() {
  WebRtcManagerBinding old;
  { std::lock_guard<std::mutex> lock(webrtc_manager_mutex_);
    old = std::move(webrtc_manager_); webrtc_manager_ = {}; }
}

bool VM::DispatchWebRtcAudioManagerInit(jobject manager) {
  WebRtcManagerBinding b;
  { std::lock_guard<std::mutex> lock(webrtc_manager_mutex_); b = webrtc_manager_; }
  auto* object = GetJNIEnv() ? mocktail_libjnivm::JNITypes<
      std::shared_ptr<Object>>::JNICast(mocktail_libjnivm::ENV::FromJNIEnv(GetJNIEnv()), manager).get() : nullptr;
  (void)object;
  return b.context && b.callbacks.init &&
         b.callbacks.init(b.context.get(), manager);
}

void VM::DispatchWebRtcAudioManagerDispose(jobject manager) {
  WebRtcManagerBinding b;
  { std::lock_guard<std::mutex> lock(webrtc_manager_mutex_); b = webrtc_manager_; }
  if (b.context && b.callbacks.dispose) b.callbacks.dispose(b.context.get(), manager);
}

void VM::DispatchWebRtcAudioManagerMicrophoneMute(jobject manager, bool muted) {
  WebRtcManagerBinding b;
  { std::lock_guard<std::mutex> lock(webrtc_manager_mutex_); b = webrtc_manager_; }
  if (b.context && b.callbacks.set_microphone_mute)
    b.callbacks.set_microphone_mute(b.context.get(), muted);
}

void VM::SetWebRtcAudioRecordCallbacks(
    std::shared_ptr<void> context, const WebRtcAudioRecordCallbacks& callbacks) {
  WebRtcRecordBinding old;
  {
    std::lock_guard<std::mutex> lock(webrtc_record_mutex_);
    old = std::move(webrtc_record_);
    webrtc_record_ = {std::move(context), callbacks};
  }
  if (old.context && old.callbacks.shutdown) old.callbacks.shutdown(old.context.get());
}

void VM::ClearWebRtcAudioRecordCallbacks() {
  WebRtcRecordBinding old;
  { std::lock_guard<std::mutex> lock(webrtc_record_mutex_);
    old = std::move(webrtc_record_); webrtc_record_ = {}; }
  if (old.context && old.callbacks.shutdown) old.callbacks.shutdown(old.context.get());
}

namespace {

bool CallNativeVoid(JNIEnv* env, jobject object, const char* name,
                    const char* signature, jint size, jlong native_handle,
                    jobject byte_buffer = nullptr) {
  if (env == nullptr || object == nullptr || name == nullptr ||
      signature == nullptr) {
    return false;
  }
  jclass clazz = env->GetObjectClass(object);
  if (clazz == nullptr) return false;
  jmethodID method = env->GetMethodID(clazz, name, signature);
  env->DeleteLocalRef(clazz);
  if (method == nullptr) return false;
  if (byte_buffer != nullptr) {
    env->CallVoidMethod(object, method, byte_buffer, native_handle);
  } else {
    env->CallVoidMethod(object, method, size, native_handle);
  }
  return true;
}

void OnWebRtcAudioRecordData(void* context, const void* identity,
                             std::size_t size_bytes) {
  auto* vm = static_cast<VM*>(context);
  if (vm != nullptr) vm->DispatchWebRtcAudioRecordData(identity, size_bytes);
}

void OnWebRtcAudioTrackData(void* context, const void* identity,
                            std::size_t size_bytes) {
  auto* vm = static_cast<VM*>(context);
  if (vm != nullptr) vm->DispatchWebRtcAudioTrackData(identity, size_bytes);
}

}  // namespace

int VM::DispatchWebRtcAudioRecordInit(const void* identity, int sample_rate_hz,
                                      int channels, void** direct_buffer,
                                      std::size_t* direct_buffer_capacity) {
  WebRtcRecordBinding b;
  { std::lock_guard<std::mutex> lock(webrtc_record_mutex_); b = webrtc_record_; }
  if (!b.context || !b.callbacks.init) return -1;
  const int frames = b.callbacks.init(
      b.context.get(), identity, sample_rate_hz, channels,
      &OnWebRtcAudioRecordData, this, direct_buffer, direct_buffer_capacity);
  if (frames < 0 || direct_buffer == nullptr || direct_buffer_capacity == nullptr ||
      *direct_buffer == nullptr || *direct_buffer_capacity == 0) {
    return -1;
  }

  JNIEnv* env = GetJNIEnv();
  auto record = ObjectFromJni<WebRtcAudioRecordObject>(env,
      reinterpret_cast<jobject>(const_cast<void*>(identity)));
  if (record == nullptr || record->native_audio_record == 0 ||
      *direct_buffer_capacity >
          static_cast<std::size_t>(std::numeric_limits<jlong>::max())) {
    if (b.callbacks.close) b.callbacks.close(b.context.get(), identity);
    return -1;
  }
  jobject byte_buffer = env->NewDirectByteBuffer(
      *direct_buffer, static_cast<jlong>(*direct_buffer_capacity));
  if (byte_buffer == nullptr ||
      !CallNativeVoid(env, reinterpret_cast<jobject>(const_cast<void*>(identity)),
                      "nativeCacheDirectBufferAddress",
                      "(Ljava/nio/ByteBuffer;J)V", 0,
                      record->native_audio_record, byte_buffer)) {
    if (b.callbacks.close) b.callbacks.close(b.context.get(), identity);
    return -1;
  }
  return frames;
}

bool VM::DispatchWebRtcAudioRecordStart(const void* identity) {
  WebRtcRecordBinding b;
  { std::lock_guard<std::mutex> lock(webrtc_record_mutex_); b = webrtc_record_; }
  return b.context && b.callbacks.start && b.callbacks.start(b.context.get(), identity);
}

bool VM::DispatchWebRtcAudioRecordStop(const void* identity) {
  WebRtcRecordBinding b;
  { std::lock_guard<std::mutex> lock(webrtc_record_mutex_); b = webrtc_record_; }
  return b.context && b.callbacks.stop && b.callbacks.stop(b.context.get(), identity);
}

void VM::DispatchWebRtcAudioRecordClose(const void* identity) {
  WebRtcRecordBinding b;
  { std::lock_guard<std::mutex> lock(webrtc_record_mutex_); b = webrtc_record_; }
  if (b.context && b.callbacks.close) b.callbacks.close(b.context.get(), identity);
}

void VM::DispatchWebRtcAudioRecordData(const void* identity,
                                       std::size_t size_bytes) {
  if (identity == nullptr ||
      size_bytes > static_cast<std::size_t>(std::numeric_limits<jint>::max())) {
    return;
  }
  JNIEnv* env = GetJNIEnv();
  auto record = ObjectFromJni<WebRtcAudioRecordObject>(
      env, reinterpret_cast<jobject>(const_cast<void*>(identity)));
  if (record == nullptr || record->native_audio_record == 0) return;
  CallNativeVoid(env, reinterpret_cast<jobject>(const_cast<void*>(identity)),
                 "nativeDataIsRecorded", "(IJ)V",
                 static_cast<jint>(size_bytes),
                 record->native_audio_record);
}

void VM::SetWebRtcAudioTrackCallbacks(
    std::shared_ptr<void> context, const WebRtcAudioTrackCallbacks& callbacks) {
  std::lock_guard<std::mutex> lock(webrtc_track_mutex_);
  webrtc_track_ = {std::move(context), callbacks};
}

void VM::ClearWebRtcAudioTrackCallbacks() {
  std::lock_guard<std::mutex> lock(webrtc_track_mutex_);
  webrtc_track_ = {};
}

int VM::DispatchWebRtcAudioTrackInit(const void* identity, int sample_rate_hz,
                                     int channels, double factor,
                                     void** direct_buffer,
                                     std::size_t* direct_buffer_capacity) {
  WebRtcTrackBinding b;
  { std::lock_guard<std::mutex> lock(webrtc_track_mutex_); b = webrtc_track_; }
  if (!b.context || !b.callbacks.init) return -1;
  const int buffer_size = b.callbacks.init(
      b.context.get(), identity, sample_rate_hz, channels, factor,
      &OnWebRtcAudioTrackData, this, direct_buffer, direct_buffer_capacity);
  if (buffer_size < 0 || direct_buffer == nullptr ||
      direct_buffer_capacity == nullptr || *direct_buffer == nullptr ||
      *direct_buffer_capacity == 0) {
    return -1;
  }

  JNIEnv* env = GetJNIEnv();
  auto track = ObjectFromJni<WebRtcAudioTrackObject>(env,
      reinterpret_cast<jobject>(const_cast<void*>(identity)));
  if (track == nullptr || track->native_audio_track == 0 ||
      *direct_buffer_capacity >
          static_cast<std::size_t>(std::numeric_limits<jlong>::max())) {
    if (b.callbacks.close) b.callbacks.close(b.context.get(), identity);
    return -1;
  }
  jobject byte_buffer = env->NewDirectByteBuffer(
      *direct_buffer, static_cast<jlong>(*direct_buffer_capacity));
  if (byte_buffer == nullptr ||
      !CallNativeVoid(env, reinterpret_cast<jobject>(const_cast<void*>(identity)),
                      "nativeCacheDirectBufferAddress",
                      "(Ljava/nio/ByteBuffer;J)V", 0,
                      track->native_audio_track, byte_buffer)) {
    if (b.callbacks.close) b.callbacks.close(b.context.get(), identity);
    return -1;
  }
  return buffer_size;
}

int VM::DispatchWebRtcAudioTrackBufferSizeFrames(const void* identity) {
  WebRtcTrackBinding b;
  { std::lock_guard<std::mutex> lock(webrtc_track_mutex_); b = webrtc_track_; }
  return b.context && b.callbacks.buffer_size_frames
             ? b.callbacks.buffer_size_frames(b.context.get(), identity) : 0;
}

bool VM::DispatchWebRtcAudioTrackStart(const void* identity) {
  WebRtcTrackBinding b;
  { std::lock_guard<std::mutex> lock(webrtc_track_mutex_); b = webrtc_track_; }
  return b.context && b.callbacks.start && b.callbacks.start(b.context.get(), identity);
}

bool VM::DispatchWebRtcAudioTrackStop(const void* identity) {
  WebRtcTrackBinding b;
  { std::lock_guard<std::mutex> lock(webrtc_track_mutex_); b = webrtc_track_; }
  return b.context && b.callbacks.stop && b.callbacks.stop(b.context.get(), identity);
}

void VM::DispatchWebRtcAudioTrackClose(const void* identity) {
  WebRtcTrackBinding b;
  { std::lock_guard<std::mutex> lock(webrtc_track_mutex_); b = webrtc_track_; }
  if (b.context && b.callbacks.close) b.callbacks.close(b.context.get(), identity);
}

void VM::DispatchWebRtcAudioTrackData(const void* identity,
                                      std::size_t size_bytes) {
  if (identity == nullptr ||
      size_bytes > static_cast<std::size_t>(std::numeric_limits<jint>::max())) {
    return;
  }
  JNIEnv* env = GetJNIEnv();
  auto track = ObjectFromJni<WebRtcAudioTrackObject>(
      env, reinterpret_cast<jobject>(const_cast<void*>(identity)));
  if (track == nullptr || track->native_audio_track == 0) return;
  CallNativeVoid(env, reinterpret_cast<jobject>(const_cast<void*>(identity)),
                 "nativeGetPlayoutData", "(IJ)V",
                 static_cast<jint>(size_bytes),
                 track->native_audio_track);
}

void VM::SetAndroidWindowCallbacks(
    std::shared_ptr<void> context, const AndroidWindowCallbacks& callbacks) {
  std::lock_guard<std::mutex> lock(android_window_mutex_);
  android_window_ = {std::move(context), callbacks};
}

void VM::ClearAndroidWindowCallbacks() {
  std::lock_guard<std::mutex> lock(android_window_mutex_);
  android_window_ = {};
}

bool VM::DispatchAndroidWindowFlags(int flags, int mask) {
  AndroidWindowBinding b;
  { std::lock_guard<std::mutex> lock(android_window_mutex_); b = android_window_; }
  return b.context && b.callbacks.set_flags &&
         b.callbacks.set_flags(b.context.get(), flags, mask);
}

void VM::SetRobloxTextInputCallbacks(
    std::shared_ptr<void> context, const RobloxTextInputCallbacks& callbacks) {
  std::lock_guard<std::mutex> lock(text_input_mutex_);
  if (text_input_.context && text_input_.callbacks.shutdown)
    text_input_.callbacks.shutdown(text_input_.context.get());
  text_input_ = {std::move(context), callbacks};
}

void VM::ClearRobloxTextInputCallbacks() {
  TextInputBinding old;
  { std::lock_guard<std::mutex> lock(text_input_mutex_); old=std::move(text_input_); text_input_={}; }
  if (old.context && old.callbacks.shutdown) old.callbacks.shutdown(old.context.get());
}

bool VM::DispatchRobloxTextInputShow(const RobloxTextInputShowRequest& request) {
  TextInputBinding b;
  { std::lock_guard<std::mutex> lock(text_input_mutex_); b=text_input_; }
  if (!b.context || !b.callbacks.show) return false;
  b.callbacks.show(b.context.get(), request); return true;
}

bool VM::DispatchRobloxTextInputHide() {
  TextInputBinding b;
  { std::lock_guard<std::mutex> lock(text_input_mutex_); b=text_input_; }
  if (!b.context || !b.callbacks.hide) return false;
  b.callbacks.hide(b.context.get()); return true;
}

bool VM::DispatchRobloxTextInputReplaceText(const std::string& text) {
  TextInputBinding b;
  { std::lock_guard<std::mutex> lock(text_input_mutex_); b=text_input_; }
  if (!b.context || !b.callbacks.replace_text) return false;
  b.callbacks.replace_text(b.context.get(), text); return true;
}

bool VM::DispatchRobloxTextInputPropertiesChanged() {
  TextInputBinding b;
  { std::lock_guard<std::mutex> lock(text_input_mutex_); b=text_input_; }
  if (!b.context || !b.callbacks.properties_changed) return false;
  b.callbacks.properties_changed(b.context.get()); return true;
}

void VM::InstallHooks() {
  auto env = GetEnv();
  HookRobloxObjects(env.get(), this);

  auto gl = env->GetClass("com/roblox/engine/jni/NativeGLInterface");
  gl->Hook(env.get(), "setBaseUrl",
      [](JNIEnv* e, Class*, jstring) {});
  gl->Hook(env.get(), "onAppBridgeNotification",
      [](JNIEnv* e, Class*, jstring, jstring) {});

  auto settings = env->GetClass("rbx/JNIRobloxSettings");
  settings->HookInstance(env.get(), "nativeInitClientSettings",
      [](JNIEnv*, jobject) {});
}

jobject CreateAndroidConfiguration(JNIEnv* env) {
  if (env == nullptr) return nullptr;
  auto* e = mocktail_libjnivm::ENV::FromJNIEnv(env);
  auto config = std::make_shared<AndroidConfiguration>();
  config->clazz = e->GetClass<AndroidConfiguration>(
      "android/content/res/Configuration");

  VM* vm = VmFromEnv(env);
  const PlatformIdentity identity = Platform(vm);
  const mocktail::runtime::DisplaySize size =
      mocktail::runtime::ParseDisplaySize(
          std::getenv(mocktail::runtime::kWindowSizeEnvironment));

  config->hard_keyboard_hidden = identity.keyboard_enabled ? 1 : 2;
  config->keyboard = identity.keyboard_enabled ? 2 : 1;
  config->keyboard_hidden = identity.keyboard_enabled ? 1 : 2;
  config->orientation = 2;
  config->screen_height_dp = size.height;
  config->screen_width_dp = size.width;
  config->smallest_screen_width_dp = std::min(size.width, size.height);
  config->touchscreen = identity.touch_enabled ? 3 : 1;

  return mocktail_libjnivm::JNITypes<
      std::shared_ptr<Object>>::ToJNIType(e, config);
}


}  // namespace jnivm
