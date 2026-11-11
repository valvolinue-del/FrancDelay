# FrancDelay

FrancDelay is a stereo delay audio plug-in (VST3 and Standalone) built with JUCE 8 and C++20.

The delay time, feedback, and mix are smoothed, and delay reads use third-order Lagrange interpolation, so moving the time control does not click. A one-pole low-pass sits in the feedback path. Ping-pong mode cross-feeds that feedback between the left and right channels. The editor shows the echo field as a gold wave: two balls travel along it at a speed set by the delay time, with fading ghost echoes whose number and lifetime follow the feedback.

## Parameters

| ID | Name | Range | Default |
| --- | --- | --- | --- |
| `delayTime` | Delay Time | 1–2000 ms, gentle skew | 350 ms |
| `feedback` | Feedback | 0.0–0.95 | 0.40 |
| `mix` | Wet/Dry Mix | 0.0–1.0 | 0.50 |
| `cutoff` | Cutoff | 20–20000 Hz, logarithmic | 8000 Hz |
| `pingPong` | Ping-Pong | off / on | off |

State is saved and restored through the plug-in's parameter tree. The UI reads per-channel peak, RMS, and wet-envelope atomics, so the audio thread never takes a lock for the animation.

## Build

Requirements: CMake 3.22 or newer, a C++20 compiler, and a network connection the first time you configure (CMake fetches JUCE 8.0.15). To build against a JUCE checkout you already have, pass `-DFRANCDELAY_JUCE_PATH=/path/to/JUCE` instead.

Web browser and curl support in JUCE are turned off, which avoids the Linux webkit dependency.

### Linux

```bash
sudo apt update
sudo apt install build-essential cmake pkg-config \
  libasound2-dev libfreetype6-dev libfontconfig1-dev libx11-dev libxext-dev \
  libxrandr-dev libxinerama-dev libxcursor-dev libxrender-dev libxcomposite-dev \
  libgl1-mesa-dev libglu1-mesa-dev

cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

`libfreetype6-dev` supplies the `libfreetype-dev` dependency JUCE links against, and `libfontconfig1-dev` is required alongside it. If `c++` on your machine is not GCC and cannot link `libstdc++`, configure with `-DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++`.

### macOS

Install Xcode (or the Command Line Tools) and CMake, then:

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

On a Mac the deployment target defaults to 11.0, which is the oldest version that can include an Apple Silicon slice. A universal build (the same one CI produces):

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64"
cmake --build build --config Release
```

The Standalone app can be launched directly. Copy `FrancDelay.vst3` to `~/Library/Audio/Plug-Ins/VST3` when you want it in a host.

### Windows

From a Visual Studio 2022 x64 developer prompt, with the Desktop development with C++ workload installed:

```bat
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

CMake passes the MSVC flags this project needs: `/permissive-`, `/Zc:__cplusplus`, `/utf-8`, `/bigobj`, and Release `/LTCG`. Visual Studio 2026 (`-G "Visual Studio 18 2026" -A x64`) uses the same flags. Web browser and curl stay off, so this path does not need extra Windows packages.

Copy the `FrancDelay.vst3` bundle to `C:\Program Files\Common Files\VST3`. The Standalone target is `FrancDelay.exe`.

### Artefacts

After a Release build:

- VST3: `build/FrancDelay_artefacts/Release/VST3/FrancDelay.vst3`
- Standalone: `build/FrancDelay_artefacts/Release/Standalone/FrancDelay` (`.exe` on Windows)

The plug-in is not copied into the system plug-in folders automatically.

### Local JUCE

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release -DFRANCDELAY_JUCE_PATH=/path/to/JUCE
```

That path is added with `add_subdirectory`. Leave the option unset to fetch the pinned JUCE 8.0.15 tag.

## Run the standalone

```bash
./build/FrancDelay_artefacts/Release/Standalone/FrancDelay
```

## CI & Releases

GitHub Actions (`.github/workflows/ci.yml`) builds Release artefacts on every push to `main` and on pull requests:

- **Windows** (`windows-latest`, MSVC, x64): VST3 and Standalone
- **macOS** (`macos-latest`, universal arm64 + x86_64, deployment target 11.0): VST3 and Standalone

Download the zips from the workflow run's **Artifacts** list on the Actions tab. A tag that matches `v*` also publishes those zips as a [GitHub Release](https://github.com/valvolinue-del/FrancDelay/releases).

The Windows zip contains `FrancDelay.vst3` (the whole bundle folder) and `FrancDelay.exe`. The macOS zip contains `FrancDelay.vst3` and `FrancDelay.app` at the archive root. They are packed with `ditto -c -k` so the bundles stay intact (`ditto -c` accepts one source, so both bundles are staged in a directory and that directory is archived). The JUCE FetchContent checkout is cached between runs.

### Code signing secrets

macOS Developer ID signing and notarization run only when all of these repository secrets are set (Settings → Secrets and variables → Actions):

| Secret | Value |
| --- | --- |
| `MACOS_CERTIFICATE_P12` | Developer ID Application certificate exported as `.p12`, then base64-encoded (`base64 -i Certificate.p12`) |
| `MACOS_CERTIFICATE_PASSWORD` | Password for that `.p12` |
| `MACOS_SIGNING_IDENTITY` | Signing identity, for example `Developer ID Application: Your Name (TEAMID)` |
| `APPLE_ID` | Apple ID used by `notarytool` |
| `APPLE_TEAM_ID` | Apple Developer Team ID |
| `APPLE_APP_PASSWORD` | App-specific password for the Apple ID |

When those secrets are present, the macOS job imports the certificate into a temporary keychain, signs with the hardened runtime and a secure timestamp, submits the bundles with `xcrun notarytool submit --wait`, then staples the ticket onto `FrancDelay.app` and `FrancDelay.vst3` with `xcrun stapler staple`. If any secret is missing, the job still succeeds and uploads ad-hoc-signed builds.

Windows Authenticode signing is a gated stub in the workflow. It does not sign anything. The stub runs only when `WINDOWS_CERTIFICATE_BASE64` is set, and that is where a `signtool` step would go.

### Cut a release

```bash
git tag v0.1.0
git push --tags
```

The tag build publishes `FrancDelay-v0.1.0-windows-x64.zip` and `FrancDelay-v0.1.0-macos-universal.zip` on the release for that tag.

### Unsigned macOS builds

Builds produced without the signing secrets are ad-hoc signed. Gatekeeper may block them the first time. Right-click the app or plug-in and choose Open, or clear the quarantine flag:

```bash
xattr -dr com.apple.quarantine /path/to/FrancDelay.app
xattr -dr com.apple.quarantine /path/to/FrancDelay.vst3
```

## Adding a GitHub remote later

From this repository:

```bash
gh repo create FrancDelay --public --source . --push
```

Or, with a remote you create yourself:

```bash
git remote add github git@github.com:USER/FrancDelay.git && git push -u github HEAD
```

## Layout

```
CMakeLists.txt
.github/workflows/ci.yml
.github/scripts/macos-sign.sh
.github/macos/entitlements.plist
Source/PluginProcessor.h
Source/PluginProcessor.cpp
Source/PluginEditor.h
Source/PluginEditor.cpp
Source/DelayVisualizer.h
Source/DelayVisualizer.cpp
```

JUCE itself is covered by the [JUCE licence](https://juce.com/legal/juce-8-licence/) (or AGPLv3). This plug-in's manufacturer code is `Frnc` and its plug-in code is `FrDl`.
