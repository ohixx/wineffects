<div align="center">

<img src="assets/logo.svg" alt="WinEffects" width="640">

[English](README.md) | **Русский**

<a href="https://github.com/ohixx/wineffects"><img src="assets/star.svg" alt="Поставь звезду" width="300"></a>

</div>

---

WinEffects: небольшое нативное приложение для Windows, аналог EasyEffects всего с двумя эффектами:

- шумоподавление через [RNNoise](https://gitlab.xiph.org/xiph/rnnoise);
- смена высоты голоса от -12 до +12 полутонов.

Обработанный сигнал должен отдаваться как виртуальный микрофон с настраиваемым названием.

## Технически

Цепочка: `микрофон -> WASAPI -> RNNoise (48 кГц, кадры 10 мс) -> pitch shifter -> виртуальный микрофон`.

- C++17, CMake, без Electron, Qt и .NET.
- Интерфейс: [Dear ImGui](https://github.com/ocornut/imgui) на Direct3D 11, собственная тёмная тема.
- Планируемый аудиостек: WASAPI, RNNoise, Signalsmith Stretch.

## Статус

Ранняя разработка. Интерфейс готов; аудиодвижок и виртуальный микрофон ещё не реализованы.

## Сборка

Нужны Windows 10/11, CMake 3.20+ и Visual Studio 2022 или MinGW-w64.

```bat
cmake -B build
cmake --build build --config Release
```

## Лицензия

BSD 3-Clause, см. [LICENSE](LICENSE).
