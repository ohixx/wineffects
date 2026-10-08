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

- **Шумоподавление**: [RNNoise](https://gitlab.xiph.org/xiph/rnnoise) с регулируемой силой. Режим *Fast* убирает 10 мс задержки.
- **Pitch**: от -12 до +12 полутонов без изменения скорости речи. Два метода: *Natural* (во временной области, как SoundTouch в EasyEffects) и *Smooth* ([Signalsmith Stretch](https://github.com/Signalsmith-Audio/signalsmith-stretch), сохраняет тембр).
- **Female**: превращает любой голос, низкий или высокий, в женский. Можно выбрать, на сколько лет звучит девушка (от 10 до 70). Эффект следит за твоей средней высотой голоса и сдвигает её одним медленно меняющимся коэффициентом, поэтому интонация и манера речи сохраняются; форманты сдвигаются вместе с ней, а придыхание и яркость добавляют воздушную окраску женского голоса. *Max shift* ограничивает, насколько сильно поднимается низкий голос: больше значит женственнее, но ближе к бурундуку.
- **Trash**: громкий, шумный и приглушённый звук убитого дешёвого микрофона, для биффов и токсичных баттлов. Дисторшн, приглушение, bit crush, шум и треск, громкость.

Цепочку можно менять: добавлять, удалять, переставлять и отключать эффекты. Чтобы добавить новый эффект, достаточно написать класс `Effect` с параметрами, интерфейс строится по нему сам (см. `src/effect.h`).

## Установка

Скачай [`dist/WinEffects-Setup.exe`](dist/WinEffects-Setup.exe) и запусти его. Он ставится для текущего пользователя (права администратора не нужны), добавляет ярлыки в меню Пуск (и на рабочий стол по желанию) и может включить автозапуск с Windows. Отдельная галочка ставит виртуальный микрофон: установщик скачивает [VB-Cable](https://vb-audio.com/Cable/) с сайта разработчика и запускает его установку (для драйвера Windows запросит права администратора). Если виртуального кабеля нет, приложение само подскажет.

Установщик собирается из `installer/wineffects.nsi` через NSIS (`makensis`, работает и на Linux).

## Использование

1. Запусти WinEffects. Если VB-Cable установлен, `CABLE Input` выбирается выходом автоматически.
2. В настройках выбери микрофон.
3. В Discord, OBS и других программах выбери микрофоном `CABLE Output`. Это всё: WinEffects обрабатывает звук в фоне с момента запуска (паузу можно включить в меню трея). Кнопка **Sound check** на 30 секунд включает прослушивание себя.

В настройках также есть постоянное прослушивание через наушники, автозапуск с Windows, сворачивание в трей и запуск свернутым. Конфигурация хранится в `%APPDATA%\WinEffects\settings.ini`.

## Технически

`микрофон -> WASAPI -> цепочка эффектов (моно, 48 кГц, блоки 10 мс) -> WASAPI -> виртуальный кабель`

- C++17 и CMake. Без Electron, Qt и .NET; один статический exe.
- Аудио: [miniaudio](https://miniaud.io). Интерфейс: [Dear ImGui](https://github.com/ocornut/imgui) на Direct3D 11.
- Между устройствами используются lock-free кольцевые буферы; аудиопоток ничего не выделяет.
- Задержка (приложение показывает оценку): около 20 мс на аудиотракт с режимом малой задержки устройств, плюс 10 мс шумоподавление в режиме Fast (20 мс в режиме Quality), около 30 мс Pitch (Natural) и 65 мс Pitch (Smooth) и Female. Размер аудиобуфера в настройках меняет задержку на устойчивость.

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
