# Tony Hawk's Pro Skater 4 — Native PC Port

An unofficial, native **Windows and Linux** version of *Tony Hawk's Pro Skater 4* for the
original PlayStation (USA, SLUS-01485), made by **static recompilation**: the game's
PlayStation program is translated ahead of time into C and compiled into a normal x86-64
executable. No emulator runs the game, and no PlayStation BIOS is needed.

> **No game files are included in this repository.** This is a "bring your own copy"
> project: the build reads the game from **your own disc image** and creates the native
> version on your computer. See [Why you need your own copy](#why-you-need-your-own-copy).

- Plays like the original: same game logic, graphics, movies, music and timing (NTSC, 59.29 fps)
- Keyboard and game controllers (Xbox / XInput and other SDL-supported controllers) acting as
  DualShock pads with **analog sticks and vibration**, fully remappable
- **Two-player split screen** with a second controller
- Window or fullscreen, scaled from the original picture (sharp or smooth, 4:3 or square pixels)
- Career saves on a standard memory card file, usable in emulators too
- Optional hold-to-fast-forward, screenshots

## Contents

- [Why you need your own copy](#why-you-need-your-own-copy)
- [Bring your own copy](#bring-your-own-copy)
- [Building](#building)
- [Playing](#playing)
- [Controls](#controls)
- [Configuration](#configuration)
- [Saves](#saves)
- [How it was ported](#how-it-was-ported)
- [Project layout](#project-layout)
- [Troubleshooting](#troubleshooting)
- [Status](#status)
- [License and legal](#license-and-legal)

## Why you need your own copy

*Tony Hawk's Pro Skater 4* is a commercial game whose program code, graphics, music, videos
and text are copyrighted by their owners. This repository contains **only original work**:
the recompiler, a native reimplementation of the PlayStation hardware and BIOS, and the build
scripts. Nothing from the game is in it.

Everything that comes from the game is created locally, from your disc image, when you
build, and is excluded from git by [`.gitignore`](.gitignore):

| Created by the build | Contains | In the repository? |
|---|---|---|
| `extract/` | the game's executable and code modules, read from your disc | no |
| `generated/` | the game's program, translated to C | no |
| `build/`, `dist/` | the compiled port: **the executable contains the game's translated code** | no |
| `THPS4.pak` (optional) | the game's data (graphics, music, videos, text), repacked from your disc | no |

This is also why there are **no ready-made downloads**: a pre-built executable would
contain the game's program code (translated, but still the game's), and the data pack would
contain the game's data. Instead, one command turns your copy into the native version on
your own machine, usually in under a minute. Please don't share your build output or your
data pack.

## Bring your own copy

You need a disc image of **Tony Hawk's Pro Skater 4 for the PlayStation, USA version
(SLUS-01485)** in `.cue` + `.bin` format (raw Mode2/2352) made from a disc you own. Any
dumping tool that creates a raw BIN/CUE image works: for example ImgBurn or Redump's
[MPF](https://github.com/SabreTools/MPF) on Windows, or `cdrdao` on Linux. Images in other
formats (CHD, PBP, ISO with 2048-byte sectors) need to be converted back to `.bin`/`.cue`
first. (This is the PS1 game, not the PS2, GameCube, Xbox or PC versions.)

Only the USA release is supported, because the recompiled code is specific to that exact
program. `build.py` checks the files it reads from your disc against SHA-256 hashes (the
main executable plus the 15 menu code modules, see [`tools/game_files.json`](tools/game_files.json))
and stops with a clear message if they don't match. The main executable's hash is:

| File on the disc | SHA-256 |
|---|---|
| `SLUS_014.85` | `8c9f090b70ece698858ed406bab66cafeb102badc961b605489333e3e72d2bb3` |

Put the `.cue` and `.bin` into the [`disc/`](disc/) folder of this repository (any file
names; git ignores everything in it), or pass the path with `--disc`.

## Building

### Windows

Requirements: [Python 3.8+](https://www.python.org/downloads/) and about 1 GB of disk
space for the compiler. (On Windows the command may be `py` instead of `python`.)

```bat
git clone https://github.com/GabrielBigardi/THPS4-Recomp.git
cd THPS4-Recomp
python tools\fetch_deps.py
python build.py
```

1. `tools/fetch_deps.py` downloads the toolchain into `deps/`:
   [llvm-mingw](https://github.com/mstorsjo/llvm-mingw) (clang) and the
   [SDL2](https://libsdl.org) development files. Each download is checked against a pinned
   SHA-256. Nothing is installed system-wide.
2. `build.py` reads your disc image from `disc/`, recompiles the game code, compiles it and
   puts a ready-to-play folder in **`dist/THPS4-Windows-x64/`**.

### Linux

Requirements: Python 3.8+, a C compiler and the SDL2 development package.

```sh
sudo apt install build-essential python3 libsdl2-dev      # Debian / Ubuntu
sudo dnf install gcc python3 SDL2-devel                   # Fedora
sudo pacman -S base-devel python sdl2                     # Arch

git clone https://github.com/GabrielBigardi/THPS4-Recomp.git
cd THPS4-Recomp
python3 build.py
```

The result is in **`dist/THPS4-Linux-x64/`**. To build the Linux version on Windows instead
(cross-compiling with [Zig](https://ziglang.org)), run `python tools\fetch_deps.py --linux`
once and then `python build.py --linux`.

### Build options

```
python build.py [--disc PATH] [--pak] [--zip] [--linux] [--debug] [--jobs N] [--clean]
```

| Option | |
|---|---|
| `--disc PATH` | your disc image (`.cue` or `.bin`); default: the first one in `disc/` |
| `--pak` | also create the data pack (see [Playing](#playing)) so the game runs without the disc image |
| `--zip` | also create a `.zip` of the dist folder |
| `--linux` | on Windows: cross-build the Linux version |
| `--debug` | unoptimised build with a console window, for development |
| `--jobs N` | number of parallel compiler processes (default: all CPU cores) |
| `--clean` | rebuild everything from scratch |

A full build takes under a minute on a modern PC; later builds only redo the steps whose
inputs changed.

## Playing

Run `THPS4.exe` (Windows) or `./THPS4` (Linux) in the dist folder. The game needs its data
(graphics, music, videos, text), which comes from your own copy, in one of two ways:

- **Disc image (default).** The game reads your `.cue`/`.bin`. `thps4.ini` remembers the
  image the folder was built from; if you move it, put the `.cue` and `.bin` into the
  `disc` folder next to the executable, set `disc =` in `thps4.ini`, or pass the path on
  the command line.
- **Data pack (`python build.py --pak`).** Your disc image is repacked into `THPS4.pak`
  (476 MB instead of 747 MB) next to the executable, and the disc image is no longer needed.
  The pack keeps the disc's exact sector layout (the game streams its movies and music
  straight from the disc), and every block is checked against a checksum of your original
  disc on load, so the game sees exactly the same data. `THPS4 --verify-data` checks the
  whole file.

## Controls

|  | Keyboard | Xbox controller |
|---|---|---|
| D-pad | Arrow keys | D-pad / left stick |
| ✕ Cross | X / Space / K | A |
| ○ Circle | Z / L | B |
| □ Square | S / J | X |
| △ Triangle | A / I | Y |
| L1 / R1 | Q / W | LB / RB |
| L2 / R2 | 1 / 2 | LT / RT |
| Start | Enter | Start (Menu) |
| Select | Right Shift / Backspace | Back (View) |
| L3 / R3 | – | press the left / right stick |

| Key | |
|---|---|
| F11 or Alt+Enter | toggle fullscreen (Esc leaves fullscreen) |
| F12 | screenshot (saved in `screenshots/`) |
| Tab (hold) | fast-forward (can be disabled) |

Controllers act like **DualShock** pads: two analog sticks and vibration. Like a real
DualShock, the pad starts in digital mode, where the left stick works as the D-pad; bind
`pad_analog` in `thps4.ini` (for example to `guide`) to get the pad's ANALOG button. Any
controller that SDL2 recognises as a game controller works (Xbox, PlayStation, Switch Pro and
many others) with the Xbox-style layout above.

**Two players:** the keyboard and the first controller are player 1, a second controller is
player 2. "Two Player" in the main menu becomes available while a second controller is
connected. Controllers can be plugged in or out at any time.

## Configuration

`thps4.ini` next to the executable is a commented settings file; remove the `#` in front of
an option to change it.

| Section | Setting | Default | |
|---|---|---|---|
| `[general]` | `disc` | (automatic) | path to your `.cue`/`.bin` (or a `.pak`) |
|  | `savedir` | `saves` | folder for the memory card file |
| `[video]` | `scale` | `3` | initial window size as a multiple of 320×240 |
|  | `fullscreen` | `0` | start in fullscreen |
|  | `smooth` | `0` | `1` = bilinear (smooth) scaling, `0` = sharp pixels |
|  | `aspect43` | `1` | `1` = 4:3 picture like a TV, `0` = square pixels |
| `[behaviour]` | `fastforward` | `1` | allow holding Tab to fast-forward |
|  | `fastcd` | `0` | `1` = shorter loading pauses (music and movie streaming are unaffected) |
| `[keyboard]` | `key_up` … `key_select`, `key_l3`, `key_r3`, `key_lstick_*`, `key_analog` |  | comma-separated SDL key names, e.g. `key_cross = X,Space,K` |
| `[controller]` | `controller` | `dualshock` | `dualshock` (sticks + vibration) or `digital` (the original digital pad) |
|  | `rumble` | `1` | vibration on/off |
|  | `deadzone` | `15` | analog stick dead zone in percent |
|  | `pad_up` … `pad_select`, `pad_l3`, `pad_r3`, `pad_lstick_*`, `pad_analog` |  | player 1's controller buttons (SDL names: `a b x y back start guide leftshoulder rightshoulder leftstick rightstick dpup dpdown dpleft dpright lefttrigger righttrigger`) |
|  | `pad2_…` |  | the same for player 2's controller |
|  | `stick_as_dpad` | `1` | in digital mode the left stick also works as the D-pad |

Command line options: `--fullscreen`, `--scale N`, `--smooth`, `--fastcd`,
`--savedir DIR`, `--verify-data`, `--log 0..4`, and a disc image or data pack path. Testing
options are described in [docs/TECHNICAL.md](docs/TECHNICAL.md#testing-and-debugging).

## Saves

Saving and loading work exactly as on the PlayStation (Options > Memory Card > Save Data /
Load Data, and the career menu). The memory card is `saves/card1.mcd`, a standard 128 KB raw
memory card image as used by DuckStation, ePSXe, PCSX and others, so you can bring an
existing career from an emulator or take yours to one. If the game folder is not writable,
the card is kept in `%APPDATA%\THPS4` (Windows) or `~/.local/share/THPS4` (Linux).

## How it was ported

**Static recompilation.** An emulator reads the game's machine code while the game runs
and interprets (or JIT-compiles) it instruction by instruction. This port translates the
whole program **once, before it runs**:

1. **Extract.** `tools/iso.py` reads the ISO 9660 file system of your disc image, and
   `tools/wad.py` the game's own archive (`CD.HED`/`CD.WAD`, whose file names are stored as
   hashes of a CRC variant the game uses). Out come the main executable (`SLUS_014.85`) and
   the 15 menu code modules (main menu, level select, create-a-skater, park editor, options
   and others) with their relocation tables.
2. **Analyse.** `tools/recomp.py` decodes the MIPS R3000A machine code and finds every
   function: call targets, code addresses built in registers, function-pointer tables,
   switch jump tables. In this game, string data sits between the functions, so candidates
   have to look like real function starts. Analysis repeats until nothing new turns up
   (about 3,850 functions, 232,000 instructions).
3. **Translate.** Each MIPS function becomes a C function. Registers become fields of a CPU
   structure, memory accesses go through small inline helpers, branches (including MIPS
   branch delay slots) become `goto`s, and jump tables become `switch` statements. Every
   block also counts the CPU cycles the original would take, because the game's timing
   depends on it.
   - The menu modules are **relocatable**: the game loads them to memory it allocates at run
     time and patches their addresses. The recompiler uses their relocation tables to emit
     every patched constant relative to the module's load address, and the runtime
     recognises which module is in memory (they replace each other in the same block).
   - The video decoder's hand-written bitstream reader jumps into its own subroutines and
     keeps state in the return-address register; it is translated as one function with
     explicit return points.
4. **Compile.** The ~480,000 lines of generated C are compiled with clang/gcc into a native
   x86-64 program, together with the runtime.

**The runtime.** The translated game still believes it is running on a PlayStation, so
`runtime/` provides everything it talks to, written natively from public hardware
documentation: the graphics chip (a software renderer reproducing the PS1 GPU's exact
drawing rules, including 24-bit video output), the geometry coprocessor (GTE), the motion
decoder (MDEC) that plays the movies, the sound chip (24 ADPCM voices with envelopes,
interpolation and reverb), the CD drive (including streamed XA music and movie data), two
DualShock controllers and the memory card, DMA, timers and interrupts, plus the BIOS
kernel functions the game calls. This hardware layer is a reimplementation, like the one an
emulator contains, but the game's own code runs as native machine code. SDL2 provides the
window, input and audio.

**Checking it.** The hardware pieces have unit tests (`python tools/run_tests.py`). The
whole game was checked with long random-input sessions in career mode, two-player split
screen, create-a-skater and the park editor; by comparing runs frame by frame (screenshots,
memory, audio and saves bit-identical between debug and release builds and between disc
image and data pack); and by playing: intro movies, menus, a career level, saving and
loading.

Details for developers: [docs/TECHNICAL.md](docs/TECHNICAL.md).

## Project layout

```
build.py              one-step build: disc image -> native game in dist/
disc/                 put your .cue/.bin here (ignored by git)
tools/
  fetch_deps.py       downloads the Windows toolchain into deps/
  iso.py              disc image + ISO 9660 reader
  wad.py              the game's CD.HED/CD.WAD archive
  mips.py, mdis.py    MIPS decoder, disassembler / cross-reference helper
  recomp.py           the static recompiler (MIPS -> C)
  modules.json        main executable, relocatable menu modules, flat (hand-written) regions
  game_files.json     SHA-256 of the files the recompiled code is made from
  extra_entries.json  entry points that can't be found statically
  compile.py          compiles runtime + generated code
  makepak.py          builds the optional data pack from your disc image
  package.py          assembles the dist folders
  run_tests.py        runtime unit tests
  sheet.py            screenshot contact sheets (testing)
runtime/              the native PlayStation hardware + BIOS reimplementation, frontend
  tests/              unit tests (GTE, GPU, SPU, MDEC, DualShock, memory card file system)
dist-template/        thps4.ini, README.txt and licence text copied into each build
docs/TECHNICAL.md     internals
LICENSE               MIT license for this port's source code
```

Created locally and ignored by git: `deps/` (toolchain), `extract/`, `generated/`,
`build/`, `dist/`.

## Troubleshooting

- **"Game data not found"**: the game can't find your disc image. Put the `.cue` and
  `.bin` into the `disc` folder next to the executable, or set `disc =` in `thps4.ini`.
- **"does not match the USA release"** when building: your image is a different region or
  revision, a different platform's version, or a bad dump. Only SLUS-01485 is supported.
- **The `.bin` named in the `.cue` is missing**: the `.cue` refers to the `.bin` by file
  name; keep both together and don't rename one without editing the other.
- **No sound**: the game opens the default audio device when it starts and picks up a
  device plugged in later. Check `THPS4.log` next to the executable.
- **Controller not working**: make sure it works in other games; SDL2 must recognise it as
  a game controller. Buttons can be remapped in `thps4.ini`.
- **"Two Player" is greyed out**: connect a second controller.
- Anything else: the log file `THPS4.log` next to the executable usually says what went
  wrong.

## Status

The whole game is recompiled. Tested so far: boot, all intro movies, the main menu and its
sub-menus, career mode in College (tricks, goals, competitions, bails, pause menu),
two-player split screen, create-a-skater, the park editor, options, memory card saving and
loading, and long random-input sessions without errors. The later career levels and the pro
skater videos use the same code paths but haven't all been played through yet. The Linux
version builds (natively or cross-compiled) but has had less testing than the Windows
version. Bug reports are welcome in the
[issue tracker](https://github.com/GabrielBigardi/THPS4-Recomp/issues); please attach
`THPS4.log`.

## License and legal

The source code in this repository (the recompiler, the runtime and the scripts) is released
under the [MIT License](LICENSE). The license covers only this port's own code. It grants no
rights to *Tony Hawk's Pro Skater 4*, and it does not cover what the build generates from
your disc (the extracted files, the translated game code, the executable and the data pack),
which is derived from the game.

This is an unofficial fan project, not affiliated with or endorsed by Activision, Neversoft,
Vicarious Visions or Sony Interactive Entertainment. *Tony Hawk's Pro Skater* is a trademark
of its respective owners. This repository contains no code, graphics, music, videos, text or
other data from the game; you need your own legally obtained copy, and what the build
produces from it is for your personal use only.

The port uses [SDL2](https://libsdl.org) (zlib license, see
`dist-template/LICENSE-SDL2.txt`). The toolchain downloaded by `tools/fetch_deps.py`
(llvm-mingw, SDL2, Zig) is used only to build and is not part of this repository.
Hardware information came from the public PlayStation documentation, above all
[psx-spx](https://psx-spx.consoledev.net/); the MDEC's exact rounding follows the
hardware-verified behaviour described by the [Mednafen](https://mednafen.github.io/) project.
