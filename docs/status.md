# Status and next steps

## Current state (2026-10-16)

- **Before the prototype release (weekend of 2026-10-17/18):**
  1. Fullscreen: check Alt+Enter and a resized window (black bars, no stretching).
  (License chosen 2026-10-16: GPL-2.0-or-later.)
- **After the release:**
  - XFONT (the XOnline sign-in UI's text) writes through the frame buffer's tiled
    view (address | 0xF0000000), which is not mapped; only reachable through Sign In,
    which is cut off now.
  - Windows: the loader runs under Wine (2026-10-16: menus, mission 1, quit-to-menu
    reboot). cod_bro_loader.exe is a stub at 0x10000 that holds the Xbox ranges
    and loads cod_bro_loader_core.dll; FS stays the TEB (StackBase/StackLimit
    switched to the Xbox stack, 36 KPCR-field reads patched to TEB TLS slots).
    Next: a run on real Windows (address-space layout and SEH differ from Wine),
    then a Windows release package. Untested: non-ASCII paths (narrow strings use
    the ANSI code page), the console window the stub opens when started from the
    launcher.
  - In-game brightness: `SetGammaRamp` is ignored, so the game's brightness
    setting does nothing (apply the ramp in the final pass).
  - Mission 6 (Gela) ran at ~20 fps for ~12 s (60-71 s into `--level 6`, after the
    landing-craft cinematic; playtest 2026-10-16): check whether the game or the
    renderer is the bottleneck there.
  - Mission 6+ playtests; death/mission-failed screens and dialogs in widescreen.
  - Launcher: a .desktop file / icon for Linux desktops.
- Aim assist (2026-10-16): `--aim-assist on|off`, launcher checkbox (default on).
  Off patches the sticky aim function (FUN_002db7d0) to clear its target and
  return; confirmed by the user (moving enemies no longer tracked).
- Water (2026-10-16): vertex program EXP/LOG now follow the Xbox (vs_1_1 expp/logp:
  EXP = 2^floor, frac, 2^s, 1). The water's wave program uses EXP.y as its phase;
  with D3D9's full-precision exp the sea was flat murky fog. Confirmed by the user in
  mission 6.
- Fullscreen (2026-10-16): `--fullscreen`/`--windowed`, `fullscreen = true` in
  settings.ini, "Display mode" in the launcher, Alt+Enter while playing. Borderless at
  the desktop's mode; the swap chain is the largest display's size and the window
  shows its corner (Present source rect), so no device Reset. Render scale defaults
  to the window's/screen's height (2.25 at 1080p). Fullscreen start confirmed by the
  user; Alt+Enter and resizing not yet tried.
- Multiplayer and Xbox Live (2026-10-16): the Multiplayer item (console command
  `load_mp_exe`) and Sign In (main menu, Xbox Live Options) keep the player in the
  menu with a 4 s notice over the game (`render::show_notice`); the status line says
  "Not signed in". Confirmed by the user.
- Sound through the release's bundled OpenAL Soft confirmed by the user
  (2026-10-16).
- Launcher (2026-10-16): `cod_bro_launcher` (Dear ImGui on SDL3) sets the game-file
  folder and the player options, saved as `settings.ini` (see the README, Running),
  and starts the game; the game starts it when it finds no game files.
- Release (2026-10-16): `packaging/sniper/build.sh` (README, Releasing) builds
  cod_bro-0.1.0-linux.tar.gz in the Steam Runtime sniper SDK: glibc 2.30+, bundles
  SDL3/OpenAL Soft/DXVK built from source. Checked: starts on bare 32-bit Debian
  11; plays on Fedora with its own libraries (mission 1, same frames as the dev
  build). Sound confirmed by the user.
- Missions 1-5 played through or started clean under `--diag` (see
  [playtesting.md](playtesting.md)). Mission 2 confirmed polygon offset: the barracks
  sign, floor and wall decals, maps and posters no longer z-fight. Mission 3 (tank
  level) and mission 4 (19 min, through the menus with the profile) played clean with
  F11 on: no untranslated features, no new warnings. Mission 5 (Liberator, 15 and
  16 min) clean, 30 fps; one note from it is open: a dead gunner's body sank through
  the floor grate of the turret well (likely the original's, not checked).
- Mission 5 frame pacing: frames that missed a vblank (50 ms) clustered in heavy
  fights (the town battle at the end, ~25 in 25 s). Likely cause, fixed: a vsync'd
  Present waited on the display's clock on top of the title's wait for our vblank;
  Present is now immediate + DXVK tearFree (mailbox) and the vblank runs at the
  display's rate. Not yet confirmed in that fight.
- No game file has a cube or volume texture (all 9,652 DDS in the .cod packs and the
  229 loose ones are 2D; the DDS loader `0x6b09e` sets the cube flag only from caps2,
  and the code-built normalization cube `nglNormalCubeMapTex`, `FUN_0006f080`, is
  never called), so those stay unbuilt until a run hits one.
- Route B focus: replace graphics (D3D8 -> D3D9/DXVK), sound, input, kernel and
  saves. Game code stays original; decompile only what route B needs (raw GPU
  command sites `BeginPush`/`RunPushBuffer`, PC bugs/timing, wanted features).
  Otherwise: run, implement what it stops at, repeat.

## Kernel and XAPI (`xbox/`)

- Threads (`PsCreateSystemThread[Ex]`, `PsTerminateSystemThread`), handles
  (`NtClose`, `xbox/objects.cpp`), critical sections (`xbox/sync.cpp`, in place on
  the title's struct). `HalRegisterShutdownNotification` records registrations (not
  yet run).
- Dispatcher (`xbox/dispatcher.cpp`): waits (single/multiple, timeouts), events,
  mutants, timers, DPCs on a kernel thread, IRQL (DISPATCH_LEVEL = lock shared with
  DPCs). User APCs (`ReadFileEx`/`WriteFileEx` completions) run at the thread's next
  alertable user-mode wait. Not yet: named objects, semaphores.
- Settings (`xbox/settings.cpp`): a full default EEPROM image (NTSC, English);
  `XboxHDKey` is a fixed port key, so saves from a real console won't verify.
- Memory (`xbox/kernel_memory.cpp`, host side `platform/vmem*`): NT-style virtual memory at
  0x10000000-0x50000000, contiguous memory at 0x80000000 + physical, and one physical
  page budget (`--memory MB`, default 64 like a retail Xbox) shared by the image,
  contiguous and committed memory. Our own renderer/audio code is outside it.
- Files (`xbox/filesystem.cpp`, host side `platform/file*`): symbolic links,
  `\Device\CdRom0` = assets (read-only), partition 1 (E:/T:/U:) and the cache (Z:) in
  the user data folder, case-insensitive paths. File handles are FILE_OBJECTs that
  `ObReferenceObjectByHandle`/`IoQueryVolumeInformation` work on.
- Saves: XAPI signs them with the kernel's SHA-1/HMAC (`xbox/crypto.cpp`; the
  signature key is the zero-filled data export, so saves only verify on this port).

## Loader and title patches (`loader/`)

- XAPI's GDT code-selector restriction is a no-op; `XMountUtilityDrive` links Z: to
  the cache folder (no raw partition table / FATX format).
- Every SDK function in a replaced library's section that we do not replace traps by
  name (`loader/patches.cpp`).
- Title reboots (`XLaunchNewImage` -> `HalReturnToFirmware(QuickReboot)`): the game
  reboots into `default.xbe` between levels and when quitting to the menu, with its
  state in the LaunchDataPage; the loader re-execs itself with the page (memory file)
  and the same options (minus `--press`); log and `--diag` times/marks continue.
  `--launch-data 3 3<DIR/launch_data.bin` repeats the last reboot. Multiplayer
  (`codmp_xboxf.xbe`) is refused (its command `load_mp_exe` shows a notice instead);
  XONLINE's launch hook is cleared.
- Timing: the title reads the CPU's time stamp counter (RDTSC, 27 sites incl. XAPI
  QueryPerformanceCounter) and assumes the Xbox's 733 MHz; each site goes through a
  trampoline to `xbox::cpu_time_stamp_counter()`. Without it the game ran ~5x fast on
  a 3.6 GHz TSC. Unverified against real hardware: overall game speed (judged by eye).
- SEH (`loader/seh.cpp`): RtlRaiseException/RtlUnwind call the title's handlers on
  its own stack; the script VM's coroutines throw C++ exceptions in normal play.
- Threads run truly in parallel and ignore priorities (the Xbox is single-core with a
  priority scheduler) - watch for races that the original never had.

## Audio (`audio/`)

DirectSound is replaced on OpenAL Soft: the loader redirects the 53 DSOUND functions
the game calls, found by name in the Ghidra symbols. Buffers play from title memory
with play/loop regions, streams take packets through the XMediaObject vtable, 3D uses
the game's rolloff curve, HRTF on headphones. `--silent` plays into a silent loopback
device. Unverified: ADPCM's 65th sample (dropped); envelopes and doppler are ignored.
File media objects do not close the title's handle (the game streams every sound from
one handle to sp/global.cod).

## Input (`input/`)

SDL3. Port 0 is always connected (keyboard and mouse plus the first PC gamepad); more
gamepads take ports 1-3 with hot-plug. The keyboard layout is in `input/devices.h`
(CoD BRO's default pad layout from config/default.cfg, PC-style keys). The action
keys follow the profile's controller style (style_a..l.cfg rebind the pad):
`loader/patches.cpp` moves each to the button bound to the same command and logs the
pad's bindings. Mouse look adds to cl.viewangles in a thunk before CL_MouseMove,
because the Xbox build runs its right stick through the Quake 3 mouse path (stick
acceleration, 370 deg/s limit). `--mouse-sensitivity` sets degrees per count (default
0.11). `--aim-assist off` turns off sticky aim (the game has no setting for it).

## Rendering (`render/`)

See [rendering.md](rendering.md).

## Online (`online/`)

Xbox Live is unavailable: no network, no accounts.

## Later

- Raise the memory budget and enlarge the game's memory pools (`FUN_00043fb0`
  allocates fixed 35.2 MB + 3.7 MB arenas) for bigger assets/mods.
- Route C: decompile the rest; see [comparison-harness.md](comparison-harness.md).
