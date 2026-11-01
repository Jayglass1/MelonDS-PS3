# melonDS PS3 frontend (work in progress)

An experimental port of the melonDS core to the PS3 (PowerPC, big-endian,
PSL1GHT). **Nothing here has been compiled or run on PS3 hardware or a PS3
toolchain.** This document says what has and hasn't been checked.

## What has been checked (on x86, in a sandbox)

- The core builds natively, and also with the little-endian wrapper types
  forced on (`-DMELONDS_FORCE_ENDIAN_WRAPPERS`), so every converted memory
  access type-checks.
- `src/Endian.h`'s swap logic passes a self-test run as a big-endian host would
  run it (`-DMELONDS_FORCE_ENDIAN_SWAP`).
- The shared frontend code (`EmuCore`, `Platform_PS3`, both sync backends)
  builds and links into a headless runner, and the bare console runs 120 frames
  with identical output from the threaded and single-threaded backends.
- **Not checked:** running any game. No ROM was available.

## Layout

| File | Role | Verified |
|---|---|---|
| `EmuCore.*` | Boots a ROM, runs frames, exposes the screens | host build + bare-console run |
| `Platform_PS3.cpp` | The core's `Platform::` hooks (files, saves, stubs) | host build + run |
| `Sync_single.cpp` | Single-threaded semaphores/mutexes/clock (PS3 build) | host build + run |
| `Sync_std.cpp` | Threaded version for the host runner | host build + run |
| `main_headless.cpp` | Host runner, writes screens as PPM + checksum | host build + run |
| `Video_PS3.cpp`, `Input_PS3.cpp`, `main_ps3.cpp` | PS3 video, pad, entry point | **not compiled** |
| `Makefile` | PS3 build | **never run** |

Host runner:

    cmake -S . -B build -DBUILD_QT_SDL=OFF -DENABLE_JIT=OFF -DENABLE_OGLRENDERER=OFF \
          -DENABLE_GDBSTUB=OFF -DBUILD_PS3_HEADLESS=ON -DPS3_FRONTEND_SINGLE_THREADED=ON
    cmake --build build --target melonds-ps3-headless
    ./build/src/frontend/ps3/melonds-ps3-headless game.nds 120 out

## Known blockers and gaps

1. **`std::filesystem`.** `FATStorage.h` uses it and is pulled in by core
   headers such as `Args.h`. The usual PS3 GCC (7.x) only has
   `<experimental/filesystem>`, if that. Needs a compatibility shim or
   excluding the homebrew SD-card code.
2. **ROM memory.** `ParseROM` takes the whole ROM in RAM. The PS3 has roughly
   200 MB usable; large ROMs need streaming.
3. **Speed.** No PowerPC JIT exists, so the interpreter runs everything.
   Expect it to be slow. SPU offload of the 3D renderer would be the big win.
4. **No audio output yet** (`SPU.ReadOutput` exists; the PS3 audio port isn't
   wired up).
5. **DSi mode.** `UcodeBase::GetDataMemPointer` (DSP HLE) still returns a plain
   `u16*` into little-endian memory and is wrong on big-endian. DSi only.
6. **Savestates** store host-order data and aren't portable to PC.
7. **PS3 API calls are from memory.** `Video_PS3.cpp`, `Input_PS3.cpp` and the
   Makefile assume the PSL1GHT sample API (`rsxutil`, `ioPad*`,
   `sysUtil*`, `ppu_rules`); check them against your installed headers.
8. Video is drawn at 1x, unscaled.

## Controls

Circle=A, Cross=B, Triangle=X, Square=Y, L1/R1=L/R, D-pad, Start, Select.
Right stick moves the touch cursor, R3 taps it. Start+Select+L1+R1 quits.

ROM path: `/dev_hdd0/game/MELONDS00/USRDIR/rom.nds` (or the first argument).
The save is written next to the ROM as `rom.sav`.
