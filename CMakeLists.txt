cmake_minimum_required(VERSION 3.20)

project(Mocktail
  VERSION 1.0.4
  DESCRIPTION "Minimal C++ Android Roblox compatibility runtime for Linux"
  LANGUAGES CXX
)

include(GNUInstallDirs)
find_package(Threads REQUIRED)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(mocktail_compile_options INTERFACE)
target_compile_options(mocktail_compile_options INTERFACE
  "$<$<COMPILE_LANGUAGE:CXX>:-Wall;-Wextra;-Wpedantic;-Wno-unused-parameter;-fno-exceptions;-g>"
)

function(mocktail_apply_compile_options target_name)
  target_link_libraries(${target_name} PRIVATE mocktail_compile_options)
endfunction()

option(MOCKTAIL_ENABLE_UPSTREAM_JNIVM "Build upstream libjnivm" ON)
if(MOCKTAIL_ENABLE_UPSTREAM_JNIVM)
  set(JNIVM_ENABLE_TRACE OFF CACHE BOOL "" FORCE)
  set(JNIVM_ENABLE_GC ON CACHE BOOL "" FORCE)
  set(JNIVM_ENABLE_DEBUG OFF CACHE BOOL "" FORCE)
  set(JNIVM_ENABLE_RETURN_NON_ZERO OFF CACHE BOOL "" FORCE)
  set(JNIVM_FAKE_JNI_MINECRAFT_LINUX_COMPAT OFF CACHE BOOL "" FORCE)
  set(JNIVM_ENABLE_TESTS OFF CACHE BOOL "" FORCE)
  set(JNIVM_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)

  add_subdirectory(third_party/libjnivm EXCLUDE_FROM_ALL)
  if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(jnivm PRIVATE -include string)
  endif()
  target_link_libraries(jnivm PUBLIC Threads::Threads)
endif()

add_subdirectory(third_party/mcpelauncher-linker)
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  target_compile_options(linker PRIVATE -w)
endif()

set(VULKAN_HEADERS_ENABLE_TESTS OFF CACHE BOOL "" FORCE)
set(VULKAN_HEADERS_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
add_subdirectory(third_party/Vulkan-Headers EXCLUDE_FROM_ALL)

find_package(OpenSSL REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(PkgConfig REQUIRED)

include(${CMAKE_SOURCE_DIR}/cmake/MocktailPlatformGraphics.cmake)
include(${CMAKE_SOURCE_DIR}/cmake/MocktailAudio.cmake)
include(${CMAKE_SOURCE_DIR}/cmake/MocktailElfCompatibility.cmake)

add_library(mocktail_bionic_stdio_runtime STATIC
  src/compat/bionic_stdio_runtime.cc)
target_include_directories(mocktail_bionic_stdio_runtime PUBLIC include)
mocktail_apply_compile_options(mocktail_bionic_stdio_runtime)
add_library(Mocktail::BionicStdio ALIAS mocktail_bionic_stdio_runtime)

add_library(mocktail_legacy_jnivm STATIC src/jnivm/jnivm.cc)
target_include_directories(mocktail_legacy_jnivm PUBLIC
  include third_party/jni/include)
target_link_libraries(mocktail_legacy_jnivm PUBLIC Threads::Threads)
mocktail_apply_compile_options(mocktail_legacy_jnivm)
add_library(Mocktail::LegacyJni ALIAS mocktail_legacy_jnivm)

add_library(mocktail_runtime STATIC
  src/runtime/command_line.cc
  src/runtime/device_memory_profile.cc
  src/runtime/device_profile.cc
  src/runtime/environment.cc
  src/runtime/external_launch_broker.cc
  src/runtime/game_session_coordinator.cc
  src/runtime/owned_pthread.cc
  src/runtime/roblox_app_lifecycle.cc
  src/runtime/roblox_browser_service_bridge.cc
  src/runtime/roblox_capability_resolver.cc
  src/runtime/roblox_experience_launch_bridge.cc
  src/runtime/roblox_game_session_native_adapter.cc
  src/runtime/roblox_launch_uri.cc
  src/runtime/roblox_permissions_bridge.cc
  src/runtime/roblox_web_view_bridge.cc
  src/runtime/runtime_config.cc
  src/runtime/runtime_paths.cc
  src/runtime/startup_state_machine.cc
)
target_include_directories(mocktail_runtime PUBLIC include)
target_include_directories(mocktail_runtime PRIVATE
  src third_party/jni/include)
target_link_libraries(mocktail_runtime PUBLIC Threads::Threads)
target_link_libraries(mocktail_runtime PRIVATE
  OpenSSL::Crypto nlohmann_json::nlohmann_json
  ${CMAKE_DL_LIBS})
mocktail_apply_compile_options(mocktail_runtime)
add_library(Mocktail::Runtime ALIAS mocktail_runtime)

add_library(mocktail_services STATIC
  src/services/client_settings_service.cc)
target_include_directories(mocktail_services PUBLIC include)
target_link_libraries(mocktail_services PUBLIC
  Mocktail::Runtime nlohmann_json::nlohmann_json)
mocktail_apply_compile_options(mocktail_services)
add_library(Mocktail::Services ALIAS mocktail_services)

add_library(mocktail_auth_runtime_composition STATIC
  src/runtime/auth_runtime_composition.cc)
target_include_directories(mocktail_auth_runtime_composition PUBLIC include)
target_link_libraries(mocktail_auth_runtime_composition PUBLIC
  Mocktail::LegacyJni Mocktail::Runtime)
mocktail_apply_compile_options(mocktail_auth_runtime_composition)
add_library(Mocktail::AuthRuntimeComposition ALIAS
  mocktail_auth_runtime_composition)

add_library(mocktail_webview_launcher STATIC
  src/runtime/webview_helper_launcher.cc)
target_include_directories(mocktail_webview_launcher PUBLIC include)
target_link_libraries(mocktail_webview_launcher PUBLIC
  Mocktail::Runtime)
mocktail_apply_compile_options(mocktail_webview_launcher)
add_library(Mocktail::WebViewLauncher ALIAS mocktail_webview_launcher)

add_library(mocktail_linker STATIC src/linker/linker.cc)
target_include_directories(mocktail_linker PUBLIC include)
target_link_libraries(mocktail_linker PUBLIC
  dl linker mocktail_compat mocktail_bionic_stdio_runtime
  mocktail_bionic_abi_exports)
target_compile_definitions(mocktail_linker PUBLIC MOCKTAIL_USE_BIONIC_LINKER=1)
target_link_options(mocktail_linker INTERFACE
  -Wl,--defsym=__rela_iplt_start=0
  -Wl,--defsym=__rela_iplt_end=0)
mocktail_apply_compile_options(mocktail_linker)

add_library(libc_shim STATIC src/libc_shim/libc_shim.cc)
target_include_directories(libc_shim PUBLIC include)
target_link_libraries(mocktail_linker PUBLIC libc_shim)
mocktail_apply_compile_options(libc_shim)

add_subdirectory(stubs)
foreach(stub_target IN ITEMS
    stub_sys_m stub_sys_z stub_sys_dl stub_sys_c
    stub_android stub_log stub_mediandk stub_openmax
    stub_egl stub_glesv2 stub_vulkan)
  if(TARGET ${stub_target})
    mocktail_apply_compile_options(${stub_target})
  endif()
endforeach()

add_library(mocktail_window STATIC
  src/window/roblox_fullscreen_menu_request_gate.cc
  src/window/video_driver_policy.cc
  src/window/window.cc
  src/window/window_creation_policy.cc
  src/window/window_fullscreen_request_gate.cc
  src/window/window_game_surface_bridge.cc
  src/window/window_pointer_capture_owner.cc
  src/window/window_resize_readiness_gate.cc
  src/window/window_state_store.cc
  src/window/window_surface_lifecycle.cc
  src/window/window_text_input_owner.cc)
target_include_directories(mocktail_window PUBLIC include)
target_include_directories(mocktail_window PRIVATE src)
target_link_libraries(mocktail_window PUBLIC
  Mocktail::PlatformSdl Mocktail::SdlVulkanWsi SDL3::SDL3
  PRIVATE Mocktail::Runtime Mocktail::GlesTextOverlay)
mocktail_apply_compile_options(mocktail_window)

add_library(mocktail_roblox_output_device_runtime STATIC
  src/audio/roblox_output_device_bridge.cc)
target_include_directories(mocktail_roblox_output_device_runtime PUBLIC include)
target_include_directories(mocktail_roblox_output_device_runtime PRIVATE src)
target_link_libraries(mocktail_roblox_output_device_runtime PUBLIC
  mocktail_compat Mocktail::AudioSdl PRIVATE mocktail_linker)
mocktail_apply_compile_options(mocktail_roblox_output_device_runtime)

add_library(mocktail_roblox_fullscreen_runtime STATIC
  src/runtime/roblox_fullscreen_runtime_bridge.cc)
target_include_directories(mocktail_roblox_fullscreen_runtime PUBLIC include)
target_include_directories(mocktail_roblox_fullscreen_runtime PRIVATE src)
target_link_libraries(mocktail_roblox_fullscreen_runtime PUBLIC
  mocktail_compat
  PRIVATE mocktail_linker mocktail_window stub_log)
mocktail_apply_compile_options(mocktail_roblox_fullscreen_runtime)

include(${CMAKE_SOURCE_DIR}/cmake/MocktailInput.cmake)

add_library(mocktail_experience_runtime STATIC
  src/runtime/jnivm_platform_web_callbacks.cc
  src/runtime/roblox_experience_composition.cc
  src/runtime/roblox_fresh_game_launch_controller.cc
  src/runtime/roblox_platform_web_symbols.cc)
target_include_directories(mocktail_experience_runtime PUBLIC include)
target_include_directories(mocktail_experience_runtime PRIVATE src)
target_link_libraries(mocktail_experience_runtime PUBLIC
  Mocktail::Runtime Mocktail::WebViewLauncher PkgConfig::UTF8PROC
  PRIVATE Mocktail::AuthRuntimeComposition Mocktail::LegacyJni
          mocktail_linker mocktail_window)
mocktail_apply_compile_options(mocktail_experience_runtime)
add_library(Mocktail::ExperienceRuntime ALIAS mocktail_experience_runtime)

add_library(mocktail_bionic_abi_exports STATIC
  src/compat/bionic_abi_exports.cc
  src/compat/http_client_spin_guard.cc)
target_include_directories(mocktail_bionic_abi_exports PUBLIC include
  PRIVATE src)
target_link_libraries(mocktail_bionic_abi_exports PRIVATE
  mocktail_bionic_stdio_runtime mocktail_window Threads::Threads)
mocktail_apply_compile_options(mocktail_bionic_abi_exports)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  set_property(SOURCE src/compat/bionic_abi_exports.cc APPEND PROPERTY
    COMPILE_OPTIONS -fno-lto -fno-builtin)
endif()
add_library(Mocktail::BionicAbiExports ALIAS mocktail_bionic_abi_exports)

include(CheckCXXSourceCompiles)
include(CheckIncludeFileCXX)
include(CMakePushCheckState)

check_include_file_cxx(execinfo.h MOCKTAIL_HAVE_EXECINFO_H)
if(NOT MOCKTAIL_HAVE_EXECINFO_H)
  message(FATAL_ERROR "execinfo.h is required for runtime diagnostics")
endif()

set(MOCKTAIL_EXECINFO_LINK_LIBRARY "")
set(_mocktail_execinfo_link_probe [=[
  #include <execinfo.h>
  int main() {
    void* frames[1] = {};
    const int count = backtrace(frames, 1);
    return backtrace_symbols(frames, count) == nullptr;
  }
]=])
check_cxx_source_compiles("${_mocktail_execinfo_link_probe}"
  MOCKTAIL_EXECINFO_IN_HOST_LIBC)
if(NOT MOCKTAIL_EXECINFO_IN_HOST_LIBC)
  find_library(MOCKTAIL_EXECINFO_LIBRARY NAMES execinfo REQUIRED)
  cmake_push_check_state(RESET)
  set(CMAKE_REQUIRED_LIBRARIES "${MOCKTAIL_EXECINFO_LIBRARY}")
  check_cxx_source_compiles("${_mocktail_execinfo_link_probe}"
    MOCKTAIL_EXECINFO_IN_EXTERNAL_LIBRARY)
  cmake_pop_check_state()
  if(NOT MOCKTAIL_EXECINFO_IN_EXTERNAL_LIBRARY)
    message(FATAL_ERROR "execinfo backtrace support is unavailable")
  endif()
  set(MOCKTAIL_EXECINFO_LINK_LIBRARY "${MOCKTAIL_EXECINFO_LIBRARY}")
endif()

add_library(mocktail_legacy_runtime STATIC
  src/legacy/legacy_runtime.cc
  src/runtime/roblox_text_input_jni_bridge.cc)
target_include_directories(mocktail_legacy_runtime PRIVATE include src)
target_link_libraries(mocktail_legacy_runtime PRIVATE
  Mocktail::LegacyJni mocktail_compat Mocktail::Runtime
  Mocktail::InputRuntime Mocktail::ExperienceRuntime Mocktail::Services
  mocktail_linker mocktail_bionic_abi_exports libc_shim
  mocktail_window mocktail_graphics_foundation
  ${MOCKTAIL_EXECINFO_LINK_LIBRARY})
mocktail_apply_compile_options(mocktail_legacy_runtime)

add_executable(mocktail src/main.cc)
target_include_directories(mocktail PRIVATE include src)
target_link_libraries(mocktail PRIVATE
  mocktail_legacy_runtime Mocktail::AuthRuntimeComposition
  mocktail_fmod_jni_audio_bridge
  mocktail_roblox_output_device_runtime
  mocktail_roblox_fullscreen_runtime
  mocktail_window libc_shim)
mocktail_apply_compile_options(mocktail)
target_link_options(mocktail PRIVATE -Wl,--export-dynamic)
set_target_properties(mocktail PROPERTIES
  BUILD_RPATH "\$ORIGIN"
  INSTALL_RPATH "\$ORIGIN;$ORIGIN/${CMAKE_INSTALL_LIBDIR}/mocktail")

install(TARGETS mocktail
  RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
