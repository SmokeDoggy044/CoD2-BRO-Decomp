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
   stays the main work until then. Decision (user, 2026-10-23): a module is finished
   only when no function is left unnamed and every function is sorted into a module (modules.csv / progress.py);
   until then an unnamed or unsorted function may still belong to it, so no module is Replaced before that.
3. Stand-ins for common pure kernel calls in test processes (critical sections), so
   more functions become testable.
4. The level sections (~5,000 compiler-generated script functions): a translator,
   not function-by-function decompilation.

**Resume here (decomp, 2026-10-23, day 17): matched 1419 -> 1541 (main session + two subagents, all merged).**
Main session: CL_Frame and its per-frame callees, Key_TranslateButtonName / SCR_UpdateLoadingDots (C, not
reached in --level runs), Actor_PopBlockingStates fixed to take its state in ECX (was declared REGS2 = EDX; it
matched 114 tests anyway), Sentient_ClaimGoalNode (settled), multi_wait, Entity_YawToForward and
Phys_FindBodyForEntity (not reached), sound and streaming (paragraph below). Subagents: the collision
sight-trace tree and movement collision, the server module (linking, SV_Trace, per-frame server and send path), the
sound update leaves and game path-node/actor helpers (paragraphs below). Names of the day (~110) are in
manual_names.csv only - apply them in Ghidra (ApplyNames, save, ExportSymbols, smoke test) next session.
Next:
1. Harness: record the DirectSound calls (audio::dsound_functions(), like kRecordedD3dCalls; GetStatus needs a stand-in
   reading the replacement's buffer state). That makes CL_Frame, the sound update 0x000c3700 (SndVoice_ApplyParams,
   Snd_UpdateListenerAxis are C but untested), S_StopAllSounds, CL_ClearState and CL_MapLoading testable.
2. Game: Water_GetHeight (0x00044ee0, 1.2 KB packed SSE), BG_FindItem (0x002b6a70), PathNode_First/Next, further
   `rank.py game 1` rows.
3. Collision/physics leftovers: 0x0020da90, the 0x00231770 physics siblings, 0x002635c0; game rank.py rows.
4. On record (dead-stack words only): SV_SectorSightTrace_r, SV_ClipMoveToEntities_r, SV_CapsuleTrace, SV_Trace,
   the point/box sector walkers.

**Earlier (decomp, 2026-10-23, day 17, main session): CL_Frame and its per-frame callees, sound and streaming.**
- CL_Frame (0x002f2630) is C: gap entry (C frame 0x20000 below), every callee at the original's ESP with the
  original's registers (`call_at`), the original's saved EBX/ESI and the float time written into the dead frame; the
  screen update calls whatever the call at 0x002f27a4 targets (the loader's --level hook). Inconclusive only: the
  per-frame sound update 0x000c3700 -> 0x000c7730 calls the DirectSound replacement (19 setters such as
  IDirectSound_SetPosition/SynchPlayback, IDirectSoundBuffer_SetVolume/Play/StopEx, DirectSoundDoWork, and
  IDirectSoundBuffer_GetStatus, a getter that would need a stand-in reading the replacement's buffer state). Recording
  those (audio::dsound_functions(), like kRecordedD3dCalls) would make CL_Frame, S_StopAllChannels, S_StopAllSounds and
  the whole sound update testable.
- Harness: KeQuerySystemTime is a recorded kernel call (SdkStandIn::SystemTime: zero time on both sides), so the CRT
  _clock path (Snd_UpdateTime 0x000cb200, file request timers) no longer ends tests.
- Loader: the --level thunk restores the original `call 0x002f1e60` at 0x002f27a4 once the mission has started (tests
  of CL_Frame reached host code on every frame before).
- Matched (most settled): streaming view position (Stream_SetViewPosition, Stream_UpdateZoneDistances,
  Stream_SetZoneDistancesFromMask, Stream_PickVolumePoint, Stream_NearestPoint, Stream_PointInAnyBox,
  Stream_BoundsContainPoint, Zone_SetDistance_r, Pak_ResetZoneDistances), S_UpdateAmbientEmitters (jump table states
  0-4 at 0x002ea80c), S_UpdateCrossfade, SoundHandle_Resolve/Free, the voice pool (HandlePool_First/Next/Resolve,
  SndVoice_ForEach/ClearAttenuation/GetState/SetParam/SetOrigin/Resolve), S_StopChannel, S_StopSoundHandle,
  S_FindSoundByHash (the argument is a name pointer), SndGroup_FindByName, SndSource_CommitDirty, SndHw_ReleaseSlot,
  S_SetMasterAttenuation, Snd_GetAliasName, Zone_AddRef_r/Release_r/FindUnloadedChild/FindByHash,
  StreamMem_CountUsed/CountFree/GetBlock, G_EntityList_Compact, Subtitle_Draw.
- Register contracts: StreamMem_Alloc keeps XMM3 across StreamMem_CountFree (the entry saves it).
- C only: S_ClearVoiceAttenuation (0x000c53b0) and Snd_UpdateTime (CALL in the first 5 bytes). Not reached:
  FX_FindEffectById (the ambient replay path).
Next: DirectSound recorded calls (above); the rest of the sound update (0x000c4af0 voice parameter apply, 1.5 KB;
0x000c6080, 0x000c85c0); Zone_UpdatePriority (two-level jump table at 0x0028c8fc/0x0028c904); CL_UpdateRumble and
S_CheckWaveBankNotifies.

**Earlier (decomp, 2026-10-23, day 17, collision subagent; branch day17-collision, merged): matched 1419 -> 1441 on
its branch.** The sight-trace tree is C: CM_SightTrace (0x002e4c00, the root) with CM_ModelBrushesBlockSight,
CM_TempBoxBlocksSight, CM_CapsuleBlocksSight, CM_SphereBlocksSight and CM_VerticalCylinderBlocksSight (x87 nudge
(b * 0.125) / length, kept in asm); CM_TransformedSightTrace calls the C root and is Result::Eax. Also C: the
candidate-list traces under actor/player movement (CM_CandidateCapsuleTrace, CM_TraceThroughCandidates,
CM_TestInCandidates, CM_PushPointOutOfBrush, CM_PushSphereOutOfTriangle, G_PushCapsuleOutOfSolids), CM_RayHitsTriangle,
SV_EntityVisibleFromPoint (Q3's lastCluster off-by-one kept) and the G_SightTrace entity clips (SV_SightClipToEntity,
SV_BoxSightClipToEntity, SV_SectorBoxSightTrace_r, SV_EntitySightTrace); all settled (`--only --every 1`, missions
1/3/4/7). On record: SV_SectorSightTrace_r (0x002e8350) differs only in 1-2 dead-stack words (a value 0xb0 apart),
also with its callees at title addresses. Rarely reached (1-3 tests): CM_TraceRayThroughCandidates,
CM_CandidateRayTrace, G_RayTraceCachedCandidates; not reached: CM_RayHitsWorldMeshes (0x002d27c0). 0x002e80b0 and
0x002e88d0 were done on both branches; the merge kept the server versions.

**Earlier (decomp, 2026-10-23, day 17, server subagent; branch day17-server, merged): matched 1419 -> 1445 on its
branch.** sv.cpp covers entity linking (SV_LinkEntity with SV_LinkToSector, SV_SplitSector, SV_AllocSector; sectors
also hold static models at +0x14), the SV_Trace tree (SV_ClipToEntity, SV_ClipMoveToEntity, SV_PointTraceToEntity (DObj
hit boxes), SV_TraceStaticModels(_r) with SV_TraceStaticModel, SegmentMissesBounds, the box and point sector walkers,
SV_CapsuleTrace, SV_LocationalTrace), the per-frame server (SV_Frame, SV_PacketEvent, SV_ExecuteClientMessage,
SV_UserMove) and the send path (SV_SendMessages, SV_SendClientMessages, SV_BuildClientSnapshot,
SV_AddEntitiesVisibleFromPoint, SV_WriteSnapshotToClient, SV_UpdateServerCommandsToClient), SV_SetConfigstring,
SV_SetBrushModel. SV_PacketEvent, SV_ExecuteClientMessage, SV_UserMove and SV_Frame run their title callees at the
original's ESP (ClientThink stores a stack address at 0x00929b40). On record (mismatch only on trace words the code
never writes, stale stack in the original): the three sector walkers, SV_Trace, SV_CapsuleTrace; the box walker and
SV_CapsuleTrace also differ on fixed words 0x804fff2c and 0x81e23c5c/6c (possibly per-thread data below the Xbox
StackBase, written by title collision code - unconfirmed). SV_UpdateInfoConfigstrings is C only (unhookable).
SV_ClientCommand and SV_LocationalTrace not reached. XMM4 is a register input of SegmentMissesBounds: check the
title's xmodel caller 0x0023d070 before Replaced. Left in the module: load-time/menu functions only (SV_InitGameProgs,
SV_RestartGameProgs, SV_ShutdownServer, SV_SpawnServer, SV_Map_f, SV_ConnectionlessPacket).

**Earlier (decomp, 2026-10-23, day 17, server subagent, sound batch; merged): callees of the per-frame sound update
0x000c3700 (snd.cpp).** Settled: Snd_PopCount64, Snd_GetMixState, Snd_GetGroups, Snd_NormalizeVector (x87 length; ECX
restored), SndHw_AllocSlot, SndBank_UpdateLoads. Matching: SndBank_PollLoad (bank loads through File_QueueRequest),
Snd_InitRequest, SndVoice_ReleaseHwSlot. Not reached: SndBank_Free. C only and untested (CALL in the first 5 bytes, and
their caller 0x000c3700 ends in the DirectSound path): SndVoice_ApplyParams (0x000c4af0, 1.5 KB),
Snd_UpdateListenerAxis (0x000c85c0). Still title code: 0x000c8250, 0x000c8380, 0x000c8de0, 0x000c9db0, 0x000c91e0,
0x000c6fb0, the qsort comparator 0x000c5790.

**Earlier (decomp, 2026-10-23, day 17, server subagent, game batch; merged): ready rows from `rank.py game 1`.**
Settled: PathNode_Construct, Path_BaseName, ActorBlockList_RemoveEntity, PathNode_FromHandle / FromHandle2,
PathNode_IsAvailableFor, PathNode_CanSee, ActorMove_AddTouchEnt, AddLeanToPosition, G_RunThink (the think callback
runs at the original's ESP through a gap entry: a callback stores a stack address at 0x0098a4fc - 39 one-byte
mismatches before, 28545/0 after). Matching: ActorBlockList_Bump. Not reached: Scr_Atan2 (script table only),
PathNode_IsLinkInBadPlace. C only (CALL at byte 3): Actor_AnimStationary.

**Earlier (decomp, 2026-10-23, day 17, collision subagent, batches 2-3; merged): movement collision, game side.**
G_PushAwayFromEntities (0x002d5d50; compares p.x against the entity's x + 72 where a z test would be expected - kept),
G_SeparateFromSolids (0x002e6260, PmoveSingle's separation step), G_FilterCachedCandidates (0x00203f60),
CM_ClipSegmentToBrush (0x002cc0e0), CM_RayHitsListedObjects / CM_PushSphereOutOfListedObjects (lists 0x008fcee0 /
0x008fd3f0 / 0x008fd680), Phys_MoveAttachedPoint / Phys_UndoAttachedPushes (0x0002a340 / 0x0002ab10; the original adds a
never-written stack word to a w lane, the C adds 0 as tests see), CM_TraceSegmentThroughCandidates (0x002d2b50, vehicle
physics), Vec3_PitchDegrees (0x002a11f0: the title's own arcsine series, __CIsqrt by address, XMM0 result) - all
settled. Next near this tree: 0x0020da90 (four corner traces through G_FilterCachedCandidates and SV_CapsuleTrace /
SV_Trace), the 0x00231770 physics siblings (0x0002a620, 0x00229d20 / 0x00227290, 0x0022d7d0, unnamed physics
callees), 0x00228bf0 (only via the never-reached CM_RayHitsWorldMeshes), the turret test 0x002635c0.

**Earlier (decomp, 2026-10-23, day 16 later): client per-frame/input, matched 1402 -> 1419.** CL_KeyEvent
(299 tests, settled), SCR_UpdateScreen (5954 matched, rest inconclusive: _clock / SDK log full), CL_TogglePause (4),
14 of 37 button commands (IN_*Up/Down: lean, reload, activate, button0, speed, UpUp/UpDown) matched with a wider
press script (rt/b/x/y/lt/black/white/ls/rs/dpad/start/back after 100 s; scratchpad press list = default A presses
plus those). C exists, not reached: the keyboard-only movement commands (forward/back/left/right/move*/look*/strafe,
IN_DownUp/Down, IN_CenterView, IN_MLook*), CL_PauseForDialog (UI dialogs). CL_ClearState and CL_MapLoading end
inconclusive only (_clock under the sound stop). SCR_DrawScreenField is a C helper (CALL in its first 5 bytes).
- SCR_UpdateScreen runs its title callees at the original's ESP (asm entry 0x20000 below, title_call_at_regs):
  the first version mismatched 5558 times because title code under CG_DrawActiveFrame stores a stack-local address
  in 0x0095e2d0 (0x00246fca). Row reset to the post-fix counts.
- std::initializer_list in game code: at -O0 its begin() is an out-of-line weak symbol bound to the loader's copy
  ("new called host code" on every test); use plain arrays.
- CL_Frame next: before it becomes Replaced, the loader's --level hook rewrites its call to SCR_UpdateScreen
  (0x002f27a4, loader/patches.cpp kCallClientFrame) - the C must keep that hook. Its pushes are callee-popped
  (S_StopAllChannels etc. are stdcall; check each callee's ret N).

**Earlier (decomp, 2026-10-23, day 16): client module (cl.cpp) started, matched 1364 -> 1402.** Day 15's 224
names applied in Ghidra (symbols exported, smoke test clean). cl.cpp: 46 functions, all matched over missions 1/4/5/6
(most settled): the pad readers (IN_Pad*), key states (CL_KeyState, IN_KeyUp/Down), user commands (CL_CmdButtons,
CL_KeyMove, CL_MouseMove, CL_JoystickMove, CL_AdjustAngles, CL_FinishMove, CL_CreateCmd, CL_CreateNewCommands),
CL_GetSnapshot/GetUserCmd, CL_WritePacket, CL_PacketEvent, CL_ParseServerMessage/ParseSnapshot, CL_GetServerCommand,
CL_ConfigstringModified, CL_SystemInfoChanged, CL_ResetLocalConnection, CL_ShutdownCGame, CL_InitRenderer/InitInput,
IN_InitCvars, CL_RefPrintf, Key_* helpers, CL_DObj*.
- Register contracts the harness does not check (title callers read them; found by scanning each call site's next
  instructions): CL_KeyMove/CL_AdjustAngles keep constants in XMM2-6 across CL_KeyState (its entry saves XMM1-7),
  CL_CreateCmd reads EAX after CL_CmdButtons and EDX after CL_FinishMove, CG_InterpolatePlayerState EDX after
  CL_GetUserCmd, CL_WritePacket EAX after MSG_WriteLong (now Result::Eax).
- C exists, never reached in level runs: CL_CgameSystemCalls (the cgame calls the renderer directly),
  SCR_AdjustFrom640, CL_AddReliableCommand, Char_IsDigit (hex key names only). CL_AdjustAngles' accelerated path
  (cl_bomberKeyboardMode) is probably mission 5's turret; not checked separately.
- Before the client module becomes Replaced: the loader's PC mouse patch rewrites the CL_MouseMove call inside
  CL_CreateCmd (0x002c7ad7, loader/patches.cpp) - move that hook into the C.
- Left in the reached list: the XInput pad functions (IN_PadInit/Create/Update/CheckDeviceChanges/StopAllRumble,
  IN_SetRumbleMotor: XPP calls end inconclusive unless recorded), SCR_InitLoadingSpinner (Sys_Main, before tests),
  FE_LiberatorCreateCrossIcons (ESI-this constructor 0x00317050), CL_InitRef (refimport table + GetRefAPI).
  CL_ScaledMilliseconds starts with a CALL (unhookable).
- Module sorting (later the same day): every named function is now in a module (tools/data/modules.csv: prefix
  rules plus exact-name rules; new modules script, levelscript, sound, streaming, online, crt). Unassigned = only
  the 1607 MS library functions and the 3102 unnamed ones. Level-section functions always stay with their mission
  (progress.py). New names need a rule unless an existing prefix covers them (naming.md workflow step 4).
- Completion rule (user): no module is finished, so none becomes Replaced, while any function is unnamed or
  unsorted (the 3102 FUN_ and the ~3800 uncreated gap functions included).
Next: CL_Frame and the rest of the per-frame client (rank.py client 1), then the collision/server items below;
the XInput recorded calls / stand-ins in the harness if the user wants the pad functions testable.

**Earlier (decomp, 2026-10-23, day 15, closed): renderer + engine (user: "focus on the renderer and
engine"). matched 1116 -> 1364.** All work merged and pushed; the subagent worktrees are removed (cod_bro/wt/
keeps only the `unpacked` symlink). Names of the day's unnamed functions are in manual_names.csv, not applied in
Ghidra yet (apply + ExportSymbols + smoke test next session).
- Mine: qcommon (qcommon.cpp: strings, va/Com_sprintf, command system + Cbuf, Cvar_Get/Set2 and the cvar commands,
  info strings, MSG byte/usercmd readers + writers, NET helpers, COM_ParseExt, memset, aligned free), the
  collision module start (cm.cpp: leafs/areas/PVS, CM_BoxLeafnums(_r), CM_TempBoxModel, CM_PointContents + quad
  trees, the trace start/brush/capsule/sphere/cylinder tests, ClosestPointOnTriangle, Vec4Normalize, the box-tree
  walk stack), the server module start (sv.cpp: SV_UnlinkEntity, SV_AreaEntities(_r), SV_PointContents,
  SV_ClipHandleForEntity, the reliable command buffer and send functions, SV_RunGameFrame (G_RunFrame at the
  original's ESP), SV_DObj*Skeleton, SV_Init/AddOperatorCommands, RadiusFromBounds).
- Subagents: the renderer ready list, the whole effect loader and particle runtime (fx_load.cpp), nglBeginFrame/
  EndFrame (harness: title RDTSC recorded as EDX:EAX = 0), R_ModelBoneMatrices; the collision world trace up to
  CM_BoxTrace / CM_TransformedBoxTrace (paragraphs below).
- Never reached in level runs (C exists): console-only command helpers, FS path helpers, Com_PushEvent,
  NET_AdrToString, COM_ParseExt, area-portal flooding, CM_TraceThroughSphere, the skill setters (menu).
Next:
1. (done day 16) Client module start.
2. Collision: the rest of the sight-trace tree (collision paragraph below), CM_TransformedSightTrace -> Result::Eax.
3. Server: SV_LinkEntity (1.7 KB) after its helpers 0x002ba9d0 and 0x002b1f70 (-> 0x002aae40); SV_ClipToEntity,
   SV_Trace (needs 0x002db3e0, 0x002e84d0, 0x002e88d0).
4. Renderer leftovers: 0x00242730 (1.9 KB SSE/x87, ~1754 calls).

**Earlier (decomp, 2026-10-23, day 15, collision subagent; branch day15-collision, worktree wt/collision): matched
1312 -> 1326.** The world trace under CM_BoxTrace is C, all settled with plain C (no frame rebuilds; every callee
already C), tested with `--only --every 1` over missions 1/3/4/7: CM_ClipSphereSweepToTriangle (0x002dae30),
CM_QuadTreeTraceLeafs_r (0x002c6bf0), CM_TraceRayThroughWorldTrees (0x002d37f0), CM_TraceThroughWorldTrees
(0x002e04e0), CM_TraceThroughWorld (0x002e4980), CM_BoxTrace, CM_TransformedBoxTrace, CM_TransformedSightTrace
(0x002e6dc0), CM_FilterBoxCandidates, CM_GatherBoxCandidates + CM_GatherQuadTreeCandidates_r (0x002c6840), and
from the sight-trace tree CM_SegmentCrossesMesh (0x002d1980), CM_SightTraceRayThroughWorldTrees (0x002d2f90),
CM_SightTraceThroughWorldTrees (0x002e1740). Never-set stack words handed to CM_BoxTrace (the capsule block's
size/w words, rotation row w lanes) are zero-initialized: tests zero the dead stack. Skipped: CM_LoadMapFile (FS
file I/O: tests would end inconclusive).
Left of the sight-trace tree, bottom-up: 0x002d41f0 (model brushes, CM_TraceBoxThroughBrush until a hit), 0x002d3e10
(temp-box capsule setup + box trace), 0x002d3f40 / 0x002d4050 (sight sphere / vertical cylinder, x87 like
CM_TraceThroughSphere, return 0 when blocked), 0x002da1d0 (capsule vs capsule, 0 / -1), then 0x002e4c00 (the sight
trace root: CM_BoxTrace-style setup, returns the callee's EAX - no stack address, so CM_TransformedSightTrace could be
Result::Eax), 0x002e4940 / 0x002da080 (no direct callers). Also open: 0x002e1150 (another CM_ClipSphereSweepToTriangle
caller, via 0x002e20e0).

**Earlier (decomp, 2026-10-22, day 14, closed): matched 935 -> 1116.** All work merged and pushed; the
subagent worktrees are removed (cod_bro/wt/ keeps only the `unpacked` symlink worktree tools need).
- Mine: NAL quaternion decoders and channel-class slots (nal_codec.cpp; most classes are never used in missions 0-7,
  docs/nal_animation.md), anim managers (nal_anim.cpp), AI paths and the actor state machine (actor.cpp: Path_*,
  Actor_ApplyStateTransitions/ThinkStateMachine/CallStateSlot5 with callbacks at the original's ESP, Start/Finish
  callbacks, cover validity, move history, FindNearbyActor ...), G_MissileTrace / Bullet_RandomSpread (g_utils.cpp),
  BG_EvaluateTrajectoryDelta (bg_misc.cpp).
- Subagents: AnimScript_Update (anim_script.cpp), the rest of the NAL anim range, six cgame batches (cg_ents.cpp,
  cgame.cpp, cg_view.cpp), vehicles/turrets (g_vehicle.cpp), entity/script helpers (g_script.cpp).
- EAX rule: tools/eax_check.py lists Result::None entries whose original keeps an EAX input - now none.
- Open: CG_ClipMoveToEntities mismatches only in dead-stack words (callers match); unhookable C-only functions:
  Actor_PushState, the Scr_StartThread_* wrappers (`push imm8; call [mem]`; harness could accept FF /2 with an
  absolute operand), three deleting destructors.
- Names: all 152 of the day's names for existing functions are applied in Ghidra (2026-10-22 23:40, symbols
  exported, smoke test clean); the 19 gap-function names in manual_names.csv stay unapplied (gap functions uncreated).
Next:
1. cgame: CG_CalculateWeaponAngles + its six unnamed callees (0x002782a0, 0x00278400, 0x00278790, 0x002788b0,
   0x002789f0, 0x00282ef0), CG_DrawHudElem/CG_DrawHudElemText, CG_UpdateViewWeaponAnim, CG_CalcVehicleViewAngles,
   CG_ConfigStringModified, CG_ClearState, CG_UpdateShellShockSound.
2. game: Path_TrimAtGoal / Actor_TrimPathAtGoal, Actor_IsAtGoal (callee 0x00201e90), Actor_RegisterSuppression /
   EventBullet / SendLineEvent, Actor_AnimStationary + Grenade_Recover/Cower, Actor_ForgetSentient,
   Sentient_SetGoalNode, Actor_DropClaimedNode, Actor_GatherCoverCandidates, Actor_Exposed_Finish; Scr_Atan2,
   BG_CanItemBeGrabbed, G_InitAnimScripts.
3. Ranking: `tools/rank.py MODULE [MAX_PENDING]` (bottom-up: naming meta callees x progress.csv).

**Earlier (decomp, 2026-10-23, day 15 round 5, renderer subagent): renderer leftovers, client usage probe.**
fx_load.cpp: FxAxis_Frame (0x0009ddb0, 772 B packed SSE, settled), FxMatrix_RigidInverse, FxSystem_InitBounds,
FxSystems_New, FxEmitters_Init (startup, not reached). tr_main.cpp: R_ModelBoxBounds (0x00240f70, matched) and
R_ModelBoneMatrices (0x00241ff0, settled). A probe showed 0x00240930 (skeleton debug draw) never runs; 0x00242730
(1.9 KB SSE/x87, 1754 calls) is left. Client module: not decompiled yet; a usage probe (dummy registrations, `--only`,
missions 1/4/5/6, rows removed afterwards) over the 126 ready rows (rank.py client, 0-1 pending) found these called in
level runs: IN_PadCheckDeviceChanges IN_PadInit IN_PadCreate IN_PadUpdate IN_PadIsConnected IN_PadButtonDown IN_PadConsumeReleased IN_PadButtonPressed IN_PadConsumePressed IN_PadGetStick IN_SetRumbleMotor CL_DObjCreateSkeleton CL_DObjGetBoneMatrix SCR_InitLoadingSpinner CL_RefPrintf Key_KeynumToString CL_FinishMove CL_CmdButtons CL_KeyState IN_KeyUp IN_KeyDown CL_GetSnapshot CL_GetUserCmd Key_StringToKeynum CL_WritePacket CL_MouseMove CL_JoystickMove CL_KeyMove CL_AdjustAngles CL_ShutdownCGame IN_InitCvars CL_ResetLocalConnection CL_SystemInfoChanged CL_ParseSnapshot CL_CreateNewCommands CL_ParseServerMessage CL_InitRenderer Key_ClearStates CL_GetServerCommand CL_InitInput CL_InitRef FE_LiberatorCreateCrossIcons Key_GetKeyNameForCommand. The other ~80 (most FE_* text/menu helpers, CL_*_f commands, Key_Bindlist_f,
CL_ShellExecute_URL_f, CL_Setenv_f ...) never run in --level runs: start cl.cpp with the reached ones.

**Earlier (decomp, 2026-10-23, day 15 round 4, renderer subagent): particle runtime, NAL class check.** fx_load.cpp
gained the particle runtime in 0x0009b000-0x000a0000: pool statistics/slot functions, FxAction_Compact/Destroy,
apsPtrVector_DeleteAll/Reserve, apsLog_Printf, FxEmitters_FreeAll/Destroy, FxEffect_ViewDepth, object light selection
and FxNode_ComputeObjectLights, FxOp_SetupNode + all ten render ops' AddToList, the fade / depth-range shader constants,
FxParticleBuffers_Init, FxCamera_BillboardAxes (settled 7899/7899); ~25 matched, none mismatched. Left there:
0x0009ddb0 (772 bytes packed SSE), 0x0009ff50, 0x0009f1d0/0x0009f250. NAL classes (probe: temporary registrations with a
dummy body, `--only` over missions 0-9, rows removed afterwards - 0 calls each): the four Float4 classes (vtables
0x007c2738/0x007c27a0/0x007c2808/0x007c2870: 0x0008c0c0, 0x0008c4a0, 0x00080140..0x00081840), the classes at 0x007c2a88
(0x000823a0..0x00082580), 0x007c2af0 (0x00085170..0x000853b0, 0x00082680/0x00082750) and 0x007c2b58 (0x00082c30), the
packed PosOri class (0x0008cc40, NAL_PackedPosOri_Blend) and 0x00093250 (from NAL_GenericAnim_EvaluatePose) never run
in missions 0-9 - not decompiled. Note: `--only` runs do not count calls of the other functions (settled ones are not
hooked), so a probe must be in the --only list itself. R_ range: R_LodDistanceForRadius, R_FindEffectByName (not reached);
0x00240930, 0x00240f70, 0x00241ff0, 0x00242730 remain (large packed SSE / x87).

**Earlier (decomp, 2026-10-23, day 15 round 3, renderer subagent): the clock in tests, frame begin/end, the whole
effect loader.** Harness: the recorded title_rdtsc now sets EDX:EAX = 0 (new `Clock` stand-in; EDX used to keep the
caller's value) and D3DDevice_InsertCallback/InsertFence/BlockOnFence/Swap are recorded calls (docs/comparison-harness.md).
Decompiled code reads the clock through the title's XAPI QueryPerformanceCounter (0x001ed6ca, a patched RDTSC site) - no
loader export. nglBeginFrame / nglWaitFrameInterval match, nglEndFrame settled (132/132; it calls nglRenderSceneTree by
its title address because the shader Draws read a stale constant lane; new platform_x87_int64_mul for MSVC's int64 to
float); nglVBlankCallback is unhookable (its RDTSC trampoline jump is in the first 5 bytes). Effect loader complete:
FxDomain_Create (was FUN_00098820) + the disc/circle domain SSE constructors, apsParseActions (dispatch tables walked
by a script), FX_LoadEffectFile (its two parameter buffers persist across stages/actions like the original stack slots:
FX_ParseEffectTexture fills only some words) and FX_LoadEffectFromStream (EAX = MemStream_Destroy's flag byte over the
effect slot address) all settled at the first run. Then particle pool/runtime helpers (FxPool*, FxNode_AddToScene, ...)
and 11 small NAL helpers (not reached: unused classes). Left in the range: the larger NAL class methods
0x00080000-0x00086000, 0x0008c0c0/c4a0/cc40, 0x00093250, and effect runtime 0x0009b830, 0x0009c3c0/c470, 0x0009cf00,
0x0009d840-0x0009e0c0, 0x0009f3c0, 0x0009ff50.

**Earlier (decomp, 2026-10-23, day 15 round 2, renderer subagent): the effect loader tree.** New game/code/fx_load.cpp,
~120 functions named from their bodies (manual_names.csv, Fx* names): the effect object (FxEffect_Init/SetName/
SetStageCount/SetStage/ComputeStageSizes/...), growable arrays (FxArray16, stage and emitter tables), the particle action
base FxAction_Init + 35 fixed-argument action constructors (generated from the disassembly), render op constructors,
FX_CreateRenderer (settled 429/429) and its ten creators (two families, one C routine each), the range/constant parameter
objects, the ops' shader setup and small FxSystem helpers. Tested at level load with `--only --every 1` (missions
1/4/5/6): ~55 matched, none mismatched. Not done: FUN_00098820 (the "domain" factory) with its two 813-byte SSE
constructors 0x000a5a80/0x000a5f20, apsParseActions (2.3 KB), then FX_LoadEffectFile itself (stack-address callees,
allocation context word 0x00909948) and FX_LoadEffectFromStream. RDTSC functions (nglBeginFrame/EndFrame/
WaitFrameInterval/VBlankCallback) stay skipped: the recorded title_rdtsc returns EAX = 0 but leaves EDX as the caller had
it, and these store EDX (the high word); a C version needs a clock entry point the loader provides plus EDX = 0 in the
recording (harness change, not made). Still open in the range (rank.py unassigned 0-1): ~100, mostly the NAL classes at
0x00080000-0x00093000 and effect runtime helpers 0x0009b000-0x000a0000.

**Earlier (decomp, 2026-10-23, day 15, renderer subagent): the renderer's bottom-up ready list.** ~85 functions in C
(ngl.cpp, shaders.cpp, nal_codec.cpp, nal_anim.cpp, tr_main.cpp, dobj.cpp, fx.cpp): the 18 shader NewVertexIter slots,
cdDebug_GetSortInfo, the NGL managers' skip-list clears + 7 deleting destructors, nglResourceManager_Add/ForEachPass,
nglDrawQuad (settled 252/252; the original keeps the depth in its argument slot: registered with 4 argument bytes),
nglSetFrameMemorySize, nglInitMultisample/InitFrameBuffers/SelectVideoMode/InitShaders, nglSelectMeshLodWithMatrix,
nglSelectMeshProjectors (was FUN_0006fb30), the debug mesh draws, nglReadFile, TGA/screenshot writers, nglDumpPrintf,
nglToggleShaderByName + Str_EqualN (was FUN_000442a0), the remaining entropy float DecodeFrames (Float1/Float4,
Packed8/16 Float3 + channel decoders), XModel_Free, R_Init, RE_BeginRegistration/RE_EndFrame, R_DrawTextWithShadow,
Anim_InitManagers, FX_ResetParticleSystem. Matched: XModel_Free, nglMeshSection_FreeBuffers, R_Init,
NalManager_SkipListRemoveKey8, nglDrawQuad, nglSetFrameMemorySize (startup: `--only --every 1 --missions 1`). Inconclusive
only (host calls: D3D Reset, XGAssembleShader heap, back-buffer GetDesc): nglReadBackBufferSize, nglInitMultisample,
nglInitFrameBuffers, nglInitShaders. Not reached in missions 1/4/5/6: iterators, manager teardown, the NAL classes above,
debug draws/dumps, projectors (no type-2 projectors in these levels). Unhookable (CALL/JMP/RET in the first 5 bytes):
nglShutdown, cdSimpleInstance_Init/FreeList, nglUpdateScene_Nop, nglResetDevice, nglResourceManager_Add, RE_EndFrame,
RE_BeginRegistration, nglDrawSingleQuadFrame, nglSaveScreenshot, Anim_InitManagers, FX_ResetParticleSystem.
Skipped: RDTSC-bound nglBeginFrame/EndFrame/WaitFrameInterval/VBlankCallback; dead code with no references
(nglDebugDrawLine/Axes, nglCreateNormalCubeMap, FUN_00068f70); FX_LoadEffectFromStream (Stream_LoadBpe returns its EAX =
MemStream_Destroy's leftover, called at the original's ESP); nglAllocFrameMemory, nglDebugMesh_AddBox/InitDebugMeshes,
nglAssemble*Shader, nglDumpCamera/SceneMeshes, R_DrawDObjSkeleton (startup/debug, large). Gotcha: fnpack disassembles a
switch's jump table as garbage - read the table words (xbedis.py) instead of guessing the case order
(nglSetFrameMemorySize mismatched on it).

**Earlier (decomp, 2026-10-22, day 13f): Float3 + quaternion class methods.** 31 functions in C
(game/code/nal_codec.cpp; docs/nal_animation.md "Float3 and quaternion classes"): the shared Float3 / quaternion slots
(type, size, Blend, BlendWeighted, Fixup, Init/Free/CopyState), the plain Float3 class's block methods, the entropy
Float3 class's BeginBlock/DecodeFrames/SkipBlock/Evaluate, the packed quaternion class's SkipBlock/SkipCachedFrame,
NAL_Entropy_SkipStatic4 and the quaternion entropy reader NAL_QuatReader_Init (0x000843c0, settled 301/301) with its
BeginBlock 0x000842e0. Missions 1/4/5/6: 22 matched (12 settled), none mismatched; not reached: the plain Float3
block methods, the entropy Float3 BeginBlock/DecodeFrames, NAL_EntropyQuat_BeginBlock, NAL_Const4, NAL_Quat_TypeId
(other levels / --only --every 1 runs). One m4 run crashed in nglInit's host device creation (parallel DXVK startup).
Next:
1. 0x00086280 (1.1 KB packed SSE, rebuilds w then quaternion products; psssym.py) - it unlocks the quaternion slots
   6/7 (0x000861b0, 0x00086730) and 0x0008d510; then 0x00082170/0x000822b0 and the other quaternion kinds' slots
   (list in docs/nal_animation.md "Not done yet").
2. The entropy PosOri block methods (0x00085600/0x000856e0, 0x000857b0/0x00084970, 0x00085930/0x00085480).
3. 0x0001e150 (2.5 KB vtable method, "camera"), then the cgame ones with one title callee left.

**Earlier (decomp, 2026-10-22, day 13e): the position + orientation classes.** 27 functions in C
(game/code/nal_codec.cpp, nal_math.cpp; docs/nal_animation.md "Position + orientation classes"): the shared PosOri
methods (Blend, BlendWeighted, Fixup, Init/Free/CopyState, SkipCachedFrame, type/size), the entropy trajectory
class's SkipBlock/Evaluate/ApplyStatic/ApplyWraps, the cursor helpers in most vtables, NAL_Trajectory_AccumulateRoot
(0x0008d3f0, root motion across clip wraps) with QuatTrans_Inverse and QuatTrans_Power, and the plain trajectory
class's ApplyWraps (0x00082ee0) + AccumulateRoot copy (0x0008d2d0). Missions 1/4/5/6: 22 matched (8 settled), none
mismatched; not reached: NAL_Cursor_Skip8, the plain trajectory pair. Harness fix: xbcmp_resume left EFLAGS below
the entry ESP (AccumulateRoot's never-written result w read it: 156 false mismatches, reset in functions.csv).
NAL_AnimTrack_Sample / AnimTree_CalcPose_r / DObj_UpdateSkeletons still differ only in w words: Blend never writes
w either, and the sampling temporaries are on the stack - they stay on record (the earlier "match once 0x0008d3f0
is ours" was wrong). Next:
1. The quaternion (0x007c2a30) and position (0x007c2620) classes, with the entropy PosOri block methods that share
   their readers (0x00085600/0x000856e0, 0x000857b0/0x00084970, 0x00085930/0x00085480, 0x000843c0).
2. 0x0001e150 (2.5 KB vtable method, "camera"), then the cgame ones with one title callee left (CG_AddLocalEntities,
   CG_Check*Events, CG_CopySaveState).
Gap functions (user, 2026-10-22): decompile them uncreated - register by address, bound each at the next
table/vtable entry (fnpack overstates their sizes); do not create them in Ghidra. Later one CreateGapFunctions.java
pass over the NAL range 0x0007c2b0-0x00093090 + ApplyNames, when the user wants the counter complete.

**Earlier (decomp, 2026-10-22, day 13d): the entropy mode decoders.** All 46 distinct decoders behind the
64-entry table 0x008bb4a0 are C code (game/code/nal_entropy.cpp; NAL_EntropyReader_Decode dispatches through our
table). New test tests/nal_entropy_test.cpp maps default.xbe's decoder bytes (pure, relative jumps only) and runs
them against ours on random streams: 64/64 modes identical (skipped without the XBE). In game (missions 1/4/5/6):
modes 0x00-0x1d reached, 1,351 tests matched, none mismatched; the negative-scale-only decoders 0x2f-0x3d and
0x1e are not reached in these levels (covered by the offline test). Only 11 of the 46 are created functions in
Ghidra; the 35 gap ones stay uncreated (names in manual_names.csv, not applied yet).

**Earlier (decomp, 2026-10-22, day 13c): DObj_UpdateSkeletons' whole animation subtree, a cgame batch, the
game module started.** Matched 649 -> 907 with both subagents' batches. Renderer/stream (subagent, merged 9283f88): the texture/mesh load chain, nglParseMesh, the stream zone searches and file-type handlers, MemContext_Push/Pop - 73 matched, 11 not reached, Stream_LoadStb inconclusive only (LIBCMT _clock); harness gained a D3DSurface_GetDesc stand-in and MmSetAddressProtect as a recorded kernel call. Skipped there: nglBeginFrame/EndFrame (RDTSC), FX_LoadEffectFromStream (needs Stream_LoadBpe's frame rebuild). Names from this session are applied in Ghidra (2918bd8) except 0x0007c360 NAL_SignalCounter_SkipBlock, an uncreated gap function.
Game module (subagent, merged ca925e7): game/code/g_scr_math, g_scr_runtime, g_utils, actor, bg_misc - 142 matched,
30 not reached (script math, actor state callbacks, pmove ammo helpers, trivial SP_* spawns; list in that commit's
branch report), none mismatched; Scr_GetEntObjectOrEmpty, Scr_Acos, Scr_Asin are unhookable (CALL in the first
5 bytes). cgame (a012446): CG_AllocLocalEntity, CG_DrawHudElemAlignedText/Material, CG_StartShakeCamera,
CG_CalcWeaponBob, CG_UpdateCvars, CG_DObjEnsureSkeleton, CG_GetHudElemText, CG_DrawScreenBlackout,
CG_GetCurrentWeaponDef, CG_DrawDamageBlend matched; CG_Viewpos_f, CG_AddNearCulledLocalEntity, CG_ScoresDown_f not
reached. NAL_EntropyReader_DecodeFloats + the entropy Float3 DecodeBlock/DecodeChannelBlock matched (the bit
position needs 64 bits: byte address * 8). Next cgame: the ones with one title callee left (CG_AddLocalEntities,
CG_CycleWeaponSlot, CG_DrawProgressBar, CG_Check*Events, CG_CopySaveState; rank with fnpack as in this session). New modules: game/code/nal_pose.cpp (NAL_GenericAnim_EvaluatePose, the byte-code
pose evaluator: settled at the first run; its IK opcode 5 is written but never reached in missions 0-6, nor
NAL_IK_TwoBoneAngles / Mat44_InverseRigid; 0x00093250 is still called at its title address), game/code/nal_anim.cpp
(anim states, block cache, track sampling, AnimTree_CalcPose_r), game/code/nal_codec.cpp (stream readers + 17 small
channel-class methods; only the Float3/Entropy ones are reached here), DObj_UpdateSkeletons / DObj_GetAnimRootTransform /
XModel_TransformBones in dobj.cpp (dobj_call_regs: one thunk for the EAX/ECX/EDX game callees). 21 of the subtree
matched (20 settled); DObj_UpdateSkeletons, AnimTree_CalcPose_r and NAL_AnimTrack_Sample differ from the original
only in the w word (+0x1c) of PositionOrientation records, which the title never sets (stale stack in 0x0008d3f0):
leave them on record until that channel class is C code (docs/nal_animation.md). Not reached: NAL_FindSkeleton,
NAL_RleReader_ReadBytes and 14 codec methods of classes these clips don't use; untested: Packed8Float1 Skip/DecodeBlock,
SignalCounter Decode/SkipBlock, EntropyFloat1_Evaluate. (The e4a0e9a commit message's "27 matched / 19 not reached /
6 untested" is off: 21 / 16 / 5.) Next: the entropy mode decoders (table 0x008bb4a0, 32 prefix-code readers, reached
every frame) and NAL_EntropyReader_DecodeFloats (0x0007cb10), then the used channel classes - most of their methods
(vtables 0x007c2a30 quaternion, 0x007c2620 position, 0x007c2bd0 position+orientation) are uncreated gap functions in
Ghidra: ask before creating them. Also 0x0001e150 (2.5 KB vtable method, "camera", calls the anim-state functions).

**Earlier (decomp, 2026-10-22, day 13b): cgame batch + NAL math leaves.** Matched 637 -> 649. cgame: CG_Fade_f,
CG_DrawFlashFade, CG_DrawCenterString, CG_DrawRotatedQuadPic (ESI/XMM0 entry), CG_ShareWeaponTagBModels (ESI) matched;
CG_DrawTextScaled (EAX/EDX/ECX/XMM1) and CG_DrawRotatedPic (XMM0) not reached in level runs. New module
game/code/nal_math.cpp (DObj_UpdateSkeletons' math): Mat44_MultiplyInPlace, Quat_ToMat33, Vec3_MulMat44,
QuatTrans_Concat, QuatTrans_ToMat44, Quat_Slerp, Quat_FromAxis3 all settled at the first run; Mat44_InverseRigid not
reached (IK opcodes only). Lifting: psssym.py handles the shuffles (start after the `mov reg,[ebp+N]` argument loads,
pass --in for the pointers; it ignores `xorps` with a mask - here 0x0082e0b0 = all sign bits - and x87 code: the
"radicand" factor it prints is really the x87 0.5/sqrt). New compiler.h helpers: platform_x87_half_over_sqrt,
platform_x87_sqrt_sub_mul. Next: the rest of DObj_UpdateSkeletons bottom-up (NAL_IK_TwoBoneAngles, 0x00093250, then
the pose evaluator), the earlier list below otherwise unchanged.

**Earlier (decomp, 2026-10-22, day 13): renderer runtime nearly done, cgame started, harness stand-ins.**
Matched 576 -> 637 this session (subagents in worktrees for part of it; all merged). Renderer: FX spawn path
(FX_SpawnQueuedEffect, FX_UpdateEffect, FX_SpawnEffect, FX_KillEffectsOnEntity, FX_ReleaseEffectHandle),
RE_RenderScene, RE_StretchPicRotate, RE_AddPolyToScene + mesh-builder helpers, R_DrawFogPass, R_RenderTestShadow,
cdOcean_SetWaveConstants, DObj_Create/DObj_Free/AnimTree_Free, DObj_GetOwnerPlacement. Patterns that made them match:
callees at the original's ESP (stack-address globals, stale stack read by later callees: RE_RenderScene, DObj_Create,
FX_SpawnQueuedEffect), and a still-mismatching callee of ours called by title address (R_RenderView).
Harness: RDTSC trampolines executable in test processes + title_rdtsc recorded (Sys_Milliseconds callers testable);
stand-ins for GetLevelCount, GetSurfaceLevel2/GetCubeMapSurface2 (surface pool in the test area), Release-to-0,
texture/surface/VB/palette locks (pure arithmetic; arg kind `x` = popped, not logged); the Lock path found a real bug
in our nglUpdateFogTable (fixed). cgame: game/code/cgame.cpp (small HUD/draw/fade/ammo functions) and cg_view.cpp
(gun offsets, kick, shell shock, tilt, camera shake, tracers): 24 + 10 matched. Names: 214 applied in Ghidra
(renderer FUN_ pass, the NAL animation codec - docs/nal_animation.md, DObj_UpdateSkeletons' subtree, 46 hubs).
Not reachable in level runs: ~100 renderer console toggles/front-end paths (an --only --every 1 sweep reached 2),
the icon text, CG_CalcCubemapViewValues (cube-map capture only). Next:
1. cgame bottom-up (tools/progress.py --module cgame; 42 were ready, ~34 done): CG_DrawCenterString, CG_DrawFlashFade,
   CG_Fade_f, CG_DrawRotated(Quad)Pic, CG_DrawTextScaled (EDX/XMM1/ECX/EAX inputs), CG_ShareWeaponTagBModels.
2. DObj_UpdateSkeletons bottom-up: math leaves first (Mat44_MultiplyInPlace, QuatTrans_*, Quat_ToMat33,
   Mat44_InverseRigid; conventions in docs/nal_animation.md), the NAL pose evaluator (10 KB byte-code interpreter) last.
3. Texture load chain (nglLoadDDS & co.: D3DSurface_GetDesc still has no stand-in), nglBeginFrame/EndFrame (RDTSC
   site inside: game/code needs a title-clock hook), R_RegisterModel (fixed-string helpers).
4. Hub naming: next targets listed in docs/naming.md.

**Earlier (decomp, 2026-10-22, day 12c): all four FX_Add*Components done.** FX_AddSoundComponents
(0x0029eea0), FxSoundComponent_Create (0x0029df60, ESI), SndVoice_GetCount/SndVoice_Get - all settled at the first
run (784 tests). Snd_CountBusyVoices (0x00293310, CALL in its first 5 bytes) is a C helper, not registered. Sound
differs from particles: a source with its own aliased sound (ZoneAliasList_NextVariant) makes one component without
triggers; only the first trigger with choices is used; only single-choice lists are cached; weapon cache at +0xac.
0x0096d8f8 = the name's stack address in the original (((ESP - 4) & ~7) - 0x6b0); S_FindSoundByHash compares
strings (bsearch + stricmp), so our own buffer gives the same result. Names for FxSoundComponent_Update's callees are
in manual_names.csv (15 rows, not applied in Ghidra yet). Then those leaves (new module game/code/snd.cpp: SoundRef_* {voice, alias, flag +8, ..., +0x28
source; 0x30 bytes}, S_PlaySoundAlias2 - S_StartAliasVoice2 takes EAX/EBX(out)/ESI/EDI, asm thunk; the voice
helpers moved there; fx.cpp: FX_LimitSoundComponents, Entity_EndSoundNotify with a Scr_Notify ESI thunk): 10 matched
(S_PlaySoundAlias2 14 tests), none mismatched. Then FxSoundComponent_Update 0x0029d7c0 (1.9 KB): settled at
the first run (347 tests); its asm entry passes the caller's EBX (the original pushes EBX with only its low bytes
replaced as an S_PlaySoundAlias argument); FxSoundComponent_Create now calls it with EBX 0 (as the original). The
whole FX component tree under FX_SpawnQueuedEffect is now ours except 0x00293310 (unhookable, C helper). The day's
15 names are applied in Ghidra (symbols exported, smoke test clean). Next: the next renderer/FX callers bottom-up (tools/progress.py --module renderer --state untested).

**Earlier (decomp, 2026-10-22, day 12b): three of the four FX_Add*Components done.** Since 14ba77a: the
ParamSet resolver (ParamSet_Resolve 0x0029b9f0 with ParamNode_CollectChoices, ParamSet_CompareValue,
ParamSet_TestCondition), FX_AddShakeRumbleComponents and FX_AddParticleComponents - all settled at the first run
(TestCondition not reached: these levels only branch on kind 0/9 parameters, which use the binary search). Light and
shake/rumble use list 1 of the resolver's output (highest priority), particles list 0 (every choice). Byte-sized
parameters (0x18, 0x3a, 0x3b) index their value arrays without the * 4 (fx.cpp choice_byte). FX_AddParticleComponents
stores the address of its stack copy of the effect file name in 0x00971800: the C version writes the original's
address, computed from the entry ESP (((ESP - 4) & ~7) - 0x770), and uses its own buffer - no frame rebuild needed
because nothing reads the global after the function's own hash loop. Next: FX_AddSoundComponents (3.4 KB, stack
address in 0x0096d8f8) and its sound tree (0x0029df60 -> 0x0029d7c0, 1.9 KB, 18 callees); 0x00293310 cannot be
hooked (CALL in its first 5 bytes).

**Earlier (decomp, 2026-10-22, day 12): the FX component helpers under FX_Add*Components.** New module
game/code/param_set.cpp: parameter sets (ParamList_Init/Clear/Add/Delete, ParamSet_Destruct/Delete/AddString/
AddValue/AddFxOverrides, ParamChoice_GetValue; the stack object FX_Add*Components and the animscript event builders
0x00200450/0x00203280 fill). In fx.cpp: ParamChoice_GetFlags, ParamChoiceList_PickRandom/PickPerGroup (weighted pick,
particle LCG 0x008d5a70), FX_CollectTriggers, FXTriggerTable_Find, FixedStr32_Format, ZoneAliasList_NextVariant,
FxComponent_GetEntity/GetEntityOrigin/GetPosition, DObjHandle_Resolve, WeaponModel_GetDObjHandle, FX_AxesFromForward,
FX_RandomInOctaves, FX_AnglesToMatrix, the camera shake (CameraShake_Start/AllocSlot/InitSlot/SetAmplitude/
SetDistanceScale), the rumble (Rumble_SetScale, ShakeRumble_Start, ShakeRumbleRecord_Init, ShakeRumbleDesc_Init/
Destroy) and the shake/rumble component's Update (0x002a3570, vtable 0x008376d0 slot 1); UintMap_Find (ngl.cpp),
AnglesToAxis and MatrixMultiply43To44 (qcommon.cpp). Harness: `Result::Xmm0` (float results in XMM0) and the hook's
instruction decoder takes SSE arithmetic/compare opcodes (CameraShake_SetAmplitude starts with UCOMISS). Title CRT x87
intrinsics (__CIsqrt/__CIsin/__CIcos/__CIpow) are called at their title addresses (fx.cpp title_ci_*), so error
cases match. Gotcha: cdecl functions that reuse an argument slot as scratch (FX_RandomInOctaves, AnglesToAxis) get
that slot compared - the asm entry hands the C version the slot's address. Not reached in missions 1/4/5/6: the
ParamList/ParamSet destructors, FixedStr32_Format (only with a trigger prefix). Also: FX_PlayEffectWithAxis/
AlongDir, the particle component's Update (FxParticleComponent_Update 0x002a46d0: reads the spawn parameters
FX_AddParticleComponents leaves in 0x009717f4-0x00971838) and the first of the four, FX_AddLightComponents (settled
at once; it calls the ParamSet resolver 0x0029b9f0 through the set's vtable). FX_PlayEffectAt now returns
FX_SpawnEffect's result (registered Eax; it was None, so the dropped return value went unnoticed). 44 new functions
this session (commit 14ba77a says 45/41 by mistake: 44 new, 40 matched, 4 not reached), none mismatched. Next: the ParamSet resolver 0x0029b9f0 (952 bytes: a decision tree over the
definition's nodes; callees 0x00296e50, 0x00299310, 0x002994f0), the sound component 0x0029d7c0/0x0029df60, then
FX_AddShakeRumble/Particle/SoundComponents; 0x00293310 cannot be hooked (CALL in its first 5 bytes).

**Earlier (decomp, 2026-10-22, day 11d): the emitter Apply and the last particle actions.** 16 more, 10 matched,
6 not reached, none mismatched: apsActionEmitter_Apply (0x000a2e40, SrcA/Brst/RSpn: the 16 generators at self +0xc,
vtable slot 1, called in a fixed order; settled), FxEmitter_AddParticle, FxBox_Expand, apsSinCos (0x000a06e0 =
scene_sine_poly), Quat_FromAxisAngle, Quat_FromEuler, Mat44_TransformVector, AngT (apsActionAngleToVelocity_Apply),
Traj (apsActionTrajectory_Apply, path lookup 0x00909940) + apsPath_FindSegment - all settled or matching. Not reached
in missions 0-7: LAtr/DLAt (line attract), KTau (apsActionFrenetMove_Apply; particle fields +0x9b-0x9f) and AngT's
flag-0x20 subtree (apsOrientToVelocity, Mat44_FromDirection, Quat_FromMat44); grep of the level .cod files: Traj in
prologue/gela/omaha, WPRf in oran, AngT in many; LAtr/DLAt/KTau/PAtr/ATEX not found (maybe packed). New x87 helpers:
platform_x87_add_sqrt, platform_x87_sub_sub_add_sqrt. Missions 8-11 reach none of them either (start-of-level
runs): they need a played --compare run (XBCMP_ONLY = these + ATEX/Y/Z, PAtr, WPRf). Day-11 names (60) applied in
Ghidra (13 functions created: total 16517), symbols exported. Next: the rest of the particle library
(FX_Add*Components and their param-block writer tree).

**Earlier (decomp, 2026-10-22, day 11c): more particle actions.** 11 more, 6 matched, 5 not reached, none mismatched (commit 8ac7562 says 12/8 by mistake):
apsAtan2 (0x000a4ac0, reuses FxSystem_SetView's asin series), ClSh, MFxV, Forc, OMov + Quat_RotateVector (0x0009f6c0),
all settled or matching; not reached in missions 1/4/5/6: ATEX/ATEY/ATEZ, PAtr, WPRf (need a played --compare run or
other missions). New x87 helpers: platform_x87_add_mul, platform_x87_rcp_norm4 (and the portable fallbacks, also for
exp2_series). Never-set stack lanes in ClSh/WPRf are taken as 0 (test processes zero dead stack). Names still not
applied in Ghidra (day 11 batches). Next: AngT 0x000a4640 (callees 0x000a4460, 0x000a4d30), LAtr 0x000a3f00, DLAt
0x000a41c0, KTau 0x000a2300, Traj 0x000a2aa0, the emitter Apply 0x000a2e40; then a played run over effect-heavy
levels for the not-reached actions.

**Earlier (decomp, 2026-10-22, day 11b): the particle action classes.** The action vtables (0x007c8df0-0x007c90a4,
5 slots: Apply, type (four-character code, names the class), a4980 (returns 1.0), dtor a4d10, size 0x0009c370; the
emitters SrcA/Brst/RSpn share Apply 0x000a2e40 and add slot 5 = particles to emit). Class list with addresses: fx.h.
Particle field offsets (emitter +0x8d..+0xa0 bytes): fx.cpp "The particle actions". 22 more, 15 matched (13 settled),
none mismatched: FxEmitter_QueueParticleKill, the counts of SrcA/Brst (RSpn not reached), Life, AlFd/AFIO/RFIO, LnSc/
LScW/LScH, ExSc (EScW/EScH not reached; x87 FYL2X series = platform_x87_exp2_series), Move, PsMv, VlDr/AVDr (VAVD not
reached), UVFr not reached, apsMemoryPool_FreeSlots/apsPoolSet_Clear not reached (shutdown). Renamed: 'SpDe' = spawn on
death (apsActionSpawnOnDeath_Apply, apsElementList_SpawnDeathEffect; 0x00909f10 = the current element list). Names
not applied in Ghidra yet (this and the previous batch). Next: the bigger actions - OMov 0x000a18f0, MFxV 0x000a39f0,
Forc 0x000a3bb0, ClSh 0x000a1bc0, WPRf 0x000a1d20, AngT 0x000a4640, ATEX/Y/Z 0x000a1fa0/0x000a20c0/0x000a21e0, PAtr
0x000a3d80, LAtr 0x000a3f00, DLAt 0x000a41c0, KTau 0x000a2300, Traj 0x000a2aa0, the emitter Apply 0x000a2e40; the 34
type getters (mov eax,imm; ret) if worth registering.

**Earlier (decomp, 2026-10-22, day 11): the particle pools and spawn actions.** 11 more, 6 matched, none
mismatched: apsMemoryPool_AllocSlots (0x0009c210) / apsMemoryPool_Init (0x0009b9d0), apsPoolSet_AddPool (0x0009bf50) +
apsAddPool (0x0009c050, global set), apsPtrVector_Resize (0x0009c620), apsPtrVector_PushFront (0x0009cb60: with room
left the original only shifts the items, item not stored), apsElementList_Add (0x0009cb00, FX_LoadEffectFile; 'SpDe'
element type 0x53704465 remembered at +8), apsElementList_SpawnDeathEffect (0x0009c9e0), the spawn actions
apsActionSpawnTimed_Apply (0x000a1730, vtable 0x007c907c) / apsActionSpawnOnDeath_Apply (0x000a1850, 0x007c9090; birth
"SpDe" = spawn on death, pass = delta 8888.89), FxSystemList_DestroyAll (0x00095300, CG_Shutdown tail). Not reached: apsAddPool/AddPool (17 calls
from 0x00234a00 at startup, before tests run; also with --only --every 1), apsElementList_SpawnDeathEffect (its caller 0x000a0970 is
a vtable slot), OnBirth, DestroyAll. Dead code (no refs): 0x0009c3c0 (array compaction), 0x00095360 (an older
apsEffect_Init). Names not applied in Ghidra yet. Next: the other Resize/Init callers (0x0009bee0, 0x0009bff0,
0x0009c070 from Mem_InitSmallBlockPools, 0x000951c0, 0x0009f250), 0x000a0970, then the remaining action classes
(apsParseActions 0x00097cd0 vtables) and the FX_Add*Components tree.

**Earlier (decomp, 2026-10-22, day 10): particle allocation and effect creation.** 19 more, all matched,
none mismatched: apsMemoryPool_Alloc, apsPoolSet_Alloc (best-fit pool set, 0x0009bba0) + apsAlloc (global set
0x00909f08), apsGroup_Create (emitter setup), apsEffect_Init/Create/Spawn (active list 0x009099ac, at most 0x78;
the definition's created count +0xac is raised twice), Mat44_TransformPoint, Mat44_FromQuatTranslation (quaternion
scaled by sqrt 2; each row's 4th lane repeats its first), apsPtrVector_PushBack (0x0009f570, seeded flag 0 around
the heap calls) and the effect arrays (new ApsArray {data, int16 capacity, count}: apsArray_Free, apsPtrArray_* and
apsParamArray_* Alloc/Resize/Reserve/PushBack - heap calls with effect id -1, seeded flag 1). FxEmitterList_Add and
FxEmitter_TransformBounds now call ours. Not reached: apsParamArray_Alloc/Resize/PushBack (only effects with
parameter overrides); Mat44_FromQuatTranslation 4 tests. ApplyNames.java skips the CSV's first row (header): start
name files with one. Next: the other callers of the new helpers - 0x0009bf50 (-> 0x0009b9d0), 0x0009c3c0
(-> 0x0009c620), 0x0009cb00 (-> 0x0009cb60, from FX_LoadEffectFile), 0x0009c9e0 / 0x000a1730 / 0x000a1850
(apsEffect_Spawn callers), 0x00095300 (CG_Shutdown tail); then the particle action classes (Apply methods: vtables
from apsParseActions 0x00097cd0) and the FX_Add*Components param-block writer tree (see below).

**Earlier (decomp, 2026-10-22, night 9): the particle update tree.** FxSystem_Update (0x00094c60) and its whole
subtree are ours: FxEmitterDef_Update/Finish (0x0009c7b0/0x0009c730: actions = vtable slot 0 Apply(begin, end,
emitter, system, time, delta) inside +4 {start, end} windows; emitter-level ones have +0x14 clear; hidden emitters wait
+0xb4 from a depth curve, max step 0.3), FxEmitter_BeginParticles / AdvanceParticles (one range per pass; ages
particles, grows the bounds accumulator 0x00909ee0/f0 by sprite or two-size extents) / EndParticles / CompactParticles
/ UpdateVisibility / TransformBounds, FxBox_GetCorners, FxCurve_Evaluate, FxSystem_SwapParams, FxParamBlock_Set,
FxSystemDef_IsEmitterVisible, FxEmitter_FreeParticles, apsMemoryPool_Free, FxEmitterList_Add/Remove, FxSystem_Destroy,
FxSystemList_Update, FX_DrawGlobalSystems, FxRandom_Reset/Restore. compiler.h: platform_x87_mul,
platform_x87_greater (FCOMIP, no MXCSR flags). Batches 1-2: 17 matched (all settled), none mismatched; batch 3:
7 matched (all settled), none mismatched - 24 this session. Names applied in Ghidra (also night 8's), symbols exported. Next: the rest of the particle library bottom-up - apsMemoryPool_Alloc (0x0009ba90), the emitter
list's push_back (0x0009f570), Mat44_TransformPoint (0x00097ba0, packed), apsGroup_Create (0x00097a10),
apsEffect_Create/Init, the particle action classes (Apply methods: find their vtables from apsParseActions
0x00097cd0); the four FX_Add*Components still need the param-block writer tree (0x00023690/0x000237b0/0x00023b20 ->
0x00024630/0x00024730 -> 0x00023710).

**Earlier (decomp, 2026-10-22, night 8): big functions + the particle library.** FX_UpdateEffects fixed:
0x0096d8f8/0x00971800 hold addresses of stack locals (FX_AddSoundComponents, FX_AddParticleComponents), so its
callees must run at the original's ESP - FX_UpdateEffects_entry moves the C frame 128 KB down and calls each
callee through the new `title_call_at_regs` (title_data.h; all six registers); old mismatches reset. New:
XModel_CalcSkeleton, XModel_CalcBoneChildren, BoneMatrix_Load/Store (0x0029c790/0x0029c9e0), FX_GetBoltMatrix,
FX_DrawEffects (FxSystem_SetView takes a 2nd argument, -1.0f, left on the stack from nglGetMatrix's pushes), and
the start of the particle library ("aps", 0x94000-0xa0000, 217 unnamed): FxSystem_SetView (atan2 series, x87
helpers platform_x87_rcp_hypot / platform_x87_sqrt_abs_sub_mul), FxBuffers_Flip, FxMatrix_Multiply, FxSystem_SetMatrix,
FxEmitter_SetMatrix, FxSystem_IsFinished/Stop/UpdateSortKey, FxSystem_Draw, FxEmitter_Draw, FxBox_Center/Radius/Sphere
- 22 matched (all settled), none mismatched; XModel_CalcBoneChildren not reached (only DObj_CalcAnimatedSkeleton
calls it). Next: FxSystem_Update (0x00094c60) bottom-up - leaves read and understood:
0x000965a0 (def +0x50 [i*0x3c +0xc] & 1), 0x0009c330 (obj+4 [i] = value, ret 8), 0x00094990 (swap the +0x5c param
list {obj, index, value} 0xc each into the objects), 0x000971a0 (free +0xbc via (*0x008bb760) slot 2, 0x00909ec0 -=
+0xc0; ret 4 with an unread argument), 0x0009b770 (pool free: index (ptr - +0x18) / +0x14, free list +0x20),
0x0009c6e0 (curve: 4 keys {time +8, value +0xc}, first time > t, else +4 * +0 on the x87); then 0x00097200,
0x00097580 (642 bytes packed SSE), 0x000979d0, 0x000973b0, 0x0009c7b0, 0x0009c730, 0x0009f2d0/0x0009f350; then
FX_DrawGlobalSystems (0x00236ae0, needs 0x00095250). Also open: the four FX_Add*Components (7.7 KB) need the
param-block writer tree (0x00023690/0x000237b0/0x00023b20 -> 0x00024630/0x00024730 -> 0x00023710) first.

**Earlier (decomp, 2026-10-21, night 7): renderer bottom-up, ninth batch.** 14 more: FX_PlayBoltedEffect
(EAX origin; FX_GetBoltMatrix EDI thunk; bolt rows rotated axes-first), FX_ParseEffectTexture (EDI stream, ESI out),
FX_InitEffectPool, FX_UpdateEffects (EAX list; FX_UpdateEffect EBX and FX_SpawnQueuedEffect EDI thunks), RE_TextWidth,
RE_RegisterShader (EDX name; StringMap_Find EBX thunk; "FrontEnd" zone bank then +0x34, else white texture),
R_LoadCellMeshes (Stream_FindMeshFile EBX thunk, 5th push unread), RE_Shutdown, nglInitTextureManager,
nglCreateSysFont, nglFreeTexture, and two new names cdOcean_LoadWaveSet/StoreWaveSet (0x0004ef90/0x0004f370:
wave set rows <-> tweak globals). FX_ParseEffectTexture, RE_TextWidth, FX_PlayBoltedEffect settled; RE_RegisterShader,
R_LoadCellMeshes, FX_InitEffectPool, RE_Shutdown (--only run) matching. Open: FX_UpdateEffects 73 of 463 mismatched,
one byte each at 0x0096d8f8/0x00971800 (88 vs 40, c8 vs 80); not a register input (FX_UpdateEffect, 0x00296c90,
FX_SpawnQueuedEffect checked) and calling the title's FX_UpdateFadingComponents changes nothing - probably stale
stack read by a component vtable method; find the writer of those bytes next. Inconclusive (SDK host code):
nglInitTextureManager, nglCreateSysFont, nglFreeTexture. Not reached: the cdOcean wave-set pair (only when the ocean
material is the tweak slot, cdOcean_SetWaveConstants). Next: FX_UpdateEffects' byte writer; then rebuild the list;
candidates cdOcean helper 0x00050910 (packed matrix, psssym), cdOcean_SetWaveConstants, nglSelectVideoMode,
nglInitFrameBuffers (SDK), 0x0007cb10.

**Earlier (decomp, 2026-10-21, night 6): renderer bottom-up, eighth batch.** 16 more: FX_SortEffects,
FX_ClearEffects, FX_FreeAllEffects (ESI entry), FX_FreeAllEffectLights (0x00236aa0, was also named FX_FreeAllEffects),
DObj_CreateSkeleton (DObj +0x2f = bone_count), XModel_ResolveBoneNames (ESI entry), and 10 newly named leaves:
R_Set/GetDebugViewMode (0x00901e94), R_AddFogPass (+ R_DrawFogPass 0x00045850 named only), ShaderPCUV_InitMaterial,
Map_EmptyKey, nglSkipList_RandomLevel/InitHead (NglSkipList +0 random_bits, +4 bits_left - were seed/max_level),
nglFlushDebugLineBatch, nglTexture_FreeHeaders, nglIndexBuffer_Free. PtrVector_Resize/PushBack (ESI/EBX) called
through asm thunks in fx.cpp. 14 matched (6 settled), none mismatched; not reached: R_SetDebugViewMode (console
toggles only), nglIndexBuffer_Free. Ready-list notes: most remaining zero-pending renderer FUN_s have no direct
callers - 22 shader-class pairs (45/47 bytes: a functor {vtable, this, 0} cloned with Z_Malloc; vtables next to
each class name string, e.g. 0x007b441c) and a 0x7c000-0x80000 cluster of size-counting/serializer methods; the
skip-list inserts (0x00063f10, 0x00069e00, 0x00083db0, 0x00083f70) sit behind unrecognised gap code. Next: rebuild
the list; candidates FX_PlayBoltedEffect, FX_InitEffectPool, R_LoadCellMeshes, nglInitTextureManager,
RE_TextWidth, the cdOcean helpers (0x0004ef90, 0x0004f370, 0x00050910), 0x0007cb10 (54 callers).

**Earlier (decomp, 2026-10-21, night 5): renderer bottom-up, seventh batch.** 7 more: nglRenderTextNode
(the text node's Draw: one raw pushbuffer of quads; new NglTextNode, NglFont +0x40..+0x48 named), FX_StopEffect,
nglMatrixOrthoOffCenter, nglUnprojectPoint, FX_PlayEffectAt (ESI origin), RE_ParseButtonToken (EBX out, stdcall),
nglPalette_Unlock (bare ret, called from nglCreateTexture). Harness: BeginPush/EndPush stand-ins (the pushbuffer's
dwords are compared; planted-bug check caught). compiler.h: platform_x87_unsigned_mul_add. nglRenderTextNode 301
and FX_StopEffect 156 matched (settled), none mismatched; not reached: the two matrix functions (no callers),
FX_PlayEffectAt, RE_ParseButtonToken (only text with "{"). Skipped: RE_RenderScene (calls R_RenderView, which
mismatches on stale stack), nglEndFrame (D3D Swap/fences not recorded), debug overlays, zlib. Callees outside
the module (_ftol, HashStr_Init, PtrVector_*) do not block: call them by address. Next: rebuild the bottom-up list;
candidates FX_SortEffects, XModel_ResolveBoneNames, FX_ParseEffectTexture, DObj_CreateSkeleton, FX_ClearEffects,
FX_PlayBoltedEffect; RE_ParseButtonToken needs a "{button}" text run (XBCMP_ONLY).

**Earlier (decomp, 2026-10-21, night 4): renderer bottom-up, sixth batch.** 11 more: the pack relocation
chain (Pack_RelocateTable, Pack_RelocateChain (EAX chain, EBX bits; asm entry), Pack_DecodeRelocWord - new names),
nglCreateTexture (NglTexture +0x38 companion surface, +0x54 name), nglCreateWhiteTexture, nglCreateFogTextures,
RE_DrawTextExt (^N colours, {button} icons through RE_ParseButtonToken: stdcall + EBX out buffer, called with an asm
thunk), R_FindFont, Q_strncpyLen, Q_strncpySubLen (new name). 9 matched (RE_DrawTextExt 124, R_FindFont 332, the
Q_ copies and Pack_DecodeRelocWord settled), none mismatched. nglCreateTexture/WhiteTexture/FogTextures mostly
inconclusive: nglTextureGetSurfaces calls host D3D getters (no stand-in). Not reached / no callers:
RE_StretchPicRotate, RE_TextWidthWithIcons, nglDrawFogPass, RE_EndFrame (skipped). Next: rebuild the bottom-up
list; candidates nglEndFrame (1201 bytes, x87; unblocks RE_EndFrame, nglDrawSingleQuadFrame), R_DrawTextWithShadow
(debug callers only), nglLoadTexture chain (file I/O), RE_BeginRegistration/R_Init/R_Register (startup).

**Earlier (decomp, 2026-10-21, night 3): renderer bottom-up, fifth batch.** 14 more: nglParseFontFile (EDI
entry; new NglFont/NglFontGlyph), nglLoadFontFile, nglLoadFont, nglUpdateFogTable (takes over only the first
changed fog value per call), nglParseDDSPalette, MemStream_Destroy/DeletingDestroy, DObj_GetTagIndex (obj EAX,
hash ESI) + Com_HashKeyNoCase, FX_FreeEffectsById, R_CheckCvarRange, FastSinCos (= scene_sine_poly lanes),
RE_RegisterModel, RE_DrawText - 11 matched, none mismatched; not reached: MemStream_DeletingDestroy,
nglParseDDSPalette, RE_RegisterModel. compiler.h: platform_x87_div_by_unsigned. Skipped: nglBeginFrame (RDTSC).
Next: rebuild the bottom-up list; candidates RE_DrawTextExt (1333 bytes, x87), Pack_RelocateTable (name
FUN_00055ab0/FUN_00055b40 first), nglLoadDDSTexture chain.

**Earlier (decomp, 2026-10-21, night 2): renderer bottom-up, fourth batch.** 25 more: nglGetTextExtentf/
SizeF, nglCreateVertexBuffer(Physical), nglInitTextureHeader, nglAllocTextureMemory, nglCreatePalette (palette
size 32/64/128/256 -> 3/2/1/0, others 0), nglMeshBuilder_Begin/AddSection/End, nglDynamicVB_Begin/EndFrame,
nglQuadBatch_Reserve, nglAllocSceneRenderTargets, nglAllocFogTextures, nglFontParseKeyInt (regparm(2)),
nglIndexBuffer_Alloc, XModelParts_FindBone (EBX/ESI asm entry) + DObj_FindBoneIndex2 - all matched, none
mismatched. Inconclusive only: nglTextureGetSurfaces and nglDynamicVB_BeginFrame (D3D getters/Lock are host code;
no stand-in - GetSurfaceLevel allocates host surfaces). Not reached: nglCreateDDSPalette. Unhookable bare-ret
stubs, newly named: nglVertexBuffer_Unlock, nglVertexBuffer_GetD3D, nglTexture_Unlock, nglBeginFrame_Nop.
NglTexture now has format +8, width +0x10, height +0x14, levels +0x18, surfaces +0x34; XBoneInfo +0 = name text.
Next: rebuild the bottom-up list (progress.py --module renderer --state named -> CalleeNames.java ->
bottomup_rank.py FILE 1); ready-after-this: nglParseFontFile, nglParseDDSPalette, nglWriteTGA, nglUpdateFogTable,
nglBeginFrame, RE_DrawText (RE_DrawTextExt first); still skipped: zlib/deflate, nglReadFile and
nglSwizzleTextureLevels (file I/O / D3D getters), debug overlays.

**Earlier (decomp, 2026-10-21, late night): renderer bottom-up, third batch.** 33 more, all matched, none
mismatched: ShaderCache_FindOrAdd (the shader cache 0x00902ac0 is a skip list keyed by NglHashedName, returns the
existing node or null after inserting) + ShaderCache_RandomLevel (was FUN_00093c30), Mem_AlignedAlloc (new
qcommon module), nglGetTextExtent, nglBeginSceneRender (SetViewport and Clear now recorded calls),
nglInitRenderStates, nglFreeSceneRenderTargets, nglFreeFogTextures (mostly inconclusive: Release stand-in), and
the 22 shader-class Register functions (vtable slot 0; GetName is slot 1). Register runs once at startup in a
burst: test it with `compare_batch.py NAMES --only --every 1 --missions 1 --seconds 45` four names at a time
(4 tests at once). Trivial stubs nglIndexBuffer_Unlock / nglShader_Nop cannot be hooked (ret in the first 5
bytes), WaitTilOutputInst1_str_VFunc2 not reached. compiler.h: x87 helpers mul_mul_add, unsigned_mul_mul,
add_mul_mul_add_ftol, round_to_int. Next: rebuild the bottom-up list (167 renderer functions left) - skipped so
far: nglDrawFrameTimeBar (debug overlay), nglRenderTextNode (BeginPush/EndPush need a stand-in), nglDumpPrintf,
nglAssemble*Shader.

**Earlier (decomp, 2026-10-21, late night): renderer bottom-up, second batch.** Ten more whose callees
are all ours (bottom-up list rebuilt: 34 ready, 19 of them zlib/deflate - skipped, not used in --level runs):
nglSetDefaultSceneParams, nglSetDefaultRenderStates / nglRestoreRenderStates (the state cache sent to D3D,
inlined setters write D3D's shadow table and dirty bits in title memory), nglInitShaderConstants,
nglBuildTextGlyphs (text -> runs of glyphs, control codes 1/2/3/9/10/32; new NglTextRun), nglQuadBatch_SetMaterial,
nglNode_GetWorldViewRotation (cdGlass_Draw now calls ours), nglGenerateRenderTargetMips,
nglReleaseTextureSurfaces, strcmpcasenosensitive_internal. 7 matched, none mismatched; not reached in
missions 1/4/5/6: RestoreRenderStates, GenerateRenderTargetMips, strcmpcase; ReleaseTextureSurfaces only
inconclusive (Release stand-in: count reaching 0). Harness: recorded-call sizes `:aK*aM` (count x stride),
DrawVerticesUP recorded; compiler.h platform_x87_mul_add. Skipped: RE_RenderScene (drives R_RenderView,
which still mismatches on stale stack), nglMatrixOrthoOffCenter and nglUnprojectPoint (no callers),
nglWaitFrameInterval/nglReadBackBufferSize (frame pacing patch, GetDesc read-back). Next: the 85
1-pending-callee renderer functions (rebuild the list: progress.py --module renderer --state named ->
CalleeNames.java -> bottomup_rank.py), then the shader classes' startup slots.

**Earlier (decomp, 2026-10-21, night): renderer decomp resumed** (naming is done for now: hubs
dry, naming.md). Fixed the cdWorld_Draw/cdWorldBlend_Draw mismatch (word 54 = constant 18.w): the light
vector's w is the light record's +0x24 (point-light range, 1e24 directional) that nglLightVectorWorld
returns, not light->flags, and the record is 0x28 bytes (our out[4] overflowed); 30,390 tests since, none
mismatched, old rows reset to the post-fix counts. Then 30 renderer functions whose callees were all done
(bottom-up list: CalleeNames.java over `progress.py --module renderer --state named`, then count callees
still named/unnamed): NGL texture/vertex-buffer/palette/render-target wrappers, nglListAddTextf,
nglQuadBatch_Draw, the FX by-id helpers and fading components, a new stream module (MemStream_*,
Stream_Read*: a read that writes nothing returns the original's PUSH ECX slot = the stream pointer).
22 matched, none mismatched. Recorded calls added (loader/patches.cpp): SetRenderTarget(Fast),
BlockUntilIdle, BlockUntilNotBusy, D3DResource_Register. Still inconclusive: nglLockVertexBuffer,
nglTexture_Lock (Lock calls need a stand-in), nglVertexBuffer_Free (XPhysicalProtect). Next: the
renderer's 1-pending-callee list (170) bottom-up, and the shader classes' startup slots (109 untested).

**Earlier (decomp, 2026-10-21, evening):** script container templates (hash_map/list/vector/DynArray)
named in .text and their level-section copies by code shape, functor dtors, drone/mortar/scripted-anim clusters
and ~120 small script leaves up to 0x1b4000 (naming.md "Containers and script-range pass"). Next: the rest of
0x1b4000-0x1e4000, then the level sections. Counter: named 7005/16503.

**Earlier (decomp, 2026-10-21, late night):** script functors named from their RTTI (thread-start
helpers, ctors, invoke/dtor slots; 176 functions created) and the anim-handle tables (naming.md "Functor RTTI
and anim handles"). Next: the rest of 0x120000-0x1e4000, the anim-handle globals, then accessor templates
over the level sections. Counter: named 6032/16460.

**Earlier (decomp, 2026-10-21, night):** anim-script mains from their name hashes, 881 missing
functions created in the script range, 399 field accessors by body template (naming.md "Uncovered code and
hash words"). Next: the rest of 0x120000-0x1e4000, then accessor templates over the level sections.
Counter: named 5129/16284.

**Earlier (decomp, 2026-10-21, late):** compiled common scripts from callers and field/value hashes. Counter: named 4653/15399.

**Earlier (decomp, 2026-10-21, later):** open script slots named (152 functions + 2 globals). Counter: named 2990/15260.

**Earlier (decomp, 2026-10-21):** script builtin slot bodies named (137 functions + 100 slot
pointers; naming.md "Slot bodies, third pass"). Counter: named 2838/15191.

**Earlier (decomp, 2026-10-20, late night):** address-order pass over the engine range
0x1fa000-0x2f6d30 done (126 names; naming.md "Engine range, address-order pass"). Next: the
script builtin bodies 0x2f4be0-0x30c000 by slot (SlotStores.java on the init functions), then
the front end 0x30c000-0x340000 beyond its string-anchored functions (32 named: bonus screens, loading quote, hud setup, pause, profile settings), then the streaming loader (77: one load handler per file extension, load/unload states, per-zone resource managers). Counter: named 2701/15191.

**Earlier (decomp, 2026-10-20, night):** engine range 0x1fa000-0x30c000 named hub-first
(the skill's `HubRank.java` ranks unnamed functions by distinct callers): every function with
3+ callers that had a readable role, plus the string-anchored ones (95 names; naming.md
"Engine hubs"). Next: the remaining ~1,450 unnamed in the range have 1-2 callers - read them
in address order with DumpUnnamed (now that the hubs are named the dumps read better), then
the front end 0x30c000-0x340000 (bonus screens, MEM_* dialogs). Counter: named 2384/15171.

**Earlier (decomp, 2026-10-20, late):** the script interface's field accessors
(slots 590-827) and 180 builtin slots by body are named (naming.md "Script engine
interface"; the skill's `DecompToFile.java` dumps many decompiles to one file). Next: the
engine range 0x1fa000-0x340000 in address order (it also covers the open builtin slots'
callees). Counter: named 2289/15171.

**Earlier (decomp, 2026-10-20):** back on naming (step 1) before more rewriting
(user, 2026-10-20). Microsoft library code is now named by signature (3008 functions,
tools/data/library_names.csv; still counted in the total); next: the engine range
0x1fa000-0x340000 in address order, then the Treyarch libraries and the front end; the
compiled common scripts last (no bulk lever yet). Details in [naming.md](naming.md), "Next".

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
Tenth batch (2026-10-19): test processes now record the D3D replacements' calls
instead of stopping there ([comparison harness](comparison-harness.md), "Recorded SDK
calls"; 39 state setters and draws), so code that draws is testable - nglSetDecalZBias,
inconclusive in 2,018 tests before, matched 80 of 80 at once. Then NGL's state layer:
the render-state cache's eight setters (0x00902f08: alpha test/function/reference,
blend enable/source/destination/equation/colour; newly named nglStateCache_*),
nglSetBlendMode (modes 0..8), nglSetPixelShaderProgramCached, nglSetVertexShaderCached,
nglSetTexture (animated frames, references, SwitchTexture within a format, palette,
address mode), nglUnbindTextures, nglSetPaletteCached, nglSetVertexStreams,
nglDrawMeshSection, nglSetTransformConstants (fog parameters and the camera in object
space); 16 of 17 matched in missions 1 and 4 (133-202 tests each, none mismatched or
inconclusive; 197 matched in all). nglSetPaletteCached is not reached (no palettized
textures there). Next: the shader classes' Draw slots (0x00046900-0x00054ad0, 22
classes, 490-2,800 bytes; they inline the state cache's tests and call the helpers
above), then their Register slots (startup only).
Eleventh batch (2026-10-19): the first shader Draw slots in
game/code/shaders.cpp with shared helpers (cull mode, scene fog colour/enable, WVP
constants 6..9, object lights 6..17, stage-0 texture, blend-cache reset): cdWorldVertexLit,
cdBackground, cdSimplePrelit, cdPrelit (fog off, optional stage-1 texture), ShaderPCUV
(decal bias from the instance parameters), cdGunSight/cdGun/cdSimple (one lit template),
plus nglSetTextureAnimFrame (0x000476b0, newly named); all reached ones matched in
missions 1/4 (cdSimple 201, cdSimplePrelit 200, cdGunSight 143, cdBackground 42,
ShaderPCUV 24); cdGun, cdPrelit, cdWorldVertexLit not reached there - try missions 2/3/5.
Twelfth batch (2026-10-19), the remaining shader Draw slots (read with the skill's
`xbefunc.py`, no Ghidra session): cdChar (skinning constants through the title's
nglSetSkinningConstants), cdSimpleUVAnim (+ cdSimpleUVAnim_GetUVParams 0x0004d560, newly
named), cdDebug, cdDecal, cdWater, cdOcean (shaders from the node class's vtable;
cdOcean_SetWaveConstants 0x0004f810 newly named, called through title_function),
cdPropeller, cdWorld and cdWorldBlend (one draw_world with per-class textures: lights,
the first shadow projector, the dynamic-light passes), cdAirplaneMetal, cdSky (x87
angle: platform_x87_sin_cos_of_product), cdGlass (nglNode_GetWorldViewRotation
0x00049280, newly named) and nglDebugShader_Draw; packed lane math moved to
game/code/lanes.h. Several Draws send constant lanes the title never sets: test
processes now zero the dead stack below the entry ESP ([comparison
harness](comparison-harness.md)), and the decomp writes zeros there (planted check: without
it cdWorld_Draw mismatched 839 of 1,344). Missions 1-12: 16 of 21 Draws reached, all
matched, none mismatched (cdWorld 1,081, cdChar 645, cdSky 608, cdWorldBlend 535, cdDecal
514, cdGlass 349, cdSimpleUVAnim 287, cdPropeller 203, cdAirplaneMetal 124, cdOcean 100);
219 matched in all. Not reached in 1-12: cdGun, cdWater, cdDebug, cdWorldVertexLit,
nglDebugShader (their AddToList slots are not reached either). The three new names are applied in Ghidra and exported.
Thirteenth batch (2026-10-19): register-argument entries - `PLATFORM_ASM_ENTRY` (platform/compiler.h)
defines a title-convention entry in assembly that moves ESI/EDI/EBX/XMM arguments to the
stack and calls an extern "C" hidden C version ([comparison harness](comparison-harness.md),
"Register arguments"). With it: cdSimpleAlpha_Draw (0x00049e40, the last Draw: material or
instance colour, debug override 0x0089aff0, depth pre-pass, blend modes) with
cdSimpleAlpha_PulseColor (0x00049cd0: EAX/ECX + XMM1, newly named) and
cdSimpleAlpha_ClampDebugPulse (0x00049c30), nglStateCache_SetFogEnable (0x00046d50),
DObj_AnyModelHasFlags (EDI), R_DrawFilledRect (ESI colour), R_CullPointAndRadius (XMM2,
0x00235890), R_SetInstanceColor (ESI, 0x0023b2e0); with the zeroed dead stack:
nglSelectMeshLightSet (0x0006fe00; the Draws call ours now) and DObj_GetBounds (0x0023be90).
Missions 1-5: 7 matched (nglSelectMeshLightSet 404, DObj_AnyModelHasFlags 400,
DObj_GetBounds 400, cdSimpleAlpha_Draw/PulseColor/SetFogEnable 139, R_DrawFilledRect 20
with Back held), none mismatched; 226 matched in all. A planted bug in PulseColor's XMM1
speed was caught (helper and Draw). Not reached: cdSimpleAlpha_ClampDebugPulse (debug
override only), R_CullPointAndRadius (its only caller 0x00235910 has no callers),
R_SetInstanceColor (the mesh-builder draw 0x002d0680, behind the jump stub 0x0001d0d6).
huft_build stays skipped (zlib is not used in --level runs).
Fourteenth batch (2026-10-19), the ESI/EDI callers in game/code/tr_main.cpp:
R_ApplyCachedLighting (ESI; RCachedLighting: ambient, three colours, three directions,
count), R_AddRefEntityToScene (ESI = refEntity, record starts 0x80 before it),
R_AddCellEntityLink (EDI/ECX/EDX; RCellEntityLink), R_AddStaticModel (EAX; RStaticModel:
cached lighting, light-grid lookup at the origin then raised twice, scale block, bone
matrices times the model's in packed lanes, nglListAddMesh per bone), R_RotateForEntity
(EAX/ESI/EDI; ROrientation), R_SetupEntityOrientation, R_LayoutTextWithIcons (EAX/EBX + 3
stack args). Missions 1-5: R_AddRefEntityToScene, R_AddCellEntityLink,
R_ApplyCachedLighting 300 matched each, R_AddStaticModel 108, none mismatched; 233 matched
in all. Not reached: R_SetupEntityOrientation and R_RotateForEntity (no callers at all -
dead code), R_LayoutTextWithIcons (only RE_DrawTextWithIcons/RE_TextWidthWithIcons through
the refexport table; not reached in missions 1-5 or the front end).
Fifteenth batch (2026-10-19): the rest of the draw lists' std::sort in game/code/ngl.cpp, one
template set over both node types (MSVC VC7 Dinkumware: _Adjust_heap, _Make_heap, sort_heap,
_Insertion_sort with _Rotate, _Median, _Unguarded_partition with its hidden-pointer pair,
_Sort as nglSortOpaqueNodes_r/nglSortTranslucentNodes_r; 14 functions). Missions 1/4/5: the
two sorts, both insertion sorts, both medians and both partitions matched (50-300 tests
each), none mismatched; 241 matched in all. The heap fallback (std_MakeHeap/SortHeap/
AdjustHeap/PushHeap) never runs: _Sort only takes it when `ideal` runs out.
Cell/lighting batch done (2026-10-19, tr_main.cpp): CM_CellForPoint, R_CellForViewOrigin,
R_SetDefaultLighting, R_GetCellLighting (R_AddStaticModel now calls ours), R_UnlinkEntityFromCells,
R_LinkEntityToCell (EDI entry), R_LinkEntityToCells_r, R_AddCellModels, R_AddCellSurfaces (ESI
entry), R_VisitCell_r; all 10 matched in missions 1/4/5, none mismatched.
Then R_ClipFrustumToPortal and R_ClipPortalWinding (the portal frustum: edge planes
through Q_rsqrt, bevels from the projected bounds with r_portalbevels 0.7; MSVC's 4x
unrolled loops sum in a different order in the first copy of each group, see
`unrolled_first`): both matched (200 tests), R_VisitCell_r calls ours.
View batch (2026-10-19, tr_main.cpp): R_SetupCullPlanes, R_AddWorldSurfaces (asm entry: its
0x2000-byte link buffer sits right below the return address and 0x0095e2d0 keeps that
address), R_SetupPortalBevels with MatrixInverse4x4 (0x00294610, newly named; its cofactor
sums lifted with the skill's scripts/ssesym.py), R_SetupFrustum, R_RotateForViewer,
R_AddEntitiesToCells, R_AddEntityToCells_r, R_AddEntitySurfaces: all matched (139-313 tests),
none mismatched. R_ClearAlpha is testable now (310 matched) through the harness's new
reference-count/back-buffer stand-ins. R_RenderView (0x00247350, the 3D view pass) is
decompiled and rebuilds the original's frame so that every callee runs at the original's
stack position (title_call_at); it matches with R_AddEntitySurfaces original, but with ours
~75% of its tests mismatch in mesh records that R_AddEntityModel's subtree (title code)
fills with permuted rotation values - most likely matrix lanes it never sets and takes from
stale stack (in the original, what nglGetMatrix left there). Expected to go away once
R_AddEntityModel / R_AddDObjSurfaces are ours (zero those lanes); until then R_RenderView
stays Compare with these mismatches on record.
DObj batch (2026-10-19, tr_main.cpp): R_AddDObjSurfaces (0x00244020, 6.4 KB: lighting,
LOD limits, rigid/skinned meshes, far-distance pull-in, entity scale, shadow copies) and
R_AddEntityModel (0x00245a20, EAX). Both entries rebuild the original's frame:
R_AddDObjSurfaces' two instance blocks (frame +0xf0 rigid, +0x130 skinned) are only partly
written and nglListAddMesh copies all 0x30 bytes, so they stay at the original addresses
with whatever is there; R_AddEntityModel calls it at the original's position
(title_call_at). Mission 4: R_AddDObjSurfaces 5,593, R_AddEntityModel 5,602,
R_AddEntitySurfaces (now calling both) 204 matched, none mismatched. Newly named:
DObj_SetMinLod, DObj_SetForcedLod, DObj_GetCoarsestLod, XModel_GetLodParts,
R_DrawDObjSkeleton. R_RenderView's remaining mismatches are only those unset block
fields (copied stack garbage): R_AddEntitySurfaces calls R_AddDObjSurfaces at two depths
0x120 apart (directly, and via R_AddEntityModel), so the direct call's unset fields hold
the via call's spilled matrix rows (frame +0x210..+0x27f), and the rest comes from
whatever else ran at that depth. Matching that needs every function in the subtree to
leave the same stack garbage (our C bodies and the 16 KB scrub after our calls do not):
left as known, R_RenderView stays Compare with them on record.
Lighting/draw-list batch (2026-10-19): R_SampleLightGrid (0x00242e90: bilinear 5-bit grid
colours on the x87, light references merged with x87 + _ftol weights -
platform_x87_mul_add_ftol -, the three strongest as directional lights; point lights
normalized with RSQRTSS + a Newton step) and R_SetupEntityLighting (EAX/EDI entry; timer
from the title's LCG on the x87: platform_x87_mul_div_add), both now called by ours
(R_AddStaticModel, R_AddDObjSurfaces); the LOD setters DObj_SetForcedLod/SetMinLod/SetLod
(EDI/ESI entry)/GetCoarsestLod and XModel_GetLodParts; NGL's draw-list walk
nglRenderOpaqueList/TranslucentList, nglRenderSceneTree, nglSetObjectLightConstants,
nglSetSkinningConstants (0x00076620, the packed A*B*W per bone, lifted with the skill's new
scripts/psssym.py), nglListAddText with nglAllocSceneMemory and nglListAddTranslucent
(newly named), nglListAddCustomNode; FX_SortEffects_r, FX_FreeFinishedEffects, FX_FreeEffect
(ESI entry; FX_ReleaseEffectHandle newly named). Missions 1/4/5/6, none mismatched:
R_SampleLightGrid 304, R_SetupEntityLighting 369, the LOD helpers 99-315, nglRenderOpaqueList
297, nglSetObjectLightConstants 300, nglSetSkinningConstants 291, nglListAddText 252,
FX_SortEffects_r 382, FX_FreeFinishedEffects 345. Known: nglRenderTranslucentList matches
except where a title shader Draw sends an uninitialized constant lane and picks up the
walker's dead stack (the original's sort-call return address 0x0007553c, or a host stack
address on our side) - unmatchable, left on record; nglRenderSceneTree is inconclusive
(SetRenderTarget and the push-buffer quads reach host code); nglListAddCustomNode cannot be
hooked (CALL in its first 5 bytes); FX_FreeEffect not reached. Then DObj_CalcBasePose (8
matched; few calls) and DObj_CalcAnimatedSkeleton (not reached) with XModel_CalcBoneChildren
newly named, RE_BeginFrame (always inconclusive: nglBeginFrame reaches a kernel call) with
nglAdvanceShaderTime and nglGetSceneFogColor (both newly named, 341 matched). Mismatch
reports now have their own cap of 5 (inconclusive ones no longer hide them).
**Resume here (2026-10-19, end of session):** first fix the new mismatch in cdWorld_Draw
(8 of 774 tests) and cdWorldBlend_Draw (5 of 216): only on rare calls (after call
~18,000), always SDK call D3DDevice_SetVertexShaderConstantNotInlineFast word 54 =
110.0 in the original vs 2.0 in ours - a path our Draw code misses. Seen in the first
--compare-all that reached gameplay (earlier runs had no A presses and stayed on "Press
A to start", so gameplay/AI code was never tested before; all else settled: 233).
Workflow tools now: tools/fnpack.py (context per function), tools/scaffold.py
(boilerplate), tools/compare_batch.py (runs + summary; `--report DIR` re-reads,
`XBCMP_ONLY` via `--only`). Then: more renderer callers bottom-up
(`tools/progress.py --module renderer --state named`), then the shader classes'
Register slots. Start parallel --compare runs ~5 s apart: three at once once
crashed DXVK at startup (VK_INCOMPLETE querying surface formats).
From now on (user, 2026-10-19), --compare batches run missions 1, 4, 5 and 6: mission 6
(Operation Husky) covers the water-related code (cdWater/cdOcean/cdRiver shaders and the like).
Not reached by --level 1/4 runs (zlib unused there): inflate_fast. Left among the leaves:
nglMatrixOrthoOffCenter, 0x000683a0 and 0x0006fb30 (no callers), nglInitRenderStates
(tail call). Next batch: more callers, bottom-up: nglListAddMesh's callers (R_AddStaticModel
0x002438b0 - EAX argument, calls R_ApplyCachedLighting (ESI) - and the other R_* model
adders; the debug shape draws 0x0006cb80/cc50/cd30 are reached only from nglDebugDrawLine/Axes),
the shader classes' Register and Draw slots, the tr_* callers of R_SetupProjection;
nglSetTransformConstants (0x000776f0) ends in the replaced SetVertexShaderConstant; nglSetDefaultSceneParams (0x00062a70) is testable now that
nglGetBackBufferTexture's D3D calls have stand-ins. ESI-argument functions (R_AddRefEntityToScene,
R_ApplyCachedLighting) need an asm entry shim. Needs an asm
shim: R_SetupEntityOrientation (EAX/ECX/EDX in, calls R_RotateForEntity with ESI/EDI),
R_DrawFilledRect (colour pointer in ESI).
Each batch: CSV rows -> ApplyNames -> save -> ExportSymbols ->
tools/smoke_test.sh -> tools/progress.py (refreshes tools/data/progress.csv: every
function's section, module and state; `--module renderer` lists what is left, e.g.
`--state untested` for decompiled functions no run has reached yet; modules are name
prefixes and address ranges in tools/data/modules.csv - add ranges as modules get
mapped, 7,900 .text functions are still "unassigned") -> commit. User decision 2026-10-17: decompilation only, no
static recompilation.

**Resume here (loader, 2026-10-17, end of session):** v0.1.1 is out (private pre-release,
Linux + Windows): 60 fps by default and system-link multiplayer from the campaign's
Multiplayer item or the launcher's button, with mouse look, style remap, 60 fps,
widescreen menus and aim assist in MP too (user-tested on one PC; design and open
items in [multiplayer.md](multiplayer.md)). Since then: v0.1.2 (private test pre-release)
adds sharper text (all four game fonts redrawn from bundled OFL fonts; launcher
toggle, default on); not yet user-playtested. v0.1.3 (private test pre-release) adds
the mouse in the menus (campaign and multiplayer); v0.1.4 (private test pre-release):
"Press A to start the game" after loading takes a click too (in-game screen 5, ready flag +0x38;
user-tested). Next candidates:
1. Multiplayer between two PCs, and v0.1.1 on real Windows (firewall prompt).
2. The full native build plan + a list of every game-code patch and its intent (asked for).
3. Heavy fights at 60 fps (mission 5 town), missions 7-13 playtests.
4. MP: split screen (if any), the ~40 startup memory warnings. One release-build MP
   sample read 30 fps in-match once (not reproduced; log not kept).

## Current state (2026-10-16)

- Mouse in the menus (2026-10-18), campaign and multiplayer: hovering selects an item,
  left click selects it and presses A, right click B, the wheel moves up/down; the
  cursor is free while a menu is up and the mouse is captured again when it closes.
  The UI's managers (SP front end 0x0095f8f8, in-game 0x0095f8fc, dialogs 0x0095f900;
  MP 0x004b31f8 / 0x004b3200) poll the pad each frame (FUN_0031fc10, MP
  FUN_0028d080); a thunk there hit-tests the active screen's items against their
  text widgets' boxes (only widgets drawn in the last frames, hooked at the text draw
  FUN_00321160 / MP FUN_0028e5d0) and selects through the screen's own vtable. Layout
  and offsets in loader/patches.cpp ("Menus with the mouse"). Checked unattended with
  scripted pointer positions (boxes on screenshots: profile manager, main menu, dialog,
  pause menu in 16:9, MP menus). Played with a real mouse (2026-10-20, issues #2/#3):
  option rows take clicks right of their label too - left/right of the value's middle
  (the texts on the row: value and front-end arrows; sliders: the other rows' values,
  or halfway to the screen edge for a lone one) tap left/right; a click on a screen
  that is still locked waits up to 30 frames. `--debug` logs every menu click and
  what it hit.

- Keyboard button prompts (2026-10-20, issue #4): `--button-prompts auto|keyboard|
  controller` (launcher "Button prompts", default auto = the last used device). The
  icon font i_button_icons_xb_<en|fr|it|sp> (.dds/.fdf, glyphs 33-49: RT A X Y B LT
  white black, D-pad up/right/left/down, start, left-right, up-down, D-pad, back) and
  Xbox_A/B/X/YButton.dds (menu footers) are redrawn as keycaps/mouse pictures
  (text/key_prompts.cpp) with the keys of the active controller style (inverse of the
  keyboard remap; in menus Enter/Bksp for A/B); BackendConfig::replacement_version
  makes the renderer re-ask when the labels change. User-tested: switching between
  keyboard and controller pictures works.

- Language (2026-10-20, issue #1): `--language english|german|french|spanish|italian`
  (launcher "Language") sets XC_LANGUAGE. The title takes its language from the console
  and the disc (FUN_0028cbd0: a "de" disc is German, an "eu" disc French/Spanish/
  Italian by console language, English console -> French); the North American Standard Edition disc is English
  only (a French console still shows English). The log lists the disc's text languages
  (Global.<code>.stb).

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
