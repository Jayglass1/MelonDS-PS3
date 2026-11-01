# melonDS PS3 frontend (work in progress)

An experimental port of the melonDS core to the PS3 (PowerPC, big-endian,
PSL1GHT). **It compiles and links with the real PS3 toolchain, but it has never
been run, on a PS3 or in an emulator.** This document says what has and hasn't
been checked.

## What has been checked

With the ps3dev toolchain (GCC 7.2.0, `ps3dev-ubuntu-latest-2020-08-31`):

- All 84 source files (core, teakra, frontend) compile with no warnings and
  `melonds-ps3.elf` links: a 64-bit big-endian PowerPC executable
  (~4.2 MB text, 160 KB data).
- The compiler detects the PPU as big-endian and selects the byte-swapping
  `le_*` types (`static_assert` check).

On x86 (sandbox):

- The core builds natively and with the wrapper types forced on
  (`-DMELONDS_FORCE_ENDIAN_WRAPPERS`), so every converted memory access
  type-checks.
- `src/Endian.h`'s swap logic passes a self-test run as a big-endian host would
  run it (`-DMELONDS_FORCE_ENDIAN_SWAP`).
- The shared frontend code builds into a headless runner; the bare console runs
  120 frames with identical output from the threaded and single-threaded
  backends.

**Not checked:** running anything on big-endian hardware or an emulator; running
any game (no ROM was available); the `.self` signing step (`make_self` needs
`libcrypto.so.1.1`, which the sandbox lacks, so no `.self` was produced).

## Layout

| File | Role | Verified |
|---|---|---|
| `EmuCore.*` | Boots a ROM, runs frames, exposes the screens | host run + PS3 compile |
| `Platform_PS3.cpp` | The core's `Platform::` hooks (files, saves, stubs) | host build + run |
| `Sync_single.cpp` | Single-threaded semaphores/mutexes/clock (PS3 build) | host build + run |
| `Sync_std.cpp` | Threaded version for the host runner | host build + run |
| `main_headless.cpp` | Host runner, writes screens as PPM + checksum | host build + run |
| `Video_PS3.cpp`, `Input_PS3.cpp`, `main_ps3.cpp`, `rsxutil.*` | PS3 video, pad, entry point, display setup | compiles and links; **never run** |
| `compat.h` | std::mutex / to_string / snprintf stand-ins for the PS3 libstdc++ | compiles |
| `Makefile` | PS3 build (`make` with PS3DEV/PSL1GHT set) | builds the ELF; `.self` step untested |

Host runner:

    cmake -S . -B build -DBUILD_QT_SDL=OFF -DENABLE_JIT=OFF -DENABLE_OGLRENDERER=OFF \
          -DENABLE_GDBSTUB=OFF -DBUILD_PS3_HEADLESS=ON -DPS3_FRONTEND_SINGLE_THREADED=ON
    cmake --build build --target melonds-ps3-headless
    ./build/src/frontend/ps3/melonds-ps3-headless game.nds 120 out

## Known blockers and gaps

1. **Never run.** The first real test is untested: RPCS3 or hardware, with a
   homebrew ROM. Expect bugs, especially in video setup (`rsxutil.c`) and
   anything that depends on byte order.
2. **Speed.** No PowerPC JIT exists, so the interpreter runs everything.
   Expect it to be slow. SPU offload of the 3D renderer would be the big win.
3. **ROM memory.** `ParseROM` takes the whole ROM in RAM. The PS3 has roughly
   200 MB usable; large ROMs need streaming.
4. **No audio output yet** (`SPU.ReadOutput` exists; the PS3 audio port isn't
   wired up).
5. **DSi mode.** `UcodeBase::GetDataMemPointer` (DSP HLE) still returns a plain
   `u16*` into little-endian memory and is wrong on big-endian. DSi only.
6. **Savestates** store host-order data and aren't portable to PC.
7. **No host-directory sync.** `FATStorage` builds without `<filesystem>` here,
   so the homebrew SD-card "sync a host folder" feature is stubbed out.
8. **No threads.** The PS3 libstdc++ has no `std::mutex`; `compat.h` provides
   do-nothing stand-ins, which is only correct because the build is
   single-threaded (threaded 3D rendering must stay off).
9. Video is drawn at 1x, unscaled.

## Controls

Circle=A, Cross=B, Triangle=X, Square=Y, L1/R1=L/R, D-pad, Start, Select.
Right stick moves the touch cursor, R3 taps it. Start+Select+L1+R1 quits.

ROM path: `/dev_hdd0/game/MELONDS00/USRDIR/rom.nds` (or the first argument).
The save is written next to the ROM as `rom.sav`.
