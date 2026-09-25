# Mocktail

Minimal Linux Roblox compatibility runtime with a browser-first launcher.

The executable is named `roblox`. Mocktail does not download Roblox. By default it looks for the prepared Roblox native library at
`~/libroblox.so` and assets at `~/assets`.

## JNI architecture

Mocktail uses the upstream `libjnivm` project for the JNI ABI, references,
strings, method IDs, native registration, and object lifetime. Mocktail only
defines the native C++ Android/Roblox classes and host callbacks that Roblox
actually reaches.

This keeps the compatibility layer small and avoids maintaining a second
pseudo-JVM. libjnivm also implements JNI's Modified UTF-8 rules, so Mocktail
does not duplicate UTF-16 conversion code.

The repository pins libjnivm as a git submodule under `third_party/libjnivm`.
## Launching from Roblox

The normal path is simply pressing **Play** on roblox.com.

The desktop website launches a `roblox-player:` URI containing a one-use
`gameinfo` authentication ticket and a PlaceLauncher URL. Mocktail handles
that URI, redeems the ticket at Roblox's authentication-ticket endpoint, keeps
the resulting `.ROBLOSECURITY` session only in memory, and then starts the
Roblox runtime for the requested place/server.

The one-use browser ticket is never written to disk or passed into the Roblox
runtime after redemption.

Both `roblox:` and `roblox-player:` are registered by the installed desktop
entry, so the browser can invoke `roblox %u` directly.

## Running it

From a checkout:

```sh
git submodule update --init --recursive
./roblox
```

A browser URI can also be passed directly:

```sh
./roblox 'roblox-player:...'
```

For debugging without a window:

```sh
./roblox --headless --libroblox_so=~/libroblox.so --assets_dir=~/assets
```

There are intentionally no place/server/cookie command-line options in the
launcher. Browser launch data is the source of truth. A small shell script can
construct a `roblox-player:` URI later when you need manual testing.

The runtime uses `ROBLOX_LIB_PATH` and `MOCKTAIL_ASSET_PATH` internally. When
those are not already set, they default to `~/libroblox.so` and `~/assets/`.
Override the payload location with `--libroblox_so=<path>` and
`--assets_dir=<path>`. A leading `~/` in these options is expanded to the
user's home directory.

## Build

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j"$(nproc)"
```

The resulting executable is:

```text
./roblox
```

The host needs SDL3, SDL3_ttf, Vulkan/EGL/GLES headers, OpenSSL, libcurl,
nlohmann-json, libelf, utf8proc, fontconfig, and a C++17 toolchain. JNI headers
come from the pinned libjnivm submodule; a host JVM is not used.
