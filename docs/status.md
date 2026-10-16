# Status and next steps

## Current state (2026-10-16)

- **Next:** fullscreen/window options (add them to the launcher too); mission 6
  playtest; check the death/mission failed screens and dialogs in widescreen.
- Launcher (2026-10-16): `cod_bro_launcher` (Dear ImGui on SDL3) sets the game-file
  folder and the player options, saved as `settings.ini` (see the README, Running),
  and starts the game; the game starts it when it finds no game files. Not yet
  tried by hand (Browse dialog, Play). Missing for a release: install/package rules
  (only the executables + `assets/README.md`) and shipping the 32-bit libraries the
  loader needs on Linux (SDL3, dxvk-native, OpenAL Soft, fmt, libdisplay-info).
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
- Memory (`xbox/memory.cpp`, host side `platform/vmem*`): NT-style virtual memory at
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
  (`codmp_xboxf.xbe`) is refused; XONLINE's launch hook is cleared.
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
0.11).

## Rendering (`render/`)

See [rendering.md](rendering.md).

## Online (`online/`)

Xbox Live is unavailable: no network, no accounts.

## Later

- Raise the memory budget and enlarge the game's memory pools (`FUN_00043fb0`
  allocates fixed 35.2 MB + 3.7 MB arenas) for bigger assets/mods.
- Route C: decompile the rest; see [comparison-harness.md](comparison-harness.md).
