# MTA on Definitive Edition: reuse boundary

The October 13 user direction is to integrate the DE adapter into MTA. MTA owns
startup, input policy, gameplay, resources and networking. New backend work is
under `Client/game_de`, with Core consuming its SDK interface. The CLEO/UDP
experiment remains historical compatibility evidence.

Current checkpoint: real opt-in Release/x64 Core builds and links, and ABI5
implements a bounded local movement-write trial inside the normal Game DE
backend. The existing Core module consumer loads that backend for diagnostic
testing. Shared MTA action synthesis supplies walk/forwards/sprint state;
native callback/provider routing applies it only within the verified player
control call and restores the saved pad pair afterward. The trial retains
failed cleanup state for retry and does not claim remote ownership or full
input coverage. The full CGame, launcher, rendering and multiplayer paths
remain required. Earlier checkpoints below retain their original limits.

The corrected local movement trial is now validated in the game: the human
confirmed walking and sprinting, with 1,317 applied/restored callback scopes and
zero recorded failures. Normal shutdown and installed ownership pass. The
compatibility script still schedules fixed MTA action masks; configured Core
bindings and its backend-neutral context/pulse boundary remain to be connected.
No new walking-rate estimator or replacement multiplayer implementation is used.

The next candidate loads the real normal-build x64 MTA Core in DE, rather than
only compiling Core's module consumer into the diagnostic bridge. Its explicit
Smoke startup reference calls the real exported initializer and records exact
image binding, backend pulse, the retained gameplay gate and adapter teardown.
The legacy D3DX9 import is delayed only for the opt-in x64 build. Ninety-three
actual Core entry checks and both classic Win32 configurations pass. The corrected
in-game Core probe now passes exact-image binding, pulse, expected gameplay
rejection and cleanup in 70 ms. This establishes that startup boundary, while configured binds,
the complete game facade and DE rendering still require implementation.

The first actual DE Core load exposed a pre-entry classic-layout dependency:
global timing-log construction resolved the install root before localization
existed. A nonclassic-layout host reproduces the same fault as the saved game
dump. Core now defers that path until timing is enabled and reuses its existing
explicit root override from the x64 directory setter. Both real Core layout
fixtures pass. The startup reference now keeps all Core calls and cleanup on
one worker while CLEO polls value-only results, avoiding its synchronous-call
watchdog. The human reported normal GTA continuation; all 21 installed hashes
and a separate post-cleanup module snapshot pass. Normal exit and the complete
5,730-byte shutdown log are verified.
Full gameplay capabilities remain gated.

The current candidate now holds the actual Core owner on that worker for a
bounded configured-input session. It loads the real x64 XML module and explicit
MTA bind records, polls eligible keyboard routes from MTA's own key table and
publishes through the normal DE adapter. Focus loss releases controls and
refocus requires a clean key baseline. Both canonical frames are neutral before
the bind/XML owners are destroyed and the original startup gate cleans up.
The compatibility script only schedules and logs this session. It creates no
bind manager, key map or game input writer. Engine context is explicitly assumed
on-foot; full context discovery and unsupported input routes remain work ahead.
The new candidate still needs its human in-game validation.

The next input boundary is now implemented inside actual `CKeyBinds`:
`ProcessConfiguredControlKey` uses its existing bind list/control policy,
`BuildConfiguredControlFrame` snapshots context before transition callbacks
and actions afterward, and `FinishConfiguredControlFrame` retains wheel
release after publication. Classic input delegates to the same methods while
retaining its joystick and native pad policy. A disposable DLL linked from
normal Core objects passes 185 actual-class/adapter checks and 33 host checks;
the preserved old object fails to resolve all three methods. This reuses MTA's
configured controls without a second bind manager. A live DE event/context
owner and existing XML/default imports remain required before claiming live
configured input. The tested in-game Core remains the earlier validated build.

```mermaid
flowchart LR
    Core[MTA Core] --> Adapter[Game DE adapter module]
    Core --> Gameplay[MTA gameplay and resources]
    Gameplay --> Network[Existing MTA networking]
    Adapter -. required bindings .-> DE[Definitive Edition engine]
```

The normal `MTASA` build target produces `game_de.dll`. Its versioned
`Client/sdk/game/CGameAdapter.h` contract receives the MTA owner and provides
Initialize/Pulse/Reset/Shutdown/Release. `CGameAdapterModule` in Core owns module
loading and unloads only after Shutdown and Release. Typed controller storage
now lives in `Client/game_de/ControllerSnapshot.h`; the old probe header includes
that implementation for compatibility.

`--with-definitive-adapter` enables Core's early adapter branch. It checks the
module and its gameplay capabilities before classic configuration, GUI, hooks
or game factories in the CCore constructor. Existing DLL-load file hooks and
the classic process-detection entry gate still precede that constructor.
The current adapter implements caller-observation ingestion and MTA digital
action synthesis. It supplies
no full `CGame`, full engine input capability or remote player context, so startup returns
an explicit failure. Core/multiplayer/render startup also remains gated until
ported. The default classic build retains its original backend.

This is an MTA-owned integration and build boundary. A focused x64 host compiles
the same Core consumer and loads the actual adapter DLL; it does not construct
the full Core or launch GTA. The existing renderer uses D3D9, and
its opt-in x64 entry now recognizes the DE executable candidate. Exact native
binding runs outside loader lock and rejects an unrecognized engine image;
the gameplay capability gate remains in place.

Normal MTA project builds and the real-DLL consumer tests pass. The module has
1,079 checks; the x64 consumer has 45 and its x86 rejection path has three.
Migrated controller storage passes 930 checks on each architecture. Classic
Release/Win32 Core translation units compile with the option enabled and with
the default configuration. Core was not linked or launched by these checks.
The final certificate binds 73 source files:
`Build/mta-owned-adapter-check/20261013-113152-f54e411c11b540eb9e6ec7f8b57577cc/test-result.json`.
See `Client/game_de/README.md` for the module/build contract.

The following input slice extracts the existing `CKeyBinds` digital controller
synthesis into `Shared/sdk/CControlStateSynthesis.h`. Classic Core now calls that
helper after its existing context-transition callbacks and before its existing
joystick merge and native pad writes. The DE module consumes the same helper
through ABI2 `SubmitControlActions` / `GetMtaInputState`, with per-instance
current/previous state and direction history. All 44 original action values are
retained in a shared header; invalid mask bits are rejected before mutation.
Logical MTA state includes walk and radio flags and remains separate from the
twenty-field native observation mask. No engine capability is inferred from it.
The early full-gameplay gate remains in place. The dedicated x86/x64 regression
runner compares the shared policy with the original committed implementation.

ABI3 adds an explicit read-only native pad binding inside `Client/game_de`.
It uses Plugin-SDK's SA Unreal signature/layout after an offline unique-match
check on the exact local executable. `NativePadReader` verifies that fingerprint,
the loaded instruction/relative data target, image bounds and page protections.
It copies native current/previous state on its binding thread; invalid reads
preserve output. No hooks or engine writes are installed. `NativePadCandidateRead`
is advertised only while explicitly bound, and reset/shutdown discard the binding.
Static-image and bounded-copy tests establish the read contract; the live trial
below rejects its controller interpretation. Full startup remains gated.

The first live candidate trial completed 225 samples and cleanup while the
human walked and sprinted normally. The proposed axes/sprint and OldState stayed
zero; other bytes decode as static floats and a changing 32-bit value. The SDK
layout is therefore not accepted as controller state, and EngineInputRead remains
unavailable. Offline consumer code also substitutes an alternate object pointer
before querying separate action providers. Further binding work must follow that
active input path; neither readable bytes nor the public signature establish it.

ABI4 follows the observed provider to its raw source-state table, with a
Core-owned `ReadNativeDeviceState` wrapper. The reader captures all 127 signed
source values after validating the exact live instructions, image sections and
pages. It exposes only a diagnostic candidate capability. The next isolated
capture records this table alongside the rejected SDK offsets, allowing the
human's physical inputs to be checked without calling native query methods or
writing engine state. Successful byte capture alone cannot enable EngineInputRead.

The minute trial completes 449 captures, with D and W+LeftShift observed and
overlapping W/D/LeftShift sources despite the uncertain combined movement.
The binding bridge now has a shared production policy boundary:
`CControlBindState.h` contains the original bind mutation, multi-bind release,
disabled-control, wheel-zoom and radio-skip rules. Classic `CallGTAControlBind`
uses it while keeping its callback loop and context filtering unchanged.
The helper matches the frozen original policy on x86/x64. Loading DE bindings
and feeding them through Core remain follow-up integration work; a callback-free
replacement would lose existing MTA behavior and is not introduced here.

The first Core x64 cleanup fixes the existing `StackTraceHelpers.h` callbacks
and module descriptions to preserve native pointer widths, with read-range
overflow rejection. The actual header passes protected-memory and real-module
checks on x86/x64, including x64 image addresses above 4 GiB. This keeps the
current crash SDK/report ABI intact; register capture, native pool diagnostics
and full startup remain separate port work.

The next startup slice also initializes the three production stack walkers
from native CONTEXT registers, including AMD64 unwind lookup without symbols.
Core's opt-in DE entry skips classic file hooks for an x64 `SanAndreas.exe`
candidate and binds the exact engine image from the constructor. The default
classic route is preserved. This is production startup preparation; no fake
game facade or bypass of missing multiplayer/render capabilities is supplied.

For engine writes, trace the actual pinned pad updater rather than overwriting
the global physical keyboard table. The updater identifies a `0x188` pad stride
and 48-byte current/previous blocks at `+0x38` / `+0x68`; its timer at `+0x164`
explains the earlier rejected layout. A remote override must enclose native
player simulation and restore local context, as MTA's existing keysync hook
does. The public mouse-update hook alone does not establish that boundary.

The corrected native reader now uses those block offsets and eight exact
instruction guards, with the diagnostic capability unchanged. A native
player-control callback candidate was traced at `0x124EAF0`, referenced by
vtable `0x4319730` slot `+0x50`; it already saves/restores provider context.
The human's corrected-layout trial completes 440 clean captures with normal
movement and changing physical sources/velocity, while the pad blocks remain
zero. A passive read also confirms the actual local player's vtable/callback
identity. Remote ownership, callback ordering and scoped query routing remain
necessary before engine writes; physical source queues must not become remote
controller storage.

The concrete next input boundary is the common native provider route, rather
than a timer writing the pad: movement actions `0x1E` / `0x1F` and sprint action
`2` are proven in the normal on-foot chain. Membership, availability and state
must be routed together during the owned player callback; otherwise inactive
remote actions can fall through to physical mouse/touch controls. Preserve
native getter/task gates and MTA's canonical current/previous state, with local
context restored after simulation. The hooks and engine writes are not yet
implemented.

The existing Core now also compiles and links in the normal opt-in Release/x64
project, with all 99 C++ translation units and real vendor dependencies.
Pointer-sized window APIs, opaque SDK animation types and native crash handling
remove the observed compiler blockers while preserving the classic SDK ABI.
Classic Release/Win32 compiles still pass; default project generation is unchanged.
The actual x64 Core entry passes 26 isolated-host checks for directory validation,
exact-image rejection, retry, backend cleanup and unload. Its x64 bundle is
`Bin/mta/x64`, keeping the classic output separate. The launcher, DE rendering
and full game facade remain prerequisites; the gameplay gate is retained.

MTA's ordinary remote movement already combines synchronized controls and
sender velocity, GTA player simulation, and correction of network position
error. The next work follows those existing paths. A new receipt-speed estimator
and continuously forced `Walk_civi` playback are not the intended locomotion
implementation.

| Existing MTA component | Reuse or port work |
| --- | --- |
| `CNetAPI` pure sync | Keep controller/velocity synchronization and packet policy. Sender velocity is already read from the game; it does not have to be estimated from irregular received positions. |
| `CClientPed::SetControllerState`, `StreamedInPulse`, `UpdateKeysync` | Keep current/previous remote input state and client pulse policy. |
| `CClientPed::UpdateTargetPosition` | Share its pure incremental error correction. Keep native position/velocity, contact origins, jacking guards and game-speed thresholds in the backend caller. |
| `CInterpolator` / `CNetAPI::GetInterpolatorPosition` | Reuse timestamped local history for the existing latency-compensation role. It is distinct from remote player locomotion. |
| `CGame` and `CMultiplayer` module interfaces | Provide DE implementations behind the existing client module boundary. |
| `CPlayerPedSA`, `CPadSA`, pools and `ProcessControl` context hooks | Replace classic engine implementation/layouts with verified DE player, input and simulation bindings. |
| Existing MTA resources, Lua and gameplay logic | Retain as part of the end goal; the harness has not demonstrated compatibility. |

The current source traces are concrete:

- `Client/core/CCore.cpp::CreateGame` loads `game_sa` through `GetGameInterface`;
  `CreateMultiplayer` loads `multiplayer_sa` independently.
- `Client/mods/deathmatch/logic/CNetAPI.cpp::ReadPlayerPuresync` reads sender
  velocity and delivers target position/rotation to the existing client player.
- `CClientPed::SetControllerState` stores remote current/previous controls;
  `StreamedInPulse` combines interpolation with keysync.
- `Client/multiplayer_sa/multiplayer_keysync.cpp::HOOK_CPlayerPed__ProcessControl`
  switches to a remote player's pad/camera context, calls GTA player simulation,
  then restores local context.
- `CPlayerPedSA::SetMoveAnim` selects a movement animation group. The separate
  named-animation cache path supports explicitly requested scripted animations.

The first shared-code slice extracts the existing position correction into
`Shared/sdk/CPedPositionInterpolation.h`, consumed by the classic method and a
focused x86/x64 test harness. This keeps one implementation of MTA's policy.
The correction adds incremental network error to the **current engine position**;
it does not replace continuing engine movement with a target-position lerp.
Contact origins and caller-computed warp thresholds stay in the game layer.
The existing Windows client clock contract is preserved. This slice cannot
make a frozen civilian proxy behave as a remote MTA player.

The shared slice is now implemented and source-tested. `Advance` consumes the
incremental correction before native reads; `ApplyWarpThreshold` applies the
existing warp policy afterward. The classic caller persists progress/completion
before reading velocity, then looks up the live target and clears completion
only on a warp. This preserves ordering when the native velocity getter restores
invalid physics. `Update` composes both stages for consumers without that native
boundary.

`Tests/definitive/test_mta_ped_interpolation.ps1` compiles the self-contained
helper on x86 and x64 under `/W4 /WX /O2`. The final October 13 run passes
117 independently specified checks per architecture and binds seven source
hashes before/after. The required formatter and focused dry-run pass.
These focused tests do not prove a DE runtime binding. The existing Win32
Release deathmatch gameplay project now also compiles and links, including this
classic caller. Its `client.dll` build is recorded under
`Build/definitive-classic-build/20261013-080556-link/`. This is the gameplay
module; the complete core, loader, game, hooks and rendering stack is not built.

The second shared slice moves the original, unchanged 48-byte
`CControllerState` declaration from `CPad.h` to `Shared/sdk/CControllerState.h`.
Both classic MTA and the x64 harness consume that exact type. There are 219
runtime checks on each architecture, plus compile-time layout assertions.
`MtaControllerSnapshot.h` stages all 20 documented DE pad fields, publishes a
complete transaction once, and preserves published history on invalid or
partial input. Its 930 checks pass on each architecture. Seven additive bridge
exports enforce the bound game thread and retain ABI version 2.

The next game test is a read-only local input reference, selected with
`set-mode.ps1 -Mode Smoke -NativeInputReference`. Ctrl+Shift+F9 activates one bounded
30-second observation while the human stands, walks and sprints normally.
No transport, remote ped or game-input writes are used. Each raw input sample
passes through the canonical C++ type and is read back before logging alongside
native velocity and position. Previous state means the previous probe sample,
not GTA's previous frame. Start and cleanup reset the snapshot masks.

The pinned 0.500 definitions expose pad indices 0–19 and `GET_CHAR_VELOCITY`.
The four MTA-specific logical fields remain explicitly unknown, and velocity
units remain unknown. Metadata support is not runtime verification. The saved
log reader checks ownership, masks, provenance, clocks, bounds and cleanup;
observed analog, velocity and sprint-button activity are separate findings.
This test can establish a local read binding, not remote input simulation,
native walk/jog classification or complete MTA compatibility.

The first local-input attempt captured 22 idle samples and reset cleanly after
a render gap. The human confirmed normal movement, then identified Home opening
the Rockstar console as the interruption. It does not validate movement input
or velocity. The input reference now uses F9; legacy experiments retain Home.

The F9 retry completed its 30-second budget with 227 accepted samples, cleanup
and normal exit. The human used keyboard/mouse and reported normal walking and
sprinting. Raw engine velocity changed in 192 samples, while all 4,540 pad
values remained zero. Stream validity and velocity observation do not establish
keyboard aggregation or physical input mapping. The full trial is preserved
with the pinned source/install hashes; changing PadId to 1 is not supported by
the metadata or public implementation evidence.

The build boundary also needs work. Core, game, multiplayer, deathmatch and
loader projects currently exclude non-x86 builds. SDK interfaces expose classic
`CEntitySAInterface`, `CPedSAInterface` and RenderWare clumps; core graphics use
D3D9 interfaces. These dependencies must be isolated or given DE implementations
before the complete client can run as x64. Simply removing architecture filters
would leave classic addresses, object layouts and hooks in the DE process.

The next backend work stays inside MTA: port Core startup and provide the real
`CGame`/multiplayer services required by its existing client. Input comes from
MTA's configured action system and needs a verified DE engine application path;
remote player simulation needs its own player data and game-thread context.
Natural gait and the shared MTA correction follow those services. Each engine
binding needs evidence on the pinned executable, including restoration and
cleanup. Unsupported bindings remain unavailable.

The custom UDP harness currently carries only transforms, with no sender
velocity or input state. It can validate native operations but cannot stand in
for the above MTA network/game contracts. Its successful NPC walking reference
and October 13 root-lock comparison remain scoped compatibility results.

For commits, include the user's MTA-reuse goal, the exact existing code shared,
why engine-specific operations stay behind the backend, x86/x64 verification,
and the remaining complete-client/gameplay limits.
