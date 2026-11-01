# melonDS PS3 frontend (work in progress)

An experimental port of the melonDS core to the PS3 (PowerPC, big-endian,
PSL1GHT). **It compiles and links with the real PS3 toolchain, but it has never
been run, on a PS3 or in an emulator.** This document says what has and hasn't
been checked.

## What has been checked

With the ps3dev toolchain (GCC 7.2.0, `ps3dev-ubuntu-latest-2020-08-31`):

- All source files (core, teakra, frontend) compile and `melonds-ps3.elf` links.
  The only compiler warning is a `-Wstringop-overflow` in upstream's
  `VRAMTrackingSet::DeriveState` (GPU VRAM dirty tracking). It is probably a
  GCC 7.2 false positive, but that has not been confirmed.
- The result is a 64-bit big-endian PowerPC executable (~4.2 MB text,
  160 KB data).
- `fself` (ships with ps3dev) wraps the ELF into a fake-signed `.self`: valid
  `SCE` header, SELF category, ELF embedded byte-for-byte. `make_self` is not
  used because it needs `libcrypto.so.1.1`.
- The compiler detects the PPU as big-endian and selects the byte-swapping
  `le_*` types (`static_assert` check).
- A wrapper-mode build with format warnings on shows no `le_*` value passed
  through `printf` varargs (which would print swapped bytes on big-endian).

On x86 (sandbox):

- The core builds natively and with the wrapper types forced on
  (`-DMELONDS_FORCE_ENDIAN_WRAPPERS`), so every converted memory access
  type-checks.
- `src/Endian.h`'s swap logic passes a self-test run as a big-endian host would
  run it (`-DMELONDS_FORCE_ENDIAN_SWAP`).
- The audio ring buffer passes its unit tests under ASan/UBSan
  (`ps3-audio-ring-test`).
- The shared frontend code builds into a headless runner; the bare console runs
  120 frames with identical output from the threaded and single-threaded
  backends.

**Not checked:** running anything on big-endian hardware or an emulator; running
any game (no ROM was available); whether the `.self` actually loads. It is
fake-signed, so it needs custom firmware or RPCS3; stock firmware will refuse it.

## Layout

| File | Role | Verified |
|---|---|---|
| `EmuCore.*` | Boots a ROM, runs frames, exposes the screens | host run + PS3 compile |
| `Platform_PS3.cpp` | The core's `Platform::` hooks (files, saves, stubs) | host build + run |
| `Sync_single.cpp` | Single-threaded semaphores/mutexes/clock (PS3 build) | host build + run |
| `Sync_std.cpp` | Threaded version for the host runner | host build + run |
| `main_headless.cpp` | Host runner, writes screens as PPM + checksum | host build + run |
| `Video_PS3.cpp`, `Input_PS3.cpp`, `main_ps3.cpp`, `rsxutil.*` | PS3 video, pad, entry point, display setup | compiles and links; **never run** |
| `AudioRing.h` (+ `AudioRing_test.cpp`) | Audio ring buffer and block-scheduling policy | unit-tested on host (ASan/UBSan) |
| `Audio_PS3.cpp` | Writes float blocks into the PS3 audio port | compiles and links; **never run** |
| `compat.h` | std::mutex / to_string / snprintf stand-ins for the PS3 libstdc++ | compiles |
| `Makefile` | PS3 build (`make` with PS3DEV/PSL1GHT set) | builds the ELF and a fake-signed `.self` (`fself`) |

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
4. **Audio is untested on hardware.** The ring/scheduling logic is unit-tested,
   but `Audio_PS3.cpp` assumes `audioPortConfig::readIndex` is the address of a
   64-bit block counter and that the port runs without a notify event queue.
   The DS (59.83 Hz) and the display (60 Hz) also drift slightly, so expect an
   occasional dropped or silent block until audio is properly synced.
5. **DSi mode.** DSP HLE now accesses DSP memory through `le_u16` (compiles in
   all three configurations) but has never been run, and DSi mode needs real
   DSi BIOS/NAND files that this frontend doesn't load yet.
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
