# Call of Duty 2: Big Red One - PC port

A private project to run the original Xbox game natively on PC, from the owner's
own disc.

**Route B (now):** `cod_bro_loader` maps the original `default.xbe` into a 32-bit
Linux process and runs it directly on the CPU. Our code replaces the Xbox kernel
and, step by step, the SDK libraries (Direct3D, DirectSound, input) and then the
game's own functions, which are decompiled one at a time and hooked in.

**Route C (goal):** once every function is decompiled, the loader is deleted and
`cod_bro` is a standalone game for Linux and Windows that needs only the game's
assets.

## Layout

| Folder | Contents | Fate |
|---|---|---|
| `loader/` | XBE mapping, fake Xbox `fs`/KPCR, kernel import table, hooks, crash reports | Route B only, Linux 32-bit |
| `xbox/` | Xbox kernel/XAPI functions on top of `platform/` | Shrinks into the game's platform code |
| `platform/` | Logging, paths, later window/input/audio/threads (SDL3) | Kept, portable |
| `render/` | Xbox Direct3D 8 replacement (D3D9/DXVK first, Vulkan later) | Kept, portable |
| `game/` | Decompiled game code and the `cod_bro` executable | Kept, portable |
| `assets/` | Game files in the final version (empty during development) | Kept, not in git |
| `tools/` | Kernel export generator, Ghidra scripts, data files | Kept |
| `third_party/xbox-includes/` | Xbox kernel/SDK headers (GPLv2, github.com/mborgerson/xbox-includes) | Reference |

Rules for portable code: no compiler extensions outside `platform/compiler.h`,
fixed-width types in structs that mirror Xbox memory (with `static_assert`s), and
32-bit Xbox pointers stored as `xbox::XbPtr`.

## Building

Fedora packages for the 32-bit loader build:

    sudo dnf install glibc-devel.i686 libstdc++-devel.i686

Then:

    cmake --preset linux-loader && cmake --build --preset linux-loader
    ./build/linux-loader/bin/cod_bro_loader               # add --debug or --trace

The `linux` and `windows` presets build only the portable code (`cod_bro`), to keep
both platforms compiling from the start.

## Dependencies

- Linux: `openal-soft-devel` and, for the loader, `openal-soft-devel.i686`
  (plus `glibc-devel.i686 libstdc++-devel.i686`).
- Windows: OpenAL Soft is built from source (CMake FetchContent).

## Assets

Builds use the release layout: executables in `build/<preset>/bin/`, next to an
`assets` link to the game files (`../unpacked` by default; CMake setting
`COD_BRO_GAME_FILES`). The game only reads there. What it writes (saves, the cache
drive) goes to the per-user data folder: `~/.local/share/cod_bro` (Linux),
`%APPDATA%\cod_bro` (Windows), or `--data DIR`. Override assets with `--assets DIR`
or `COD_BRO_ASSETS`. See `assets/README.md`.

## Symbols

Run `tools/ghidra/ExportSymbols.java` in Ghidra on `default.xbe` and save to
`tools/data/default_xbe_symbols.txt`; the loader uses it to name functions in
backtraces.

## Kernel exports

`xbox/kernel_exports.inc` is generated from `tools/data/xboxkrnl_ordinals.txt` and
`third_party/xbox-includes/include/xboxkrnl.h` (`--target gen_kernel_exports`). Each
export's calling convention and argument size come from the header.

## Status and next steps

- Loader builds and runs. Done: threads (`PsCreateSystemThread[Ex]`,
  `PsTerminateSystemThread`), handles (`NtClose`, `xbox/objects.cpp`), critical
  sections (`xbox/sync.cpp`, in place on the title's struct). First title patch
  (`loader/patches.cpp`): XAPI's GDT code-selector restriction is a no-op.
  `HalRegisterShutdownNotification` records registrations (not yet run).
  Dispatcher (`xbox/dispatcher.cpp`): waits (single/multiple, timeouts), events,
  mutants, timers, DPCs on a kernel thread, IRQL (DISPATCH_LEVEL = lock shared with
  DPCs). Not yet: named objects, semaphores.
  Settings (`xbox/settings.cpp`): a full default EEPROM image (NTSC, English);
  `XboxHDKey` is a fixed port key, so saves from a real console won't verify.
  Memory (`xbox/memory.cpp`, host side `platform/vmem*`): NT-style virtual memory at
  0x10000000-0x50000000, contiguous memory at 0x80000000 + physical, and one
  physical page budget (`--memory MB`, default 64 like a retail Xbox) shared by the
  image, contiguous and committed memory. Our own renderer/audio code is outside it.
  Files (`xbox/filesystem.cpp`, host side `platform/file*`): symbolic links,
  `\Device\CdRom0` = assets (read-only), partition 1 (E:/T:/U:) and the cache (Z:)
  in the user data folder, case-insensitive paths. Patch: `XMountUtilityDrive`
  links Z: to the cache folder (no raw partition table / FATX format).
  XAPI init completes; the game allocates its pools and starts DirectSound.
  DirectSound is replaced (`audio/`, on OpenAL Soft): the loader redirects the 53
  DSOUND functions the game calls, found by name in the Ghidra symbols. Buffers play
  from title memory with play/loop regions, streams take packets through the
  XMediaObject vtable, 3D uses the game's rolloff curve, HRTF on headphones.
  `--silent` plays into a silent loopback device.
  Current stop: D3D (`FUN_003eef95` reads NV2A register 0xFD001804) - graphics.
  User APCs (`ReadFileEx`/`WriteFileEx` completions) run at the thread's next
  alertable user-mode wait; file handles are FILE_OBJECTs that
  `ObReferenceObjectByHandle`/`IoQueryVolumeInformation` work on.
- Later, once the game runs: raise the budget and enlarge the game's memory pools
  (`FUN_00043fb0` allocates fixed 35.2 MB + 3.7 MB arenas) for bigger assets/mods.
- Threads run truly in parallel and ignore priorities (the Xbox is single-core with
  a priority scheduler) - watch for races that the original never had.
- **Resume here (2026-10-13): replace D3D (graphics)**, same procedure as DirectSound
  (skill: "Replacing an SDK library"). Agreed, not started:
  1. Inventory: `SdkCallUsage.java D3D,D3DX,XGRPH` (earlier count: 204 functions,
     84 unnamed) and read the game's callers.
  2. Conventions: `SdkConventions.java D3D,...` - D3D is LTCG, so expect register
     arguments and custom conventions; hooks may need per-function thunks.
  3. Test whether dxvk-native builds and runs as 32-bit (i686 toolchain, clear the
     screen). Works -> D3D9 via DXVK first; fails -> direct Vulkan (or SDL3 GPU).
     Either way `render/` = Xbox D3D8 front end (state, shaders, textures) + a
     replaceable back end.
  4. Propose the design to the user before implementing; then tests + commit.
- Audio, unverified until something audible runs: real in-game sound, ADPCM's 65th
  sample (dropped), media objects closing their file handle on release; envelopes and
  doppler are ignored.
- Keep going: run, implement the export it stops at, repeat.
- Route B focus: replace graphics (D3D8 -> D3D9/DXVK), sound, input, kernel and
  saves. Game code stays original; decompile only what route B needs (raw GPU
  command sites `BeginPush`/`RunPushBuffer`, PC bugs/timing, wanted features).

## Tests

`ctest --preset linux-loader` runs the kernel-layer tests in `tests/` (critical
sections, dispatcher, memory, file system, audio; the file tests build a fake disc in a
temp folder and the audio test mixes on an OpenAL loopback device, so neither needs
game files or a sound card). They need a 32-bit build because `xbox/` stores the title's
pointers as 32-bit `XbPtr`; making `xbox/` 64-bit-clean is part of route C.

## Planned: comparison harness

Automatic check that a decompiled function behaves like the original. Only needed
if/when we start large-scale decompilation (route C); route B's handful of
decompiled functions can be tested in-game. Roughly 500-800 lines in `loader/`.

**How it works.** When the game calls a function marked `compare`, the hook
`fork()`s. The child runs the *original* on a copy-on-write copy of memory and sends
back its return value, registers and changed pages through a pipe, then exits. The
parent runs the *new* version and compares. Changed pages are found with Linux
soft-dirty tracking (`/proc/self/clear_refs` + `/proc/self/pagemap`), so the harness
never needs to know what a function reads.

**Compared:** return value (EAX, EDX:EAX, or ST0 for floats); EBX/ESI/EDI/EBP
preserved; ESP after return (catches wrong calling conventions / pop sizes); every
memory byte either side changed.

**Known problems and handling:**
- Side effects outside memory (file I/O, waits, timers) inside the function: the
  kernel layer knows it is in the test child and marks the test *inconclusive*
  instead of failed. File reads matter most: the child shares file positions.
- Float differences (original uses x87): compare bytes exactly first, then retry
  mismatches as float/double with a small tolerance; report as "float drift".
- Other threads changing memory while the new version runs: confirm mismatches
  over several calls before trusting them.
- Cost: sample (first N calls, then 1 in K), only for `compare` functions.
- LTCG/custom register conventions: reuse the per-function register layout that
  the hooks need anyway.

**Workflow.** `tools/data/functions.csv` tracks each function:

    address   name                status    calls  mismatches
    0012a6c0  Cvar_Set            compare   312    0

Decompile a batch and mark it `compare` → play normally → functions with many
matching calls become `replaced`; mismatches are reported with the differing bytes.
Functions never called stay unverified (shows coverage gaps).

**Later, optional:** record a function's inputs during play and replay them offline
as unit tests, running the original under an emulator library (e.g. Unicorn) to
capture exactly what memory it reads.
