#ifndef MOCKTAIL_JNIVM_JNIVM_H_
#define MOCKTAIL_JNIVM_JNIVM_H_

// The upstream library is compiled with its namespace renamed so Mocktail can
// retain its small jnivm::VM compatibility facade without owning JNI itself.
#define jnivm mocktail_libjnivm
#include <jnivm.h>
#undef jnivm

#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>

namespace jnivm {

using Object = mocktail_libjnivm::Object;
using Class = mocktail_libjnivm::Class;
using ENV = mocktail_libjnivm::ENV;

struct RobloxAuthIdentity {
  std::int64_t user_id = -1;
  std::string username;
  std::string display_name;
};

struct PlatformIdentity {
  bool touch_enabled = false;
  bool mouse_enabled = true;
  bool keyboard_enabled = true;
  bool pc_hardware = true;
  std::string platform_name = "Linux";
  std::string device_name = "Mocktail Headless";
  std::string manufacturer = "Mocktail";
  std::string model = "Mocktail Headless";
  std::string brand = "Mocktail";
#if defined(__aarch64__)
  std::string device_code = "linux-arm64";
  std::string device_sku = "mocktail-arm64";
  std::string soc_model = "aarch64";
#else
  std::string device_code = "linux-x86_64";
  std::string device_sku = "mocktail-x86_64";
  std::string soc_model = "x86_64";
#endif
};

struct RobloxCredentialView {
  const char* data = nullptr;
  std::size_t size = 0;
};

using RobloxCredentialProvider = RobloxCredentialView (*)(const void*);
using RobloxCookieGetter = jstring (*)(JNIEnv*, jclass, jstring);

struct RobloxCredentialSinkCallbacks {
  bool (*store)(void*, const char*, std::size_t) = nullptr;
};

struct FmodAudioDeviceCallbacks {
  bool (*init)(void*, const void*, int, int, int, int) = nullptr;
  bool (*write)(void*, const void*, const std::uint8_t*, std::size_t) = nullptr;
  bool (*close)(void*, const void*) = nullptr;
  void (*shutdown)(void*) = nullptr;
};

struct WebRtcAudioManagerParameters {
  int sample_rate_hz = 0;
  int output_channels = 0;
  int input_channels = 0;
  bool hardware_aec = false;
  bool hardware_agc = false;
  bool hardware_ns = false;
  bool low_latency_output = false;
  bool low_latency_input = false;
  bool pro_audio = false;
  bool aaudio = false;
  int output_buffer_size_frames = 0;
  int input_buffer_size_frames = 0;
};

struct WebRtcAudioManagerCallbacks {
  bool (*get_parameters)(void*, WebRtcAudioManagerParameters*) = nullptr;
  bool (*init)(void*, const void*) = nullptr;
  void (*dispose)(void*, const void*) = nullptr;
  void (*set_microphone_mute)(void*, bool) = nullptr;
};

using WebRtcAudioRecordDataCallback =
    void (*)(void*, const void*, std::size_t);

struct WebRtcAudioRecordCallbacks {
  int (*init)(void*, const void*, int, int, WebRtcAudioRecordDataCallback,
              void*, void**, std::size_t*) = nullptr;
  bool (*start)(void*, const void*) = nullptr;
  bool (*stop)(void*, const void*) = nullptr;
  void (*close)(void*, const void*) = nullptr;
  void (*shutdown)(void*) = nullptr;
};

using WebRtcAudioTrackDataCallback =
    void (*)(void*, const void*, std::size_t);

struct WebRtcAudioTrackCallbacks {
  int (*init)(void*, const void*, int, int, double,
              WebRtcAudioTrackDataCallback, void*, void**, std::size_t*) = nullptr;
  int (*buffer_size_frames)(void*, const void*) = nullptr;
  bool (*start)(void*, const void*) = nullptr;
  bool (*stop)(void*, const void*) = nullptr;
  void (*close)(void*, const void*) = nullptr;
};

struct AndroidWindowCallbacks {
  bool (*set_flags)(void*, int, int) = nullptr;
};

struct RobloxTextBoxInfo {
  float x = 0.0f;
  float y = 0.0f;
  float width = 0.0f;
  float height = 0.0f;
  float font_size = 0.0f;
  bool multiline = false;
  int x_alignment = 0;
  int y_alignment = 0;
  int text_color = 0;
  int font = 0;
  int text_input_type = 0;
  int return_key_type = 0;
  bool manual_focus_release = false;
  bool text_wrapped = false;
};

struct RobloxTextInputShowRequest {
  std::int64_t text_box = 0;
  bool show_native_input = false;
  std::string text;
  RobloxTextBoxInfo info;
};

struct RobloxTextInputCallbacks {
  void (*show)(void*, const RobloxTextInputShowRequest&) = nullptr;
  void (*hide)(void*) = nullptr;
  void (*replace_text)(void*, const std::string&) = nullptr;
  void (*properties_changed)(void*) = nullptr;
  void (*shutdown)(void*) = nullptr;
};

class VM final : public mocktail_libjnivm::VM {
 public:
  VM();
  ~VM();

  VM(const VM&) = delete;
  VM& operator=(const VM&) = delete;

  static VM* FromJavaVM(JavaVM* vm);

  std::shared_ptr<Class> RegisterClass(const char* name);
  void RegisterMethod(const std::shared_ptr<Class>& clazz, const char* name,
                      const char* signature,
                      void (*callback)(JNIEnv*, jobject));
  std::size_t GetClassCount() const;
  void RestoreFunctions();

  void SetRobloxAuthIdentity(const RobloxAuthIdentity&);
  void ClearRobloxAuthIdentity();
  RobloxAuthIdentity GetRobloxAuthIdentitySnapshot() const;

  void SetPlatformIdentity(const PlatformIdentity&);
  PlatformIdentity GetPlatformIdentitySnapshot() const;

  void SetRobloxCredentialProvider(const void*, RobloxCredentialProvider);
  void ClearRobloxCredentialProvider();
  bool CopyRobloxCredentialFromProvider(std::string* credential) const;

  void SetRobloxCookieGetter(RobloxCookieGetter);
  bool RefreshRobloxCredentialFromEngine(JNIEnv* env);

  void SetRobloxCredentialSink(std::shared_ptr<void>,
                               const RobloxCredentialSinkCallbacks&);
  void ClearRobloxCredentialSink();
  bool DispatchRobloxCredential(const char*, std::size_t);

  void SetFmodAudioDeviceCallbacks(std::shared_ptr<void>,
                                   const FmodAudioDeviceCallbacks&);
  void ClearFmodAudioDeviceCallbacks();
  bool DispatchFmodAudioDeviceInit(const void*, int, int, int, int);
  bool DispatchFmodAudioDeviceWrite(const void*, const std::uint8_t*, std::size_t);
  bool DispatchFmodAudioDeviceClose(const void*);

  void SetWebRtcAudioManagerCallbacks(std::shared_ptr<void>,
                                      const WebRtcAudioManagerCallbacks&);
  void ClearWebRtcAudioManagerCallbacks();
  bool DispatchWebRtcAudioManagerInit(jobject);
  void DispatchWebRtcAudioManagerDispose(jobject);
  void DispatchWebRtcAudioManagerMicrophoneMute(jobject, bool);

  void SetWebRtcAudioRecordCallbacks(std::shared_ptr<void>,
                                     const WebRtcAudioRecordCallbacks&);
  void ClearWebRtcAudioRecordCallbacks();
  int DispatchWebRtcAudioRecordInit(const void*, int, int, void**, std::size_t*);
  bool DispatchWebRtcAudioRecordStart(const void*);
  bool DispatchWebRtcAudioRecordStop(const void*);
  void DispatchWebRtcAudioRecordClose(const void*);

  void SetWebRtcAudioTrackCallbacks(std::shared_ptr<void>,
                                    const WebRtcAudioTrackCallbacks&);
  void ClearWebRtcAudioTrackCallbacks();
  int DispatchWebRtcAudioTrackInit(const void*, int, int, double, void**,
                                   std::size_t*);
  int DispatchWebRtcAudioTrackBufferSizeFrames(const void*);
  bool DispatchWebRtcAudioTrackStart(const void*);
  bool DispatchWebRtcAudioTrackStop(const void*);
  void DispatchWebRtcAudioTrackClose(const void*);
  void DispatchWebRtcAudioTrackData(const void*, std::size_t);

  void SetAndroidWindowCallbacks(std::shared_ptr<void>,
                                 const AndroidWindowCallbacks&);
  void ClearAndroidWindowCallbacks();
  bool DispatchAndroidWindowFlags(int flags, int mask);

  void SetRobloxTextInputCallbacks(std::shared_ptr<void>,
                                   const RobloxTextInputCallbacks&);
  void ClearRobloxTextInputCallbacks();
  bool DispatchRobloxTextInputShow(const RobloxTextInputShowRequest&);
  bool DispatchRobloxTextInputHide();
  bool DispatchRobloxTextInputReplaceText(const std::string&);
  bool DispatchRobloxTextInputPropertiesChanged();

 private:
  struct CredentialSinkBinding {
    std::shared_ptr<void> context;
    RobloxCredentialSinkCallbacks callbacks;
  };
  struct FmodBinding {
    std::shared_ptr<void> context;
    FmodAudioDeviceCallbacks callbacks;
  };
  struct WebRtcManagerBinding {
    std::shared_ptr<void> context;
    WebRtcAudioManagerCallbacks callbacks;
  };
  struct WebRtcRecordBinding {
    std::shared_ptr<void> context;
    WebRtcAudioRecordCallbacks callbacks;
  };
  struct WebRtcTrackBinding {
    std::shared_ptr<void> context;
    WebRtcAudioTrackCallbacks callbacks;
  };
  struct AndroidWindowBinding {
    std::shared_ptr<void> context;
    AndroidWindowCallbacks callbacks;
  };
  struct TextInputBinding {
    std::shared_ptr<void> context;
    RobloxTextInputCallbacks callbacks;
  };

  mutable std::mutex auth_mutex_;
  RobloxAuthIdentity auth_identity_;

  mutable std::mutex platform_mutex_;
  PlatformIdentity platform_identity_;

  mutable std::mutex credential_mutex_;
  const void* credential_context_ = nullptr;
  RobloxCredentialProvider credential_provider_ = nullptr;
  std::string credential_override_;

  mutable std::mutex cookie_mutex_;
  RobloxCookieGetter cookie_getter_ = nullptr;

  mutable std::mutex credential_sink_mutex_;
  CredentialSinkBinding credential_sink_;

  mutable std::mutex fmod_mutex_;
  FmodBinding fmod_;

  mutable std::mutex webrtc_manager_mutex_;
  WebRtcManagerBinding webrtc_manager_;

  mutable std::mutex webrtc_record_mutex_;
  WebRtcRecordBinding webrtc_record_;

  mutable std::mutex webrtc_track_mutex_;
  WebRtcTrackBinding webrtc_track_;

  mutable std::mutex android_window_mutex_;
  AndroidWindowBinding android_window_;

  mutable std::mutex text_input_mutex_;
  TextInputBinding text_input_;

  void InstallHooks();
};

jobject CreateAndroidConfiguration(JNIEnv* env);

}  // namespace jnivm

#endif
