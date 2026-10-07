<div align="center">

<img src="assets/logo.svg" alt="WinEffects" width="640">

**English** | [Русский](README.ru.md)

<a href="https://github.com/ohixx/wineffects"><img src="assets/star.svg" alt="Star this repo" width="300"></a>

</div>

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

## License

BSD 3-Clause, see [LICENSE](LICENSE). Dependencies: RNNoise (BSD-3-Clause), Signalsmith Stretch and Dear ImGui (MIT), miniaudio (public domain / MIT-0).
