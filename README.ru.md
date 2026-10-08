<p align="center">
  <img src="assets/logo.svg" width="480" alt="WinEffects">
</p>

<p align="center">
  <a href="README.md">English</a> | <b>Русский</b>
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

WinEffects: небольшое нативное приложение для Windows в духе EasyEffects. Берёт сигнал с микрофона, пропускает через цепочку эффектов и отправляет в виртуальный кабель, который другие программы используют как микрофон.

Доступные эффекты:

- **Шумоподавление**: [RNNoise](https://gitlab.xiph.org/xiph/rnnoise) с регулируемой силой.
- **Pitch**: от -12 до +12 полутонов без изменения скорости речи ([Signalsmith Stretch](https://github.com/Signalsmith-Audio/signalsmith-stretch)).

Цепочку можно менять: добавлять, удалять, переставлять и отключать эффекты. Чтобы добавить новый эффект, достаточно написать класс `Effect` с параметрами, интерфейс строится по нему сам (см. `src/effect.h`).

## Использование

1. Установи виртуальный кабель, например [VB-Cable](https://vb-audio.com/Cable/).
2. В настройках выбери микрофон, а выходом укажи `CABLE Input`.
3. Нажми Start, а в Discord, OBS и других программах выбери микрофоном `CABLE Output`.

В настройках также есть прослушивание через наушники, автозапуск с Windows, сворачивание в трей, запуск свернутым и автостарт обработки. Конфигурация хранится в `%APPDATA%\WinEffects\settings.ini`.

## Технически

`микрофон -> WASAPI -> цепочка эффектов (моно, 48 кГц, блоки 10 мс) -> WASAPI -> виртуальный кабель`

- C++17 и CMake. Без Electron, Qt и .NET; один статический exe.
- Аудио: [miniaudio](https://miniaud.io). Интерфейс: [Dear ImGui](https://github.com/ocornut/imgui) на Direct3D 11.
- Между устройствами используются lock-free кольцевые буферы; аудиопоток ничего не выделяет.
- Ожидаемая задержка: около 120 мс с включенным pitch и около 45 мс без него.

## Статус

Аудиодвижок и интерфейс реализованы, но пока только собраны и проверены юнит-тестами на Linux. Проверка на реальных устройствах в Windows ещё идёт. Встроенный драйвер виртуального микрофона (с произвольным названием) пока не реализован, поле названия зарезервировано под него.

## Сборка

Нужны CMake 3.20+ и MinGW-w64 (RNNoise написан на C и собирается GCC). На Linux это кросс-компиляция:

```sh
cmake -B build -DCMAKE_SYSTEM_NAME=Windows \
  -DCMAKE_C_COMPILER=x86_64-w64-mingw32-gcc \
  -DCMAKE_CXX_COMPILER=x86_64-w64-mingw32-g++
cmake --build build -j
```

В Windows с MSYS2/MinGW-w64 работает то же самое: `cmake -B build -G Ninja`. Зависимости скачивает сам CMake.

## Поддержать проект

Если WinEffects вам пригодился, **[поставьте звезду](https://github.com/ohixx/wineffects)**: так проект проще найти другим, и он продолжает развиваться. Нашли баг или есть идея для эффекта? [Создайте issue](https://github.com/ohixx/wineffects/issues).

## Лицензия

BSD 3-Clause, см. [LICENSE](LICENSE). Зависимости: RNNoise (BSD-3-Clause), Signalsmith Stretch и Dear ImGui (MIT), miniaudio (public domain / MIT-0).
