# Mocktail

Minimal C++ Roblox Android compatibility runtime for Linux.

The runtime does not download, locate, or inspect Roblox APKs. Supply the
prepared Roblox native library and assets directory directly.

## Payload

```text
libroblox.so
assets/
```

APK files are not part of the runtime path.

## Launch

The normal authentication path is the Roblox browser deep link:

```text
roblox-player:...
```

Mocktail reads the launch ticket from `gameinfo`, redeems it over HTTPS,
and supplies the resulting Roblox session credential to the pseudo-JVM.
The persistent `.ROBLOSECURITY` cookie is not required.

A cookie can still be supplied explicitly as a fallback:

```sh
./build/mocktail \
  --libroblox_file ~/libroblox.so \
  --assets_dir ~/assets \
  --ROBLOSECURITY '<your-cookie>'
```

Supported launch overrides:

```text
--launch-uri <roblox-player-or-roblox-uri>
--place-id <id>
--server-id <id>
--headless
```

A URI may also be passed as the single positional argument.

The installed desktop entry registers both `roblox:` and `roblox-player:`
with the operating system.

## Build

Mocktail vendors only project-specific compatibility components. JNI and
Vulkan headers are taken from the host system.

The host must provide compatible system packages for SDL3 (3.2 or newer),
SDL3_ttf, Vulkan headers, EGL/GLES headers, OpenSSL, nlohmann-json, libelf,
utf8proc, fontconfig, and a JDK/JNI development environment.

Then:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j"$(nproc)"
```

This repository intentionally contains no Roblox downloader, APK updater,
embedded WebView, or browser UI runtime.
