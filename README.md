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

The Standalone app can be launched directly. Copy `FrancDelay.vst3` to `~/Library/Audio/Plug-Ins/VST3` when you want it in a host.

### Windows

Install Visual Studio 2022 or Visual Studio 2026 with the Desktop development with C++ workload, plus CMake. Visual Studio 2026 needs CMake 4.2 or newer, which is the first release that knows the `Visual Studio 18 2026` generator. Nothing in the build uses pkg-config or other Linux-only tools.

From an x64 developer prompt:

```bat
cmake -B build -A x64
cmake --build build --config Release
```

`-A x64` builds the 64-bit configuration of whichever Visual Studio CMake discovers. To pin one, pass `-G "Visual Studio 17 2022"` or `-G "Visual Studio 18 2026"`.

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

GitHub Actions builds Release artifacts on every push to `main` and on every pull request. A tag named `v*` (for example `v0.1.0`) runs the same builds and publishes a GitHub Release that contains both zips.

| Zip | Runner | Contents |
| --- | --- | --- |
| `FrancDelay-windows-x64.zip` | `windows-latest`, MSVC, x64 | `FrancDelay.vst3` (the whole bundle) and `FrancDelay.exe` |
| `FrancDelay-macos-universal.zip` | `macos-latest` | `FrancDelay.vst3` and `FrancDelay.app`, universal arm64 + x86_64, macOS deployment target 11.0 |

The macOS zip is made with `ditto`. Each bundle is copied with `ditto`, then `ditto -c -k` archives that directory, so the zip root is `FrancDelay.vst3/` and `FrancDelay.app/`. `ditto -c` cannot take both bundles as sources in one command, and archiving a bundle without its parent directory would flatten `Contents/` to the zip root. Unzip it with Finder or `ditto -x -k FrancDelay-macos-universal.zip .`.

### Where to download

- **Pull requests and pushes to `main`:** open the [Actions](https://github.com/valvolinue-del/FrancDelay/actions) tab, select the CI run, and download the artifacts at the bottom of the run summary. They are kept with the run, not attached to a release.
- **Tags `v*`:** open the [Releases](https://github.com/valvolinue-del/FrancDelay/releases) page. The release for that tag has both zips.

Windows artifacts are not Authenticode signed. The workflow has a gated stub for that; see the comment in `.github/workflows/ci.yml`. macOS artifacts are ad-hoc signed when the Apple secrets below are absent, so the job still passes. Gatekeeper will refuse an ad-hoc build until you open it from the Finder context menu. When all six secrets are set, the bundles are Developer ID signed, notarized, and stapled.

### Apple signing secrets

The macOS job reads these repository Actions secrets. It does not read environment secrets unless you change the workflow.

| Secret | What it is |
| --- | --- |
| `MACOS_CERTIFICATE_P12` | Base64 of a Developer ID Application `.p12`, including its private key |
| `MACOS_CERTIFICATE_PASSWORD` | Password you set when exporting that `.p12` |
| `MACOS_SIGNING_IDENTITY` | Certificate name, for example `Developer ID Application: Your Name (TEAMID)` |
| `APPLE_ID` | Apple ID email of the developer account |
| `APPLE_TEAM_ID` | 10-character Team ID |
| `APPLE_APP_PASSWORD` | App-specific password used by `notarytool` |

Signing runs only when the three `MACOS_*` secrets are all set. Notarization runs only when those three and the three `APPLE_*` secrets are all set. A partial set fails the job so a typo is not shipped as an ad-hoc build.

1. Enroll the Apple Developer Program (a paid membership). Developer ID certificates can be created by the Account Holder or an Admin.
2. On a Mac, open Keychain Access. Choose Keychain Access → Certificate Assistant → Request a Certificate From a Certificate Authority. Enter the Apple ID email, select Saved to disk, and save the `.certSigningRequest` file.
3. Open [Certificates](https://developer.apple.com/account/resources/certificates/list) and click the add button. Choose **Developer ID Application** (Software). This is the certificate for software distributed outside the Mac App Store. Do not choose Apple Development or Mac App Distribution. Upload the certificate signing request and download the `.cer`.
4. Double-click the `.cer` to install it in the login keychain. In Keychain Access, select the login keychain and My Certificates. Expand **Developer ID Application: Your Name (TEAMID)** and confirm a private key is listed under it. If the private key is missing, the signing request was created on another Mac and this certificate cannot sign.
5. Select the certificate and the private key together. Choose File → Export Items, pick Personal Information Exchange (`.p12`), and set an export password. That password is `MACOS_CERTIFICATE_PASSWORD`. The certificate's common name, including the team id in parentheses, is `MACOS_SIGNING_IDENTITY`.
6. Base64-encode the `.p12` as a single line. On a Mac:

   ```bash
   base64 -i DeveloperIDApplication.p12 | tr -d '\n' | pbcopy
   ```

   The clipboard is `MACOS_CERTIFICATE_P12`. Do not commit the `.p12` or the base64 text.
7. Create the notarization password at [account.apple.com](https://account.apple.com) under Sign-In and Security → App-Specific Passwords. Name it `FrancDelay notarization`. Apple shows the password once. That value is `APPLE_APP_PASSWORD`. `APPLE_ID` is the Apple ID email. `APPLE_TEAM_ID` is the Team ID on [Membership details](https://developer.apple.com/account) (the same 10 characters as in the certificate name).
8. In this GitHub repository, open Settings → Secrets and variables → Actions → New repository secret. Add each of the six names above as its own secret. Paste the base64 string for `MACOS_CERTIFICATE_P12` with no extra quotes.

The first Developer ID certificate Apple issues for an account can take some time to appear. `notarytool` accepts the app-specific password only for the Apple ID that created it.

### Cut a release

On the commit you want to ship:

```bash
git tag v0.1.0 && git push --tags
```

The tag push builds both platforms and, if both succeed, creates the GitHub Release. Pushing the tag again does not move it; delete the remote tag first if you need to reuse the name.

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
.github/scripts/macos-sign-and-package.sh
.github/macos/entitlements.plist
Source/PluginProcessor.h
Source/PluginProcessor.cpp
Source/PluginEditor.h
Source/PluginEditor.cpp
Source/DelayVisualizer.h
Source/DelayVisualizer.cpp
```

JUCE itself is covered by the [JUCE licence](https://juce.com/legal/juce-8-licence/) (or AGPLv3). This plug-in's manufacturer code is `Frnc` and its plug-in code is `FrDl`.
