# Status and next steps

**Decompilation fork (2026-10-17):** this repository (CoD2-BRO-Decomp, local
`cod_bro/decomp/`) is the native loader's fork for the decompilation; the loader
repo (`upstream`) stays as it is. Merge loader fixes with `git pull upstream main`.
Done: the [comparison harness](comparison-harness.md) (`--compare`,
`tools/data/functions.csv`), the `game/code/` library (libcod_bro_game.so in Linux
loader builds) and the level-script string class (game/code/script_string.cpp,
0x0011d540-0x0011db70): 11 functions, 8 matched in every test over missions 1 and 4
(57 to 9,669 tests each); Find, FindLast and SetChar are not reached from `--level`
runs (only a front-end text dialog calls them). Next:
1. Name engine functions after their Quake 3 / RTCW counterparts (Ghidra): in
   progress, 1085 functions + 560 globals so far - workflow, findings and next steps in
   [naming.md](naming.md). Decision (user, 2026-10-17): keep naming before step 2,
   including CoD-specific code (AI, script, anim trees, vehicles) named from its
   bodies, so the decompilation and --compare work covers both CoD-specific and
   Q3-derived functions. Order (user, 2026-10-17): module by module - the game
   module is done except the AI; then the cgame (CG_DrawActiveFrame), then client/UI.
2. Decompile in batches by subsystem (leaf functions first), `--compare` runs per
   batch. Decision (user, 2026-10-18): matched functions stay `Compare` until their
   whole module is finished, then the module becomes `Replaced` at once; naming
   stays the main work until then.
3. Stand-ins for common pure kernel calls in test processes (critical sections), so
   more functions become testable.
4. The level sections (~5,000 compiler-generated script functions): a translator,
   not function-by-function decompilation.

**Resume here (decomp, 2026-10-18, end of session):** naming is step 1 above
([naming.md](naming.md), "Next"). The game module is named, AI included (a
handful of low-value AI helpers stay open, listed in naming.md); next is the
cgame from CG_DrawActiveFrame (done except ~40 low-value leftovers) and the XAnim
tree module (core named); the client/UI is in progress (Q3 client paths, CL_Frame's
per-frame subsystems, the front-end panel/screen setup named; next: the screen classes'
vtables, memory-unit dialogs, the streaming loader - see naming.md). Names are neutral and
descriptive (user, 2026-10-18): no identifiers from later titles of the series.
**Next module (user decision 2026-10-18): the renderer**, ahead of the rest of the
client/UI. There are requests for a recomp, and the renderer is needed for the decomp
anyway: it is the game code tied to the hardware (NV2A commands and inlined D3D, now
translated by render/), so a decompiled renderer that calls a PC graphics API serves
both. Scope: the NGL scene code (~0x0005d000-0x00066000), the RE_* refexport side and the
inlined render-state code. Start: measure its size and list its entry points, then
name -> decompile -> --compare (screenshots), replaced as one module.
Started 2026-10-18: the tr_* layer, NGL (frame, scenes, quads, meshes, textures, fonts,
states), the 23 shader classes, lighting, models, effects, NGL's matrices, fog pass, light sets and
mesh builder are named (third pass 2026-10-18); open items in naming.md (Renderer).
Decompiling began 2026-10-18: game/code/ngl.cpp (quad setters and rotate/scale, scene
setters, matrix builders, nglDepthToZValue, light sets, frame allocators, texture helpers,
mesh-builder bounds; second batch: scene depth/time setters, pre-swap/end-frame
callbacks, the mesh (file) manager setters/getters and mesh walk, mesh relocation,
shader-handle and index-buffer helpers, nglEvalPointLight, the draw lists' std::sort
helpers), game/code/tr_main.cpp (myGLMultMatrix, RE_ClearScene, R_ClearFrameLists, far
cull, GLimp_Init, Q_PrintStrlen, RE_LoadWorldMap, R_AddViewModelRefEntity, R_CullPortal,
R_SetupProjection, R_CopyFrustumPlanes, R_ClipPolygonToPlane), game/code/dobj.cpp,
game/code/fx.cpp and game/code/unzip.cpp (inflate_trees_fixed); 86 functions matched,
none mismatched. Third batch (2026-10-18): nglSkipList_Find, nglMeshFileFindMesh,
nglVideoMode_Init/Copy, nglResetFrameStats, nglTextureSetPixel, nglSetTextureAddressMode,
nglLightSetAddProjector, nglGetTargetSampleScale, nglInitShaderConstantTable,
R_AddCellEntities, FX_FindEvictableEffect, XModel_GetBoneCount, nglSelectMeshLod,
nglScaleMeshMatrix; 101 matched in all. Fourth batch (2026-10-18): the big leaves -
DObj_TraceBoneBoxes (0x0023a590, bullet traces against bone hit boxes; its sqrt test
runs on the x87: platform_x87_sub_sqrt_ge), nglBuildProjectorPlanes (0x00070b00),
nglMatrixSceneClipToScreen (packed DIVPS lanes kept for MXCSR's invalid flag:
platform_keep), inflate_fast; 103 matched in all.
Fifth batch (2026-10-18), the first callers: the scene camera/viewport setters
(nglSetSceneViewportRect, nglSetViewportRect, nglSetAspect/Perspective/Orthographic,
nglSetViewMatrix, nglSetCameraMatrix, nglGetViewMatrix), nglListAddQuad,
nglAddDirectionalLight, nglEvalFirstObjectLight, R_SetupFog, RE_SetColor (MSVC _ftol:
platform_ftol), RE_StretchPic, RE_DrawQuadPic, RE_TextHeight; title functions not
decompiled yet are called through `title_function<F>(address)` (title_data.h), and a
hooked function reached inside a test process now runs its original straight from the
stub. 123 matched in all (recounted from functions.csv). Not reached: nglGetMatrixCopy,
nglProjectPointCopy, nglEvalLight, R_DrawPic.
Sixth batch (2026-10-18): nglSetViewportNormalized, nglGetMatrix, nglProjectPoint
(all four DIVPS/MULPS lanes kept), nglInitScene, nglPushScene (scene tree links +0x2d4..
+0x2dc, slot block +0x3d8), nglSphereInFrustum (0x00029200, newly named), nglAddPointLight,
R_ClearAlpha; 129 matched in all. nglProjectPoint (and the *Copy wrappers) not reached
by --level 1/4; R_ClearAlpha always inconclusive (its last scene's nglSetDefaultSceneParams
calls the replaced nglGetBackBufferTexture - host code); nglAddPointLight 23 of 23 so far.
nglUpdateScene (2026-10-18, ~1,900 instructions): viewport mapping, the field of view's
packed sine polynomial (cos/sin of half the angle), projection = base * viewport *
target scale, the camera (rigid inverse of the view matrix), view * projection,
clip-to-screen, the six world-space frustum planes (+0x270; x87 1/sqrt(v*v+1):
platform_x87_rcp_sqrt_sq_plus_one) and screen-to-target; 280 matched in missions 1 and 4,
none mismatched (130 matched in all). std::fabs is out of line at -O0 (the loader's copy
runs: "called host code") - use abs_bits.
Seventh batch (2026-10-18): the target scale matrix (nglMatrixTargetScale 0x00073ce0,
nglGetSceneTargetScale 0x00073e90; nglUpdateScene now calls ours, re-checked with
--compare-all: 1,229 matched), nglAddShadowProjector (0x000704f0, RCPPS + Newton step:
platform_rcp_estimate), nglNode_ViewDepth, nglNode_CalcWorldMatrix, nglListAddNode (scene
+0x304..+0x310 opaque/translucent lists), nglMaterial_Relocate/Relocate2 and the resource
managers' find/add-ref/release wrappers (vtable slots 2/7/8); 143 matched in all.
nglReleaseMeshFile is always inconclusive (the release reaches the replaced D3D code),
nglFindFontAddRef not reached.
Eighth batch (2026-10-18): nglListAddMesh (0x00068850; the per-instance NglMeshDrawNode in
scene memory: world matrix, world * view_projection transposed, LOD, frustum cull, each
section's shader slot 2), nglComputeObjectLights (0x00070200, four object lights in object
space + colours + ambient), nglNode_GetWorldInverse (0x00049750, formerly misnamed
GetWorldViewProj: the cached rigid inverse), nglSetDecalZBias (x87 unsigned-to-float:
platform_x87_unsigned_to_float; inconclusive whenever it sends a render state, which
reaches the replaced SetRenderState_Simple), nglMaterialFindTexture(OrDefault) and the
eight nglMaterial_* load/release slots; 154 matched in all. Not tested yet:
nglMaterial_LoadTextureRef, nglMaterial_ReleaseTwoTextures(Clear) (no tested calls in
missions 1/4). functions.csv is now written under a lock with a per-process temporary
file, so --compare runs of several missions can run at once (a shared .tmp had
interleaved two runs' writes).
Not reached by --level 1/2/4 runs: nglColorToVec4, nglPaletteSetEntry, RE_GetFarCullDist,
Q_PrintStrlen, nglScaleQuadVerts, nglRotateQuad, std_PushHeap_* (heap sort fallback
only), nglIndexBuffer_Clear, inflate_trees_fixed, FX_FindEvictableEffect (pool never
full), XModel_GetBoneCount. x87 sin/cos/tan go through platform_x87_* and RSQRTSS through
platform_rsqrt_estimate (compiler.h). `--compare` runs without the PC features
(`--pc-patches off`). Skipped on purpose:
nglDynamicVB_EndFrame (tail calls into undecompiled code), DObj_AnyModelHasFlags (flags
in EDI - needs an asm entry shim; also R_AddCellEntityLink, 0x0023b2e0 in ESI),
nglSelectMeshLightSet, 0x0023be90 and huft_build (paths copy uninitialized stack
slots; huft_build also takes EAX/ECX), nglWaitFrameInterval (busy-waits on the vblank
thread's counter),
0x00235890 (radius in XMM2), R_LayoutTextWithIcons (colour out-pointer in EBX), nglResourceManager_Find (its tail JMP sits in the first 5
bytes), 1-4 byte functions (too short for the harness's jump).
Ninth batch (2026-10-19): the shader classes' small slots, game/code/shaders.cpp - Toggle,
Init, GetName and AddToList of all 22 classes (AddToList: a 0x18-byte NglMeshNode in scene
memory onto the opaque list keyed by the class id, or the translucent list by
nglNode_ViewDepth; decal ids | bit 31, water/ocean/river fixed keys, sky 1000 - layer,
cdWorld/cdWorldBlend cull the section's sphere first), Register's 15 shader-table loops
(newly named cd*_InitVertex/PixelShaders) and cdWorldBlend_Load/ReleaseTextures,
cdGlass_LoadTextures (107 functions). Missions 1-5: 26 tested, all matched (181 matched in
all). The Init functions cannot be hooked (CALL in the first 5 bytes; startup only); the
other startup calls (GetName, the loops) mostly hit "all busy" in the startup burst; Toggle
only runs from nglToggleShaderByName. Not reached in missions 1-5: the AddToList of cdGun,
cdDebug, cdRiver, cdOcean, cdWater, cdWorldVertexLit.
Not reached by --level 1/4 runs (zlib unused there): inflate_fast. Left among the leaves:
nglMatrixOrthoOffCenter, 0x000683a0 and 0x0006fb30 (no callers), nglInitRenderStates
(tail call). Next batch: more callers, bottom-up: nglListAddMesh's callers (R_AddStaticModel
0x002438b0 - EAX argument, calls R_ApplyCachedLighting (ESI) - and the other R_* model
adders; the debug shape draws 0x0006cb80/cc50/cd30 are reached only from nglDebugDrawLine/Axes),
the shader classes' Register and Draw slots, the tr_* callers of R_SetupProjection;
nglSetTransformConstants (0x000776f0) ends in the replaced SetVertexShaderConstant; nglSetDefaultSceneParams (0x00062a70) stays untestable while it calls
the replaced nglGetBackBufferTexture. ESI-argument functions (R_AddRefEntityToScene,
R_ApplyCachedLighting) need an asm entry shim. Needs an asm
shim: R_SetupEntityOrientation (EAX/ECX/EDX in, calls R_RotateForEntity with ESI/EDI),
R_DrawFilledRect (colour pointer in ESI).
Each batch: CSV rows -> ApplyNames -> save -> ExportSymbols ->
tools/smoke_test.sh -> commit. User decision 2026-10-17: decompilation only, no
static recompilation.

**Resume here (loader, 2026-10-17, end of session):** v0.1.1 is out (private pre-release,
Linux + Windows): 60 fps by default and system-link multiplayer from the campaign's
Multiplayer item or the launcher's button, with mouse look, style remap, 60 fps,
widescreen menus and aim assist in MP too (user-tested on one PC; design and open
items in [multiplayer.md](multiplayer.md)). Since then: v0.1.2 (private test pre-release)
adds sharper text (all four game fonts redrawn from bundled OFL fonts; launcher
toggle, default on); not yet user-playtested. Next candidates:
1. Multiplayer between two PCs, and v0.1.1 on real Windows (firewall prompt).
2. The full native build plan + a list of every game-code patch and its intent (asked for).
3. Heavy fights at 60 fps (mission 5 town), missions 7-13 playtests.
4. MP: split screen (if any), the ~40 startup memory warnings. One release-build MP
   sample read 30 fps in-match once (not reproduced; log not kept).

## Current state (2026-10-16)

- Sharper text (2026-10-17): `--sharp-text on|off` (default on; launcher "Sharper
  text"). The game's text is four bitmap fonts, each a DDS atlas + a plain-text
  `.fdf` glyph table in the packs (sp/global.cod: garamond 512x256 DXT1,
  GemFontOne 256x256 DXT5, arialblack_14 256x256 DXT1 (a bold Times); sp/frontend.cod:
  impact 256x256 DXT1 with mipmaps; mp/ has identical copies). All are white with
  on/off alpha; all but impact have a 1-pixel dark shadow to the lower right inside
  the glyph box. `text/` reads the packs (format in text/kapf.h), redraws each atlas
  at max(2, ceil(render scale)) from the OFL stand-ins in fonts/, every glyph scaled
  into its original ink box (the game's spacing stays), and the renderer swaps it in
  by content hash (BackendConfig::replace_texture). GARA.TTF is a regular weight and
  only feeds XFONT (Xbox Live UI), so it is not used. Checked in screenshots at
  render scale 3: menus, dialogs, mission title cards, briefing hints; all four fonts
  are replaced in a menu-to-mission run (Linux, and Wine for the Windows build).
  Not swapped: the button icon fonts, the legal screen (an image).

- **Prototype release:** private GitHub pre-release v0.1.0 (2026-10-16), Linux
  tar.gz + Windows zip; license GPL-2.0-or-later. Before going public:
  1. ~~Fullscreen: check Alt+Enter and a resized window.~~ Both work (user, 2026-10-16).
  2. ~~A run of the Windows zip on real Windows.~~ Runs (2026-10-16, RTX 3080).
- **After the release:**
  - XFONT (the XOnline sign-in UI's text) writes through the frame buffer's tiled
    view (address | 0xF0000000), which is not mapped; only reachable through Sign In,
    which is cut off now.
  - Windows (2026-10-16): runs on real Windows (RTX 3080; Alt+Enter and resizing
    confirmed) and under Wine. cod_bro_loader.exe is a stub at 0x10000 that holds
    the Xbox ranges and loads cod_bro_loader_core.dll; FS stays the TEB
    (StackBase/StackLimit switched to the Xbox stack, 36 KPCR-field reads patched to
    TEB TLS slots); `os::keep_executable_headers` keeps valid PE headers at 0x10000
    (real Windows reads them for CreateThread's stack sizes; Wine does not). D3D9 is
    loaded at run time: bundled DXVK (`dxvk/d3d9.dll`) first, the system's as the
    fallback (`--d3d9`). Every run writes `log.txt` in the user data folder; error
    boxes on startup failures, crashes and uncaught C++ exceptions. Untested:
    non-ASCII paths (narrow strings use the ANSI code page).
  - Mission 6+ playtests; death/mission-failed screens and dialogs in widescreen.
  - Launcher: a .desktop file / icon for Linux desktops.
- Mission 6 (Gela) slowdown (2026-10-17): ~20 fps for ~13 s in the opening
  landing-craft cutscene, only in the Debug build (`linux-loader`, -O0) that the
  playtests used: the title's render thread was CPU-bound in our unoptimized draw
  translation (vertex fetch `draw_streams`, texture `hash_bytes`, std::fill, TLS
  lookups). The Release build (`linux-loader-release`, as players get) holds 30 fps
  there. Playtests now run the release build. Found with the sampling profiler
  (`XB_PROFILE=auto`, loader/profiler.h; see the README).
- Gamma and brightness (2026-10-17): the game sets one gamma ramp at device
  creation (FUN_00064bc0, static table 0x008a29b0: an S-curve, 0/128/255 -> 3/123/
  252) and never changes it; its GLimp_SetGamma-like wrapper 0x00064540 has no
  callers, and the menus have no brightness option (the Brightness00-10 widgets in
  frontend.cod have no code or text behind them). The ramp is now applied while
  copying to the window (ps_2_0 lookup pass, `D3D9Backend::draw_with_gamma`;
  checked pixel-exact against the table), so the picture has the Xbox's contrast.
  PC option on top: `--brightness X` (0.5-2, launcher slider), 255*(ramp/255)^(1/X).
  Screenshots (`--screenshots`, --diag) are taken before the ramp.
- Launcher (2026-10-17): "Reporting a problem" with Open log folder / Open save
  folder buttons, for players attaching log.txt and saves to issues.
- 60 fps (2026-10-17): `--fps 60|30`, launcher "Frame rate"; default 60 since the
  Gela playtest (2026-10-17). The title waits a number of vblanks per frame (0x008a2cb0), set only
  by FUN_000645c0(mode) - always mode 2 (startup x2, movie player); the patch
  replaces it so mode 2 means 1 vblank. Game time is clock-based (Com_Frame msec
  from Com_EventLoop): its accumulated time at 0x008ddaa8 runs at 1.000x wall time
  at both rates (Gela, 3 x 20 s). Gela release build: 59.7 fps average, 2 hitches
  in 120 s (30 fps: 10). 1/30 s constants checked: FUN_0032d890 (a fade loop, 2
  frames per 1/30 s step -> half as long at 60), FUN_00094610 (time-based update
  intervals), FUN_0022c910 (initial value, overwritten with the real time). Movies
  (.xbv) show one frame per game frame (2x speed at 60, user playtest): frames with
  a movie frame (decoder vtable 0x007cf390 slot 2) wait 2 vblanks, chosen per frame
  where the wait is set up (jumps to 0x00064370). Gela's briefing: 24 s at 30 fps,
  then 60. Tracers starting beside the boat gun: the same at 30 fps (original).
  Gela gameplay at 60 confirmed by the user (normal speed). Open: heavy fights
  (mission 5 town) at 60; per-frame tuning in later missions.
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
  to the window's/screen's height (2.25 at 1080p). Fullscreen start, Alt+Enter and
  resizing confirmed by the user.
- Multiplayer and Xbox Live (2026-10-17): the Multiplayer item (console command
  `load_mp_exe`) reboots into codmp_xboxf.xbe, and its "Back to Single Player" into
  default.xbe (checked both ways). Sign In (SP main menu, Xbox Live Options) and MP's
  Play Xbox Live keep the player in the menu with a 4 s notice over the game
  (`render::show_notice`); the status line says "Not signed in". See
  [multiplayer.md](multiplayer.md).
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
- Native loader focus: replace graphics (D3D8 -> D3D9/DXVK), sound, input, kernel and
  saves. Game code stays original; decompile only what the loader needs (raw GPU
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
  `--launch-data 3 3<DIR/launch_data.bin` repeats the last reboot. Reboots between
  default.xbe and codmp_xboxf.xbe switch the title setup (addresses, symbols);
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

- Multiplayer (outline, unverified): system link needs no server software (a
  player's game hosts, Q3-style listen server). Steps: run codmp_xboxf.xbe (own
  symbols/patches), replace XNet with host UDP sockets, LAN discovery + join by IP;
  an optional lobby server later. First: SdkCallUsage on its XNET/XONLINE calls,
  dedicated-server and split-screen code present or not.

- Raise the memory budget and enlarge the game's memory pools (`FUN_00043fb0`
  allocates fixed 35.2 MB + 3.7 MB arenas) for bigger assets/mods.
- Full native build: decompile the rest; see [comparison-harness.md](comparison-harness.md).
