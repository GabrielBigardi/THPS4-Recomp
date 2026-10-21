# Technical notes

How the port works internally, for people who want to hack on it. The player-facing
documentation is the [README](../README.md).

## Pipeline

```
your disc image (.cue/.bin)
   │  tools/iso.py, tools/wad.py   ISO 9660 reader + CD.HED/CD.WAD archive reader:
   ▼                               SLUS_014.85 and the 15 menu modules (<name>.bin + .rel),
extract/                           verified by SHA-256 (tools/game_files.json)
   │  tools/recomp.py              static recompiler: MIPS R3000A machine code -> C
   ▼
generated/                         ~3,850 C functions, ~480k lines (main executable + 15 modules)
   │  tools/compile.py             clang (llvm-mingw) on Windows, cc on Linux, zig cc for Linux
   ▼                               cross builds
build/<config>/THPS4(.exe)         runtime/ + generated/ linked into one native program
   │  tools/package.py
   ▼
dist/THPS4-<OS>-x64/               exe + SDL2 + thps4.ini + README (+ THPS4.pak)
```

`build.py` runs all of it and only redoes the steps whose inputs changed (`generated/.stamp`
holds a hash of the game files and the recompiler sources).

## The game's files

* `SLUS_014.85`: the main executable, 870 KB loaded at 0x80010000, entry 0x800A7030. Code runs
  from 0x80010000 to 0x800B3C20, with string and table data placed between functions (the
  compiler emitted each object's read-only data next to its code).
* `CD.HED` + `CD.WAD`: the game's archive. `CD.HED` is a list of (name hash, offset, size); the
  hash is a CRC variant (function 0x80057D5C: characters lower-cased, rotate-left instead of
  shift, no final XOR; see `tools/wad.py`).
* 15 **relocatable menu modules** in `CD.WAD`: `mainmenu`, `carsel`, `levelsel`, `playsel`,
  `cresel`, `creedit`, `cresave`, `statedit`, `tricksel`, `EquipSel`, `EdMod` (park editor),
  `OPTIONS`, `multisel`, `Handicap`, `credits`. The loader at 0x80024D74 reads `<name>.bin`
  into memory it allocates at run time, applies `<name>.rel` (routine 0x8002507C) and calls
  offset 0 to initialise the module. Several modules are loaded into the same memory one after
  another.
* `.STR` files: MDEC movies with interleaved XA audio; `COMPILED.XA`: the soundtrack.

## The recompiler (`tools/recomp.py`, configured by `tools/modules.json`)

* **Function discovery** is a fixpoint over: the entry point, `jal` targets, `lui`/`addiu`
  code-address constants (including ones built in branch delay slots), data words that point
  at code, a sweep after each `jr ra`, and jump-table targets. Because data sits between
  functions, candidates must look like function starts (a prologue, a BIOS call stub, or the
  previous function's `jr ra`/unconditional jump before them, maybe followed by nop
  padding), must not look like ASCII text, and must trace without invalid instructions;
  calls are only taken from functions that traced cleanly. `tools/extra_entries.json` adds
  the one entry point that can't be found statically (the libetc `setjmp` return used by
  HookEntryInt).
* **Code generation.** Each MIPS function becomes `void f_<address>(CPU *c)`. Registers live
  in the `CPU` struct, memory accesses use the inline helpers in `runtime/psx_recomp.h`,
  branches become `goto`s (the delay-slot instruction is emitted before the jump, after the
  condition is evaluated), recovered jump tables become `switch` statements, and indirect
  calls go through the runtime dispatcher `call_addr()`. Unaligned `lwl/lwr/swl/swr` and `div`
  by zero follow the R3000A rules; overflow traps are not modelled.
* **Relocatable modules** are analysed at their link address 0, using the `.rel` records
  (0 = 32-bit pointer, 1 = `lui` high half with the full target in the next record,
  2 = low-half immediate, 3 = `j`/`jal` target). Every patched immediate is emitted relative
  to `BASE`, the module's current load address:

  ```
  lui  v0, %hi(sym)      ->  R[2] = RHI(0x1C);                 // (BASE + 0x1C + 0x8000) & ~0xFFFF
  addiu v0, v0, %lo(sym) ->  R[2] = R[2] + RLO(0x1C);          // sign-extended low half
  jal  local_func        ->  R[31] = BASE + 0x5A4; omainmenu_00308(c);
  ```

  Unpatched `j`/`jal` instructions in a module call the main executable.
* **Flat regions.** libpress's hand-written bitstream decoder (`DecDCTvlc`, 0x80084EE0) enters
  its own subroutines both by `jal` and by branches, returns with `jr ra` into the middle of
  the caller, and saves `ra` with its state when it pauses mid-frame. Such ranges are listed
  in `modules.json` (`flat`) and emitted as one C function: an internal `jal` becomes "set
  `ra`, goto", and `jr ra` becomes a `switch` over the internal return points (otherwise a
  real return).
* **Timing.** Every block adds its cost to `c->cycles` (loads ~5 cycles, mult 7, div 36, GTE
  commands with their documented latencies); the game's VSync timeouts and its CD/SPU/DMA
  waits depend on time advancing realistically.

A synthetic example of the translation (not code from the game):

```
MIPS                                   generated C
---------------------------------      ----------------------------------------------
loop: lw    t0, 0(a0)                  L_loop:
      addiu a0, a0, 4                    R[8] = MEM_R32(R[4]);
      addiu a1, a1, -1                   R[4] = R[4] + 4u;
      bne   a1, zero, loop               R[5] = R[5] - 1u;
      addu  v0, v0, t0   (delay slot)    c->cycles += 9;
      jr    ra                           { int br_ = R[5] != 0;
                                           R[2] = R[2] + R[8];          /* delay slot */
                                           if (br_) { EVCHK(); goto L_loop; } }
                                         return;
```

`EVCHK()` lets the scheduler run hardware events (timers, VBlank, CD, SPU) inside long loops.

## The dispatcher (`runtime/cpu.c`)

Indirect calls (`jalr`, function pointers, C++-style vtables) go through `call_addr()`. An
address in the main executable maps directly to its function. For an address elsewhere the
dispatcher looks for a relocatable module whose **signature** (16 unpatched words chosen to
differ from all other modules at the same offsets) is in RAM at the implied load address,
records that address in `g_reloc_base[]` and evicts any module whose memory the new one now
occupies. Every later call checks the signature again plus two check words of the function,
because the menu modules share boilerplate code at the same offsets and replace each other in
the same memory. Results are cached.

## The runtime (`runtime/`)

| File | What it reimplements |
|---|---|
| `cpu.c` | Dispatcher (above), COP0, syscall/break, exceptions |
| `sched.c` | Cycle-driven scheduler, interrupt controller, the 3 root counters, NTSC video timing (263 lines × 3413 GPU clocks → 59.29 Hz) |
| `mem.c` | RAM, scratchpad, I/O map |
| `bios.c` | The BIOS kernel, natively: exception/IRQ chains, HookEntryInt/ReturnFromException (setjmp/longjmp), events, memory-card file system on a raw `.mcd` image, device table, libc helpers (including the BIOS's `strstr`/`strpbrk` bugs), `printf` → log. No Sony BIOS code is used |
| `gpu.c` | GP0/GP1 command processing and a software rasterizer reproducing the PS1 GPU, 15- and 24-bit display |
| `gte.c` | Geometry Transformation Engine (COP2), bit-exact including flags |
| `spu.c` | Sound chip: 24 voices, ADPCM, ADSR, gaussian interpolation, reverb |
| `mdec.c` | Motion decoder for the movies: run-length/zigzag decoding, IDCT and YUV→RGB with the hardware's rounding, 4/8/15/24-bit output, FIFOs and status bits; DMA0/DMA1 move data on the MDEC's requests (`dma.c`) |
| `cdrom.c` | CD controller and drive: command/response/IRQ protocol, seek/read timing, 2048/2340-byte sectors, XA-ADPCM decoding with the hardware's 37.8 → 44.1 kHz zigzag resampler |
| `cdpak.c` | Reader for the optional data pack (below) |
| `dma.c` | DMA channels (GPU linked lists, OTC, CD, SPU, MDEC) |
| `sio.c`, `memcard.c` | SIO0: two DualShock controllers (digital/analog mode, config mode, 0x4D vibration mapping) or digital pads, and the memory-card protocol |
| `platform_sdl.c` | Window, scaling, keyboard + two game controllers (analog sticks, rumble), audio output, frame pacing, screenshots, headless test mode. `sdl_dyn.h` loads SDL2 at run time on Linux |
| `main.c` | Configuration (`thps4.ini`, command line), locating the game data, boot |

Things that cost time to find, in case you touch these areas:

* The menu modules are loaded one after another into the same heap block: a dispatcher that
  trusts a previously seen module at an address runs the wrong module's code (identification
  must check the module signature every time).
* Relocated `jal 0xA0` inside a module is a call to the module's own function at offset 0xA0,
  not the BIOS A0 entry point.
* The `DecDCTvlc` flat region above: translated as ordinary functions it decodes garbage.
* libcd's streaming handler (`StCdInterrupt`) skips CD sectors while MDEC output DMA is busy,
  so anything that makes the MDEC deliver fewer words than requested stalls the whole movie
  (that's how the `DecDCTvlc` problem showed up).
* The game links for 8 MB of RAM (stack at the top of 8 MB), relying on the 2 MB RAM mirrors.
* Libraries patch the real BIOS's kernel code (pad/card handler hooks in kernel RAM around
  0xDF80); the native BIOS's tables point into placeholder ROM space so the patches are
  harmless.
* Pending interrupts must be dispatched again right after an exception handler returns, and
  libcd never sets the CD interrupt mask (the runtime starts with 0x1F like the real BIOS).

## The data pack (`THPS4.pak`, optional)

The game reads by CD sector address and streams movies and music with interleaved audio, so
the pack keeps the exact sector layout instead of loose files. `tools/makepak.py` stores all
317,737 sectors as subheader + payload, zlib-compressed in 16-sector chunks: 476 MB instead
of 747 MB.

```
"THPS4PAK"  u32 version=1, total_sectors, stored_sectors, sectors_per_chunk=16, nchunks, 0
u32 offsets[nchunks + 1]      file offset of each zlib chunk
u32 crc32[nchunks]            CRC32 of the ORIGINAL raw 2352-byte sectors of each chunk
zlib chunks                   per sector: flags (bit0 form 2, bit1 form-2 EDC is zero),
                              4-byte subheader, 2048 (form 1) or 2324 (form 2) bytes
```

`runtime/cdpak.c` inflates a chunk, rebuilds each sector bit-exactly (sync, BCD MSF header,
subheader copy, EDC, ECC P/Q for form 1), compares the chunk with its CRC32 the first time it
is used, and caches recent chunks. `THPS4 --verify-data` checks the whole file.

## Testing and debugging

```
python tools/run_tests.py     unit tests: GTE, GPU, SPU, MDEC, DualShock pads, memory-card file system
python build.py --debug       console build in build/debug with debug info
```

The executable has options for automated testing (all deterministic, so two runs with the
same options produce identical output):

| Option | |
|---|---|
| `--headless` | no window, no audio |
| `--unthrottled` | run as fast as possible |
| `--frames N` | quit after N frames |
| `--shots N --shotdir DIR` | screenshot every N frames |
| `--dumpram F` | write RAM + VRAM at frame F (repeatable) |
| `--wav FILE` | record the audio output |
| `--input "frame:BTN[:dur],start-end/period:BTN[:dur]"` | scripted button presses (`P2+BTN` = second controller, which plugs it in) |
| `--fuzz SEED --fuzzstart F` | random button presses from frame F |
| `--tracefrom F` | log every function call from frame F |
| `--calltest frame:addr:a0`, `--poke frame:addr:val` | call a game function / write memory at a frame |
| `--trace`, `--tracecalls` | log dispatcher activity / every call |
| `--log 0..4`, `--logfile F` | log level / log file |

`tools/sheet.py out.png a.bmp b.bmp ...` builds contact sheets from screenshots;
`tools/mdis.py` disassembles and cross-references the extracted executable (`-o
extract/ovl/<module>.bin@0` adds a module).

The port was checked with long random-input sessions in career mode, two-player split screen,
create-a-skater and the park editor; by comparing debug and release builds and disc-image and
data-pack runs frame by frame (screenshots, RAM, VRAM, audio and memory card bit-identical);
and by playing through the intro movies, menus, a career level, saving and loading.
