<p align="center">
  <img src="assets/logo.svg" width="480" alt="WinEffects">
</p>

<p align="center">
  <b>English</b> | <a href="README.ru.md">Русский</a>
</p>

<p align="center">
  <a href="https://github.com/ohixx/wineffects/stargazers"><img src="https://img.shields.io/github/stars/ohixx/wineffects?style=for-the-badge&logo=github&label=%E2%AD%90%20Star%20this%20repo&color=f5af3c" alt="Star this repo"></a>
</p>

<p align="center">
  <img src="https://img.shields.io/badge/platform-Windows%2010%20%7C%2011-0078d4?style=flat-square" alt="Windows 10 | 11">
  <img src="https://img.shields.io/badge/C%2B%2B-17-00599c?style=flat-square" alt="C++17">
  <a href="LICENSE"><img src="https://img.shields.io/badge/license-BSD--3-3ac97b?style=flat-square" alt="BSD-3"></a>
  <img src="https://img.shields.io/badge/status-alpha-f5af3c?style=flat-square" alt="alpha">
</p>

---

WinEffects is a small native Windows app in the spirit of EasyEffects. It takes your microphone, runs it through a chain of effects and sends the result to a virtual cable, which other programs can use as a microphone.

Effects available now:

- **Noise suppression**: [RNNoise](https://gitlab.xiph.org/xiph/rnnoise) with an adjustable strength.
- **Pitch**: -12 to +12 semitones, speech speed unchanged ([Signalsmith Stretch](https://github.com/Signalsmith-Audio/signalsmith-stretch)).

The chain is editable: add, remove, reorder and toggle effects. New effects only need an `Effect` class with a few parameters; the UI is generated from it (see `src/effect.h`).

## Usage

1. Install a virtual cable such as [VB-Cable](https://vb-audio.com/Cable/).
2. Open Settings, choose your microphone and set the output to `CABLE Input`.
3. Press Start, then pick `CABLE Output` as the microphone in Discord, OBS, etc.

Settings also cover monitoring through headphones, start with Windows, close to tray, start minimized and auto-start of processing. Configuration is stored in `%APPDATA%\WinEffects\settings.ini`.

## Technical overview

`microphone -> WASAPI -> effect chain (mono, 48 kHz, 10 ms blocks) -> WASAPI -> virtual cable`

- C++17 and CMake. No Electron, Qt or .NET; one static executable.
- Audio I/O: [miniaudio](https://miniaud.io). Interface: [Dear ImGui](https://github.com/ocornut/imgui) on Direct3D 11.
- Lock-free ring buffers between devices; the audio thread never allocates.
- Estimated latency with pitch enabled: about 120 ms, without it about 45 ms.

## Status

The audio engine and interface are implemented but have so far only been compiled and unit-checked on Linux. Real-device testing on Windows is still ongoing. A built-in virtual microphone driver (with a custom name) is not implemented yet; the name field is reserved for it.

## Build

Requires CMake 3.20+ and MinGW-w64 (the bundled RNNoise is plain C and is built with GCC). On Linux this cross-compiles:

```sh
cmake -B build -DCMAKE_SYSTEM_NAME=Windows \
  -DCMAKE_C_COMPILER=x86_64-w64-mingw32-gcc \
  -DCMAKE_CXX_COMPILER=x86_64-w64-mingw32-g++
cmake --build build -j
```

On Windows with MSYS2/MinGW-w64 the same works with `cmake -B build -G Ninja`. Dependencies are downloaded by CMake.

## Support the project

If WinEffects is useful to you, **[give it a star](https://github.com/ohixx/wineffects)**. It helps others find it and keeps development going. Found a bug or have an effect idea? [Open an issue](https://github.com/ohixx/wineffects/issues).

## License

BSD 3-Clause, see [LICENSE](LICENSE). Dependencies: RNNoise (BSD-3-Clause), Signalsmith Stretch and Dear ImGui (MIT), miniaudio (public domain / MIT-0).
