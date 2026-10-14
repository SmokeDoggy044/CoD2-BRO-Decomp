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
| `third_party/xbox-includes/` | Xbox kernel/SDK headers (GPL-2.0/CC0/LGPL-2.1 per file, github.com/mborgerson/xbox-includes) | Reference |
| `LICENSES/` | License texts and which applies where (the port's own license is still to be chosen) | - |

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
  Direct3D (`render/`, D3D9 on DXVK, design below): device, frames, states,
  shaders, textures captured; the intro movies play (the video player writes into
  the back buffer). Input (`input/`, SDL3): port 0 is always connected (keyboard and
  mouse plus the first PC gamepad), more gamepads take ports 1-3 with hot-plug; the
  keyboard layout is in `input/devices.h`. `--press start@20,a@25` scripts presses for
  unattended runs. Xbox Live (`online/`) is unavailable: no network, no accounts. Every SDK function in a replaced library's
  section that we do not replace traps by name (`loader/patches.cpp`).
  Title reboots (`XLaunchNewImage` -> `HalReturnToFirmware(QuickReboot)`): the game
  reboots into `default.xbe` between levels and when quitting to the menu, with its
  state in the LaunchDataPage; the loader re-execs itself with the page (memory file)
  and the same options (minus `--press`); log and `--diag` times/marks continue.
  Multiplayer (`codmp_xboxf.xbe`) is refused. XONLINE's launch hook is cleared.
  User APCs (`ReadFileEx`/`WriteFileEx` completions) run at the thread's next
  alertable user-mode wait; file handles are FILE_OBJECTs that
  `ObReferenceObjectByHandle`/`IoQueryVolumeInformation` work on.
- Polygon offset confirmed in mission 2 (playtest 2026-10-14): the barracks sign,
  floor and wall decals, maps and posters no longer z-fight. All marks of the
  2026-10-13 playtest are fixed. Mission 3 (tank level) played clean with F11 on: no
  untranslated features, no new warnings. Next: mission 4 onwards (`--launch-data 3
  3<DIR/launch_data.bin` repeats the last reboot), fix what the marks show.
- Later, once the game runs: raise the budget and enlarge the game's memory pools
  (`FUN_00043fb0` allocates fixed 35.2 MB + 3.7 MB arenas) for bigger assets/mods.
- Threads run truly in parallel and ignore priorities (the Xbox is single-core with
  a priority scheduler) - watch for races that the original never had.
- **D3D design** (agreed 2026-10-13: HLE to D3D9/DXVK). `render/` replaces the D3D8
  functions; the title's own D3D data stays authoritative where inlined code uses it:
  - Simple render states arrive as NV2A methods (`SetRenderState_Simple`), as do
    shader constants/programs and BeginPush/RunPushBuffer data: all go through
    `nv2a.cpp` (register file, 136 program slots, 192 constants, inline draws).
  - Deferred/complex render states and texture stage states live in the title's
    `D3D__RenderState` / `D3D_g_DeferredTextureState` arrays (XDK 5849 indexes in
    `xbox_d3d.h`); texture offset/format per stage in NV2A registers 0x1B00+.
  - Shaders: vertex programs are NV2A microcode (assembled at runtime by the game
    with XGAssembleShader), pixel shaders are D3DPIXELSHADERDEF register combiners.
  - Back end (`backend.h`): D3D9; dxvk-native has no HLSL compiler, so shaders
    must be emitted as D3D9 bytecode.
- **Draw path (2026-10-13): the menus render.** Inline primitives (BeginPush) go
  through `render/draw.cpp` (attributes -> float4, quads -> triangles) to
  `render/d3d9_draw.cpp`: NV2A vertex microcode -> vs_3_0 and register combiners ->
  ps_3_0 as hand-assembled bytecode (`d3d9_shader.cpp`), textures decoded by
  `texture_decode.cpp` (unswizzle, DXT, P8) and cached by address + content hash,
  raster state from the NV2A registers. The pixel shader is read from the NV2A
  combiner registers (SetPixelShader writes them, like D3D).
- **Resume here (2026-10-13): the first level renders.** Terrain, buildings, models,
  sky, smoke, the glow pass and the HUD draw (checked on screenshots of the first
  level only). Done: fog (deferred fog states -> NV2A fog registers as D3D does),
  render targets (D3D9 render target textures keyed by the surface's memory address;
  the back buffer is one, copied to the window at present; textures at that address
  sample it), texture modes as D3D picks them (PSTextureModes state,
  PS_GLOBALFLAGS_TEXMODE_ADJUST) and dependent reads (modes 15/16). Not yet: cube and
  volume textures, other texture modes (bump env, dot products), `RunPushBuffer`,
  fixed-function vertex (FVF) and texture stages (approximated as texture0 *
  diffuse), CPU reads of render targets, Clear with rectangles. Unverified: half-pixel
  offset, viewport-as-scissor, EXP/LOG precision. Next: play further in and fix what
  looks wrong (the user tests by playing now).
- Keyboard and mouse: see `input/devices.h` for the layout (CoD BRO's default pad
  layout from config/default.cfg, PC-style keys). Mouse look adds to cl.viewangles
  in a thunk before CL_MouseMove (`loader/patches.cpp`), because the Xbox build
  runs its right stick through the Quake 3 mouse path (stick acceleration, 370 deg/s
  limit). `--mouse-sensitivity` sets degrees per count (default 0.11). Click the
  window to capture the mouse; F10 or Alt+Tab frees it.
- Playtesting: `--diag DIR [S]` writes DIR/log.txt, a screenshot every S seconds
  (auto_<seconds>.bmp, default 10) and on F12 mark_<n>.bmp, then freezes the game
  (sound paused) for a one-line note (log + marks.txt), then logs the next frame's
  draw dump ("MARK n"). A corner overlay (not in screenshots) counts the marks.
  Automatic marks: each new warning or error once frames are shown (minus known
  harmless ones) gets automark_<n>.bmp, an "A<n>" line in marks.txt (once per
  folder) and a draw dump. F11 draws whatever needs an untranslated feature (texture
  mode, cube/volume texture, texture format) in magenta; the overlay counts those
  draws and the ones not drawn at all (FVF) per frame. On exit (window closed, crash,
  stop on an unimplemented function, Ctrl+C) DIR/summary.txt lists how the session
  ended and every distinct warning/error with count, first time and screenshot.
  Section loads (XeLoadSection) are logged at info.
- Debugging a wrong picture: `XB_DUMP_FRAME=n` logs every draw of frame n (shader
  keys, combiners, texture modes/formats, raster states, first vertex) plus each
  vertex program's microcode (decode with `tools/nv2a_vsdis.py`); `XB_DUMP_STOP=k`
  also drops that frame's draws after the k-th - with `--screenshots` on frame n-1,
  bisect for the draw that breaks it.
- Unattended level run: copy the save folder and use `--data COPY --press
  "a@4,start@5.5,a@7,start@8.5,a@10,start@11.5,a@15,a@18,a@21,a@24,a@27"` plus `a`
  every 4 s from 36 to 76 (profile -> Single Player -> New Game -> difficulty ->
  briefing "Press A to start", whose load time varies; A only jumps in the level).
  The level shows at ~60-70 s; standing still, the player dies at ~90 s.
- Timing: the title reads the CPU's time stamp counter (RDTSC, 27 sites incl. XAPI
  QueryPerformanceCounter) and assumes the Xbox's 733 MHz; the loader routes each
  site through a trampoline to `xbox::cpu_time_stamp_counter()` (loader/patches.cpp).
  Without it the game ran ~5x fast on a 3.6 GHz TSC. Unverified against real
  hardware: overall game speed (judged by eye).
- SEH (`loader/seh.cpp`): RtlRaiseException/RtlUnwind call the title's handlers on
  its own stack; the script VM's coroutines throw C++ exceptions in normal play.
- Audio plays (menus, music). Unverified: ADPCM's 65th sample (dropped); envelopes
  and doppler are ignored. File media objects do not close the title's handle (the
  game streams every sound from one handle to sp/global.cod).
- Saves: XAPI signs them with the kernel's SHA-1/HMAC (`xbox/crypto.cpp`; the
  signature key is the zero-filled data export, so saves only verify on this port).
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
