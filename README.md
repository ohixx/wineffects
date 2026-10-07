<div align="center">

<img src="assets/logo.svg" alt="WinEffects" width="640">

**English** | [Русский](README.ru.md)

<a href="https://github.com/ohixx/wineffects"><img src="assets/star.svg" alt="Star this repo" width="300"></a>

</div>

---

WinEffects is a small native Windows app, an EasyEffects-style tool with only two effects:

- noise suppression with [RNNoise](https://gitlab.xiph.org/xiph/rnnoise);
- pitch shifting, from -12 to +12 semitones.

The processed signal is meant to be exposed as a virtual microphone with a configurable name.

## Technical overview

Pipeline: `microphone -> WASAPI -> RNNoise (48 kHz, 10 ms frames) -> pitch shifter -> virtual microphone`.

- C++17, CMake, no Electron, Qt or .NET.
- UI: [Dear ImGui](https://github.com/ocornut/imgui) on Direct3D 11, custom dark theme.
- Planned audio stack: WASAPI, RNNoise, Signalsmith Stretch.

## Status

Early development. The interface is implemented; the audio engine and the virtual microphone are not.

## Build

Requires Windows 10/11, CMake 3.20+ and Visual Studio 2022 or MinGW-w64.

```bat
cmake -B build
cmake --build build --config Release
```

## License

BSD 3-Clause, see [LICENSE](LICENSE).
