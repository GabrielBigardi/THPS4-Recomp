TONY HAWK'S PRO SKATER 4 - NATIVE PC PORT
=========================================

This is a native PC (Windows / Linux) build of Tony Hawk's Pro Skater 4 for the PlayStation
(USA, SLUS-01485), made from your own copy of the game. The PlayStation game code was
statically recompiled into native x86-64 code - there is no emulator and no PlayStation BIOS
involved. The PlayStation hardware the game talks to (graphics chip, sound chip, CD drive,
video decoder, controller ports, memory card, geometry coprocessor, timers) and the BIOS
services it uses are reimplemented natively so the game behaves exactly like the original.

RUNNING
-------
Run THPS4.exe (Windows) or ./THPS4 (Linux).

Linux: requires SDL2 (normally already installed; otherwise e.g. "sudo apt install
libsdl2-2.0-0", "sudo dnf install SDL2" or "sudo pacman -S sdl2"). Make the file executable
if needed: chmod +x THPS4

{{GAME_DATA}}

CONTROLS
--------
                 Keyboard                 Xbox controller
  D-pad          Arrow keys               D-pad / left stick
  Cross (X)      X / Space / K            A
  Circle (O)     Z / L                    B
  Square         S / J                    X
  Triangle       A / I                    Y
  L1 / R1        Q / W                    LB / RB
  L2 / R2        1 / 2                    LT / RT
  Start          Enter                    Start (Menu)
  Select         Right Shift / Backspace  Back (View)
  L3 / R3        -                        press the left / right stick

  F11 or Alt+Enter   toggle fullscreen         Esc   leave fullscreen
  F12                screenshot (screenshots folder)
  Tab (hold)         fast-forward (can be disabled in thps4.ini)

The controllers act like DualShock pads: analog sticks and vibration. Like a real DualShock
the pad starts in digital mode, where the left stick works as the D-pad; set "pad_analog ="
in thps4.ini (for example to "guide") to have an ANALOG button.

TWO PLAYERS: the keyboard and the first controller are player 1, a second controller is
player 2 ("Two Player" in the main menu becomes available when it is plugged in).
Controllers can be plugged in at any time. All keys and buttons can be remapped in thps4.ini.

SAVES
-----
Saving works exactly as on the PlayStation, for example Options > Memory Card > Save Data
(Load Data to continue). The memory card is stored in saves\card1.mcd - a standard 128 KB raw
memory card image, compatible with DuckStation, ePSXe, PCSX and others, so you can copy an
existing card here to continue an old career, or take your card to an emulator. If the game
folder is not writable, the card is kept in %APPDATA%\THPS4 (Windows) or ~/.local/share/THPS4
(Linux).

NOTES
-----
* Video runs at the original NTSC rate (59.29 fps) with the original picture (512x240 or
  320x240), scaled up. The movies play from the original video streams.
* A log file (THPS4.log) is written next to the executable - useful if something goes wrong.
* This folder is for your personal use only - do not share it. The executable contains the
  game's program code translated from your copy, which, like the game's data, remains the
  property of its copyright holders (Activision, Neversoft, Vicarious Visions).
  SDL2 is used under the zlib license (LICENSE-SDL2.txt).
