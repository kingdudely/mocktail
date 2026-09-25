# Mocktail

Minimal C++ Roblox Android compatibility runtime for Linux.

The runtime does not download or locate Roblox packages. Supply the extracted
Roblox native library and assets directory directly.

## Runtime arguments

```text
--ROBLOSECURITY <value>
--assets_dir <path>        default: ./assets
--libroblox_file <path>    default: ./libroblox.so
```

The payload consists of:

```text
libroblox.so
assets/
```

APK files are not required by the runtime.

## Build

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j"$(nproc)"
```

## Run

```sh
./build/mocktail \
  --libroblox_file ~/libroblox.so \
  --assets_dir ~/assets \
  --ROBLOSECURITY '<your-cookie>'
```

This repository intentionally contains no Roblox downloader or updater.
