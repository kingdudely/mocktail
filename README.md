# Mocktail

Minimal Linux launcher/runtime for a prepared Roblox Android payload.

Provide `libroblox.so` and the Roblox assets directory yourself. Mocktail does
not download or install the Roblox client.

## Launch

```sh
./build/mocktail \
  --libroblox_file ~/libroblox.so \
  --assets_dir ~/assets \
  --ROBLOSECURITY '<cookie>' \
  --place-id 1818
```

For a specific server:

```sh
./build/mocktail \
  --libroblox_file ~/libroblox.so \
  --assets_dir ~/assets \
  --ROBLOSECURITY '<cookie>' \
  --place-id 1818 \
  --server-id '<server-id>'
```

Headless mode keeps the launcher alive and pumps Roblox main-thread work:

```sh
./build/mocktail \
  --libroblox_file ~/libroblox.so \
  --assets_dir ~/assets \
  --ROBLOSECURITY '<cookie>' \
  --place-id 1818 \
  --headless
```

Supported options:

```text
--assets_dir <path>
--libroblox_file <path>
--ROBLOSECURITY <value>
--place-id <id>
--server-id <id>
--headless
```

A single positional `roblox:` or `roblox-player:` URI is also accepted. That
is the browser-launch path; the installed desktop entry registers both schemes
with the desktop environment.

Browser launch parsing follows the URI shapes used by Mocktail/Cordial. The
desktop `gameinfo` ticket is treated as an opaque one-use credential and is
not forwarded or logged. The `.ROBLOSECURITY` value is kept in process memory
only.

## Build

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j"$(nproc)"
```

The host needs SDL3, SDL3_ttf, Vulkan/EGL/GLES headers, OpenSSL, nlohmann-json,
libelf, utf8proc, fontconfig, and a C++17 toolchain.
