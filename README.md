# Mocktail

Minimal C++ Roblox Android compatibility runtime for Linux.

The runtime does not download, locate, inspect, or require Roblox APKs. Supply the
prepared Roblox native library and assets directory directly.

## Payload

```text
libroblox.so
assets/
```

APK files are not part of the runtime path.

## Launch

The preferred launch path is the Roblox website. When the browser invokes the
registered `roblox-player:` or `roblox:` scheme, Mocktail parses the launch
request and carries the place/server selection into Roblox.

The desktop `roblox-player:` `gameinfo` value is treated as an opaque
one-use launch credential and is not copied, logged, or speculatively redeemed
by Mocktail. The current Cordial Android-runtime investigation has not
verified that the Android client accepts that ticket for authentication.

Therefore `.ROBLOSECURITY` remains available as the explicit authentication
fallback:

```sh
./build/mocktail \
  --libroblox_file ~/libroblox.so \
  --assets_dir ~/assets \
  --ROBLOSECURITY '<your-cookie>'
```

The cookie is kept in memory only for the process lifetime and is not persisted
by Mocktail.

Supported launch options:

```text
--launch-uri <roblox-player-or-roblox-uri>
--place-id <id>
--server-id <id>
--headless
--ROBLOSECURITY <value>
```

A Roblox URI may also be supplied as the single positional argument.

The desktop entry registers both `roblox:` and `roblox-player:` with the
operating system, so normal Roblox website clicks can invoke Mocktail without
copying a launch URL manually.

## Build

Mocktail vendors only project-specific compatibility components. JNI and
Vulkan headers are taken from the host system.

The host must provide compatible system packages for SDL3 (3.2 or newer),
SDL3_ttf, Vulkan headers, EGL/GLES headers, OpenSSL, nlohmann-json, libelf,
utf8proc, fontconfig, and JDK JNI headers.

On Debian/Ubuntu, the JNI header package is normally supplied by:

```sh
sudo apt install default-jdk-headless
```

Then:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j"$(nproc)"
```

This repository intentionally contains no Roblox downloader, APK updater,
embedded WebView, or embedded browser UI runtime.
