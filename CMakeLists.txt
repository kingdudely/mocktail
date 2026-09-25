cmake_minimum_required(VERSION 3.20)

project(Mocktail
  VERSION 1.0.4
  DESCRIPTION "Minimal C++ Android Roblox compatibility runtime for Linux"
  LANGUAGES CXX
)

include(GNUInstallDirs)
find_package(Threads REQUIRED)

# Mocktail implements its own pseudo-JVM. JNI is an API header surface here;
# it does not need a host libjvm or AWT. Keep the headers vendored so building
# Mocktail does not require a JDK installation.
set(MOCKTAIL_JNI_INCLUDE_DIR
  ${CMAKE_SOURCE_DIR}/third_party/jni/include)
set(MOCKTAIL_JNI_PLATFORM_INCLUDE_DIR
  ${CMAKE_SOURCE_DIR}/third_party/jni/include)

if(NOT EXISTS "${MOCKTAIL_JNI_INCLUDE_DIR}/jni.h" OR
   NOT EXISTS "${MOCKTAIL_JNI_PLATFORM_INCLUDE_DIR}/jni_md.h")
  message(FATAL_ERROR
    "Vendored JNI headers are missing under third_party/jni/include")
endif()

set(MOCKTAIL_JNI_INCLUDE_DIRS
  ${MOCKTAIL_JNI_INCLUDE_DIR}
  ${MOCKTAIL_JNI_PLATFORM_INCLUDE_DIR})

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
    target_include_directories(jnivm PRIVATE ${MOCKTAIL_JNI_INCLUDE_DIRS})
    target_compile_options(jnivm PRIVATE -include string)
  endif()
  target_link_libraries(jnivm PUBLIC Threads::Threads)
endif()

add_subdirectory(third_party/mcpelauncher-linker)

find_path(MOCKTAIL_VULKAN_INCLUDE_DIR vulkan/vulkan.h REQUIRED)
add_library(mocktail_vulkan_headers INTERFACE)
target_include_directories(mocktail_vulkan_headers SYSTEM INTERFACE
  ${MOCKTAIL_VULKAN_INCLUDE_DIR})
add_library(Mocktail::VulkanHeaders ALIAS mocktail_vulkan_headers)
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  target_compile_options(linker PRIVATE -w)
endif()

find_package(OpenSSL REQUIRED COMPONENTS Crypto)
find_package(CURL REQUIRED)
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
  include ${MOCKTAIL_JNI_INCLUDE_DIRS})
target_link_libraries(mocktail_legacy_jnivm PUBLIC Threads::Threads)
mocktail_apply_compile_options(mocktail_legacy_jnivm)
add_library(Mocktail::LegacyJni ALIAS mocktail_legacy_jnivm)

add_library(mocktail_runtime STATIC
  src/runtime/command_line.cc
  src/runtime/device_memory_profile.cc
  src/runtime/device_profile.cc
  src/runtime/environment.cc
  src/runtime/game_session_coordinator.cc
  src/runtime/owned_pthread.cc
  src/runtime/roblox_app_lifecycle.cc
  src/runtime/roblox_game_session_native_adapter.cc
  src/runtime/roblox_capability_resolver.cc
  src/runtime/roblox_launch_uri.cc
  src/runtime/runtime_config.cc
  src/runtime/runtime_paths.cc
  src/runtime/startup_state_machine.cc
)
target_include_directories(mocktail_runtime PUBLIC include ${MOCKTAIL_JNI_INCLUDE_DIRS})
target_include_directories(mocktail_runtime PRIVATE
  src)
target_link_libraries(mocktail_runtime PUBLIC Threads::Threads)
target_link_libraries(mocktail_runtime PRIVATE
  OpenSSL::Crypto nlohmann_json::nlohmann_json
  ${CMAKE_DL_LIBS})
mocktail_apply_compile_options(mocktail_runtime)
add_library(Mocktail::Runtime ALIAS mocktail_runtime)

add_library(mocktail_browser_auth STATIC
  src/runtime/browser_auth.cc)
target_include_directories(mocktail_browser_auth PUBLIC include)
target_link_libraries(mocktail_browser_auth PUBLIC CURL::libcurl)
mocktail_apply_compile_options(mocktail_browser_auth)
add_library(Mocktail::BrowserAuth ALIAS mocktail_browser_auth)

add_library(mocktail_auth_runtime_composition STATIC
  src/runtime/auth_runtime_composition.cc)
target_include_directories(mocktail_auth_runtime_composition PUBLIC include)
target_link_libraries(mocktail_auth_runtime_composition PUBLIC
  Mocktail::LegacyJni Mocktail::Runtime)
target_link_libraries(mocktail_auth_runtime_composition PRIVATE
  Threads::Threads)
mocktail_apply_compile_options(mocktail_auth_runtime_composition)
add_library(Mocktail::AuthRuntimeComposition ALIAS
  mocktail_auth_runtime_composition)

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
  Mocktail::InputRuntime
  mocktail_linker mocktail_bionic_abi_exports libc_shim
  mocktail_window mocktail_graphics_foundation
  ${MOCKTAIL_EXECINFO_LINK_LIBRARY})
mocktail_apply_compile_options(mocktail_legacy_runtime)

add_executable(roblox src/main.cc)
target_include_directories(roblox PRIVATE include src)
target_link_libraries(roblox PRIVATE
  mocktail_legacy_runtime Mocktail::AuthRuntimeComposition
  Mocktail::BrowserAuth
  mocktail_fmod_jni_audio_bridge
  mocktail_roblox_output_device_runtime
  mocktail_roblox_fullscreen_runtime
  mocktail_window libc_shim)
mocktail_apply_compile_options(roblox)
target_link_options(roblox PRIVATE -Wl,--export-dynamic)
set_target_properties(roblox PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}"
  BUILD_RPATH "\$ORIGIN"
  INSTALL_RPATH "\$ORIGIN;$ORIGIN/${CMAKE_INSTALL_LIBDIR}/mocktail")

install(TARGETS roblox
  RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")

install(FILES "${CMAKE_SOURCE_DIR}/mocktail.desktop"
  DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/applications")
