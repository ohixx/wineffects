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

- **Noise suppression**: [RNNoise](https://gitlab.xiph.org/xiph/rnnoise) with an adjustable strength. *Fast* mode cuts 10 ms of delay.
- **Pitch**: -12 to +12 semitones, speech speed unchanged. Two methods: *Natural* (time-domain, like SoundTouch in EasyEffects) and *Smooth* ([Signalsmith Stretch](https://github.com/Signalsmith-Audio/signalsmith-stretch), keeps your timbre).
- **Female**: turns any voice, deep or high, into a female one. Pick how old she sounds (10 to 70). It tracks your average pitch and shifts it by one slowly changing ratio, so your intonation and manner of speech stay; formants move along, and breath and brightness add the airy colour of a woman's voice. *Max shift* limits how far a deep voice is pushed up: more is more feminine but starts to sound like a chipmunk.
- **Trash**: loud, noisy, muffled sound of a blown-out cheap microphone, for beefs and toxic battles. Distortion, muffle, bit crush, noise and crackle, loudness.

The chain is editable: add, remove, reorder and toggle effects. New effects only need an `Effect` class with a few parameters; the UI is generated from it (see `src/effect.h`).

## Install

Download [`dist/WinEffects-Setup.exe`](dist/WinEffects-Setup.exe) and run it. It installs per user (no administrator rights), adds Start menu and optional desktop shortcuts and can enable start with Windows. A checkbox installs the virtual microphone: it downloads [VB-Cable](https://vb-audio.com/Cable/) from its vendor and runs its setup (Windows asks for administrator rights for the driver). The app also tells you if no virtual cable is present.

The installer is built from `installer/wineffects.nsi` with NSIS (`makensis`, works on Linux too).

## Usage

1. Start WinEffects. If VB-Cable is installed, `CABLE Input` is selected as the output automatically.
2. Open Settings and choose your microphone.
3. In Discord, OBS, etc. pick `CABLE Output` as the microphone. That is all: WinEffects processes in the background from the moment it starts (pause it from the tray menu). Use **Sound check** to hear yourself for 30 seconds.

Settings also cover permanent monitoring through headphones, start with Windows, close to tray and start minimized. Configuration is stored in `%APPDATA%\WinEffects\settings.ini`.

## Technical overview

`microphone -> WASAPI -> effect chain (mono, 48 kHz, 10 ms blocks) -> WASAPI -> virtual cable`

- C++17 and CMake. No Electron, Qt or .NET; one static executable.
- Audio I/O: [miniaudio](https://miniaud.io). Interface: [Dear ImGui](https://github.com/ocornut/imgui) on Direct3D 11.
- Lock-free ring buffers between devices; the audio thread never allocates.
- Latency (the app shows the estimate): about 20 ms for the audio path with low-latency devices, plus 10 ms for noise suppression in Fast mode (20 ms in Quality mode), about 30 ms for Pitch (Natural) and 65 ms for Pitch (Smooth) and Female. The audio buffer in Settings trades delay for stability.

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
