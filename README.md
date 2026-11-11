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
  libasound2-dev libfreetype6-dev libx11-dev libxext-dev libxrandr-dev \
  libxinerama-dev libxcursor-dev libxrender-dev libxcomposite-dev \
  libgl1-mesa-dev libglu1-mesa-dev

cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

`libfreetype6-dev` supplies the `libfreetype-dev` dependency JUCE links against.

### macOS

Install Xcode (or the Command Line Tools) and CMake, then:

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

The Standalone app can be launched directly. Copy `FrancDelay.vst3` to `~/Library/Audio/Plug-Ins/VST3` when you want it in a host.

### Windows

From a Visual Studio 2022 x64 developer prompt, with the Desktop development with C++ workload installed:

```bat
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

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
Source/PluginProcessor.h
Source/PluginProcessor.cpp
Source/PluginEditor.h
Source/PluginEditor.cpp
Source/DelayVisualizer.h
Source/DelayVisualizer.cpp
```

JUCE itself is covered by the [JUCE licence](https://juce.com/legal/juce-8-licence/) (or AGPLv3). This plug-in's manufacturer code is `Frnc` and its plug-in code is `FrDl`.
