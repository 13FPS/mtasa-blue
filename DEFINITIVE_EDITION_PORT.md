# GTA San Andreas Definitive Edition: findings and port tracker

Last updated: **2026-10-13 13:52** (Europe/Luxembourg).
Experimental branch: `definitive-edition-port`.
Source baseline: `860e41ddeb6635b5dd138fef6c8a492d2f982ce0`.

## Current result

**The target is MTA on Definitive Edition, reusing the existing MTA codebase.**
The user's October 13 clarification supersedes the smaller standalone-client
scope from October 3. The current CLEO/UDP prototype remains a compatibility
test harness. A new custom rate estimator is deferred while existing MTA
movement, animation, synchronization and backend interfaces are traced for
reuse. Existing MTA protocol, resources and client behavior are part of the
end goal; the test harness does not establish that compatibility.

**The adapter is being integrated into MTA's normal module build and Core.**
The user's latest direction fixes ownership: MTA hosts the adapter. The new
`Client/game_de` target and SDK `CGameAdapter.h` provide a Core-owned lifecycle;
`Client/core/CGameAdapterModule` loads, initializes, pulses, resets and releases
the module before unloading it. Canonical controller snapshot storage moved
into that module; the probe keeps a thin compatibility include.

The opt-in `--with-definitive-adapter` path is checked at the beginning of the
CCore constructor. Missing full-game/input-write/remote-context capabilities
reject startup before its classic configuration, GUI, hooks or game factories.
The exported initializer catches that rejection and returns failure. Default
classic startup is unchanged. Current capabilities are observation ingestion and
MTA digital action synthesis, plus an explicit build-pinned native pad read path;
no fake partial `CGame` is exposed. Core's existing DLL file hooks and classic
process-detection gate still precede that constructor, and x64 Core/multiplayer/
render startup remain unported. Normal build and focused host verification are
tracked separately from an actual MTA-on-DE runtime.

The preceding observation-only milestone built `Game DE` and `Game DE Host Tests`.
Its actual x64 module
SHA-256 is `8f28b05db45bfa5dbff6d1afd1e30f842d16883c3b5897fb4fbd553d86341e2c`.
The same Core consumer loads it and passes 45 checks; the x86 path passes three
checks rejecting an incompatible module before load. Focused module checks pass
1,079 cases, and migrated controller tests pass 930 on x86 and x64. Classic
Release/Win32 Core translation units compile with the adapter option enabled
and with the default configuration. This does not link or run the full Core.
All 73 source hashes remain unchanged across these final checks, with formatting
and diff checks passing. Final certificate:
`Build/mta-owned-adapter-check/20261013-113152-f54e411c11b540eb9e6ec7f8b57577cc/test-result.json`.
Microsoft's signed D3DX package supplied missing build headers/import libraries;
repository tools supplied pinned Discord headers and generated language data.
No downloaded runtime DLL or new backend was loaded into GTA.

**MTA's existing digital input policy is now shared with the DE module.**
The original 44 action identifiers moved unchanged to
`Shared/sdk/CControllerActions.h`. `Shared/sdk/CControlStateSynthesis.h` contains
the existing action-to-controller synthesis and opposing-direction history.
Classic `CKeyBinds` calls that helper after its existing context-transition
callbacks, before its existing joystick merge and native pad/horn writes.
The DE module's ABI2 accepts configured MTA actions through
`SubmitControlActions` and returns separate current/previous `MtaInputState`.
Walk and sprint remain separate MTA actions. Invalid action bits are rejected
without changing history; reset/shutdown discard logical input and observations.
This does not establish physical-key aggregation or DE engine pad writes.

The dedicated regression suite freezes the original indexed formulas and
compares all 24 controller fields and six history flags. It covers all 32
contexts, all 44 actions and action pairs, all initial histories and opposing
directions, plus deterministic mixed frames. Its ordinary premake target is
`MTA Control Synthesis Tests`; the focused x86/x64 runner is
`Tests/definitive/test_mta_control_state_synthesis.ps1`. The current build/input
certificate is `Build/mta-owned-adapter-check/current-candidate.json`.

The input milestone passes 298,447 differential frames per architecture.
Normal module/consumer/synthesis test projects build, with 1,412 module checks,
54 x64 consumer checks, three x86 rejections and 930 controller snapshot checks
per architecture. Opt-in/default Release/Win32 Core compile-only targets pass;
Core is not linked or run. All 80 certified source hashes are stable. Certificate:
`Build/mta-owned-adapter-check/20261013-121621-e64f282ab4ee4f2ba5158847fc82482f/test-result.json`.
Its actual `game_de.dll` SHA-256 is
`1df11645e19a3562368a1bca3f30b89653e7e22c2662ec4d070cd62a77b7a02e`.
The DLL exports `CreateGameAdapter` and imports only `KERNEL32.dll`.

The startup audit also confirms that classic executable patching, PE32 loader
imports, classic process detection and D3D9-dependent initialization precede a
usable full client. Those paths still require a DE/x64 implementation. The
available CLEO 1.5.1 SDK and runtime's 32 exports provide no C++ native invoker;
the compatibility harness invokes game natives through its JavaScript runtime.
No unknown function signatures, private layouts or raw offsets were introduced.

**A native pad reader is implemented inside the MTA backend.**
Plugin-SDK's dedicated SA Unreal source supplies the pad layout and a signature;
an offline scan of the exact known executable finds one matching instruction,
resolving to the proposed writable two-pad candidate at RVA `0x53e8810`. The source pin is
`15f15b60bbf74c106e1b496ff92c98764abf4605` (Zlib). This does not import the SDK's
incomplete pool/player/script implementations or its first-match resolver.
`NativePadReader` checks the executable fingerprint, AMD64/PE32+ bounds, live
instruction/RIP target and committed image page protections before copying
the proposed NewState/OldState offsets. Its reads run on the binding thread and preserve output on
failure. ABI3 exposes explicit binding and reads through the existing MTA-owned
module consumer. A successful binding exposes only diagnostic candidate reads; no writes,
hooks, remote-player context or gameplay startup are enabled by this slice.
The live trial below did not validate native controller semantics.

**Live candidate trial: controller-layout acceptance rejected.**
The human reported normal walking and sprinting. The reference completed 225
captures in 30,057 ms, cleanup and script disposal; all 17 installed hashes match.
Both proposed pads' movement axes, sprint field and entire previous-state arrays
stayed zero. Pad 0 byte pairs decode to floats 960/540; pad 1 contains a changing
monotone 32-bit value. These are raw observations, not verified controls.
The module now exposes only NativePadCandidateRead for this binding and never
EngineInputRead. No engine write or remote-player capability is enabled.
The exact captured log is 205,599 bytes / SHA-256
`76e4e2ca930d33487af58d6aab1924c1fc9a159ac035e9819c164c6b69a8cc14`.
The current runtime result is linked from
`Build/definitive-manual-test/current-native-pad-reference-test.json`.
Bounded offline disassembly identifies the static candidate as a fallback:
code substitutes an alternate pointer at RVA 0x5067940 before querying separate
action providers. That selection alone does not establish a controller layout.

Normal exit is now preserved separately: the 205,673-byte final log has SHA-256
`f58e4840b009735b110e3512dc1be81bd86ee9e444716145b3023581a9c7231b`,
contains `host shut down`/`Exit`, and still passes all 225 captures with zero
parser issues. All 17 installed files retain their hashes.

After the trial, a bounded passive metadata read identified the active provider
at slot 0 and its main-image virtual implementations. The alternate pointer was
equal to the static candidate in that snapshot; its gate at +0x30 was 1. These
post-trial values do not describe the entire walking trial. Offline analysis
distinguishes slot 0's binding-membership check from slot 4's actual state read.
For ordinary binding source indices 0..126, that state read accesses signed
32-bit entries at image RVA `0x5709A40`. The engine tests values 2/3 as active;
physical key/action mapping and frame semantics remain unverified. No engine
method was invoked and no game memory was changed. The complete bounded report
is saved under the trial's `passive-input-metadata-132605-15f6318d/` directory.

**One existing Core x64 pointer blocker is fixed.** `StackTraceHelpers.h` no
longer truncates stack-read/module-description addresses to 32 bits. Native-width
range checks also reject overflowing reads before copying. The original helper
fails the real x64 own-module case; the corrected helper passes 35 x86 and 30
x64 checks, including real image addresses above 4 GiB, protected pages and
module/absolute formatting. Repository formatting and diff checks pass.
Certificate: `Build/mta-core-stack-trace/20261013-133022-556c65e367164c0a8bd598431ecba77a/test-result.json`.
Run `Tests/definitive/test_stack_trace_helpers.ps1`. Core's x86 register context,
native pool diagnostics, dependencies and rendering/startup still require porting.

**The next MTA-owned input candidate observes the native source table.** ABI4
adds `ReadNativeDeviceState` and its Core consumer wrapper, preserving all 127
raw signed 32-bit values. It checks the exact live image-base and table-load
instructions, PE sections and committed image pages before a complete copy.
Only `NativeDeviceStateCandidateRead` is added after success; EngineInputRead,
engine writes and remote-player context remain unavailable.

Normal Game DE/host builds and opt-in/default classic Core compilation pass.
The module passes 1,484 checks, the host 64 x64 checks/three x86 rejections and
the reader 1,331 checks per architecture. All 91 source hashes remain stable.
Certificate:
`Build/mta-owned-adapter-check/20261013-134635-a3a9288303824beb8307a81bee4f038e/test-result.json`.
Module SHA-256: `4cd948e61441eb384909704b76a7b42a205da5ba2158c2b723dca3c6ab0b54a2`.
The diagnostic bridge passes 355 ABI checks; the recorder 53 checks; the log
reader 24 tests; the entry/adapter 264 mocks; protocol/math 293 checks.

Three owned files were updated after normal exit, with prior bytes backed up.
All 17 installed files and the enabled loader verify. The installed bridge is
`75f0292da4d922f7c6acc91668af64f2c88c6859d76ddb54eaf3c6b0453a5b0d`.
Capture version 2 records source values beside the rejected SDK pad fields.
The strict assessment must use `--require-complete --require-device-states`;
an older pad-only capture cannot qualify. The new human trial remains pending,
linked from `Build/definitive-manual-test/current-native-pad-reference-test.json`.

The native-reader checkpoint builds the normal Game DE/host projects and passes
1,474 module checks, 59 x64 host checks, three x86 rejections and 1,325 bounded
reader checks per architecture. Opt-in/default classic Core compilation passes;
the full Core is not linked or run. All 84 source hashes remain stable.
Certificate:
`Build/mta-owned-adapter-check/20261013-123815-061c097f099547aabb949f9e5694cf5a/test-result.json`.
The actual module SHA-256 is
`063cd5bc16c0900b498747aba226674f72bca6049145e18cd72b8225354abf7f`.

**The human-run native-reader trial completed and its evidence is saved.**
The diagnostic bridge now compiles the exact MTA `NativePadReader.cpp`, exposes
an additive read-only capture ABI, and snapshots both proposed current/previous
24-field states before component reads. A separate bounded Smoke recorder uses
Ctrl+Shift+F9 and logs position/raw velocity with the native states. This test
does not instantiate full Core or write game input. The old 00E2 result remains
preserved separately. The bridge is
`a35be5c2ccb0ed34060874df04c4353c13bda32ff44cdda5d862fe669132e7f4`.
Build/protocol, 43 recorder checks, 94 real-DLL ABI checks, 19 log-reader tests,
existing adapter/transport and loader/mode checks pass. Disposable collision and
16-to-17 migration checks pass. The actual 17-file installation and loader hashes
are verified. Preparation and the completed runtime result are recorded in
`Build/definitive-manual-test/current-native-pad-reference-test.json`.

The corrected capability mask passes the normal Game DE/host builds, 1,474
module checks, 59 x64 host checks and three x86 rejections. Reader and snapshot
checks and 298,447 shared-input differential frames pass per architecture;
opt-in/default classic Core compilation passes. All 89 certified source hashes
remain stable. Certificate:
`Build/mta-owned-adapter-check/20261013-131359-179b301cb5fd43b4a095241b1fe834d4/test-result.json`.
The corrected module SHA-256 is
`0f9f42bb6ce3536f019528278ed96ac6f0b9394f09228fae91790cef6c086d85`.

**The first shared MTA correction slice is implemented.**
`Shared/sdk/CPedPositionInterpolation.h` contains the existing incremental
position-error correction; `CClientPed::UpdateTargetPosition` now calls it.
The classic caller preserves contact origins, task guards, native-read order,
live target lookup and velocity/game-speed warp policy. Separating advancement
from the warp check preserves the velocity getter's potential physics recovery.
The helper retains classic 32-bit Windows client-clock behavior.

The same self-contained helper compiles with **/W4 /WX /O2 on x86 and x64**;
**117 independent checks pass on each**, with all seven tested source hashes
unchanged across the final run. Tests cover continuing engine movement, target
replacement, completion/gaps, threshold equality, invalid values, contact
origins and the native-read boundary. Expected red compile failures are retained.
The required repository formatter and focused dry-run/diff checks passed.
Final certificate:
`Build/definitive-mta-reuse/20261013-071049-601f554680744e38b55b2e15d3acce91/test-result.json`.
Run: `Tests/definitive/test_mta_ped_interpolation.ps1`.
No game files or existing bridge binaries were changed by that source-only slice.
The complete classic MTA client was not built; DE player/input bindings and
runtime reuse remain unverified. The next backend milestone follows the existing
MTA controls/velocity/player-simulation path. See
`Client/definitive/MTA_REUSE_PLAN.md` for the traced reuse boundaries.

**The canonical MTA controller is now shared, and the classic gameplay DLL builds.**
The original 48-byte `CControllerState` declaration is unchanged in its new
shared header; `CPad.h` includes it. Layout/value/history checks pass 219 times
on each of x86 and x64. A typed transactional snapshot adapter passes 930
checks per architecture. Seven additive bridge exports preserve ABI 2 and
require the successfully bound owning thread. Invalid/partial transactions
preserve current and previous published states.

The existing Release Win32 deathmatch gameplay project compiles and links to
`Bin/mods/deathmatch/client.dll` (4,961,280 bytes), SHA-256
`f9e0f1ee7b7ebde21b97b1f945e3f6ea47c36f6869ef3d736c86ed3153149a20`.
This is a classic gameplay module build, not the complete client or a DE build.
Build evidence: `Build/definitive-classic-build/20261013-080556-link/`.

The next candidate observes local DE controls through that canonical state.
Smoke `INPUT_REFERENCE` requires Ctrl+Shift+F9, reads 20 pad fields plus native velocity
and position for at most 30 seconds/300 samples, and resets snapshots on exit.
It uses no transport, NPC or game-input writes. The previous snapshot is the
previous probe sample. Four MTA logical fields and velocity units stay unknown.
The new mock checks cover bounds, isolation and cleanup; actual local native
bindings remain pending the human walking/sprinting trial. A passing local
reference will not establish remote player input contexts, native gait states,
full MTA networking/resources or complete gameplay compatibility.

Initial candidate certificate:
`Build/definitive-input-reference-check/20261013-101149-4c5be9a8/test-result.json`.
All 57 source hashes are unchanged across 47 input, 259 adapter, 36 gait,
250 utility, 33 native integration and 293 protocol/math checks. Independent
review resolved a published-sample gap boundary before installation. The new
bridge SHA-256 is
`8312df4398117cc815ba7d522a2c2c94b44d945db4fdb37422ce9586120ffed5`.
Four owned files were updated and the manifest-aware mode switch selected
Smoke/input-reference; all 16 owned hashes, source transformations and the known
game executable verify. Loader and definitions were unchanged, and the old
installation bytes were saved before updating. The RequireReady collector
reports GTA closed, healthy display devices and no blockers/collection issues.
Plan: `Build/definitive-manual-test/current-input-reference-test.json`.

The first actual input attempt on October 13 recorded 22 idle samples, then
stopped for a render gap at 7.103 seconds (last sample at 2.804 seconds).
Both snapshot masks reset to zero; the reader reports incomplete with no issues
or rejected samples. The human reported normal walking/sprinting and identified
Home opening the Rockstar console as the cause. No analog, velocity or sprint
activity was captured, so local movement-input mapping remains unverified.
The interrupted full evidence is retained under the original manual directory.

Input-reference activation now uses Ctrl+Shift+F9; legacy other-mode chords
remain Home. Causal integration tests prove Home does not activate the input
probe, F9 does, and ordinary Online does not use F9. The retry certificate
`Build/definitive-input-reference-check/20261013-102425-4e016e34/test-result.json`
binds 57 sources to 47 input, 261 adapter, 36 gait, 250 utility, 33 native and
293 protocol/math checks. The native DLL and controller implementation are
unchanged by the hotkey correction.

The F9 trial finished on October 13 with 227 accepted samples over 30.056 seconds,
zero issues/failures/rejections, zero cleanup masks and normal exit. The human
used keyboard/mouse and reported normal walking/sprinting. Raw velocity changed
in 192 samples and horizontal coordinate displacement was about 182 units, but
all 4,540 pad field values remained zero. The stream is valid; physical input
mapping is not verified. No velocity units or native gait states are assigned.
Final full log: 169,710 bytes, SHA-256
`eda1a5a4a0d72ae55436b580ab17ee2654c7ec0632b7aa9c6049975b9003362d`.
Reviewed result: `assessment-final-reviewed.json` under
`Build/definitive-manual-test/20261013-102753-input-reference-ready-d34c11a2/`.
Sealed actual checkpoint:
`Build/definitive-recovery/20261013-105723-f9-local-velocity-zero-pad-dfd9ff8e/`.
Public metadata identifies Pad1 as 0 and Pad2 as 1; it supplies no evidence that
switching to 1 would provide keyboard aggregation. The next work therefore uses
MTA's module/input architecture.

**The isolated horizontal walk-root-lock trial completed with a scoped human
visual improvement and residual choppiness.** Only the owned `Walk_civi`
task's X/Y lock flags changed to true.
Idle tasks keep both flags false. `GAIT_ROOT_LOCK` defaults false and requires
an explicit Online opt-in with `-LockWalkRoot`. The historical installed trial retained
`GAIT_RATE=true`; controller SHA-256 remains
`cdd943087d2e0580e411c9dc5edded9d47d226353508c2c093c23af766b4a66a`.
The human reported no repeated loop/rewind and described very slow, slow and
normal walking. No running animation was tested. Requested root policy markers
are not a getter for the engine's rendered-root mechanism.

The October 13 helper ran **06:42:20–06:42:50**, exit 0, with **571 updates**,
**264 gait samples**, seven tasks, three walking true/true policies and four
idle false/false policies. End registered **06:43:12**; normal host shutdown/Exit
occurred **06:45:14**. The final full reader has zero issues, failures or rejected
samples but remains **incomplete**: 24/14/9 rate requests left longest stable
spans of only 0.750/0.550/0.500 seconds, with no accepted two-wrap fit. Foot
contact, stride matching and complete MTA gameplay remain unverified.

Final reviewed record: `assessment-final-reviewed.json` under
`Build/definitive-manual-test/20261012-224356-horizontal-root-lock-ready-000b89ed/`.
Final full audit: `audit-064522-eca132c0/assessment.json`, raw **331,206 bytes**,
SHA-256 `182e9e7c0b9587fa120a10b1e154158b3866346025084570d8b5672f0892517b`.
The independent initial raw analysis is an exact byte prefix of this final log.
Post-exit RequireReady collector:
`Build/definitive-diagnostics/20261013-064522-c420b0bfe0744ee1a114dec59e882ec7/`.
It verifies GTA closed, the known executable, all 16 owned hashes and GPU/PnP
OK/0, with zero collection issues/blockers. All 13 trial source hashes verify.

### Root-lock preparation (historical)

Source-bound checks pass **233 adapter mocks**, **36 native-reference tests**
and **215 utility tests**. The broader Definitive Python suite passed 33 tests,
the existing native executable passed 293 protocol/math checks, and both changed
PowerShell files parse. Only index/config changed in the game; all **16 owned
hashes** verify. Certificate:
`Build/definitive-root-lock-check/20261012-224340-verified-550adda88bb14d95a4de60e81ab1a22a/test-result.json`.
Backup: `Build/definitive-update-backups/20261012-224350-horizontal-root-lock-ff6cc9e9/`.
Plan: `Build/definitive-manual-test/20261012-224356-horizontal-root-lock-ready-000b89ed/`,
through `current-root-lock-test.json`. The prior failed rate record stays intact.

The prepared loopback server **26152**, session
**2ab140de8ac145548477e8913345e63a**, expired at **October 12 23:44:03 local time**.
The 22:44 RequireReady collector records GTA closed, the known executable,
16 verified owned files and GPU/PnP OK/0, with no collection issues/blockers.
Runtime readiness verified that specific executable, command line, creation time
and UDP owner, a nonce-matching welcome and requested probe departure, without
sending a transform. It binds the unchanged 342,994-byte failed-trial log as
the new baseline. Independent source/install review found zero issues.
It stopped cleanly with zero players. Before the human test, root started server
**5384**, session **76b5767dc5244f6b8d3220ae5643937a**, expiring **October 13
07:41:48 local time**, and preserved the old runtime plan before rebinding.
Fresh game **25860**, game player **1** and owned peer **2**/ped **6148** passed
identity, source/install and advancing self-echo readiness checks. The human
owned launch and input throughout.

An ignored-Build replay retains all 263 original sampled receipt inputs and
the old controller bytes. It reproduces unstable requests, while an isolated
one-second OLS/EWMA prototype gives 1/1/2 setters within the declared 10% envelope.
Missing 50 ms network polls and 16 ms renders are not reconstructed; mock phase
does not establish native cadence. Production estimation remains unchanged for
this root-lock trial. Replay:
`Build/definitive-estimator-replay/game3-sampled-263/replay-result.json`.

Future commit messages should include the original smaller-client goal, the
human's backward-jump report, why horizontal root locking is isolated from
confirmed rate jitter, the red/green checks and hash-bound installation, the
human comparison result and any remaining cadence/stride limitations.

### Completed first rate trial with visual regression

**The first rate-enabled three-speed trial completed with a visual regression.**
The human reported repeated walking followed by backward jumps/chop. All three
stages intentionally used walking; no running clip was requested. The helper
completed at 17:54:31 with exit 0, 263 gait samples, all seven task stages and
owned deletion. End registered at 17:56:47 and normal shutdown/Exit at 17:58:15.
The game is closed; the 22:22 readiness collector verifies the known executable,
all 16 then-installed owned files and GPU/PnP OK/0. All 13 tested source snapshots
are preserved in the sealed failed-trial checkpoint.

The raw full-session reader rejects repeated connections before/after the peer.
An independently scoped, original-line-mapped game-3/peer-4 record has zero
issues/rejected samples but is still **incomplete**: 46 rate changes left no
stable two-wrap cadence fit. Requested rates ranged 0.250–0.412, 0.411–0.822 and
0.612–1.237, with longest stable spans only 0.800/0.701/0.501 seconds. Actual
short-span phase response follows the requested rates, but the stable-cadence
acceptance requirement remains unmet. The original failures are preserved.

At 100 ms sample points, received targets, displayed poses and observed body
coordinates have no backward reversal; observed/requested XY differ by less
than 0.000103 unit. These samples cannot rule out transient rendered root offsets
between reads. The next one-variable trial tests horizontal animation locking
without changing the rate estimator. Estimator stabilization is a separate
confirmed follow-up; no repaired-sliding or playable-multiplayer claim is made.

Final reviewed record: `assessment-final-reviewed.json` in
`Build/definitive-manual-test/20261012-174108-matched-gait-rate-ready-528f2dab/`.
Final full audit: `audit-222222-ae3a37ef/assessment.json`; independent scope and
analysis: `audit-222222-ae3a37ef/independent-review-scoped-game3-v2/`.
The historical `current-gait-rate-test.json` pointer status is
`completed_with_visual_regression`. Its sealed checkpoint is
`Build/definitive-recovery/20261012-223324-gait-rate-failed-trial-3d500df0/checkpoint.json`.

### First rate candidate preparation (historical)

**The first opt-in speed-matched Online candidate was prepared before its
completed three-speed visual test.** It used the accepted native reference
speed **1.5216103452786847 units/second**. A changed-target XY path window of
250–500 ms estimates received movement without treating every cached poll as a
fresh update. The walking multiplier is speed/reference speed, clamped to
0.25–2. It requires an observed playing association, a window no older than
250 ms, at least 250 ms between setters and a minimum 0.05 rate change. Task
transitions, the 350 ms movement hold, owned cleanup, smoothing and bridge
cadence retain their behavior. Rate logs identify a requested setter value;
actual phase progression must separately verify its effect.

Final source-bound checks passed **230 adapter mocks**, **36 unchanged native
reference tests** and **215 utility tests**, including **30 cadence-reader
fixtures**. All **13 certified source hashes** and **16 installed hashes** verify.
Certificate: `Build/definitive-gait-rate-check/20261012-174047-verified-20566532c63348079d90104c07795549/test-result.json`.
Three owned scripts were updated. Backup:
`Build/definitive-update-backups/20261012-174051-matched-gait-rate-9bafa543/`.
The installed Online profile explicitly enables `GAIT_RATE`; source defaults
leave both experiments disabled. The native bridge, loader, definitions and
C++ did not change.

Historical rate plan: `Build/definitive-manual-test/20261012-174108-matched-gait-rate-ready-528f2dab/`,
through `current-gait-rate-test.json`. Server **15772**, session
**4f40e691dc0548b5a9d2e4f06ccbf957**, listened only at 127.0.0.1:22040 and expired
at **18:41:09 local time**. Runtime identity, nonce-matching welcome and requested
probe departure were verified without sending an UPDATE. Post-install
RequireReady diagnostics passed with GTA closed. The human owns launch, input
and visual observation; root owns the finite synthetic peer and log audit.
The planned 30-second sweep retains the seven stages and 0.5/1.0/1.5 speeds.
The cadence reader requires two real wraps per stable walking segment and
checks measured cadence against the requested rate, retaining insufficient
evidence rather than declaring a successful repair. Actual rate effect,
multiplayer stride matching and absence of sliding remain unverified.

### Completed native walking reference

**The offline native walking reference completed its human-run GTA test.**
The accepted fit measures **1.5216103453 game units/second**, a **1.1333501459-second
cycle** and **1.7245173068 units/cycle** for natural `Walk_civi`. The user reported
**“Walked naturally; no obvious sliding”**. This is a scoped human visual report,
separate from physical foot-contact measurement and multiplayer stride repair.
The NPC reached its endpoint and was deleted at **17:20:42**; normal game
shutdown/Exit occurred at **17:22:37**. GTA was closed and all 16 installed hashes
verified before the next candidate was prepared.

Source defaults keep `GAIT_REFERENCE=false`;
the explicitly selected installed Smoke profile enables it. Home starts one
owned model-7 NPC on an eight-unit lane near the outdoor player. After one second
of settling, a single finite engine walking task drives its movement. External
coordinate/heading writes occur only during initial setup; no forced animation
or playback-rate changes are applied during measurement. It samples actual
transform and guarded `Walk_civi` phase at most every 100 ms, for at most 100
samples/10 seconds, then deletes the owned NPC and releases its model. Stop,
loading/player replacement, missing ownership, invalid data, clock/tick gaps
and lane departure also end the attempt without recreation.

The goal is to measure the engine's natural travel speed for the same clip,
because the prior three-speed experiment established fixed animation cadence
but no suitable physical stride reference. Native locomotion may choose a
different clip; the reader preserves that outcome instead of forcing playback
or inventing a successful calibration. It excludes the first second and the
destination stopping region from fitting and requires two actual phase wraps,
contiguous valid clocks, bounded lateral/height variation and complete cleanup.
Engine timing remains separate from the user's visible foot-contact/sliding
observation. Animation-rate control and two-real-client multiplayer remain pending.

Source-bound verification passed **186 adapter mocks**, **36 reference-controller
tests** and **185 utility tests**, including **32 native-reference reader
fixtures**, at **17:18:18**. Nine current source hashes were rechecked after the
suites. Certificate:
`Build/definitive-native-reference-check/20261012-171817-verified-5849a82e9eec4b5fb94443f026cbef0f/test-result.json`.
All **16 installed hashes** and exact reference configuration verify after the
three-script update and explicit Smoke selection. Pre-update bytes, manifest
and restore mapping are in
`Build/definitive-update-backups/20261012-171818-native-gait-reference-fa088030/`.
The DLL, loader, command definitions and C++ did not change.

Current plan: `Build/definitive-manual-test/20261012-171827-native-reference-ready-fc711b03/`,
discoverable through `current-native-reference-test.json`. Installation snapshots
and the previous full CLEO baseline are retained. Post-install RequireReady
diagnostics at `Build/definitive-diagnostics/20261012-171827-e102e4f9a98f48e2b2f02b23c3820e9e/`
passed with GTA closed. Gameplay/input belong to the user for this test; root has
sent no game launch or input. This controller opens no transport and needs no
synthetic peer. The earlier server expired normally with zero players.
The actual engine reference and scoped human observation are **verified**.
Physical foot-contact/no-sliding, multiplayer stride matching and rate
implementation flags remain **false** until separate appropriate evidence exists.

### Latest completed seven-stage GTA measurement

**The previous installed gait probe completed an actual seven-stage GTA measurement.**
At **12:26:01–12:26:32**, one synthetic peer submitted **570 updates** and
**405 distinct positions** in **30.003 seconds**, with all seven stages, exit 0,
no run/cleanup error and no game departure. The unchanged `PED/Walk_civi` clip
remained approximately **1.1333 seconds per cycle** at commanded translation
speeds **0.5/1.0/1.5 units per second**. This measures fixed playback cadence;
it does not calibrate foot stride or fix sliding. One owned
remote ped per connection can emit read-only transform and animation-phase
observations at most once per **100 ms**, for at most **300 samples/30 seconds**.
The samples retain actual timestamps, clip identity and age since the current
task request. They never assume a new clip starts at phase zero. Existing
one-second readbacks are shared, with no extra query when both observations are
due. Cleanup, clock discontinuities and the budget end the probe.

Only the two installed adapter scripts changed; animation tasks/rates, native
commands, DLL, loader, protocol, C++ and the proven smoothing policy are unchanged.
Source-bound checks pass **180 adapter mocks** and **153 utility tests**, including
**69 peer mocks**. All **16 installed hashes** verify. The separate audit currently
passes **66 fixtures**; this is validation of the reader, separate from the actual
gameplay measurements.
Certificates are under
`Build/definitive-stride-check/20261012-121336-verified-8eee296d920b490bbbcbb879f6a57451/`.
Old installed bytes are preserved at
`Build/definitive-update-backups/20261012-121339-movement-stride/`.

The new opt-in `stride-speed-sweep` helper uses seven stages on the same six-unit
flat lane: idle, slow +X, idle/turn, medium −X, idle/turn, fast +X, idle. Defaults
are **30 seconds/radius 5**, with **0.5/1.0/1.5 units per second** and nominal
stage durations **2/12/2/6/2/4/2 seconds**. Existing helper profiles are preserved.
Plan: `Build/definitive-manual-test/20261012-121416-stride-host-ready/`, through
`current-stride-test.json`. Its baseline is the completed smoothing log below.
Server **6740**, session **45ad570fff9c44d2ad377e50a04aa6c0**, was freshly verified
for this plan; scheduled expiry remains **12:44:16 local time**. The agent launched
through Steam at **12:14:18**; actual game **14068** was born at **12:14:29**.
The probe recorded **263 valid samples** over **29,901 ms**, finished before owned
deletion, and observed all seven tasks playing with phase progression on ped
**6153**. Controlled End at **12:27:44**, normal shutdown/Exit at **12:30:30**,
game closure, all 16 installed hashes and all five tested source hashes verify.
Both full and scoped strict readers pass: one completed session, zero active
sessions/issues/failures. Reviewed result: `assessment-final-reviewed.json` in
that plan; the current stride pointer is **completed**. Fresh post-exit diagnostics
find GPU/PnP OK/0, unchanged boot and no new grouped dump identity or collector
issue/blocker. No crash was observed during this trial. Server identity was
reverified at **12:32:20**. Sampled slow/fast walking poses and an occluded medium
pose remain separate visual evidence. Foot stride/contact, visible smoothness,
absence of sliding and two real clients remain unverified. No animation-speed
setter has been applied; next work needs a calibrated foot-contact reference
before bounded rate control.

### Completed October 12 movement smoothing

**The installed movement-smoothing candidate passed an actual GTA trial.**
The **11:52:02–11:52:32** straight-lane peer completed all five stages with
575 updates, advancing idle/walk/idle/walk/idle playback and owned deletion.
Bounded telemetry recorded **574 display writes** against **190 changed received
targets**, with **14 windows** where display writes exceeded target updates and
**7 sampled intermediate poses**. This verifies real-time bounded blending with
separate network cadence. It does **not** prove visible smoothness or stride
matching. End at **11:53:18**, normal shutdown/Exit at **11:54:40**, game closure,
all 16 installed hashes and all five tested source hashes are verified. Both the
unfiltered and scoped strict readers pass with one completed session, zero active
sessions, issues or failures.

Only the two owned game-adapter scripts changed. Active Online presentation now
yields at **16 ms**, while a separate **50 ms** deadline gates bridge/network
work. Received targets and displayed poses are separate: XY and heading blend
within a bounded 50 ms segment, heading takes the shortest arc, and received Z
snaps to the body center. There is no extrapolation or bridge catch-up burst.
Discontinuities reset the blend, animation inference uses received targets, and
existing stop/readiness/ownership cleanup remains. The actual trial adds runtime
readback/counter evidence; visible smoothness and gait matching remain unverified.

Source-bound verification passes **149 adapter mocks** and **135 utility tests**,
including **51 peer mocks**. The separate smoothing audit passes **47 fixture
checks**. All **16 installed hashes** verify. Native DLL, loader, protocol and
C++ source are unchanged. The interrupted **08:10** preparation ended normally
at **08:19:10** while the adapter was armed and unactivated; it did not load our
bridge, connect or run a peer. Its immutable evidence remains separate from the
fresh trial. Boot is unchanged; that interruption is not a recorded PC crash.

Completed current plan/baseline:
`Build/definitive-manual-test/20261012-114540-smoothing-host-ready/`, discoverable
through `current-smoothing-test.json`. Server **6740**, session
**45ad570fff9c44d2ad377e50a04aa6c0**, has scheduled expiry **12:44:16 local time**.
Its exact identity and listening endpoint were reverified after game exit at
**11:57:52**, recorded in `server-post-session-status.json` in that plan.
The agent launched through Steam under the existing authorization at **11:45:51**;
actual game PID **5816** was born at **11:46:02**. Home registered at **11:51:46**.
The reviewed result is `assessment-final-reviewed.json` in that plan, and
`current-smoothing-test.json` records **completed**. Separate inspected frames show
outbound right-facing and return left-facing walking poses, apparently at street
height, then absence. Still frames cannot establish continuous gait or no sliding.
Current source/install certificates are under
`Build/definitive-smoothing-check/20261012-080403-verified-fa4b14e38fbb404ebb8d0bf6772c7535/`.
Fresh post-exit collection reports GPU/PnP OK/0, unchanged boot and no new grouped
dump identities or collector issues/blockers. No PC crash was observed in this
run. The preceding completed calibration is preserved below. Earlier lag/sliding,
whole-PC crash causes and two real clients remain unresolved. Next work is visual
movement/stride assessment followed by two-real-client testing.

### Completed October 12 straight-walk calibration

**The October 12 straight-walk calibration completed all five stages in GTA:**
idle → walk out → idle/turn → walk back → idle, followed by owned deletion,
controlled End at **06:51:18** and normal shutdown/Exit at **06:52:49**. GTA is
closed. The helper ran **06:48:55–06:49:25**, submitting **575 updates** and
**232 distinct positions**, with exit **0** and no run or cleanup error. Game
player **3** displayed synthetic player **4**, ped **8450**. All **30 playback
readbacks** report playing=true with advancing phases; requested and observed
XYZ match exactly. A captured return-leg frame shows the actor facing left while
moving toward decreasing X with heading **90**, and apparently grounded feet.
That single frame does not establish smoothness, stride alignment, both facing
directions or every idle stage. The October 11 report of lag/sliding remains open.

This change is confined to the synthetic peer and its tests. The installed
adapter, native bridge, loader, protocol and **all 16 installed files remain
unchanged and verified**. **118 adapter mocks** and the full **135 utility tests**
pass, including **51 peer mocks**. The opt-in `straight-walk-calibration` profile
uses five nominal six-second stages, a fixed first anchor, a parallel lane three
units away, and six-unit legs at **1 unit/second**. Default orbit and the older
idle/walk/idle profile retain their behavior. Render interpolation has not been
installed; two real game clients and playable multiplayer remain unverified.

Current completed calibration evidence:
`Build/definitive-manual-test/20261012-062924-calibration-host-ready/`, discoverable
through `current-calibration-test.json`, with the reviewed result at
`assessment-final-reviewed.json`. Both the unfiltered full-session and scoped
strict readers pass: one completed session, zero active sessions, issues or
failures. Peer logs are in
`peer-064855-d27dd4a390854aac8995ed9ac5a81bbc/`; targeted game-control actions and
images are under `Build/definitive-game-control/`. The replacement server is
PID **8884**, session **ac589aeabda047a1ac1a314450ee06db**, started **06:28:45** with
scheduled expiry **07:28:45 local time**. The October 11 server expired normally
with zero players; its process and port were absent before today's replacement.
Post-exit collection verifies the known executable, all 16 owned hashes and
GPU/PnP OK/0, with the boot unchanged from October 11 19:09 and no new grouped
dump identities or collector issues/blockers. This successful test
does not identify the earlier PC-freeze cause.

The user explicitly authorized automated Steam launch and game controls on
October 12. The agent launched through Steam, resumed the observed save, moved
outside, activated Home, ran the bounded peer, verified End and exited normally.
No save was overwritten and no device, drive, security or game-executable setting
changed. Recovery remains discoverable through
`Build/definitive-recovery/current-checkpoint.json`; preserve the raw logs,
source-bound certificate, control images and separate visual limitations.

### Preserved October 11 results

**The installed walking/idle candidate completed an actual idle → walk → idle
synthetic trial.** The user confirmed moving feet and disappearance, while reporting
lag and sliding. Controlled End at **20:40:57** and normal shutdown/Exit at
**20:41:21** are verified.
That candidate passed **118 adapter mocks** and **34 peer-helper
mocks**. All **16 installed files** verify. The verified no-offset setter and
owned cleanup remain, with state-transition-only built-in PED tasks and bounded
observed-playback/position checks, including stationary idle. The fresh server
was PID **20868**, session **e7c5e1a7aac5435aa62daab1e81c889d**, on
`127.0.0.1:22040`, scheduled to expire **October 11 21:27:52 local time**. Full live identity,
owned endpoint, nonce-matching welcome and requested probe departure passed;
zero UPDATE packets were sent. No synthetic peer or game launch/input was
automated during preparation.

Current animation plan/baseline:
`Build/definitive-manual-test/20261011-202954-animation-host-ready/`, through
`current-animation-test.json`. At **20:38:45–20:39:15**, game player **8** displayed
synthetic player **9**, ped **5898**. The helper completed all three stages with
**574 updates**, **385 distinct positions**, exit **0**, and no run/cleanup error.
The game logged requested and observed `idle_stance → Walk_civi → idle_stance`,
with **30 valid playback readbacks** (5 initial idle, 21 walk, 4 final idle),
advancing normalized animation phases, zero XYZ error, then deletion. The user
confirmed visible walking legs and disappearance, with **lag and sliding**.
Initial/final idle and foot ground height were not explicitly visually confirmed
in this trial; the earlier position trial's grounding result remains separate.
The final assessment is `assessment-final-reviewed.json` in this plan directory.
All **16 installed hashes** and tested code hashes still verify after exit.
Fresh collection found unchanged boot and no new grouped dump identity.

The earlier **20:37:37** attempt lasted **1.897 s** and reached only initial idle:
game player **5** timed out; helper **6** departed normally, with exit **1** because
it had no synthetic motion. The game subsequently processed its queued snapshot
and played idle on ped **8709**, then deleted it and reconnected. A seven-second
game-log gap coincides with this timeout; its cause is unconfirmed. This failed
full-sequence attempt is preserved and must not be erased by the successful retry.
Waiting for an anchor counts toward the deadline and can shorten final idle;
helper exit 0 alone is insufficient to establish complete or visible playback.

The unchanged full-session strict reader reports **incomplete** because the first
idle-only ped never moved; its event order, cleanup, stop and exit flags all pass,
with zero issues/failures. A clearly documented projection of original lines for
retry player 9/ped 5898 passes the same strict reader. Keep these scopes distinct;
the earlier interrupted run is not a full-sequence success.

**The body-center position candidate now has actual game readback evidence.**
During the October 11 **20:00:47-20:01:17** synthetic test, game player 4 created
ped **8706** for synthetic player **5**, applied and read back **29 distinct
transforms**, then logged deletion. Requested and observed **X/Y/Z match exactly**
in all 29 samples; every Z error is **0**. Maximum heading difference is
0.00006103515625 degrees. The separate helper exited 0 after 30.009 seconds,
submitted 575 distinct synthetic positions, and reported no run or cleanup error.
It observed one stationary game position, so its remote-motion flag is false.

This verifies body-center coordinate writes/readback on this owned remote ped.
The user confirmed appearance, movement, disappearance and feet **on the ground**,
with sliding and no floating or sinking. The helper copies CJ's Z while moving
at different X/Y; this location's observation does not prove general terrain alignment.
That completed position test had no walking animation; the proxy remains frozen
and collision-disabled in the installed animation candidate too.
The final strict reader **passes**, with no issues or failures: controlled End at
**20:07:25**, then normal host shutdown and exit at **20:08:08**. Human visual
confirmation remains separate from automated rendering flags. Current immutable
evidence is in
`Build/definitive-manual-test/20261011-195314-position-host-ready/audit-200818423094-final/`;
the peer output is in that plan's `peer-200047-3ddade44b51247488342e01b64e13936/`.

The earlier **15:15** experiment used the older setter: the user confirmed visible
appearance, circling movement and disappearance, but reported floating and no
animation. That run exited normally at 15:24:10 without a recorded End stop.
Do not combine its visual confirmation or missing-stop result with the new
candidate. That position candidate matched its **87 passing Node mocks** and
all **16 installed files** verified before this test; source/native bytes did
not change during that run. Two real game clients and playable multiplayer
remain unverified.

The latest user-reported PC crash left the CLEO log at **18:57:26**, boot-script
scanning, with no adapter-entry/bridge/network marker. Windows records an unclean
restart at **19:09**; collected 141 records repeat older dump identities, with
no newly grouped dump for this attempt. Cause remains unresolved. The user
reported a GPU undervolt afterward; its values and gameplay effect are unverified.

Full access restored direct command execution. Server PID **5248**, started by
the user at 19:13, passed a fresh UDP welcome/requested-departure probe at 19:36.
It served `127.0.0.1:22040` until its scheduled **20:13:01 local expiry**. The bounded
synthetic peer above has finished; no second GTA client was used.
At 20:14, the server process and endpoint were absent, and its log recorded
`stopped` with zero players. No process was killed. The October 11 replacement
server described above was started directly by the agent at **20:27:52**.
The one manual base-game control completed: the user confirmed gameplay and
normal exit with the owned loader bypassed. No new boot, grouped dump identity
or CLEO-log change was found in the collected interval. At **19:50:56**, the exact
loader was restored and **all 16 installed hashes verified**; fresh RequireReady
collection passed. The Online/manual profile is active again. This single control
does not identify the earlier crash cause. The current position plan/baseline is at
`Build/definitive-manual-test/20261011-195314-position-host-ready/`, discoverable
through `current-position-test.json`, which links `assessment-final-reviewed.json`
and records the completed position trial. Console/server/helper operations are handled by the agent;
game launch, gameplay, hotkeys and exit were manual in those October 11 trials.

The branch contains a read-only preflight, a separate Windows x64 transport DLL,
one custom UDP server and a CLEO Redux game adapter. Diagnostic, Smoke, corrected
signed-coordinate submission/self-echo, and manual Home/End activation have
actual game-runtime evidence. The 14:38 session stopped with End and exited
normally at 14:58:39. The newer remote test also exited normally at 15:24:10,
but has no recorded End stop, so its strict full-session gate did not pass.
The user reports pressing End before closing; registration was not observed in
the log, and its cause is unconfirmed.
Human visual observations are stored separately from the
log reader, whose rendering/multiplayer flags remain false by design.

**Game launches and controls were kept manual after repeated PC freezes/forced
reboots, until the user's October 12 automation authorization.** October 11 failures at 13:29, 14:18 and 14:21 ended at the recorded CLEO
boot-script scanning boundary, before adapter entry or server join; fresh 141
dump identities are preserved. Their causes remain unresolved. Online waits for
**Ctrl+Shift+Home** before loading our bridge or connecting; **Ctrl+Shift+End**
cancels while armed or stops afterward. That gate does not postpone ASI/CLEO/plugin
boot stages. A newly observed boot at 15:05:37.5 has no user-confirmed cause and
must not be labeled a user-reported crash. Windows does contain a new 15:05
141 dump identity predating this game launch. The remote peer exited normally;
that historical game closure and all 16 owned hashes were verified at 15:24.
The current body-center trial closed normally at 20:08, with all 16 hashes reverified.

The historical October 3 scope, superseded by the October 13 MTA-reuse request,
was **a smaller separate online client and
one dedicated server**. Existing MTA server/protocol/Lua/resource compatibility is
outside the initial scope. The target is on-foot free roam with two real DE players
first, then additional gameplay in steps. This avoids most full-MTA feature work
and the external `netc` dependency, but DE still needs a new game-thread/player
adapter. A different installation path cannot supply that adapter.

The existing classic installation and installed MTA client/server have not been
modified. The DE executable is unchanged; 16 independently owned mod/runtime
files were installed for testing and their bytes are preserved. The owned loader
has been restored after a single control test, with all 16 hashes verified.

## Verified local installations

| Installation | Root directory | Observed executable |
| --- | --- | --- |
| Classic GTA San Andreas | `C:\Program Files\Rockstar Games\Grand Theft Auto San Andreas` | `gta_sa.exe` |
| Installed MTA 1.6 | `C:\Program Files (x86)\MTA San Andreas 1.6` | `Multi Theft Auto.exe`; client modules under `MTA\` |
| Steam Definitive Edition | `C:\Program Files (x86)\Steam\steamapps\common\GTA San Andreas - The Definitive Edition` | `Gameface\Binaries\Win64\SanAndreas.exe` |

The DE root-level `PlayGTASanAndreas.exe` is a separate launcher/bootstrap. It must
not be mistaken for the game executable. The installation also contains `Engine\`,
`Gameface\Content\Paks\`, and `Gameface\Content\rageunreal\`.

| File | Size in bytes | PE machine | PE format |
| --- | ---: | --- | --- |
| DE `Gameface\Binaries\Win64\SanAndreas.exe` | 89,497,064 | AMD64 (`0x8664`) | PE32+ (`0x020B`) |
| DE `PlayGTASanAndreas.exe` | 376,816 | AMD64 (`0x8664`) | PE32+ (`0x020B`) |
| Classic `gta_sa.exe` | 5,685,688 | I386 (`0x014C`) | PE32 (`0x010B`) |
| Installed MTA `MTA\core.dll` | 2,996,592 | I386 (`0x014C`) | PE32 (`0x010B`) |
| Installed MTA `MTA\game_sa.dll` | 1,107,824 | I386 (`0x014C`) | PE32 (`0x010B`) |
| Installed MTA `MTA\multiplayer_sa.dll` | 439,664 | I386 (`0x014C`) | PE32 (`0x010B`) |
| Installed MTA `MTA\netc.dll` | 7,795,592 | I386 (`0x014C`) | PE32 (`0x010B`) |

DE Windows file/product version: **`1.0.113.49697469`**.
The fixed numeric Windows version fields are **`1.0.113.21181`**, matching the
Steam build in CLEO Redux's support table. The display string retains the larger
private component; `49697469 % 65536 == 21181`. This resolves the apparent version
mismatch, but does not replace a runtime compatibility test.
Steam app ID: **`1547000`**; locally installed Steam build ID: **`16391096`**.
These are observed identifiers, not claims of supported builds.

The game PE declares preferred image base `0x140000000`, entry-point RVA
`97140736`, and image size `97141760` bytes. Its header timestamp is `1731481822`
(`2024-11-13T08:10:22+00:00`). The timestamp is an unverified header value, not
the installation date; the preferred base is not a runtime address under ASLR.

SHA-256 fingerprints observed on 2026-10-03:

```text
DE game:       63e633bb51c35fe35ca98f503b62e7ddb8403abe4c9d501cfa9162892453846f
DE bootstrap:  f5a82bae35948975b5ca3cf2fca7bc6154c3a4a877f4945d69338643cf7ff2b7
Classic game:  73b4195d442f5e0ff82bbd27bc21c60c14acc215d4121fd19718a308785d273e
MTA core:      2190b19f00a76a86ae6900aa2e1bd9ec662aa1f44710eb1a9748acd7d7db8987
MTA netc:      ae0058d2c10d0cec6573f5bc4024d8ea32fee27f9784de347a0ecebf21f2eef9
```

Rerun the preflight after a game update. These hashes identify files for future
research; none is an approved DE hook profile.

The checked-out source declares MTA **1.7.0**, with a custom development build type
in `Shared/sdk/version.h:44-48`. The installed client is MTA 1.6. Do not assume the
installed networking module matches this source's interface or protocol version.

## Rendering evidence

Microsoft `dumpbin /imports` on the installed DE game found:

- Direct imports of `dxgi.dll` and `d3d11.dll`, including `D3D11CreateDevice`.
- A `d3d9.dll` import for `D3DPERF_BeginEvent` and `D3DPERF_EndEvent`.
- Direct imports of `D3DCOMPILER_43.dll` and `WINMM.dll`.
- Delay imports of `d3d12.dll` and `steam_api64.dll`.

This is static evidence. The D3D9 profiling imports do not establish that the game
calls `Direct3DCreate9`, which is MTA's current initialization trigger. The presence
of D3D11/D3D12 imports does not establish the renderer selected at runtime. A future
prototype must observe the actual device/swap chain and game-thread lifecycle.

The full local import listing is saved in
`Build/definitive-edition/SanAndreas-imports.txt` (ignored generated evidence).

## Source blockers and implications

Line references below refer to the source baseline recorded above. These describe
the work needed for full MTA compatibility. The separate-client scope avoids B09/B10
by using a new value-based adapter boundary, B11 through a new custom protocol, and
B12 by deferring classic resources. The classic module restrictions do not apply
to the new independent x64 build. DE memory/ABI/lifecycle discovery remains required.

| ID | Verified finding and source | Required work |
| --- | --- | --- |
| B01 | Non-x86 client builds are excluded in `Client/launch/premake5.lua:43`, `Client/loader/premake5.lua:51`, `Client/loader-proxy/premake5.lua:33`, `Client/core/premake5.lua:74`, `Client/game_sa/premake5.lua:50`, `Client/multiplayer_sa/premake5.lua:41`, `Client/gui/premake5.lua:44`, and `Client/mods/deathmatch/premake5.lua:67`. | Add an isolated x64 client path and audit dependencies. Removing the exclusions alone cannot port the implementation. |
| B02 | `Client/loader/Main.h:14-16` names classic executables. `Client/loader/GameExecutablePatcher.cpp:41` accepts I386/PE32; its `Load` function at line 393 requires a particular classic file size/hash after restoring its own known patches. | Implement independent DE executable selection and version validation. Never feed the DE executable into the classic patcher. |
| B03 | `Client/loader/GameExecutablePatcher.cpp:154` redirects the classic WINMM import. `Client/loader-proxy/main.cpp:132` uses `GetVersionExA` as a startup trigger, and line 558 parses `IMAGE_NT_HEADERS32`. | Establish an x64 bootstrap with a verified initialization point. The classic bootstrap assumes a different executable and import layout. |
| B04 | `Client/game_sa/CGameSA.cpp:66` binds a reference to `0xB5F878` during static initialization; line 68 reads `0x59FB4C`. `Client/game_sa/CGameSA.h:29-35` defines classic singleton addresses. | Resolve DE equivalents independently, using verified build profiles and validated signatures. Classic `game_sa` is unsafe to load in DE even before normal setup. |
| B05 | `Client/multiplayer_sa/multiplayer_hooksystem.cpp:16-53` uses DWORD addresses and five-byte x86 branch patches. Numerous hooks use naked functions and inline assembly. | Replace game hooks with x64-compatible implementations, preserving instruction boundaries, register state and the Windows x64 ABI. |
| B06 | Classic layouts are asserted in `Client/game_sa/CEntitySA.h:229` (`0x38`), `CPedSA.h:355` (`0x79C`) and `CVehicleSA.h:415` (1440 bytes). `CVehicleSA.h:574/579` contains pointer-to-32-bit-integer casts. | Verify DE object layouts, pools, vtables and function calling conventions. Audit pointer width throughout reused client code. |
| B07 | `Client/core/DXHook/CDirect3DHook9.cpp:41` hooks `Direct3DCreate9`; lines 82-91 initialize network/game/multiplayer/XML/GUI from that call. `Client/core/CCore.cpp:1060-1062` passes an `IDirect3DDevice9*` to GUI initialization. | Separate bootstrap from graphics and provide a renderer/GUI integration for the observed DE runtime. |
| B08 | `Client/core/CCore.cpp:1041/1051` loads `game_sa` and `multiplayer_sa` unconditionally. `Client/multiplayer_sa/multiplayersa_init.cpp:40` immediately installs classic hooks. | Select isolated DE implementations before any classic module initialization. |
| B09 | `Client/sdk/game/CEntity.h:59` exposes `CEntitySAInterface*`; `CPools.h:73/81/90` exposes `CVehicleSA`, `CObjectSA`, `CPedSA` and DWORD-based interfaces. `CRenderWare.h:85-94` exposes TXD/DFF/RpClump operations. `Client/sdk/multiplayer/CMultiplayer.h:117-137` also exposes classic types. | Create engine-independent boundaries and adapt their callers. Adding `game_de` alone is insufficient. |
| B10 | `Client/mods/deathmatch/logic/CClientVehicle.cpp:24-25` includes classic game headers; `CClientGame.cpp:4313` accesses an `SClientEntity<CPedSA>`. | Remove classic implementation assumptions from retained deathmatch/entity logic. |
| B11 | `Client/core/CCore.cpp:1095` loads external `netc.dll` and checks its interface. `utils/buildactions/install_data.lua:13/78` downloads that module; the installed one is x86. | Establish an x64 networking implementation compatible with the chosen server protocol. Its source/build availability has not been established here. |
| B12 | Resource graphics/model APIs depend on classic engine integration and RenderWare formats. | Define and test a Lua/resource compatibility matrix; do not promise that existing resources will all work. |

Scale indicators: the game and multiplayer integration directories contain 348
files, including 346 C/C++ source/header files. Searches found 601 lines mentioning
`__declspec(naked)` and 1,228 mentioning `__asm`. These are textual counts, not a
count of distinct hooks or a development-time estimate.

## Code likely reusable after adaptation

The dedicated server, resource management and much server Lua logic are separate
from the injected client game implementation. Shared math, XML, serialization and
protocol definitions are also potential reuse candidates. Client interpolation,
resource downloading, entity bookkeeping and Lua VM integration can be assessed
after removing classic implementation dependencies.

Reuse does not establish wire compatibility, gameplay equivalence, or resource
compatibility. Existing-server support requires validating network interface
versions, packets, model IDs, synchronization semantics and each exposed API.

## Public project findings

- MTA's [compatibility page](https://wiki.multitheftauto.com/wiki/Where_to_buy_GTASA)
  explicitly excludes Definitive Edition.
- The [MTA discussion about Definitive Edition](https://github.com/multitheftauto/mtasa-blue/discussions/2420)
  records discontinuation of a DE multiplayer effort on 2023-04-17. This is
  historical project context, not a technical impossibility proof.
- The [Steam product page](https://store.steampowered.com/app/1547000/) specifies
  a 64-bit processor/OS requirement. The local PE inspection confirms AMD64.
- Rockstar's [release announcement](https://www.rockstargames.com/mx/newswire/article/25ak3481oa1384/grand-theft-auto-the-trilogy-the-definitive-edition-now-available)
  identifies the remasters as using Unreal Engine. Local Engine/Gameface/Paks
  directories are consistent with that announcement.

### Separate source repository worth researching

[`trilogy-multiplayer/game`](https://github.com/trilogy-multiplayer/game) contains
another DE multiplayer implementation. The inspected default branch ends at
[`95e7b6df4095ec8503682df66fb35610a6e445c8`](https://github.com/trilogy-multiplayer/game/commit/95e7b6df4095ec8503682df66fb35610a6e445c8)
(2023-12-06). Its README does not document exact supported game builds. Repository
metadata/tree inspection found no LICENSE/COPYING file; reuse permission remains
unestablished. No source was imported into this fork.

Relevant research leads:

- [Memory initialization](https://github.com/trilogy-multiplayer/game/blob/95e7b6df4095ec8503682df66fb35610a6e445c8/trilogy-client/memory/memory.cpp#L45):
  x64 signatures and MinHook.
- [Renderer initialization](https://github.com/trilogy-multiplayer/game/blob/95e7b6df4095ec8503682df66fb35610a6e445c8/trilogy-client/renderer/renderer.cpp#L165):
  D3D11 Present interception through Kiero.
- [Game hooks](https://github.com/trilogy-multiplayer/game/blob/95e7b6df4095ec8503682df66fb35610a6e445c8/trilogy-client/hooking/hooks/hook_game.cpp#L84):
  script processing/game-state initialization and player creation through opcodes.
- [Ped layout assumptions](https://github.com/trilogy-multiplayer/game/blob/95e7b6df4095ec8503682df66fb35610a6e445c8/trilogy-client/sdk/sdk_ped.hpp#L10):
  matrix offset `0x18`, position offset `0x30`, ped size `0x840`.

Inspected initialization does not demonstrate exact executable fingerprint
validation, and several signature results are used immediately. Its offsets,
signatures and networking must be tested independently against the installed
Steam build. Do not assume this repository is the discontinued project referenced
in MTA's discussion; affiliation was not established.

[CLEO Redux's DE FAQ](https://github.com/cleolibrary/CLEO-Redux/blob/master/docs/en/the-definitive-edition-faq.md)
is another version-specific scripting research lead. Its listed San Andreas
versions differ from the full local file-version string. This does not establish
either compatibility or incompatibility with this build. No CLEO component was
installed or incorporated.

## Local development environment

Visual Studio Community 2026 and Build Tools 2026 were found. Community includes
MSVC `14.51.36231`, the x64 compiler, MSBuild and `dumpbin`. Python `3.14.7` and
PowerShell 7 are available. The standard June 2010 DirectX SDK header path was not
found; no wider SDK availability audit or full MTA C++ build was performed. The
new transport build succeeds with the installed Windows SDK and does not require
that legacy DirectX SDK.

The original audit changed no C++ source. The later separate-client prototype adds
isolated C++ files under `Client/definitive` and `Tests/definitive`, using its own
MSVC build helper rather than altering classic premake projects. The repository
formatter must run after these and future C++ changes as required by `AGENTS.md`.

## Preflight usage and evidence

The new utility requires Python 3.9 or newer, uses only its standard library,
and inspects exact known executable
paths. It reports PE architecture, header metadata, byte size, SHA-256 and bounded
direct/delay import inventories. It keeps
the bootstrap separate from the game and reports `multiplayer_supported: false`.
An exit code of zero means the inventory completed, never that DE supports MTA.

From the repository root, generate the local report with:

```powershell
New-Item -ItemType Directory -Path 'Build/definitive-edition' -Force | Out-Null
python utils/definitive_edition/preflight.py `
  --game-dir 'C:\Program Files (x86)\Steam\steamapps\common\GTA San Andreas - The Definitive Edition' `
  --classic-dir 'C:\Program Files\Rockstar Games\Grand Theft Auto San Andreas' `
  --mta-dir 'C:\Program Files (x86)\MTA San Andreas 1.6' `
  --output 'Build/definitive-edition/local-inventory.json'
```

`--output` requires a new `.json` file outside all supplied installation roots;
existing reports are never overwritten. Use a fresh report name for subsequent
runs, or omit `--output` to print JSON without creating a report. The output
directory must already exist. Missing, malformed or non-AMD64 required game files
return exit code 2 and an incomplete inventory. Optional files have individual
presence/error results, so read them even when the required-game inventory succeeds.

The JSON report and import listing are ignored build artifacts. Keep the summary
in this tracked Markdown file updated after reruns. Never commit commercial game
executables or private Steam/account data as research evidence.

Run the utility's tests with:

```powershell
python -m unittest discover -s utils/definitive_edition -p 'test_*.py' -v
```

Validation completed on 2026-10-03:

- **17 unit tests passed**, using synthetic PE32/PE32+ files and temporary directories.
  They cover truncated/malformed headers and imports, missing/wrong game targets,
  bootstrap separation, hashing, optional binaries and output write protection.
- The CLI successfully inspected the actual DE/classic/MTA installations and saved
  `Build/definitive-edition/local-inventory.json`, with no required-target errors.
- DE engine imports: 46 direct DLL names and 20 delay DLL names. All 66 unique names
  agree with the independent Microsoft `dumpbin /imports` listing.
- All four inspected installed MTA client DLLs are x86. The optional classic
  `gta-sa.exe` variant is absent; `gta_sa.exe` is present.
- Reports retain `experimental_port_required` and `multiplayer_supported: false`.
- Independent review checked parser bounds, installation write protection,
  compatibility claims and the source references in this tracker.

These checks verify the inspection tool. They do not exercise a DE multiplayer
client, run the classic client, or establish networking/gameplay compatibility.

## Separate-client implementation and validation

The [standalone client README](Client/definitive/README.md) records build/run
instructions, protocol layout, DLL ownership and limitations. Implemented files:

- `Client/definitive/MtaMath.h`: directly includes existing MTA math headers.
- `Client/definitive/Protocol.h`: standalone explicit packet codecs, bounded
  validation and sequence handling. It does not reuse MTA's incompatible test
  MockBitStream as a production transport.
- `Client/definitive/TransportClient.cpp`: nonblocking x64 Winsock client,
  welcome retries, per-session snapshots, transform submission and disconnect.
- `Client/definitive/Bridge.cpp`: transport C ABI with no automatic game hooks or
  game-memory reads. Socket setup/cleanup occur through explicit calls.
- `Client/definitive/Probe.cpp`: synthetic motion for independent network tests.
- `Client/definitive/build.ps1`: separate x64 build using installed Visual Studio.
- `Server/definitive/server.py`: one Python standard-library UDP server, protocol
  limit 16 sessions, 20 Hz snapshots, five-second expiry and bounded receive work.
- `Tests/definitive`: native literal packet/math checks and Python server/native
  integration tests.

MTA reuse is deliberately small: `Shared/sdk/CVector.h`, `CVector4D.h`, and
`SharedUtil.Defines.h` are included unchanged. The wrapper supplies standard
`size_t` declarations and avoids defining legacy `WIN32`, whose release assertion
macro would complicate an independent build. Clamped interpolation uses the
existing vector operations; it is a helper, not a running entity interpolator.

Additional reuse findings: `CInterpolator.h` depends on `Utils.h`, `SFixedArray`,
`Lerp` and `Unlerp`; `Shared/animation/TInterpolation.h` depends on easing/Lua/timer
implementation. Whole ped/vehicle client classes remain classic-engine-dependent.
`Shared/sdk/net/bitstream.h` is an interface with broad SharedUtil dependencies,
not a standalone networking implementation. A new small protocol is simpler for
the accepted one-server scope.

Validation performed for the new prototype:

- MSVC x64 C++17 compilation with `/W4 /WX` succeeded for the probe, DLL and tests.
- Native tests passed **293 protocol and MTA math checks**, including independently
  encoded byte fixtures, malformed/truncated packets, session mismatch, nonfinite
  transforms, sequence wrap and unchanged output on decoding failures.
- **28 Python tests passed**, including 26 server tests and two real native
  integration tests. They verify two distinct native clients receiving each
  other's changing positions, the DLL ABI receiving remote movement, and peer
  departure removing the remote state.
- `dumpbin` confirmed the bridge is AMD64/PE32+ and exports the seven documented
  transport functions. It does not load the classic MTA game/network modules.
- Ran `./utils/clang-format.ps1`; the new C++ files pass the pinned formatter's
  `--dry-run --Werror` verification. Existing tracked MTA source remains unchanged.
- No test launches GTA. Probe results explicitly report `game_adapter_active: false`.

Generated binaries are under `Build/definitive-online` and remain ignored.
The server defaults to loopback `127.0.0.1:22040`. Tests have not established
LAN/Internet play, loss/jitter behavior, loaded-game stability or actual gameplay.
Endpoint/token checks validate sessions; this is not an account or anti-cheat
system. The server relays transforms supplied by clients and does not simulate
authoritative gameplay.

## Milestones and acceptance criteria

| ID | State | Deliverable | Acceptance criterion |
| --- | --- | --- | --- |
| M00 | Complete | Source audit, branch, executable inventory and tracker | Preflight passes fixture tests and inventories the installed game while reporting no multiplayer support. |
| M01 | Manual Diagnostic/startup passed | Independent x64 diagnostic bootstrap | October 4 Diagnostic completed with normal closure; October 11 Online passed bridge/ABI/build/runtime gates and the corrected stop shortcut, then exited normally. Automated startup stability remains under investigation. |
| M02 | Partially verified | Build profile and game-thread callback | Exact hash/host/SDK guards passed in the game; cooperative reads ran for five minutes. Save/new-game transitions and crash stability remain pending. |
| M03 | Local reads/submission/echo verified | Local player adapter | October 11 game sessions submitted changing position/heading, including negative Y, and decoded server self-echoes. The 15:15 synthetic peer also produced remote creation, changing transforms and deletion. |
| M04 | Deferred | Custom graphics and input integration | Optional diagnostic overlay/UI; remote peds initially use the game's own rendering and controls through the verified game adapter. |
| M05 | Synthetic transport and one game client verified | Networking foundation | Two native clients exchange movement; the October 11 real game client also joined, submitted changing transforms and decoded self-echoes. Two real game clients remain M06. |
| M06 | One-game proxy, grounding, walking, bounded smoothing and gait phase measurement have runtime evidence; visual gait/smoothness and two real clients pending | First multiplayer proof | October 11 trials established exact XYZ, visible grounding and moving feet, with lag/sliding. October 12 calibration/smoothing complete five-stage playback and End/exit; the seven-stage gait trial adds 263 samples and approximately 1.1333 s/cycle at three translation speeds. Current checks pass 180 adapter mocks, 153 utilities (69 peer mocks) and 66 stride-audit fixtures. Animation rate is unchanged; physical stride and no sliding are unverified. Two DE clients must see one remote ped each and synchronize position/rotation, disconnect and reconnect cleanly. A synthetic peer alone does not complete this gate. |
| M07 | Later scope | Gameplay synchronization | Vehicles, animation, weapons, damage/death, chat and world state are implemented and tested in steps after on-foot replication. |
| M08 | Outside initial scope | Lua/resources and assets | Consider only if the user later expands the smaller-client scope. |
| M09 | Private install implemented | Distribution and regression work | Pinned local package, owned-file install/uninstall and mode-switch checks exist; broader distribution and stability checks remain pending. |

The accepted small-client goal needs M01-M03 and M06 before its on-foot multiplayer
can be called playable. M05 proves synthetic transport independently; it does not
complete those game milestones. M07 adds further gameplay afterward, while M08 is
outside the initial scope. Keep unfinished items unfinished even if a DLL builds.

## Decisions and open questions

- **Target compatibility:** resolved by the user: separate smaller client, one
  custom server. Existing MTA servers/Lua/resources are outside the initial scope.
- **Networking dependency:** new protocol avoids `netc` entirely. Availability of
  x64 `netc` is unresolved but is no longer a blocker for this scope.
- **Runtime discovery:** CLEO loading and local player opcodes have actual runtime
  evidence. Low-level object layouts are bypassed; reset/entity lifecycle still
  requires validation.
- **Graphics:** the active D3D backend must be observed if a custom overlay is added.
  Initial ped rendering should use the game's engine through the adapter.
- **Resource parity:** deferred; model replacement, shaders and engine-control APIs
  are not included in the accepted smaller initial client.
- **Reference reuse:** establish source permissions and build compatibility before
  integrating any code from external DE projects.
- **Game updates:** each accepted build needs its own validated identity/profile;
  do not treat the version string or a matching filename as proof of compatibility.

## Work log and handoff

### 2026-10-03

- Created local branch `codex/definitive-edition-port` from the recorded baseline.
- Audited launcher/build, game, multiplayer, core and SDK boundaries in parallel.
- Inspected installed files, PE architectures, file version, Steam build ID and hashes.
- Saved a local `dumpbin` import listing without executing the game.
- Researched upstream support status and separate DE implementation references.
- Implemented the read-only Python preflight; all 17 fixture tests pass.
- Saved the local installation inventory and independently compared all 66 game
  import DLL names with Microsoft `dumpbin`; no differences were found.
- Created this tracker; runtime hooks, networking and multiplayer are still pending.

### 2026-10-03: scope narrowed to a smaller client and one server

- Accepted the user's separate-client scope; retained full-port findings as context.
- Reused MTA math headers directly in an independent x64 client component.
- Added one small custom UDP server, a native client probe and transport DLL ABI.
- Built all native components and passed 293 C++ checks plus 28 Python tests.
- Demonstrated two native synthetic clients exchanging movement, plus DLL-host
  movement/departure checks. No real game entities were controlled or synchronized.
- Reviewed DLL lifetime and made departure transmission best effort so allocation
  failures cannot prevent socket cleanup during destruction.
- Ran the repository formatter and verified all new C++ files against its pinned
  version; original tracked source and installed games remain unchanged.

For the next engineering step, complete M01 as an isolated x64 diagnostic component
before adapting gameplay. Revalidate this exact installed build before using any
hook or memory offset. Record every new assumption together with the test that
verified it.

When committing, include the original goal, the architecture mismatch motivating
the work, the reason for the smaller custom protocol and CLEO adapter,
the exact validation performed, and the limitations still open. Future commits
should carry the same motivation and testing detail rather than a generic
"add DE support" message.

## 2026-10-03: CLEO adapter implementation

### Chosen game interface

[CLEO Redux's DE support table](https://re.cleo.li/docs/en/the-definitive-edition-faq.html)
lists the installed game's numeric Steam version. The independent adapter uses
the runtime's JavaScript game commands, avoiding copied classic offsets or Unreal
object layouts. The chosen dependency is [CLEO Redux 1.5.1](https://github.com/cleolibrary/CLEO-Redux/releases/tag/1.5.1),
loaded through [Ultimate ASI Loader 9.7.4](https://github.com/ThirteenAG/Ultimate-ASI-Loader/releases/tag/v9.7.4)
as `version.dll` beside the actual Win64 executable.

The official x64 portable archive contains the runtime and plugins but no ASI
loader. It also omits command definitions; reproducible packaging pins the
[Sanny Builder definitions](https://github.com/sannybuilder/library/tree/34bedee0be2f57047ed8f0705c81f137ede3d3ce)
for `sa_unreal` (0.500) and `unknown_x64` (0.239).

| Dependency | Pinned artifact | SHA-256 |
| --- | --- | --- |
| CLEO Redux 1.5.1 | `cleo_redux_1.5.1.x64.zip` | `4205147c6b62a0393e4ed77f0e59cbbf52715601c5a1ec09c755b0a1320185dd` |
| Ultimate ASI Loader 9.7.4 | `Ultimate-ASI-Loader-NoPDB_x64.zip` | `e5860e7d9a1805267535b65749575b5e406cc6ea3325c7392189c578815045d1` |

### Implemented boundaries

- `BuildGuard.cpp` hashes the running game executable read-only with Windows
  BCrypt and accepts only the exact inventoried SHA-256. A Steam update fails
  closed until its build is investigated and separately accepted.
- Bridge ABI v2 adds integer-only transform bit patterns and indexed snapshot
  components. This avoids float-register FFI ambiguity and script-owned foreign
  buffers. The existing transport ABI remains available for native tests.
- `DE_BindRuntime` checks CLEO's host ID and SDK version, registers a documented
  `OnRuntimeInit` disconnect callback, and pins the bridge in the process. CLEO
  has no public unregister operation, so unloading that callback's DLL would be
  unsafe. Pinning ends at process exit; no socket setup runs under the loader lock.
- `DE_ConnectLocal` permits only `127.0.0.1:22040` after successful runtime binding.
  The game adapter remains a local experimental build; the transport probe's
  configurable endpoint is not an Internet-gameplay claim.
- `game-adapter/index.js` validates the host/runtime/definitions/bridge/build
  before invoking player commands. Smoke mode only reads the local ped's position
  and heading. Online mode sends those values and renders remote sessions as
  inert, collision-disabled model-7 peds using game-native commands.
- `controller.mjs` owns the proxies, waits for model streaming, rejects invalid
  snapshots before changing the world, removes departed peds, and cleans up on
  controlled disconnect, the stop shortcut, local readiness loss, and script exceptions.

These boundaries use the documented [JavaScript API](https://re.cleo.li/docs/en/api.html),
[integer FFI](https://re.cleo.li/docs/en/using-memory-64.html),
[script permissions](https://re.cleo.li/docs/en/permissions.html), and
[public SDK callbacks](https://raw.githubusercontent.com/cleolibrary/CLEO-Redux/1.5.1/SDK/cleo_redux_sdk.h).

### Lifecycle and licensing findings

CLEO starts gameplay scripts on new game/save load and supports automatic script
reload. No documented JavaScript forced-unload callback was found. `finally`
handles controlled exits/exceptions, but is not evidence of cleanup during forced
reload or engine shutdown. **Stop with Ctrl+Shift+End and close the game before editing or
removing the adapter.** New-game/load cleanup and crash stability still need real
game tests. Runtime disconnect protects transport identity; it does not prove
engine entity cleanup.

CLEO Redux is a separately licensed freeware runtime, not GPL source code. Its
[agreement](https://github.com/cleolibrary/CLEO-Redux/blob/1.5.1/LICENSE.txt) must
accompany a local bundle. Ultimate ASI Loader carries its
[MIT notice](https://github.com/ThirteenAG/Ultimate-ASI-Loader/blob/v9.7.4/license).
The Sanny definitions repository had no separate license file in the inspected
tree; its attribution and pinned source are preserved. The bundle is for local
experimental use. Public redistribution/licensing compatibility has not been
established. Repository code and reused MTA headers retain the repository license.

### Validation so far

- Rebuilt the x64 bridge/probe under MSVC `/W4 /WX`; all 293 native checks pass.
- 30 Python tests pass: 26 server tests plus four native integration checks.
  Added integer-bit snapshot/submit checks, rejection of NaN/out-of-range
  components, rejection of non-game runtime binding, and a read-only hash check
  against the installed game executable.
- The numeric Windows version fields were read independently with PowerShell.
- Controller tests and actual CLEO/game runtime validation are tracked below
  as their results become available. Source implementation alone does not complete
  M01-M03 or M06.

## 2026-10-03/04: runtime evidence, crash and recovery

The Smoke package installed 16 new owned files after exact game/package hash
verification. The game was launched legitimately through Steam. The user reported
a PC crash following the initial test and later confirmed the regular game worked
after reboot. No uninstall ran: the user declined removal and requested continued
development with the mod installed.

Windows recorded Kernel-Power event 41 at 21:45:33 on October 3, and a queued
LiveKernelEvent 141 referring to `WATCHDOG-20261003-2144.dmp`. Code 141 identifies
a display-engine timeout according to
[Microsoft's documentation](https://learn.microsoft.com/en-us/windows-hardware/drivers/debugger/bug-check-0x141---video-engine-timeout-detected).
It is a live-dump classification, not a blue-screen bugcheck by itself. Event 41's
BugcheckCode is zero. The cause and any connection to the mod are unproven. Older
queued GPU reports appeared at the same reporting time and must not be mistaken
for new failures. Protected report/dump access was unavailable; no ACL or driver
settings were changed.

The post-reboot CLEO log proves:

- 21:47:01: CLEO 1.5.1 x64 identified SA DE `1.0.113.21181`, SDK 7 and API 0.500.
- 21:47:35: the adapter passed its gates and logged startup with `mode: smoke`.
- 21:47:35 through 21:52:42: local ped 1 returned changing position and heading,
  with `online: false`. Interior transitions produced high Z values naturally;
  interior synchronization is outside the first outdoor online test.
- 21:52:45: CLEO logged `host shut down` and `Exit`.

Thus the user's working regular-game session also exercised the installed smoke
adapter. These reads prove the local adapter path, not networking or remote peds.

Recovery evidence is in ignored
`Build/definitive-recovery/20261003-215539/`: `cleo_redux.log`, the original install
manifest, `windows-events.json`, a source checkpoint and the damaged README copy.
The crash left `Client/definitive/README.md` containing NUL bytes; it was restored.
Other prototype text files passed the NUL/empty-file audit. The commercial
executable still has its original SHA-256 and all installed owned-file hashes
matched before switching modes.

On October 4, 27 Node adapter tests and 30 Python/native tests passed again after
recovery. `set-mode.ps1` was added to change only the installed mode/config hash
and manifest. It rejects a running game, changed executable and edited owned files,
flushes staged bytes before replacement, and restores originals on a normal write
failure. A system crash between its two replacements still requires manifest
review. Isolated Smoke -> Online -> Smoke tests maintained valid removal hashes;
an edited config was rejected before changes.

Online mode was then enabled and the single loopback server started. Current
session metadata/output lives under `Build/definitive-online-test/`, with
`current-session.json` naming the process and evidence directory. The synthetic
`game_peer.py` waits for a real transform and circles nearby. Its outputs explicitly
identify synthetic movement and do not claim game rendering or a second DE client.
Actual online results must be recorded below before marking M06 complete.

### October 4 online test: submission error and second forced restart

At 03:19:19, the first online run passed its startup gates and joined the local
server as player 1. Initial movement submission then raised
`TypeError: Error converting from js 'float' into type 'i32'`. The controller
cleaned up; the server recorded the requested departure and GTA continued running.
The game was exited through its UI before updating any installed script.

The adapter now uses standard JavaScript DataView conversion and signed 32-bit
IEEE bit patterns for FFI arguments, and limits its nonce to the signed integer
range. Native uint32_t arguments preserve those low bits. Twenty-seven mock tests
passed, including signed-bit argument assertions. `update.ps1` was added and
tested in an isolated installation: owned hashes remained valid, a second update
was idempotent, and prior file bytes were backed up. One installed file, index.js,
was updated with a matching manifest hash.

A second automated Steam launch followed at 03:23:54. Window capture/inspection
and later shell reads stopped responding. The user reported another PC freeze
and confirmed they had to force a restart. The next boot was at 03:29:37.
The preserved game log stops during CLEO's boot-script scan, before any new
`adapter_start`, `connecting` or `joined` record. Thus the corrected gameplay
script has no runtime execution evidence from this second attempt. The synthetic
peer was never started and no remote ped was created during either online attempt.

Windows recorded LiveKernelEvent 141 referring to `WATCHDOG-20261004-0324.dmp`,
additional code-193 reports referring to `WATCHDOG-20261004-0326.dmp`, and
Kernel-Power event 41 at 03:29:40 after reboot. These records are preserved without
claiming a responsible driver/module; the protected dumps have not been analyzed.

All further game launches and window automation were stopped. The installation
was retained as requested. The local server stopped with the forced reboot.
Evidence is preserved under `Build/definitive-recovery/20261004-033155`, including
the second game log, installed manifest and relevant Windows records. Source text
passed the NUL/empty-file audit again. Runtime stability remains unresolved;
startup, loader, overlays/graphics and automated launch/window interaction need
isolation before any further gameplay test. No driver or system settings were
changed. Do not mark the new conversion fix or remote-ped rendering verified.

## 2026-10-04: crash investigation and diagnostic profile

The user authorized investigation and further work after the second forced reboot.
No further game launch or window automation ran during this investigation. The
installation remains present, and neither graphics devices nor driver/OS settings
were changed.

### New observations

| Item | Read-only observation | Implication |
| --- | --- | --- |
| Graphics device | Radeon RX Vega, `Status=Error`, `ConfigManagerErrorCode=22`; independently `Get-PnpDevice` reports `Problem=22` | The device is currently disabled. This does not establish when or why it was disabled. |
| Driver | `31.0.21925.1001`, date 2026-05-20 | AMD identifies this as Adrenalin 26.5.2 for Polaris/Vega and lists RX Vega compatibility; compatibility does not prove stability. |
| OS | Windows 11 Pro, 26H2, `26300.9457`; last boot 03:29:37 +02:00 | Record the exact environment. Do not infer Insider enrollment from the major build number. |
| Game integrity | Original executable SHA-256 still matches; all 16 owned files verified | No executable patch or missing owned file was found. |
| Latest CLEO log | 2,100 bytes; last write 03:23:54; stops at boot-script scanning | No recovered record of ordinary script discovery, bridge loading, adapter startup or connection in the failed run. |
| Existing timeout history | Distinct dump references include September 30 and April/May, before this prototype, as well as October 3/4 | A graphics-timeout history predates the mod; this neither attributes nor excludes the new mod as a trigger. |

[Microsoft defines device code 22 as disabled](https://learn.microsoft.com/en-us/windows-hardware/drivers/install/cm-prob-disabled).
The user was asked whether they deliberately disabled it; the investigation has
not established the actor or timing. A bounded Kernel-PnP event query found no
record establishing the transition. The observed status is not a driver diagnosis.

The [AMD driver release notes](https://www.amd.com/en/resources/support-articles/release-notes/RN-RAD-WIN-26-5-2-POLARIS-VEGA.html)
map the observed version to 26.5.2. Microsoft's
[Windows release information](https://learn.microsoft.com/en-us/windows/release-health/windows11-release-information)
lists 26300.9457 as a general-availability release dated September 29, 2026. No
specific game/driver/OS incompatibility has been established.

Code [141 identifies a display-engine timeout](https://learn.microsoft.com/en-us/windows-hardware/drivers/debugger/bug-check-0x141---video-engine-timeout-detected);
code [193 identifies a DirectX graphics-kernel live dump](https://learn.microsoft.com/en-us/windows-hardware/drivers/debugger/bug-check-0x193--video-dxgkrnl-livedump).
Both are live-dump classifications, not blue-screen stop codes by themselves.
[Kernel-Power 41](https://learn.microsoft.com/en-us/troubleshoot/windows-client/performance/event-id-41-restart)
records the unclean reboot, not its initiating cause. Protected dump files remain
unavailable to this process. No responsible module has been identified from them.

### Source review and limits of attribution

The pinned [Dylib plugin](https://raw.githubusercontent.com/cleolibrary/CLEO-Redux/1.5.1/plugins/Dylib/dllmain.cpp)
logs a dynamic-library load request before `LoadLibrary`. That record exists in
the successful online session and is absent in the second failed startup log.
[Ultimate ASI Loader](https://raw.githubusercontent.com/ThirteenAG/Ultimate-ASI-Loader/v9.7.4/source/dllmain.cpp)
discovers `.asi` files; it does not automatically load our `.dll` nested under
`CLEO/de-online`. No CLEO boot scripts were found in the installed tree. Ordinary
scripts run on new-game/save load according to
[CLEO's lifecycle documentation](https://re.cleo.li/docs/en/script-lifecycle.html).

Our bridge imports only `bcrypt.dll`, `KERNEL32.dll` and `WS2_32.dll`, and contains
no graphics calls, custom DllMain or background worker. Its transport is explicitly
created on connection; it is not initialized in a DLL static constructor. No
confirmed source defect explaining a whole-machine freeze was found in that
review. Recovered milestones strongly suggest the second failure preceded bridge
loading, but forced reboot can lose log evidence. ASI Loader/CLEO bootstrap, game
startup, launch/capture interaction, graphics driver and hardware remain possible
investigation areas. None is established as the cause.

Smoke mode still loads the bridge, checks the executable and binds the CLEO reset
callback before local-player reads. It therefore does not isolate those startup
operations. The new Diagnostic mode isolates adapter execution more clearly while
retaining all installed files.

### Implemented and verified without launching GTA

- Added `Diagnostic` packaging/configuration mode. The entry logs HOST/CLEO
  metadata and returns before bridge loading, Memory FFI, native/game commands,
  controller construction, wait loops or transport. The source default remains
  Smoke. Normal CLEO script dispatch still occurs after a save/new-game starts;
  Diagnostic cannot instrument the earlier runtime/loader startup.
- Added bounded `adapter_phase` records around entry, bridge load, ABI validation,
  executable hashing and runtime binding. Failures include the last phase.
- Added `uninstall.ps1 -ValidateOnly`, which returns after the complete owned-file
  checks and before ShouldProcess/removal. Configuration/update/collection use
  that read-only path; no uninstall ran.
- Added `utils/definitive_edition/collect_diagnostics.ps1`. It saves device/OS/game
  integrity status, CLEO log, manifest, Windows events, JSON and Markdown under
  ignored `Build/definitive-diagnostics`. WER reports are grouped by dump identity,
  preserving duplicate reporting counts without treating them as new failures.
  `-RequireReady` returns exit 2 for failed prerequisites. A passing device check
  does not prove game stability. No game launch/window capture/device change occurs.
- Rebuilt all three staged packages from pinned, hash-verified cached dependencies.
  Tested Diagnostic -> Smoke configuration/update in an isolated installation;
  every owned file remained present and passed hashes. The update was idempotent
  after switching back to the matching current Smoke package.
- Changed the real installation to Diagnostic and updated two owned script/config
  files; all 16 files pass hashes. Prior bytes/manifest are preserved under
  `Build/definitive-update-backups/16e05ab183284daf9fac6c6f2a087301`.
- Passed 32 Node adapter mocks (including traps for prohibited Diagnostic access),
  30 Python server/native integration tests, 17 preflight tests and 293 compiled
  native protocol/math checks. No C++ changes were made in this investigation.
- The readiness collector returned the expected exit 2 for the disabled Radeon.
  Latest evidence, including the full OS release number:
  `Build/definitive-diagnostics/20261004-035250-f813d944c3434b8f966c12de3a24802f/`.
- Preserved all 29 prototype/tracker source files, with no empty/NUL files, in
  `Build/definitive-recovery/20261004-035252-investigation/source-checkpoint.zip`.
  Its `checkpoint.json` records the archive SHA-256, Diagnostic mode and unresolved
  GPU/runtime status. This is a local recovery artifact, not a Git commit.

### Next acceptance sequence

1. Device-health prerequisite completed at 06:21 on October 4: Radeon status OK,
   problem code 0. The earlier disabled-state reading did not establish hardware
   failure or the cause of the freezes. Recheck prerequisites before later tests.
2. Analyze the existing WATCHDOG dumps when access is available, retaining the
   report/module evidence. Do not claim a culprit from event codes alone.
3. After reviewing that evidence and rechecking device/file health, perform a
   user-controlled Steam launch in Diagnostic mode with no automated window
   capture. Record whether startup, save load and exit remain stable. This is an
   isolation experiment, not a confirmed crash fix.
4. Progress separately to Smoke, then Online local-transform submission, then one
   short synthetic remote-ped test. Only then attempt two real clients. M06 remains
   incomplete; the float-bit correction has not been verified inside GTA.

Keep goals, motivation, implementation reasoning, the console test results and
unresolved game-test limitations in any future commit message. This tracker and
on-disk recovery/source checkpoints provide continuity independently of chat
memory.

## 2026-10-04 06:21: graphics device healthy; manual Diagnostic run ready

The user reported that the GPU was back in a healthy state. Independent read-only
queries at 06:21:52 +02:00 confirmed:

- `Win32_VideoController`: Radeon RX Vega, `Status=OK`,
  `ConfigManagerErrorCode=0`, active resolution 1920 x 1080.
- `Get-PnpDevice -PresentOnly -Class Display`: `Status=OK`, `Problem=0`.
- Driver remains `31.0.21925.1001`; this work performed no device/driver change.
- No SanAndreas or game-bootstrap process was running.

The user also reports being able to launch/play the game with low resource use.
That report is recorded as user evidence, not evidence of a damaged GPU. Device
status and the earlier timeout records do not establish why the automated tests
froze. Low utilization alone does not identify a triggering component.

The collector completed successfully with `-RequireReady` at 06:22:44, with no
blockers. The game executable still matches the verified SHA-256 and all 16 owned
files pass integrity checks. Evidence is saved under
`Build/definitive-diagnostics/20261004-062231-0e01b7a7331247f689db83a1e32d7d37/`.
The installed mode remains Diagnostic. No game launch, window capture or OS/device
change was performed. The recovered CLEO log remains the 2,100-byte 03:23:54 log;
there is no new Diagnostic runtime result yet.

Next manual step: launch San Andreas Definitive Edition through Steam, Resume/load
a save (or start a new game), play briefly, then exit normally. Title-menu startup
alone does not dispatch the ordinary adapter script. Inspect the new CLEO log for
`adapter_phase` with `phase: entry`, `mode: diagnostic`, followed by
`adapter_diagnostic_complete` with `bridge_loaded: false`,
`game_adapter_active: false` and `network_active: false`. F11 is not needed because
Diagnostic returns immediately. This verifies script dispatch/completion while
keeping bridge/network/gameplay operations out of this stage. It does not verify
the float-bit fix or remote-ped replication; those remain later acceptance gates.

## 2026-10-04 06:29: manual-session evidence tooling

The user requested continued work. At 06:29, the game was closed and the CLEO log
still ended at the historical 03:23:54 startup; the Diagnostic profile had not
produced a new runtime result. The next manual-launch/save-load status was
requested while independent source work continued. No game was launched by this
work, and the installed profile remained Diagnostic.

Saved a baseline log, installed manifest and expected marker schema under
`Build/definitive-manual-test/20261004-063134-d4db023408a243babc0e60d6836aaadd/`.
This allows a future result to be compared to preserved pre-test evidence rather
than assuming that an old log is a new session.

Implemented `utils/definitive_edition/session_log.py`, a standard-library text-log
reader with optional bounded watching. It accepts only an ordered Diagnostic
entry and completion with literal false bridge/gameplay/network activity flags.
It parses CLEO's quoted JSON messages, scopes results to the latest adapter entry,
and reports failures, native Dylib load requests and shutdown evidence separately.
Watch mode rejects stale success after unrelated appends or header rewrites. Both
file size and actual reads are capped at 16 MiB. It invokes no game process, DLL,
window capture or graphics API.

Commands:

```powershell
python -B utils/definitive_edition/session_log.py
python -B utils/definitive_edition/session_log.py --watch-seconds 300
```

Exit 0 establishes Diagnostic markers only. Exit 2 means incomplete, failed or
watch timeout, distinguished in JSON. Neither result establishes stable gameplay,
rendering, GPU health or multiplayer. One-shot analysis covers the existing log;
watch mode starts with a baseline and requires fresh markers.

Twenty-nine fixture tests pass, including the actual CLEO quoted-message format,
marker order, latest-entry handling, strict boolean flags, library-load evidence,
stale append/rewrite cases, missing/oversized files and bounded watch behavior.
All 46 utility tests pass together (29 reader tests plus 17 existing preflight
tests). A one-shot check correctly returned incomplete/exit 2 for the unchanged
installed historical log. No C++ changes were made. The controlled Diagnostic
game session and M06 remain pending actual runtime evidence.

The 60-second installed-log watch finished with `watch_timeout`, last status
`incomplete`; no fresh Diagnostic marker arrived during that interval. Its JSONL
output is preserved as `session-watch.jsonl` in the manual-test baseline directory.
This is a missing runtime observation, not a failed game run. A later manual run
can be analyzed with the one-shot reader after the user reports completion.

Recovery checkpoint: `Build/definitive-recovery/20261004-063958-log-reader/`
contains 31 prototype/tracker source files and archive-hash metadata. The source
audit found no empty or NUL-containing files. Commit messages should retain the
goal, motivation, acceptance-test reasoning and distinction between passing
fixtures and the still-pending actual game session.

## 2026-10-04 07:12: manual Diagnostic passed; Smoke prepared

The user reported completion of the manual launch/play/exit test. The fresh CLEO
log and independent reader/source review confirm:

| Time (+02:00) | Evidence |
| --- | --- |
| 07:08:21-22 | CLEO 1.5.1 x64 identified host `sa_unreal`, build `1.0.113.21181`, SDK 7 and definitions 0.500; both plugins loaded. |
| 07:08:50 | Runtime initialized after save/new-game load, discovered and loaded index.js. |
| 07:08:50 | `adapter_phase` entry, mode Diagnostic; `adapter_diagnostic_complete` with bridge/gameplay/network flags all false. Script was disposed. |
| 07:12:21 | `host shut down` and `Exit` records. |

The log is 3,268 bytes, SHA-256
`639798bf8269496d91bef08c8d4e040268258b9da9313c11a45e4a94fd5f45b5`,
different from the preserved pre-test 2,100-byte baseline. The reader returned
`diagnostic_complete`/exit 0, with no adapter failure, bridge load or active
gameplay/network evidence in this session. No game/bootstrap process remained
at inspection. The Radeon still reported status OK and device code 0.

The read-only prerequisites passed again. Preserved session log, Diagnostic
installation manifest, OS/device status, Windows records and
`diagnostic-session-result.json` are under
`Build/definitive-diagnostics/20261004-071343-dbddb0cc52024cdb8249b323792de9d0/`.
Collected relevant System events contain no entry after this session started.
This is bounded event/log evidence, not proof that every possible error source
was absent. The original forced-reboot cause remains unresolved.

The event inventory also exposed an older `LiveKernelEvent 1b0` referring to
`WATCHDOG-20261004-0357.dmp`, reported at 06:23:18. It predates the 07:08 manual
session. [Microsoft identifies 0x1B0 as a video-miniport failure live dump](https://learn.microsoft.com/en-us/windows-hardware/drivers/debugger/bug-check-0x1b0--video-miniport-failed-livedump).
Its queued parameters are redacted, so add-device versus start-device failure
and a responsible module cannot be determined from this record. Do not classify
it as a failure of the successful Diagnostic session or proof of damaged hardware.

After preserving Diagnostic evidence, changed only the installed config and
matching manifest mode/hash to Smoke. The current Smoke package required no
additional file update; all 16 owned files verified. No game/server launch,
window capture, driver change or C++ change occurred during this work.

The next manual test exercises bridge load, ABI/build guards, CLEO reset binding
and local ped coordinate/heading reads using the current script. Smoke does not
connect/pump/submit transport, load remote models, create peds or write transforms.
Launch through Steam, load a save, move CJ briefly, press F11 to stop the adapter,
then exit normally. Expected evidence: startup phases through `runtime_bound`,
`adapter_start` with `mode: smoke`, changing `local_transform` records with
`online: false`, `adapter_stopped` with reason F11, and normal runtime shutdown.
Online submission and remote-ped commands remain later acceptance stages.

The prepared Smoke baseline log, installed Smoke manifest and explicit test plan
are saved under `Build/definitive-manual-test/20261004-071820-smoke/`.

## 2026-10-04 07:26: current Smoke passed; fullscreen shortcut corrected

The user completed the manual Smoke run and reported that F11 toggles fullscreen.
Independent log review verified the current script and bridge path:

- CLEO startup at 07:24:49; adapter entry at 07:25:20.
- Bridge load, ABI check and exact executable guard passed; CLEO lifecycle binding
  reached `runtime_bound` at 07:25:21, followed by `adapter_start`, mode Smoke.
- 75 `local_transform` records from 07:25:21 through 07:26:35, all ped 1 and
  `online: false`. Y changed from -1707.45 to -1686.64, heading changed, and Z
  transitioned from the house interior (1014.74) to outdoors (13.54).
- `adapter_stopped`, reason F11, at 07:26:35; script disposed; normal host shutdown
  and `Exit` at 07:26:55. No adapter/plugin failure or networking/remote-ped event.
- GPU status remained OK, code 0, with the game closed at inspection.

The 19,550-byte log has SHA-256
`d6e2b9b7aaa297d2da56424a7873151d14a861548c831ebf4d154ba312b4e164`.
Preserved log, installed Smoke manifest, device/OS/Windows evidence and
`smoke-session-result.json` are under
`Build/definitive-diagnostics/20261004-073016-113fbdd22e85494082b67ef9f9b4ca59/`.
This establishes the current bridge/local-read path for one manual session. It
does not establish online submission, remote rendering or the original crash cause.

### Shortcut and online-evidence changes

Changed adapter stop input from F11 to **Ctrl+Shift+End** (virtual keys 17, 16, 35).
F11 is no longer polled by the adapter. The
[pinned CLEO Input implementation](https://raw.githubusercontent.com/cleolibrary/CLEO-Redux/1.5.1/plugins/Input/src/impl.rs)
confirms `IS_KEY_PRESSED` reads held state; `IS_KEY_DOWN` is edge-only. Held-state
checks avoid losing the chord between 50 ms ticks. The adapter only reads these
keys and does not suppress the game's fullscreen control. Startup metadata and
controlled-stop reason now identify the new chord. Diagnostic still exits before
any input read. The new chord awaits actual game evidence.

Added `online_transform` evidence, bounded to once per second and emitted only
after a successful native submission and complete snapshot validation. It records
the owning player ID, successful-submission count, submitted coordinates and the
server's echoed own-player state, or null before an echo is available. Submitted
and echoed values remain separate because snapshots can lag movement. Counters
reset on reconnection. This enables a single real client to test the signed-bit
FFI correction and return decoding without a synthetic peer or remote-ped creation.
A `joined` record alone cannot verify those operations.

All 49 Node adapter mocks pass, including complete/incomplete/split-key chords,
F11/F12 remaining inactive for the adapter, negative-coordinate submission and
unsigned snapshot decoding, echo logging bounds, invalid/failing transport paths
and reconnection counters. No C++ changes were made. Rebuilt all three staged
packages, set the real installation to Online and updated two owned script files.
All 16 owned hashes verify; prior bytes/manifest are under
`Build/definitive-update-backups/585ead51eaa34c5091aa7b8f9db27213/`.

Next: start one loopback server, then manually launch DE, go outdoors, move briefly,
hold Ctrl+Shift+End to disconnect, and exit normally. Confirm changing, nonzero
submitted and echoed values (including negative Y), rising submission counts,
controlled stop and server departure. The first test uses only the game client;
no synthetic helper is started. Real remote-ped and two-client acceptance remain
pending after this transport proof.

The loopback server started at 07:37:57 with PID 15148, bound only to
`127.0.0.1:22040` and verified by its `listening` record and owning UDP endpoint.
It uses `--duration 1800` and is expected to stop around 08:07:57 +02:00. Evidence,
the pre-test Smoke log, installed Online manifest and session metadata are under
`Build/definitive-online-test/20261004-073757/`; `current-session.json` points to
this run. No game or synthetic peer was launched. PID metadata alone must not be
used to stop a process after reboot or expiry without verifying its identity.

## 2026-10-11: manual Online session after reboot; server absent

The user completed the requested manual test after restarting Windows. The boot
time was 12:02:00.5 +02:00. Independent log review found the following evidence:

| Time (+02:00) | Evidence |
| --- | --- |
| 13:08:36 | CLEO 1.5.1 x64 identified the supported DE build, SDK 7 and definitions 0.500; both plugins loaded. |
| 13:10:10 | Save/runtime initialization dispatched the Online adapter. All nine phases through `controller_run` passed, including bridge load, ABI, executable guard and runtime binding. |
| 13:10:10-21 | Eleven `connecting` and eleven `transport_lost` events, with one-second retries; no `joined` or `online_transform` event. |
| 13:10:21 | `adapter_stopped`, reason `Ctrl+Shift+End`; script disposed. This is actual game evidence for the corrected shortcut. |
| 13:11:59 | Normal `host shut down` and `Exit` records. |

The current log is 6,255 bytes, SHA-256
`fbbfbe907c7f25e4162dbe9f4804655a60dfca2109ada4f11683497b7387a7fe`.
There is no `adapter_failed` event. At inspection the game was closed and UDP
port 22040 had no listener. The old temporary server metadata referred to October
4, PID 15148, and an expiry of 08:07:57 that day. That server could not survive
the later Windows restart. The missing server explains the unsuccessful
connection attempt; this result does not verify online coordinate submission or
the signed-bit FFI fix. No remote-ped command was reached and M06 remains pending.

The read-only collector passed again: exact game SHA unchanged, all 16 installed
owned files verified, installed mode Online, Radeon RX Vega status OK and device
code 0. Saved log, manifest, device/OS/events and `online-session-result.json` are
under `Build/definitive-diagnostics/20261011-132103-17e7dca8929048f78379071cfffc2065/`.
The stale server metadata and its historical stdout were also preserved there.
That stdout contains an un-timestamped join/timeout; it cannot establish a join
in the October 11 game session.

Collected relevant Windows records contain no entry after this game's 13:08:36
startup. The event inventory includes `WATCHDOG-20261011-1201.dmp`, code `1b0`,
which predates this session and reboot. It also exposes historical October 4
07:58 (`141`) and 08:01 (`193`) dump identities among queued reports. These are
preserved findings, with no demonstrated association to a particular missing
game log or server join. Replayed reports are grouped by dump identity and are
not counted as separate crashes. The earlier freeze cause remains unresolved;
the current bounded shutdown/log evidence does not establish general stability.

No game launch, window capture, synthetic peer, driver change, uninstall or game
adapter change was performed during this inspection. The installed Online mode
and scripts remain in place. A new server must be started after reboot before
repeating the one-client submission/echo acceptance test.

### Repeatable server startup and validation

Added `utils/definitive_edition/start-server.ps1` (PowerShell 7):

```powershell
pwsh -NoProfile -File ./utils/definitive_edition/start-server.ps1
```

It starts the standard-library server hidden, bound only to `127.0.0.1:22040`,
for 1,800 seconds by default. `-DurationSeconds` accepts 60-7,200 seconds;
`-PythonPath` can select an executable. Python is identified without loading the
native game bridge. All occupied binds on UDP 22040 cause refusal, without
reusing or stopping an existing process. A new instance is verified by executable,
quoted command line, creation time, listening JSON and owning loopback endpoint.
The helper preserves unique stdout/stderr and durable session metadata, publishing
`current-session.json` only after readiness. Failure cleanup uses its retained
process instance and verified creation time, never a stale PID from metadata.
It installs no automatic startup task and performs no game/window action.

The first live helper check exposed a log-sharing exception while reading the
redirected stdout writer. Its own new process was stopped and no UDP listener
remained; failure metadata is preserved under
`Build/definitive-online-test/20261011-132410-94f2310016cc4dd4877601b1b411e9da/`.
Changed that reader to explicitly share read/write access, dispose streams, read
at most 32 lines, and retry partial JSON. Parsing and argument-quoting checks pass.

The corrected helper started PID 8972 at **13:25:13 +02:00**, with expiry around
**13:55:13 +02:00**. A second invocation correctly refused the occupied port and
left that instance running. A short protocol-only probe, with GTA closed, verified
WELCOME after the launcher exited and sent LEAVE. The server logged requested
departure for probe player 1 / source port 65233. This probe was not a game client,
did not load the native bridge or create a ped, and does not establish submission
or multiplayer acceptance. No synthetic gameplay helper was run.

Successful launch, protocol check and server logs are under
`Build/definitive-online-test/20261011-132513-bb04d7c4cca14170b994affe2c96b22b/`.
The final endpoint check confirmed only `127.0.0.1:22040`, owned by PID 8972;
stderr was empty. These records distinguish the launcher probe from the next
actual game join. No C++ or game adapter files changed during this continuation.

Next: manually launch DE through Steam while this server is running, go outdoors,
move CJ for about one minute, hold Ctrl+Shift+End briefly, then exit normally.
Inspect fresh `joined`, increasing successful-submission counts and changing
submitted/echoed transforms, plus controlled stop and server departure. Restart
the helper after Windows restarts or the server expires. Remote-ped creation and
two real DE clients remain later gates.

Recovery checkpoint: `Build/definitive-recovery/20261011-132700-server-restart/`
preserves the prototype sources and this tracker with archive-hash metadata.
Commit messages should explain the smaller-client goal, reboot/server failure,
launcher choices, checks performed and which game-runtime gates remain pending.

## 2026-10-11 13:29: manual startup freeze; server restarted

The user reported another PC crash and requested a server restart, expressing
uncertainty about the mod versus the GPU. Windows booted at **13:29:59.5 +02:00**.
The current log is 2,100 bytes, SHA-256
`867880a6dd5d6bea00ef384c3f869fda4031b7f6d1b5d8d9469a03d19ba4221a`.
Its records are all at 13:29:15: CLEO host/build recognition, SDK/API registration,
Input/Dylib plugin loading, then beginning the boot-script directory scan. There
is no ordinary-script discovery/load, `adapter_phase`, bridge-load request,
connection event, remote-ped event, host shutdown or `Exit` record.

Independent event review confirms a **fresh** `LiveKernelEvent 141`, referencing
`WATCHDOG-20261011-1329.dmp`, Application record 39937, reported at
13:30:14.9206216 after reboot. [Microsoft identifies 0x141 as a display-engine
timeout live dump](https://learn.microsoft.com/en-us/windows-hardware/drivers/debugger/bug-check-0x141---video-engine-timeout-detected).
It is not itself a blue-screen bugcheck or proof of defective GPU hardware.
The report's parameters are redacted, so no responsible driver/module can be
resolved from the event. The session cannot read the protected dump (access
denied); standard Windows SDK x64 `cdb.exe`/`kd.exe` paths are also absent. No
ownership/permission change or debugger installation was attempted.

Kernel-Power 41, System record 32142, was recorded at 13:30:03.0464386 with
BugcheckCode 0. [Event 41 records an unclean restart and does not establish its
cause](https://learn.microsoft.com/en-us/troubleshoot/windows-client/performance/event-id-41-restart).
Event 6008 at 13:30:08.4995441 reports a previous-shutdown time of 13:22:08;
that value conflicts with the later game startup and must not be substituted for
the observed freeze chronology. The reported `193` dump identities are historical,
not evidence of another new `193` failure during this run.

Inference: a graphics timeout occurred during early game startup. The adapter's
game reads, networking and remote-ped code have no execution evidence in this
recovered session. The installed ASI loader, CLEO and its plugins were already
active, however; their role cannot be excluded. Hardware, driver, game startup
and loader interaction remain unresolved possibilities. A healthy post-reboot
device query cannot decide among them. Startup isolation or dump analysis is
needed before attributing the failure or repeating the Online game test.

The read-only collector passed after reboot: Radeon RX Vega status OK, code 0,
driver 31.0.21925.1001 unchanged, exact game hash unchanged, all 16 installed
owned-file hashes verified, Online mode retained and game closed. Log, manifest,
OS/GPU records, Windows events and `crash-session-result.json` are preserved under
`Build/definitive-diagnostics/20261011-133153-4d92e78a2770460188bb0115b32c5c46/`.
The earlier server's only join/departure records were the explicitly identified
launcher probe; they do not show a game join before this freeze.

Restarted the server with the validated helper at **13:32:00 +02:00**, PID 20180,
bound only to `127.0.0.1:22040`, expiring around **14:02:00 +02:00**. Process
identity, listening log and owning endpoint passed; an independent endpoint
inspection confirmed it. New server evidence is under
`Build/definitive-online-test/20261011-133159-a2e060dccc434d64bc723d71b0d10274/`.
The stale prior PID was not used to control a process. No game, window capture or
synthetic gameplay helper was launched; no mod, game, GPU or driver setting was
changed. This continuation changed only documentation and preserved evidence.

Recovery checkpoint: `Build/definitive-recovery/20261011-1329-crash/` includes
the 32 prototype/tracker source files and archive-hash metadata. Online game
submission/echo, remote peds and two-client multiplayer remain unverified.

## 2026-10-11 13:36: first real game-client submission/echo passed

The user reported completion of a further manual session. The fresh game log and
independent review establish actual game-to-server communication:

| Time (+02:00) | Evidence |
| --- | --- |
| 13:35:43-44 | CLEO startup, supported DE host/build, SDK 7, definitions 0.500 and both plugins loaded. |
| 13:36:29 | Save/runtime initialization dispatched the Online adapter; all nine startup phases passed. One connection attempt joined as player 1. |
| 13:36:29-52 | 23 `online_transform` telemetry records; the first echo is null, then 22 decoded self-echoes. Logged successful-submission counts strictly increase from 1 to 331. |
| 13:36:52 | `adapter_stopped`, reason `Ctrl+Shift+End`; script disposed. |
| 13:37:00 | Normal `host shut down` and `Exit`. |

Submitted and echoed values are finite and bounded, with changing position and
heading. Submitted Y ranges from **-1707.449951171875 to -1659.89990234375**, and
negative Y returns correctly in the server echo. Z changes from the interior
coordinate 1014.7421875 to outdoor values near 13.30205154; these coordinate values
do not establish interior/world synchronization. The final logged submission and
echo match exactly: `(2498.498291015625, -1662.9393310546875, 13.302051544189453,
293.294921875)`. Movement samples can differ from their echoed values because
snapshots are asynchronous.

This verifies the signed-bit FFI conversion fix and own-snapshot return decoding
in an actual joined game session. **331 is a recorded lower bound on successful
bridge submissions**, not a count of telemetry records or proven individual
server receipts, and not necessarily the total before stop. There are no
`adapter_failed` or `transport_lost` events. The matched server log records one
join for player 1 / source port 49517, followed by requested departure for the
same identity and port. Server stderr is empty. No synthetic/helper peer ran
against that server. Remote-ped commands and two real clients remain untested.

The game log is 12,128 bytes, SHA-256
`9cea4a1dede43f76facb6504ceba3fefdc8792968d4466b22fb170ea72b96f29`.
Read-only prerequisites passed: all installed hashes/executable verified, Online
mode retained, game closed and GPU status OK / code 0. Windows boot time remains
13:29:59.5. Bounded relevant event review found no new System/LiveKernel report
after the 13:35:43 startup. This successful session does not resolve the earlier
startup freeze or establish general stability.

Preserved log, installed manifest, device/Windows records, actual server logs and
`online-session-result.json` are under
`Build/definitive-diagnostics/20261011-133756-5b90b984c72a4eb2a9db7832b4447f82/`.
M03 now includes local read/submission/echo runtime evidence; M05 includes one
real game client. **M06 remains pending** remote rendering and two real clients.

### Preventing a new peer's temporary origin proxy

A source review of the next remote stage found that JOIN previously published a
player's default `(0,0,0,0)` before the first accepted UPDATE. The synthetic helper
waits for a usable real-player transform before submitting, so the game could
create its proxy at the map origin during that interval. This is a source finding,
not an observed remote-game failure; no remote-ped test has run.

Changed only `Server/definitive/server.py` and `Tests/definitive/test_server.py`:
snapshots still go to every joined recipient, preserving session liveness, but
publish records only after a valid UPDATE sets that player's sequence. Empty
snapshots remain valid; deliberately submitted zero coordinates and sequence 0
remain publishable. The wire format and native bridge are unchanged.

**29 server tests and 4 native integration tests pass (33 total, no skips).**
Coverage includes waiting recipients receiving empty session-bound snapshots,
first valid publication to all peers, rejected UPDATEs remaining unpublished,
valid zero state, session restart hiding the prior identity, full 16-player
published capacity, departure and CLI/native compatibility. These tests do not
launch GTA or execute remote-ped commands. No C++ change required formatting.

With GTA closed, replaced the old test server only after verifying its retained
process instance, creation time, executable, exact command line and owned endpoint.
The updated server started at **13:42:42 +02:00**, PID 9176, bound only to
`127.0.0.1:22040`, with expiry around **14:12:42 +02:00**. Evidence is under
`Build/definitive-online-test/20261011-134242-86b7aa1516f04a9a9b0f574bd5aeeda1/`.
Readiness passed. Installed game scripts were not changed; no game or synthetic
gameplay helper was launched by this work.

The next gameplay gate is a short remote-ped test after startup investigation:
while the manually launched game is stable outdoors and submitting coordinates,
start the bounded helper; require game-side creation and changing applied-transform
records plus visual confirmation, then deletion when the helper departs. Two real
DE clients are a later, separate acceptance check. No further game launch has
been requested during this continuation.

Recovery checkpoint: `Build/definitive-recovery/20261011-1336-online-passed/`
preserves all 32 prototype/tracker source files and archive-hash metadata. Commit
messages should describe the smaller-client goal, why uninitialized players must
not be rendered, the 33 off-game checks, actual one-client evidence and still-open
startup/remote-player acceptance work.

## 2026-10-11 14:12: remote experiment prepared; loader comparison preserved

The user authorized proceeding. No game, game window, screenshot, synthetic peer,
driver setting or installed mod was activated/changed during this preparation.
Actual installed state remains **Enabled / Online**, with all 16 owned hashes
verified by read-only collection at 14:04:39. GPU status is OK / code 0, the
supported executable hash matches, and the game is closed. This establishes
prerequisites only; the earlier startup freezes remain unresolved.

### Separate synthetic peer: reliable failure results

Review found that a disconnect exception in `game_peer.py` could suppress its
final JSON result or replace the original failure. The helper now reports
`cleanup_error`, retains an earlier failure, and returns failure when disconnect
alone fails. Its origin-wait comment now reflects the server's accepted-UPDATE
publication gate. No new game command or bridge change was needed.

**24 mocked peer tests pass.** A guard rejects accidental native-library loading.
Coverage includes timeout/rejoin without placeholder transforms, bounded orbit
updates, distinguishing received motion from synthetic motion, excluding the own
identity, rejecting unusable anchors, leaving when the selected peer departs,
connect/pump/submit failures, interruption, and result preservation on cleanup
failure. These tests do not verify real game rendering.

### Fresh remote log evidence

Added `utils/definitive_edition/remote_session_log.py`, with **38 fixture tests**.
It reads at most 16 MiB, understands quoted CLEO JSON, scopes to the latest adapter
entry and can subtract a saved pre-run baseline after append/truncation/replacement.
It cannot accept a stale prior success or the synthetic helper's own stdout as
game evidence. A pass requires a joined Online identity, a different remote
identity with a valid ped handle, at least two changed finite/bounded transforms
for each lifecycle, matching cleanup before Ctrl+Shift+End stop, then host shutdown
and Exit. Reconnects and reused handles cannot combine separate lifecycle evidence.
Recorded adapter/cleanup failures or inconsistent event order reject acceptance.

The result is named `remote_command_sequence_passed`. Both `rendering_verified`
and `multiplayer_verified` remain false even on a pass. Log markers show recorded
commands; they cannot prove the ped was visible or that deletion had an engine
effect. Human visual observations and two real game clients remain separate gates.
The complete utility suite passes **108 tests** (17 preflight, 29 Diagnostic-log,
24 mocked peer and 38 remote-log). No game or DLL is loaded by these utility tests.

### Reversible startup comparison without removing the work

Added `Client/definitive/set-loader.ps1` and
`Tests/definitive/test_loader_toggle.ps1`. Status is read-only. Disable, with GTA
closed, renames the manifest-owned `version.dll` to
`version.dll.de-online-disabled`; Enable restores it. All 16 files are validated
before any rename, including exact package/manifest paths, link-free ancestors and
hashes. The helper refuses both/missing loader names, collisions and mismatches;
it never overwrites a destination. It writes a flushed preparation journal before
the rename and completion metadata afterward. Recovery uses current file ownership
and hashes, so a stale journal cannot authorize a conflicting rename.

**22 fixture checks pass, none skipped; `realGameUsed: false`.** Coverage includes
unchanged bytes/manifest/mode, WhatIf, idempotence, collision/missing-file refusal,
duplicate or missing manifest entries, traversal/link refusal, unrelated hash
failure and interrupted journal state. Real Status validates all 16 owned files
and reports Enabled. **No real Disable or Enable action was performed.** The mod
files stay installed as requested. Restoring the original loader name is required
before install/update/mode/uninstall validation. A comparison result would still
need repeated manual evidence before assigning a crash cause.

### Prepared runtime gate

The previous 30-minute server expired normally at approximately 14:12:42; no
process was killed. Started one fresh hidden server at **14:12:45 +02:00**, PID
10340, session `381abcc9a2c24ee59acbed6c5dfb46c5`, bound only to
`127.0.0.1:22040`, expiring around **15:12:45 +02:00**. Launcher readiness verifies
the actual Python process identity, listening record and owning UDP endpoint.
Server evidence is under
`Build/definitive-online-test/20261011-141245-381abcc9a2c24ee59acbed6c5dfb46c5/`.

Prepared `Build/definitive-manual-test/20261011-141248-remote/`, containing the
12,128-byte pre-run CLEO log (SHA-256
`9cea4a1dede43f76facb6504ceba3fefdc8792968d4466b22fb170ea72b96f29`), manifest,
read-only prerequisite report, current server metadata, installed remote command
definitions and a seven-step JSON/Markdown plan. Its current pointer is
`Build/definitive-manual-test/current-remote-test.json`. Source-built and installed
bridge DLLs are identical, SHA-256
`f1ddf05e854b9c3e12f6339ee739db55ae48850bea84b511e7156e3594ab0956`.

Independent read-only review matches all **13 remote/model command signatures**
against installed SA Unreal definitions 0.500. `CREATE_CHAR` takes five inputs
(ped type, model, x, y, z) and returns one Char handle; its `num_params=6` includes
that output. Existing calls match. Definitions do not establish model 7's
appearance or ped type 4's meaning. Whether coordinate writes move a frozen ped,
the model loads/renders and cleanup removes it still requires the manual test.

Running the new reader with the saved baseline currently yields **incomplete,
exit 2**, as expected: no fresh remote session has begun. All acceptance flags
remain false. The next action requires the user to manually launch through Steam,
load an outdoor save and report readiness while leaving the game running. After
fresh joined/submission/self-echo evidence, start one hidden **30-second** synthetic
peer, observe creation/motion/removal, then stop with Ctrl+Shift+End and exit
normally. Preserve and evaluate game/server/helper logs and human visual evidence.
The helper is not a second real GTA client. No hot reload or automatic game launch
is part of this stage.

Recovery checkpoint: `Build/definitive-recovery/20261011-1412-remote-prepared/`
preserves the prototype/tracker sources with flushed ZIP/JSON and SHA-256 metadata.
No C++ files changed during this continuation. Commit messages should state the
smaller-client goal, why fresh evidence and reversible startup isolation matter,
the failure-result fix, 108 utility and 22 fixture checks, one-client runtime proof,
and the remaining startup/rendering/two-client limitations.

## 2026-10-11 14:18 and 14:21: two further manual startup freezes

The user reported a freeze on the manual launch, requested a server restart,
then reported another freeze on retry and asked to inspect the log. Both logs and
Windows records were preserved before further tests or installed changes.

| Evidence | First launch | Retry |
| --- | --- | --- |
| CLEO timestamps | 14:18:42 | 14:21:52 |
| Log bytes | 2,100 | 2,100 |
| Last record | Started scanning the CLEO directory for boot scripts | Same |
| Windows reboot | 14:19:20.5 +02:00 | 14:22:35.5 +02:00 |
| Fresh live dump | `141`, `WATCHDOG-20261011-1418.dmp` | `141`, `WATCHDOG-20261011-1422.dmp` |
| Fresh WER record | 40057, 14:19:36.0705678 +02:00 | 40131, 14:22:51.1417282 +02:00 |
| Kernel-Power 41 | 32289, 14:19:24.0234612 +02:00 | 32426, 14:22:38.8664541 +02:00 |

Both sessions identify CLEO Redux 1.5.1, supported DE build, SDK 7, definitions
0.500, Dylib plugin 2.0 and Input plugin 1.3. Neither records ordinary JavaScript
discovery, adapter entry/phase, bridge loading, join, transform, remote-ped,
controlled stop or normal exit. After normalizing only timestamps and the ASLR
image-base address, both logs match the earlier 13:29 failure log exactly.
This is a repeated observed startup boundary, not proof that scanning causes
the freeze. The loader/runtime/plugins are already active; their role remains open.

[Microsoft describes 0x141 as a display-engine timeout live dump](https://learn.microsoft.com/en-us/windows-hardware/drivers/debugger/bug-check-0x141---video-engine-timeout-detected).
It does not establish defective GPU hardware or assign responsibility to this mod.
The 14:18 dump appears twice after the second reboot; the 13:29 and 12:01 dump
identities also gain reports. These are historical replays, not extra fresh
incidents. Event 6008 again contains an earlier shutdown time inconsistent with
the CLEO startup (14:10:08 for the first reboot, 14:19:29 for the retry); do not
use those message times as the observed freeze clock. No dump was analyzed.

Post-reboot collections pass: Radeon RX Vega OK / code 0, unchanged driver
31.0.21925.1001, supported game SHA-256, Online mode and all 16 owned hashes,
with GTA closed. These healthy queries do not establish runtime stability.
Preserved evidence directories:

- `Build/definitive-diagnostics/20261011-142052-684ac34f403e438ebd7b5bc9e819b3c3/`,
  log SHA-256 `d63e61a3aeae8e6826805ab99edc674b31d7fb7c0ac1d309a48cacc48f696d93`.
- `Build/definitive-diagnostics/20261011-142406-fc14ce06970b44b2bb66659a4022a350/`,
  log SHA-256 `fa31ff29ba72e5266951953f4b39b9898218c1842d914dace3592a1dc0e51285`.

The server was restarted at 14:20:53, PID 17700, session
`d364bacdc68b49fab38e6be2313adf62`. Its log contains only its listening event,
with no join and empty stderr before the next reboot. Both failed launches have
no observed game-to-server connection. No synthetic peer has run; the prepared
remote experiment remains unstarted.

After the second reboot, restarted one hidden server at **14:24:48 +02:00**, PID
18876, session `321c4c2d37104e018affa00a24e5675a`, with verified identity/readiness
and endpoint `127.0.0.1:22040`, expiring around **15:24:48 +02:00**. Evidence is
under `Build/definitive-online-test/20261011-142448-321c4c2d37104e018affa00a24e5675a/`.
No stale PID was used to stop a process. Loader Status remains Enabled; no bypass,
uninstall, driver/device change or automatic game launch was performed.

The user then requested an in-game enable button to investigate whether activating
the client/server connection triggers a crash. Implementation is proceeding with
manual Online activation before bridge loading. It cannot delay the ASI loader or
CLEO/plugin boot stages that precede adapter entry. Separately, the prepared
reversible loader bypass can compare startup without removing the mod files.

## 2026-10-11 14:32: requested in-game activation installed

The user asked for an in-game enable button to compare gameplay before and after
client/server activation. Added `ONLINE_ACTIVATION = 'manual'` to the existing
configuration, keeping Diagnostic/Smoke/Online modes and the same 16 owned paths.
Installed Online configuration explicitly selects manual activation. Diagnostic
stays finite and metadata-only; Smoke keeps automatic read-only behavior.

The entry script validates host/runtime/API metadata, records
`adapter_phase: awaiting_activation` and `adapter_armed`, then waits at 50 ms
intervals. **Ctrl+Shift+Home** (held VK 17/16/36) activates once only while the
player is ready. While waiting it checks held keys, and readiness only when the
full activation chord is held. It does not look up `Memory` helpers, load the
bridge, bind procedures, inspect a player handle/coordinates, open transport,
request a model or create/move a remote ped. Armed markers explicitly record
bridge/game-adapter/network activity as false.

Full **Ctrl+Shift+End** (17/16/35) cancels while armed without a bridge/controller,
logging `adapter_activation_cancelled`. Stop wins if both chords are held. After
Home, `adapter_activation_requested` precedes the existing bridge/build/runtime
phases, join and transform processing. End then performs controlled cleanup and
ends the script. This is one activation per script instance, not hot reload or
repeated toggling after disposal. Partial/separate presses cannot accumulate.

**71 Node mock tests pass** (49 existing + 22 activation checks), independently
rerun by root. Checks cover no pre-activation DLL/procedure/Memory, player
coordinate/UDP/model/remote access; bounded armed logging; not-ready and partial
chords; exactly one load/connection on ready activation; early cancellation and
stop priority; unsupported host/version/config refusal; unchanged Diagnostic/Smoke
behavior. Regression tests explicitly select automatic activation where needed;
source/default configuration is tested as manual. The 108 utility checks also
pass. No test loaded the installed game or native DLL. Diagnostic/remote readers
ignore the new markers; armed-only or cancelled Online sessions cannot produce a
false runtime/remote pass.

Packaged all three modes with pinned dependency/definition verification, then
previewed and applied the Online update with GTA closed. Exactly **two owned
files changed**: `CLEO/de-online/index.js` and `config.mjs`, plus their hashes in
the installed manifest. Runtime, plugins, native bridge and game executable were
not replaced. A separately flushed before-update recovery snapshot maps old/new
bytes and hashes and saves the original manifest at
`Build/definitive-update-backups/20261011-143152-manual-activation/`.
The update helper also created backup `61623673b2484173a865cb86fca0978b`.
Individual replacements are atomic, but a hard reboot between replacements can
leave a mixed multi-file state; reconcile saved owned hashes before recovery.

Post-update read-only collection at 14:32 passes the supported executable and
**all 16 installed owned hashes**, Online mode and healthy display-device status.
Evidence is under
`Build/definitive-diagnostics/20261011-143242-3fd15b866c254d1eb6c8962752a072bd/`.
Source and installed entry scripts match SHA-256
`caa328858d13e339ba548ded235146a2929ac6218fa0690b63423ba0b2fb7ce1`.
No C++ files changed, so no formatting/native rebuild was required.

The original remote test is marked `startup_interrupted_before_peer` in its
current pointer and durable `startup-interruption.json`; both latest crash result
JSON files are saved alongside their logs. No synthetic peer started. The current
server remains the 14:24:48 instance, PID 18876 / session
`321c4c2d37104e018affa00a24e5675a`, with expiry around 15:24:48 +02:00.

Next manual sequence: launch through Steam, load an outdoor save, play briefly
while armed without Home, then activate with Ctrl+Shift+Home after the game is
stable; stop with Ctrl+Shift+End and exit normally. Record whether any freeze
occurs before or after activation and preserve the text log before another run.
Armed markers, real shortcut behavior and the new gate's runtime stability remain
unverified. No automatic game launch, game input/capture or device change was used.
An after-Home failure could involve bridge loading, local reads or networking;
the hotkey does not isolate the standalone server alone. Loader bypass and
protected-dump analysis remain separate ways to investigate pre-adapter freezes.

Recovery checkpoint: `Build/definitive-recovery/20261011-1432-manual-activation/`
preserves source/tracker ZIP and hash metadata. Commit messages should describe
the manual-enable goal, repeated pre-adapter freezes, why bridge initialization
is deferred, the 71 mock/108 utility checks, exact installed update and remaining
startup/shortcut/remote-player limitations.

## 2026-10-11 14:38-14:43: manual activation and stop verified in GTA

The user manually launched, pressed Ctrl+Shift+Home, walked/turned during the
live audit, then reported completion of Ctrl+Shift+End. No game launch, input,
capture or synthetic peer was automated. This provides actual runtime evidence
for the new activation gate and stop key, with normal game exit still pending
at the saved stop snapshot.

| Time (+02:00) | Evidence |
| --- | --- |
| 14:38:19 | CLEO/plugin startup began on the supported game. |
| 14:38:44 | Runtime initialized and loaded the Online script; `awaiting_activation` and one `adapter_armed` record with all activity flags false. |
| 14:39:15 | Ctrl+Shift+Home produced `adapter_activation_requested`, then the bridge-load marker and actual DLL load, guards, runtime binding and player-1 join. |
| 14:40:10 / 14:40:18 | First `transport_lost`, then successful player-2 join. |
| 14:40:53 / 14:40:54 | Second `transport_lost`, then successful player-3 join. |
| 14:43:21 | `adapter_stopped`, reason Ctrl+Shift+End, followed by script disposal. |

The armed interval lasts **31 logged seconds**. Independent snapshot review
confirms no bridge-load/player-transform/network/remote marker during that
interval. After activation, all guards pass and finite/bounded changing submitted
coordinates return in decoded self-echoes. Negative Y continues to round-trip.
The stopped snapshot contains **180 telemetry records and 177 non-null self
echoes**, partitioned by connection identity:

| Player ID | Telemetry records | Self echoes | Recorded lower bound on successful bridge submissions |
| --- | ---: | ---: | ---: |
| 1 | 19 | 18 | 264 |
| 2 | 24 | 23 | 344 |
| 3 | 137 | 136 | 2,049 |

Submission counts increase within each identity; counts reset on reconnect and
must not be treated as one uninterrupted session. Each maximum is a lower bound
on successful native submissions, not proof of individual server receipts. No
adapter/cleanup-failure or remote-ped marker appears. No synthetic peer ran.
This verifies one real game's manual activation, movement exchange and controlled
stop. It does not establish remote rendering, two real game clients or general
startup stability, and does not assign the earlier freezes to a component.

The actual server log records all three joins and **timeout** removal of each
identity; no requested-departure record is present for the final stop. Two client
rejoins are confirmed after the first two removals. Game telemetry has gaps and
the server expires idle sessions after five seconds. Background/pause while
switching to chat is a plausible explanation, but focus/pause was not captured
or confirmed. The final logged stop is valid even though the server had already
timed out its identity; a requested network departure is not independently
verified for this run. Server stderr is empty.

The live audit is under
`Build/definitive-diagnostics/20261011-144305-manual-activation-audit/`.
The stopped snapshot and actual server logs/result are under
`Build/definitive-diagnostics/20261011-1443-activation-stopped/`; game-log size is
66,602 bytes, SHA-256
`e275363aecaa2b85b0eceb84c6ecfc1a554f2b7f835cd16c6638e32db0198050`.
The plan result is saved at
`Build/definitive-manual-test/20261011-1432-manual-activation/activation-and-stop-result.json`.
At 14:45:08 both GTA and its bootstrap still run, and the log has no normal host
shutdown/Exit. The user has been asked to exit through the game menu; do not
mark normal-exit or game-closed checks passed until those records arrive.

The 14:44:05 read-only collector saved OS/GPU/events but its readiness check was
blocked by the running game, as designed. This is not evidence of an install
regression; complete owned-file validation awaits game closure. Sources and
installed files were not changed during this audit. Commit messages should
distinguish actual Home/End verification from pending normal exit, idle reconnects,
and still-unverified remote rendering/startup stability.

## 2026-10-11 14:58: activation audit completed with normal game exit

The user exited normally. At **14:58:39 +02:00**, the log appends only
`host shut down` and `Exit` after the 14:43:21 End stop and script disposal.
No further adapter event follows disposal. The final log is an exact extension
of the stopped snapshot, now **66,676 bytes**, SHA-256
`13ec815860feab52dd365018b28207d87e4c786fefd90bd3b5f902c375b61cb6`.
The 180 telemetry / 177 self-echo counts and three connection identities are
unchanged. Home activation, movement exchange, End stop and normal exit now have
actual game-runtime evidence for this run.

The final read-only collection passes: game/bootstrap closed, supported game hash,
Online mode, **all 16 owned files verified**, Radeon RX Vega OK / code 0, unchanged
driver and unchanged boot at 14:22:35.5. Bounded saved System/LiveKernel events
contain no relevant report since game startup at 14:38:19. This describes the
collected interval; it is not proof of general stability or the earlier crash cause.
Evidence and `manual-activation-result.json` are under
`Build/definitive-diagnostics/20261011-145850-618dcc0a67504f2eb6fa4551cc49e2fc/`.
The current activation pointer records the completed result; earlier stopped/live
snapshots remain unchanged. No source, installed file, device or server setting
changed during this final audit, and no synthetic peer ran.

The remaining gameplay gate is one short synthetic remote-ped experiment after
a fresh manual launch and Home activation, requiring creation, changing applied
transforms, visual confirmation and removal before controlled stop/normal exit.
Two real DE clients remain a later gate. A successful activation session does not
resolve intermittent pre-adapter startup freezes. Include that distinction,
idle timeouts/rejoins and actual normal-exit evidence in future commit messages.

## 2026-10-11 15:15: first visible synthetic remote character

The user manually reached an outdoor save and reported `outside`. GTA PID 4424
started at 15:12:21. Home activation was logged at **15:13:01**, after the
**15:12:48** armed marker. The previous server was gone; Windows reports a boot
at **15:05:37.5 +02:00**, whose cause was not reported by the user. Do not classify
that reboot as a new crash. A fresh hidden server started at **15:13:38.920**,
PID 19080, session `edd7b92b573a4cedadfb7a84197026a2`, endpoint
`127.0.0.1:22040`, duration 3,600 seconds (approximate expiry 16:13:38.920).
Its actual logs/session remain under
`Build/definitive-online-test/20261011-151338-edd7b92b573a4cedadfb7a84197026a2/`.

The client retried automatically, joined as ID 1, then ID 2 at **15:14:06**.
Nonzero changing outdoor coordinates and self-echoes were verified before the
peer started. Source and installed bridge hashes match:
`f1ddf05e854b9c3e12f6339ee739db55ae48850bea84b511e7156e3594ab0956`.
Only the helper was launched by tools; no GTA launch, window capture or game
input was performed. The exact helper invocation used x64 Python 3.14.5:

```powershell
python -B -u utils/definitive_edition/game_peer.py --seconds 30 --bridge Build/definitive-online/de_client_bridge.dll
```

The synthetic peer PID 23444 started at **15:15:10.809** and exited **0** at
**15:15:40.996**. It selected actual game transport identity 2 and joined as 3.
Its report records 575 successful transform submissions, 575 distinct synthetic
positions, and 20 distinct positions observed from game ID 2 over 30.005 seconds.
These submission counts do not prove equivalent server receipts or game command
applications. Peer error and cleanup_error are null; peer and server stderr are
empty. The actual server confirms player 3's requested departure.

Fresh game evidence is one lifecycle: **player 3 / ped 6150 / model 7**, created
at **15:15:11**, **30 distinct changing transform markers** through 15:15:40,
deleted at **15:15:41**. An independent read-only audit reproduced the counts,
event order and saved hashes. No adapter or cleanup failure is present. The
saved live snapshot at **15:20:31** contains 189 telemetry records and 182
self-echoes, with successful reconnects following idle timeouts. Focus/pause
was not recorded, so the reason for the telemetry gaps is not established.

The user explicitly confirmed the character appeared, moved and disappeared.
Their further observation was: **"he was floating and circling around me, no
animation or anything."** This is direct human visual evidence, stored separately
from helper/log-reader results. Both readers correctly retain their automated
rendering and multiplayer flags as false. One visible synthetic proxy with one
real game client does not establish two real clients or playable multiplayer.

Source review explains the circle and absent animation: the helper generates a
five-unit orbit from the received local XY/heading and copies its Z. The adapter
clears remote tasks, freezes the ped, disables collision and writes coordinates
and heading; no animation state is transmitted or played. Floating may involve
the difference between body-center and feet-based positioning, or the copied Z
at a different terrain location. Neither contribution has been measured yet.
Pinned API 0.500 defines `SET_CHAR_COORDINATES_NO_OFFSET` (`0972`) as body-center
placement, providing a bounded next comparison without guessing a height offset.
A blind terrain clamp would also alter roofs, stairs, interiors and jumping.

Evidence is preserved under
`Build/definitive-diagnostics/20261011-152031-remote-audit/`: raw game log/baseline,
actual restarted server identity/logs, peer prepared/session/completed records,
peer outputs, install manifest, `remote-log-result.json` and
`remote-test-result.json`. All 13 raw-file hash/size records were independently
verified. Original prepared baseline/server records at
`Build/definitive-manual-test/20261011-150105-remote/` remain intact.
The current remote pointer now records the visual result and pending shutdown.

The reader is **incomplete** solely because this snapshot predates End stop,
normal host shutdown and Exit. Preserve a final log after the user stops/exits;
then run the baseline-aware reader and `collect_diagnostics.ps1 -RequireReady`.
Do not update installed scripts while GTA is running. The next source candidate
uses body-center placement and bounded remote coordinate readback to measure Z
error, retaining the inert setup during this isolated comparison. Walking
animation, ground-aware motion and two real clients remain distinct next steps.
Future commit messages should include the smaller-client goal, why the position
comparison exists, the mock checks, this actual synthetic visual test and its
floating/no-animation limitations, plus any later manual verification.

## 2026-10-11 15:24: remote test closed normally; End registration unverified

The user confirmed GTA was fully closed, and subsequently clarified that they
pressed **Ctrl+Shift+End before closing**. Preserve that human report alongside
the actual log: there is no `adapter_stopped` marker. The latest telemetry is at
15:23:47; `host shut down` and `Exit` are both **15:24:10**. The shortcut worked
in the earlier 14:38 session, but registration in this run is unverified and its
cause is unknown. Do not claim the user omitted the key or silently relax the
reader's acceptance requirements.

The final log is **134,199 bytes**, SHA-256
`5eb87525e608c45072a71f5587564433a10d18856cbeb4aafb754a2c8b6cfb06`:
339 telemetry records, 331 self-echoes, eight game joins (IDs 1, 2 and 4-9), and
the same one remote lifecycle with 30 changing transforms. No adapter/cleanup
failure is recorded. The strict remote reader returns **invalid_evidence** with
two stop/shutdown ordering issues caused by the missing stop marker. Appearance,
movement, deletion and normal exit remain separately supported; the complete
controlled-stop sequence for this run did not pass.

Read-only prerequisites at 15:24 pass: no running game, known executable hash,
Online mode, **all 16 owned file hashes verified**, Radeon RX Vega OK / code 0.
The bounded saved Windows event groups have no entries after the current game
start at 15:12:21. A newly observed `WATCHDOG-20261011-1505.dmp` identity has two
Code 141 reports at 15:05:58 and 15:08:47, before this session; older identities
were replayed around 15:08. System records also show preceding unclean shutdowns
at 15:03/15:05. Their circumstances are unreported and must not be attributed to
the 15:15 peer. No dump was opened or device/driver setting changed.

Final evidence and separate human observations are at
`Build/definitive-diagnostics/20261011-152522-remote-audit/remote-final-assessment.json`;
the earlier raw final capture at `20261011-152423-remote-audit/` is retained.
Collector output is at
`Build/definitive-diagnostics/20261011-152423-c47988a4fd0c4913a2e63d84af80de6d/`.
An independent audit verified all 13 raw-file hashes, the final log/collector
match, counts, lifecycle, missing End and event timing. The current remote-test
pointer references the final assessment and records both the reported keypress
and absent registration.

## 2026-10-11 15:28: body-center position candidate installed

The next change uses pinned `SET_CHAR_COORDINATES_NO_OFFSET` (`0972`) for received
positions, avoiding the ordinary setter's documented feet placement. Existing
`GET_CHAR_COORDINATES`/`GET_CHAR_HEADING` provide separate
`remote_transform_observed` records with requested/observed transforms and
`z_error`. Reads are limited to once per second per owned ped at the existing
changed-transform evidence boundary. Original applied markers continue to mean
requested command arguments. Invalid or failed readback triggers existing owned
ped/model/transport cleanup. Inert/frozen/collision behavior and the protocol are
unchanged, so this isolates placement before animation or terrain handling.

**87 Node mocks passed**, including 16 added checks for negative/native arguments,
requested versus observed state, remote identity/handle ownership, timing,
invalid later snapshot records, stale-handle recreation and failure cleanup.
They do not execute GTA or a native DLL. Durably saved test/API evidence is at
`Build/definitive-position-check/20261011-152504-495ab6cdc2ed4cb6bf901eb2e8c15067/`.
Online packaging verified 16 files from cached pinned dependencies. With GTA
closed, only `CLEO/de-online/index.js` and `controller.mjs` were updated; all 16
installed hashes and both source/installed byte matches then passed. Installed
Online remains manually activated. No C++ file changed, so no C++ formatting or
native rebuild was required for this continuation.

Before writing, original/proposed bytes and the manifest were fsynced under
`Build/definitive-update-backups/20261011-152803-position-check/`; its recovery map
and completion record retain exact hashes. The update helper also saved prior
bytes under `0bbfadcd14db4e9eaa13ebcbe7783f8f/` in that parent directory.
Installation and post-update manifest evidence are in the position-check directory.

The next test is prepared under
`Build/definitive-manual-test/20261011-152919-position-check/`, with the completed
134,199-byte log baseline, updated installed manifest and prepared server identity.
`current-position-test.json` points to that plan. Manually reach flat outdoor
ground, activate with Home and report `outside`; verify a fresh join/self-echo
before another 30-second helper. Compare readback/Z error and visually observe
feet, orbit and deletion. Hold End during active gameplay before the exit menu
and verify its marker before normal exit. Actual candidate placement, general
terrain grounding and walking animation remain unverified. Two real clients and
earlier startup-freeze causes are still open. Future commits should explain why
body-center placement/readback were added and distinguish these 87 mocks and
verified installation from the pending actual position test.

A recovery archive includes all **37 source files**, with fsynced writes, ZIP
CRC/readback, per-file SHA-256 and UTF-8/nonempty/no-NUL checks. The latest durable
checkpoint is discoverable through
`Build/definitive-recovery/current-checkpoint.json`; it records the source archive,
final remote evidence, installed candidate and next manual position plan.
Only the two adapter sources, their mock tests and the two Markdown documents
changed since the 14:58 checkpoint. No classic/MTA file, native source or game
executable changed; no commit or push was made.

## 2026-10-11 16:53: End and normal exit verified; position test not run

The user reported pressing End and exiting. A read-only command succeeded after
the earlier MXC launcher failure while enumerating BitLocker-locked F:. No
project/game path selected F:, and the reason the launcher included that volume
remains unknown; do not claim a runner fix or ask to relocate the installation.

The fresh session armed at **16:51:27**, activated with Home at **16:51:39**,
recorded **adapter_stopped / Ctrl+Shift+End at 16:53:21**, then script disposal.
**Host shutdown and Exit are both 16:53:28**. This run verifies shortcut
registration and normal closure. No join, remote event, adapter failure or
cleanup failure appears. Connection retries were recorded; the previously
started server's scheduled expiry was 16:13:38, but live server state was not
verified. A CIM process inventory was denied under the current permissions;
its failed query must not be treated as an empty process list.

The 30-second peer never started because the initial command launcher failed.
The body-center position candidate, visual grounding and animation remain
untested. The baseline-aware remote reader correctly returns **incomplete** for
this no-peer session, while stop/shutdown/Exit markers pass. Raw log, baseline,
reader result and session assessment are saved at
`Build/definitive-diagnostics/20261011-165657-position-stop-no-peer/`.
The current position-test pointer records this outcome. No game launch, input,
installed-file change or server restart was performed. The user requested a
small scope because only 5% of their usage limit remained; stop after recording
this result. A later remote comparison needs fresh server readiness and a new
log baseline before the user manually launches again.

## 2026-10-11 18:23-18:36: resumed test requires a verified host server

The candidate is unchanged: all 16 installed hashes, installed/source matches,
and the three source hashes from the earlier 87 passing mocks verify. No game
was launched or controlled. The last recorded game exit remains 16:53:28.

Current sandbox CIM/PnP and Windows event queries are denied. Collection at
`Build/definitive-diagnostics/20261011-182336-8953d5b246dd40bb9399d0b0dd7c8d90/`
verifies the known game and owned files, but full readiness does not pass because
display health is unreadable. Denied device/event results do not indicate GPU
failure or a healthy interval. The standard server helper stopped at CIM endpoint
enumeration before launching a process.

An unchanged server was then run in owned command session 31636 with private logs
under `Build/definitive-online-test/20261011-182909-64c1cb6e72f54b24acbd21197a05fefb/`.
Its source hash remains `0d3df2cdc9be963aff21005b6ed289a552631ebcc5db8c8609048e0caa7a2e07`.
It logged a loopback bind, but the separately executed three-second UDP welcome
probe timed out; no probe join appeared in its log. A listen marker alone was
not accepted. The owned command was interrupted and reported exit 1; no arbitrary
PID was stopped. Exact sandbox networking behavior is unassigned. The failure
is saved in `interrupted-result.json`. The old verified server pointer remains
historical and was not replaced by this unverified attempt. No game peer or
position UPDATE ran.

Fresh baseline/manifest/plan files are at
`Build/definitive-manual-test/20261011-182909-position-resumed/`. The current
position pointer records **awaiting_verified_host_server**. A syntax-checked
host runner is at `Build/start-position-server-on-host.ps1`; execute it from
normal PowerShell outside the restricted command environment with GTA closed.
It invokes the existing RequireReady collector, then the existing hidden server
helper for 3,600 seconds, requiring both to pass. It changes no driver, permission,
firewall, installed mod or game file. Report the host result before launching GTA
so actual server identity and a final fresh baseline can be preserved. Then repeat
Home/outdoor/30-second peer/End/exit. Actual placement, animation, two real clients
and the earlier freeze cause remain unverified.

## 2026-10-11 18:50-18:55: host server verified; fresh position baseline saved

The user executed `Build/start-position-server-on-host.ps1` from normal Windows
PowerShell with GTA closed. The RequireReady collector passed with no blockers
or collection issues. Its raw evidence is at
`Build/definitive-diagnostics/20261011-185024-cb96cf15348844a98aa9b503add84363/`.
The Radeon RX Vega and display PnP queries report OK/error 0. The known game
executable hash and all 16 owned installed files verify. These checks establish
the recorded prerequisites; runtime stability remains unproven.

The unchanged server helper verified its new process by executable, command line,
creation time, owned UDP endpoint and listening log. Session
`6a23caa5f9104c8cbc902a169cbdce8b` records PID 16260, x64 Python 3.14.5, loopback
port 22040, empty stderr and 3,600-second duration. Startup was 18:50:27.8665609
local time and scheduled expiry is **19:50:27.8665609**. Its private logs and
metadata are at
`Build/definitive-online-test/20261011-185027-6a23caa5f9104c8cbc902a169cbdce8b/`.
The agent read this host-produced evidence; it did not repeat the blocked sandbox
CIM query or failed cross-command UDP probe.

At 18:55:25, the agent preserved a fresh **17,054-byte** game-log baseline, SHA-256
`15d58e1a1528494f3e8b77be5aff666a68bc84158295aaa5657ce52204ef4a92`, which still
matched the host collector snapshot. All 16 installed hashes, installed/source
adapter matches and the source hashes from the 87 passing mocks were checked
again without executing the game or bridge. Raw collector files, the installed
manifest, server metadata and the previous position pointer are copied beside
the new plan at
`Build/definitive-manual-test/20261011-185525-position-host-ready/`.
The current position pointer is **awaiting_manual_game_ready**; the 18:29 failed
sandbox attempt and the 16:53 stopped no-peer session remain preserved.

The collector also records a boot at **15:50:52.815679** and historical 141 dump
identities `WATCHDOG-20261011-1536.dmp` and `WATCHDOG-20261011-1550.dmp`, first
reported at 15:36:56 and 15:50:13 respectively. Their circumstances are unreported.
The 18:15 records repeat those identities and do not establish another crash at
18:15. No cause is assigned to the position change, server or GPU from these
records. Dumps remain unanalyzed.

Next, the user manually loads a flat outdoor save and activates with Home. Fresh
join/self-echo evidence must precede the single 30-second synthetic comparison.
That helper must run from normal host PowerShell because sandbox loopback access
was not verified. Compare body-center readback/Z error and visible feet before
adding animation. Then verify End during gameplay, followed by normal exit.
No peer, game launch, input, installed-file change, native-code change, commit or
push occurred in this readiness step. Future commits should explain the goal,
reason for body-center placement/readback, passing mocks, and actual manual-test
results separately.

## 2026-10-11 18:57-19:44: crash audit, live server check and reversible startup control

The user reported another PC crash after the 18:55 ready baseline, then reported
undervolting the GPU before trying again. No tool changed GPU/driver/device
settings. The user ran the existing host helper, which captured diagnostics at
19:12:59 and started server session `092f8f9b716448249f0ecfc65d182c43` at
19:13:01.511707 local time, PID 5248, expiry **20:13:01.511707**. Its raw evidence
is at `Build/definitive-online-test/20261011-191301-092f8f9b716448249f0ecfc65d182c43/`.
The earlier PID 16260/session 6a23 is historical and must not be reused.

Before full access, command, Node and patch execution failed; the MXC command
launcher reported BitLocker-locked D: while all requested project/game paths
were on C:. No task path selected D:, no project moved there and no locked drive
was unlocked. The reason for MXC's volume enumeration was unassigned. The user
requested no more command-running chores. After full access was granted, the
agent performed the following checks directly; the user need only handle manual
gameplay. No game launch or input was sent by tools.

A fresh RequireReady collection passed at **19:33:53**: GTA closed, Radeon RX
Vega/PnP OK/error 0, known unchanged game hash, all 16 installed hashes verified,
no collection issues or blockers. Evidence is at
`Build/definitive-diagnostics/20261011-193349-a86f12063c334794b69a917f3ec89e06/`.
The latest boot is **19:09:35.5**. System 41 at 19:09:38 and 6008 at 19:09:44
record an unclean restart. Event 6008's previous-shutdown text says 18:31:01;
do not equate that field to the game log's 18:57:26 timestamp or derive a precise
freeze time from it.

Comparing grouped dump identities against the 18:50 collection produces
**no new identity**, both at 19:12 and 19:33. The latest 141 reports refer to
existing 15:50, 15:36 and older dumps. Reporting at 19:12 does not establish a
new 19:12 timeout. Absence of a newly grouped report does not exclude an
unrecorded GPU timeout or another crash. The Application query since 18:50 for
IDs 1000/1001/1002, excluding LiveKernelEvent replay, found only a post-reboot
CodexSandboxService event at 19:09:46 and no GTA application fault/hang entry
in that query. This is not proof that GTA had no fault.

The entire recovered CLEO log is **2,100 bytes**, SHA-256
`7d597e43aa226f24ae6a51641eceb55c1471667f58965a51301eaa9ed9a363ba`.
It contains correct host detection, both plugin loads and the start of boot-script
scanning at 18:57:26. No adapter entry, armed, bridge-load or game-join marker is
recorded. The baseline-aware remote reader correctly returns **incomplete**.
Raw log, previous baseline/pointer, Windows evidence, reader result and bounded
assessment are preserved at
`Build/definitive-diagnostics/20261011-193820-boot-failure-audit/`.

A source/installation audit verified matching index/controller/mod files and
the intended Online/manual installed configuration. Static imports export only
configuration literals and function/class declarations; they have no bridge,
game-command or socket side effects. Module resolution/parsing/evaluation,
HOST/CLEO metadata evaluation, serialization/logging, runtime dispatch and missing
or buffered output can still precede the first recorded entry. The missing marker
must not be promoted into proof that no script code executed. No unexpected
boot scripts or plugins were found. The two additional files are generated
`CLEO/.config/sa.d.ts` and `CLEO/tsconfig.json`; the declarations file was updated
at 18:57:26.

Targeted Unreal log/report searches found no report for the latest Oct11 failure.
The actual AppDataLocal/Rockstar Games/GTA San Andreas Definitive Edition/Saved/Logs
folder is empty. Two older Oct4 reports under OneDrive/Documents/Rockstar Games/
GTA San Andreas Definitive Edition/Crashes identify GPUCrash, UE4 Gameface 4.26.1
and D3D device lost `0x887A0006 HUNG`. They corroborate older graphics failures,
not the cause or details of this attempt. No protected system dump was read.

At **19:36:28**, the agent verified the currently owned loopback endpoint and
process start time, then received a nonce-matching UDP WELCOME and confirmed
the probe's requested LEAVE in the private server log. **Zero UPDATE packets**
were sent, no synthetic/game peer started and server stderr is empty.
`server-readiness.json` contains this result. Fresh CIM executable/command-line
fields for the host-created server are unreadable/null; the original host helper
recorded and verified them. Do not claim a new complete CIM identity check.

### Prepared control and exact restoration

At **19:41:45**, with two successful game-closed inventories and all 16 owned
hashes matching, the agent preserved the loader, manifest and crash-log baseline
before renaming only the owned Win64 `version.dll` in place to
`version.dll.de-online-isolation-f3d51406fe0c41318ff6983040b6b95c.disabled`.
Original and backup SHA-256:
`031a3e5576d91dce1e438d36b9a3d462c7334ab4791990a8ff1e3ddc0e132daf`.
No file was deleted or overwritten. All client/CLEO/plugin/source bytes remain.
The manifest stays unchanged; its Online label describes the preserved package,
not the effective loader-bypassed profile. A standard all-16 install check will
intentionally see the original loader name missing until restoration.

The guarded helper is `Build/switch-position-loader-isolation.ps1`. It pins paths
and known executable/manifest, rejects unknown hashes/reparse paths, requires
successful game-closed inventories, makes durable backups/records, verifies the
rename and never overwrites an existing restore destination. Syntax checking,
Prepare dry-run, actual Prepare, read-only Status and Restore dry-run passed.
Status confirms 15 other owned hashes and the renamed loader hash. Recovery is at
`Build/definitive-loader-isolation/20261011-194145-f3d51406fe0c41318ff6983040b6b95c/record.json`;
the original DLL, immutable preparation record, manifest and helper copy are beside
it. The helper hash is
`e09cdc922fc6c8c160d171263735db3ad118455e81be3317bca917bf67ef2476`.
After GTA closes, invoke that helper with **-Action Restore**; its current pointer
selects this exact recovery record. Restore must verify all 16 files before the
next online/position launch. Do not rename an arbitrary DLL or overwrite a newly
present original destination.

The next plan is
`Build/definitive-manual-test/20261011-194145-base-game-control/test-plan.json`,
discoverable through `current-startup-control.json`. It preserves the baseline,
helper, recovery record, source audit and server metadata. The user manually
launches, plays briefly with current settings and exits normally. Home/End online
activation is unavailable while the loader is bypassed. The agent then checks
the outcome and available process/module/log/event evidence, restores the loader
with GTA closed and verifies the restored profile. A fresh CLEO startup despite
the intended bypass would invalidate that isolation. A successful single launch
does not establish stability or identify a particular plugin; a freeze with a
verified bypass does not identify GPU hardware or base-game cause. The undervolt
also prevents treating this as the same conditions as earlier sessions.

Body-center placement, visible grounding, animation, two real clients and the
startup-freeze cause remain unverified. No adapter/native source changed, no
game/input/device action was automated, and no commit or push was made. Future
commits should record the goal, motivation, diagnosis limits and separate source
mocks, verified file operations and actual manual-test results.

## 2026-10-11 19:49-19:55: control gameplay confirmed; exact Online profile restored

The user first reported GTA closed, then explicitly confirmed **gameplay without
freezing and normal exit** during the loader-bypassed control. Tool inventories
also show GTA/bootstrap closed. The agent did not launch the game, inspect its
window, send input or obtain module/start-time/duration evidence during gameplay.
Human confirmation and collected evidence are recorded separately.

Collection at **19:49:40** is saved under
`Build/definitive-diagnostics/20261011-194938-0df67e4ba3574739802faf1400fd076f/`.
GPU/PnP remain OK/error 0 and the boot remains **19:09:35.5**. Its 18 System
event rows and 42 grouped dump identities match the pre-control collection; no
queried event after the 19:41:45 loader disable was found. These queries retain
their documented 2,000-entry per-log/provider bound. The sole install inspection
issue is the intentionally missing original `version.dll`; this collector did
not copy the CLEO log because game inspection stopped at that missing file.
The expected missing loader must not be called an additional GPU/file corruption.

The agent separately preserved the live CLEO log before restoring the loader.
It is byte-for-byte unchanged at **2,100 bytes**, SHA-256
`7d597e43aa226f24ae6a51641eceb55c1471667f58965a51301eaa9ed9a363ba`, still last
written at 18:57:26. This supports the intended loader bypass. Absence of a fresh
log is not a complete loaded-module inventory, which was not obtained.

At **19:50:56.5907265**, the guarded Restore operation moved the exact saved DLL
back to `version.dll`, without overwriting anything, and verified all **16 owned
installed files**. Its bytes match the original/backup hash; the temporary disabled
filename is gone. The immutable preparation record and backup remain preserved.
The latest isolation record status is **restored**, not loader_disabled.
RequireReady collection passed again at 19:51 under
`Build/definitive-diagnostics/20261011-195125-adf0409deee046558a508ef73cead8e9/`.

The full control assessment, pre-restore snapshot, restored record/manifest and
both post-test collections are saved at
`Build/definitive-manual-test/20261011-194145-base-game-control/post-control/`.
`current-startup-control.json` records **control_closed_loader_restored** with
human gameplay confirmation. The ordinary Online/manual installation is active;
Home/End activation is available again.

A new immutable game-log baseline, manifest, fresh readiness evidence, server
record and candidate-source/hash checks were saved at
`Build/definitive-manual-test/20261011-195314-position-host-ready/`.
`current-position-test.json` now records **awaiting_manual_game_ready**, links the
completed control result and states the loader is restored. The body-center
candidate still matches the earlier 87 passing mocks; no adapter/native source
changed and those mocks were not rerun. Server session 092f/PID 5248 retains its
scheduled **20:13:01** expiry; check live readiness again before starting a peer.
The agent can now handle console/peer commands directly.

The successful short control does not prove general stability, that the undervolt
fixed anything, or that a specific loader/plugin caused the earlier crash. The
undervolt settings remain user-reported, not measured. Next, the user manually
loads flat outdoor gameplay with the restored loader and holds Home after it is
stable. Fresh entry/join/self-echo evidence must precede the single 30-second
position/readback comparison. Walking animation and two real clients remain
unimplemented/unverified. Retain these limits in future commits alongside the
goal, motivation and actual testing.

## 2026-10-11 19:57-20:09: body-center grounding trial completed

The restored Online/manual profile launched manually at **19:57:04.999487**,
game PID **20964**, with the existing Steam launch's `-dx12` arguments. The log
records fresh entry/arming at 19:57:32 and Home activation at **19:57:51**.
Bridge ABI, pinned game hash and runtime binding passed before a joined session
submitted and echoed CJ coordinates. Several gaps/reconnections occurred; game
identity **4** was current for the peer trial. Tooling did not launch GTA or send
input. The server's owning endpoint/start time and all **16 owned hashes** were
checked before starting the sole new peer.

At **20:00:47.131**, the agent started one hidden synthetic helper, PID **8520**,
for **30 seconds** at radius **5**. Its owned process handle was retained through
completion. It exited **0** at **20:01:17.380**, reported elapsed **30.009 s**,
**575 updates/distinct synthetic positions**, no run/cleanup error and empty
stderr. Transport inferred game identity **4** and synthetic identity **5**.
The game stayed at one position; the helper's false remote-motion flag describes
that stationary game, not the synthetic orbit. Its automated rendering flag
remains false.

The game created owned ped **8706**, model **7**, at **20:00:47** and deleted it
at **20:01:17**. All **29 applied events** have one matching observed event for
the same player/ped and requested transform, with 29 distinct transforms and no
validation issue. Requested Z is **13.302051544189453** throughout. **X/Y/Z errors
are exactly zero in every sample**; heading error is at most
**0.00006103515625 degrees**. This is actual evidence for
`SET_CHAR_COORDINATES_NO_OFFSET` with body-center positions, not just mock or
command-argument agreement.

The user's direct observation was: **“yes, on ground sliding around me, not
floating not sinking”**. This confirms appearance, movement, disappearance and
grounding at this location. Sliding remains expected without locomotion animation.
The helper copies CJ's Z at different X/Y, so slopes, stairs, other model heights
and general terrain alignment remain untested. There is still only one real
game client; synthetic evidence cannot establish two-client gameplay.

The first reported End press had no corresponding stop marker while the log
was not advancing. After another press in focused gameplay, **Ctrl+Shift+End
registered at 20:07:25** and the script disposed. Normal host shutdown and Exit
followed at **20:08:08**. The baseline-aware strict remote reader now returns
**remote_command_sequence_passed**, with every flag true, one completed lifecycle,
zero active remotes, zero issues and zero failures. This complete result is
separate from the older 15:15 trial that lacked a stop marker.

The server recorded requested departure for synthetic player **5**. Game IDs
2/3/4 and the final rejoined ID **6** have timeout departures. The final game
LEAVE therefore is not verified as requested, even though the adapter stop and
normal exit are verified. Gameplay/background gaps can exceed the server's
five-second idle limit; their exact causes were not captured. No change to the
protocol or timeout behavior was made during this test.

Final immutable game log: **186,028 bytes**, SHA-256
`db1b168cebb02b2db0cae7cb1d1023011ae78ca881e466a44566892b562361cb`.
Evidence root:
`Build/definitive-manual-test/20261011-195314-position-host-ready/`.
Preserved subdirectories include `peer-200047-3ddade44b51247488342e01b64e13936/`,
post-peer/stop snapshots and `audit-200818423094-final/`. The separate
`human-position-observation.json` retains the user's exact answer.
`assessment-final-reviewed.json` combines references and bounded conclusions;
`current-position-test.json` links it with status
**position_trial_complete_readback_and_visible_grounding_verified**.

Post-exit RequireReady collection passed at **20:08:25**:
`Build/definitive-diagnostics/20261011-200825-8cdd8dd1203b404eba42422f58e8e1cb/`.
GTA/bootstrap are closed, the known executable and all 16 installed files match,
GPU/PnP are OK/error 0, boot is unchanged at **19:09:35.5**, and no new grouped
dump identity was found against the pre-run collection. The bounded System and
LiveKernel queries contain no event after this game start; issues and blockers
are empty. This short successful run does not resolve earlier startup freezes
or establish long-term stability. Undervolt values remain user-reported.

No adapter/native source changed during this run, so the prior 87 mocks were
not repeated. The recovery checkpoint now separates the historical floating/
missing-End trial from this successful candidate and copies its final log,
quantitative audit and human observation beside the source archive. Next work
is bounded walking/idle animation while preserving the verified coordinate
setter and owned cleanup, followed by manual visual acceptance and eventually
two real game clients. Future commits should explain that goal, motivation,
implementation reasoning, actual tests and remaining limits.

## Next candidate research: built-in walking and idle animation

Read-only research on October 11 identified a small next candidate that preserves
the measured no-offset setter, type-4/model-7 NPC, frozen position and disabled
collision. **No animation source/package/install change or additional gameplay
test was made in this phase.** The installed position build remains the proven
candidate. Do not promote the following design into completed playback evidence.

The pinned [SA Unreal 0.500 definitions](https://github.com/sannybuilder/library/blob/34bedee0be2f57047ed8f0705c81f137ede3d3ce/sa_unreal/sa_unreal.json)
are already packaged. Local copy:
`Build/definitive-package/downloads/sa_unreal.json`, SHA-256
`cab1b9d6bd25a4fff41fef63aef0fd771a923c0d39cb018f030f44eceb7713f7`.
The applicable native signatures are:

| Native | Inputs | Returned value |
| --- | --- | --- |
| `TASK_PLAY_ANIM` / 0605 | handle, animationName, animationFile, blendSpeed, loop, lockX, lockY, keepLastFrame, time | none |
| `IS_CHAR_PLAYING_ANIM` / 0611 | handle, animationName | condition |
| `GET_CHAR_ANIM_CURRENT_TIME` / 0613 | handle, animationName | normalized numeric time |

**The condition and time getter take no animation-file argument.** The getter's
`num_params: 3` includes its output; it does not mean three inputs. Other task
variants 0620/06A6 are marked NOP in these inspected definitions and should not
be substituted.

Use built-in **`PED`**, with **no request or removal lifecycle**. The game-specific
[0605 snippet](https://raw.githubusercontent.com/sannybuilder/library/34bedee0be2f57047ed8f0705c81f137ede3d3ce/sa_unreal/snippets/default/0605.txt)
documents these PED exceptions, and the pinned
[removal documentation](https://raw.githubusercontent.com/sannybuilder/library/34bedee0be2f57047ed8f0705c81f137ede3d3ce/shared/docs/REMOVE_ANIMATION.md)
warns that releasing PED can crash the game. Never add `REMOVE_ANIMATION('PED')`
to owned-ped cleanup. Deleting the owned proxy and releasing its requested model
remain the appropriate existing cleanup operations. No new DLL/dependency is
needed for these built-in animations.

Additional resources read from this pinned commit on 2026-10-11, without writing
local copies:

| Resource | Bytes | SHA-256 |
| --- | ---: | --- |
| `sa_unreal/snippets/default/0605.txt` | 348 | `414b221aee8656e48176d1debd356a30d22f7a454f09ddf55fd60b71bbfac440` |
| `shared/docs/REQUEST_ANIMATION.md` | 297 | `93861f414bdc0e91f7a5d806964b06eacbd0ba36aee116383b9345572ac148dd` |
| `shared/docs/REMOVE_ANIMATION.md` | 171 | `3e28b6d25742639007357afd3cfb8592ff118ae0ef2edf73e688d692abd9d5d8` |
| `sa/docs/TASK_PLAY_ANIM.md` | 188 | `a8b8648177ac7af104ce06cf08dc3107052b0a72bae6dbdb322bd83ae81a94e9` |

Exact installed DE executable strings include **`Walk_civi`** at file offset
**70128152** and **`idle_stance`** at **70128104**. Classic `data/animgrp.dat`
associates the corresponding names with `ped`, and MTA's
`Client/mods/deathmatch/logic/CClientPed.cpp` uses `ped`/`idle_stance` at line 302.
These support candidate names; successful DE playback still needs an actual test.
The proposed task invocation is:

```javascript
native('TASK_PLAY_ANIM', handle, clip, 'PED', 4.0, true, false, false, false, -1);
```

Classic [CPed::Teleport](https://raw.githubusercontent.com/gta-reversed/gta-reversed/master/source/game_sa/Entity/Ped/Ped.cpp)
flushes tasks conditionally for a player or a leave-car task, rather than every
NPC. The existing proxy is neither. Classic freezing sets a speed-related flag
while animation processing is separate. This comparison supports trying the
existing setup, but does not prove DE's behavior; retain paired observed playback
and coordinate evidence instead of changing the verified setter speculatively.

Suggested controller policy, to be implemented and tested separately:

- A new owned handle begins idle. Dispatch a task once per animation-state
  transition; avoid clearing/restarting tasks each coordinate update.
- Infer motion from X/Y only, initially with `0 < dt <= 250 ms`, displacement
  at least 0.02 units and inferred speed 0.25-15 units/s. These are proposed
  thresholds, not measured game movement constants.
- Keep walking for 350 ms after plausible horizontal movement, then select idle
  across identical cached snapshots. Heading/Z changes and small jitter cannot
  start walking. Large jumps, long pauses or reversed time reset inference.
- Discard animation/motion state on handle replacement, departure and session
  cleanup. The current protocol lacks packet timestamps, so cached stationary
  and stale transforms cannot be distinguished precisely.
- Emit requested-task and observed-playback events separately, with observed
  evidence at most once per second even when coordinates are unchanged. Query
  playing first; only then query normalized time. Record absent playback with
  null time. Validate finite time in [0,1] and allow initial task-scheduling grace.
- If a requested animation remains absent after that grace, report failure and
  use existing owned cleanup; avoid endless task replay. Matching time values at
  one-second intervals can alias a whole loop, so phase equality alone is not
  proof of a stalled animation.

Meaningful next tests cover native argument order, no PED request/removal,
conditional time reads, initial grace and invalid/absent playback cleanup,
state-change-only tasks, stationary/heading/Z/jitter cases, teleport/pause/clock
reversal, replacement ownership and all existing stop/transport cleanup paths.
The next manual peer should exercise **idle → move → idle**, retaining XYZ
readback and observing feet, animation, deletion, End and normal exit. Playback
alone will not establish stride alignment or full movement synchronization.

At **20:14**, the prior server's process and UDP endpoint were absent and its
log recorded `stopped` with **zero players** after scheduled expiry. This is
recorded separately in the current plan's `server-expiry-status.json`; no process
was killed and no replacement server started. The next test requires the agent
to start/verify a fresh server and save a new baseline before manual gameplay.

## 2026-10-11 20:20–20:30: walking/idle candidate prepared and installed

Implemented the researched policy in `controller.mjs` and `index.js`, preserving
the verified no-offset coordinate setter, frozen/collision-disabled owned NPC,
native bridge, loader and wire protocol. Plausible X/Y motion selects `Walk_civi`;
350 ms without it returns to `idle_stance`. Tasks dispatch only on transitions.
Built-in `PED` is never requested or released. Each owned remote logs conditional
playing/normalized-time evidence and position readback at most once per second,
including stationary idle. Invalid evidence or absent playback after scheduling
grace uses existing owned cleanup. Clock reversal resets observation timing.

The opt-in peer profile `idle-walk-idle` fixes its first game anchor and submits
nominal 5-second idle, 20-second orbit motion, then 5-second idle stages. Default
orbit behavior remains available. Successful stage submissions are logged
separately, because an exit code alone can conceal truncated final idle.

Source-bound verification passed **118 adapter mocks** and **34 peer mocks**:
`Build/definitive-animation-check/20261011-202537-8914654daf23461e8bad58c30c41adcc/`.
The first verification wrapper invocation used the wrong Python working directory;
its import failure is retained in the `20261011-202520-3075e84af19d4167a1c86de03b292306/`
directory. The corrected invocation passed, with all five code/test hashes unchanged.
Independent review found no blocker and retained the truncated-stage caveat.

Only two installed scripts changed. Original and prepared bytes, manifests,
hashes and recovery mapping were flushed to
`Build/definitive-update-backups/20261011-202702-walking-animation/` before updating.
The update helper also retained its own backup under
`Build/definitive-update-backups/89b9835b78474d6f9be786572c8e1ef6/`.
WhatIf, closed-game inventory, real update, all **16 installed hashes** and fresh
RequireReady collection passed. No C++ source changed, so no C++ build/format was
needed. GPU status/PnP code stayed OK/0 in that collection; runtime crash cause
remains unresolved.

| Tested source | SHA-256 |
| --- | --- |
| `game-adapter/controller.mjs` | `abbf18e8d8aaf7689ffb94075d7dce4153e7163fe28d52c8e1122952120992d1` |
| `game-adapter/index.js` | `666fa5bf97fd3c70419d8d150f25859129d07b740cec2e7764cfb9583c750f1b` |
| `Tests/definitive/test_game_adapter.cjs` | `12db55f4744dd3af59961e632d607340958bf6dec542a7010e96300fab7aa0be` |
| `utils/definitive_edition/game_peer.py` | `5c4be7079355960e732ac33d930c41e0c762915cb3202cc6a8d63cde123f336b` |
| `utils/definitive_edition/test_game_peer.py` | `ddd000db22ed5bd637643eb3ab818b56f6eadddf5d95d64de7bbfceed0e72c09` |

The agent started fresh server **20868** at **20:27:52**, verified exact executable,
command line, birth time and sole loopback UDP ownership, then received a nonce-
matching welcome and logged requested probe departure. The readiness probe sent
zero position UPDATE packets. Baseline/test plan was saved at **20:29:54**.
GTA launch, Home activation and gameplay remained manual.

## 2026-10-11 20:32–20:42: animation runtime trial and shutdown verified

The user manually launched through Steam, loaded outdoor gameplay and pressed
Home. Fresh activation at **20:32:47**, joins and self-echo were verified. Several
subsequent game update gaps caused server timeouts/rejoins. Their cause was not
observed. The server's five-second idle limit and later stale queued snapshots
explain why a matching old echo alone is insufficient readiness evidence.

The first peer started at **20:37:37**, game player 5/helper 6, and ended after
**1.897 seconds**, with 36 `idle_start` updates and exit 1. It observed game
departure, then sent requested LEAVE without run/cleanup exceptions. A seven-
second CLEO publication gap from 20:37:34 to 20:37:41 coincides with the timeout.
On resumption, the game created ped 8709 from queued data and played advancing
`idle_stance`, then removed it during transport cleanup. It reconnected afterward.
The preparation runner itself had two pre-start validation failures (timestamp
conversion and doubly quoted log parsing); neither launched a peer. Both were
corrected before the recorded attempt. No installed or production source changed.

After fresh matched self-echo for game player **8** and the user's focused/
responsive confirmation, the unchanged candidate was retried. Owned helper
**7116** ran **20:38:45–20:39:15**, player **9**, ped **5898**. Its profile submitted
96 initial-idle, 383 walking and 95 final-idle updates: **574 total**, 385 distinct
positions, **30.008 seconds**, exit **0**, no run/cleanup error. Game task requests
were idle at 20:38:45, walking at 20:38:50, idle at 20:39:10; deletion at 20:39:15.

Independent final audit found **30 animation observations**, all playing=true,
finite phase in [0,1], zero pending/invalid evidence, and distinct phases in each
stage. Requested/observed **XYZ errors are zero** across changed-motion and
stationary animation readbacks. Maximum wrapped heading error is
**0.00006103515625 degrees**. There are **18 paired changed-transform records**;
the 30 playback records additionally cover unchanged idle poses.

The user reported, verbatim: **“it was walking front and back and around me but
laggy and he was still sliding, and then dissapeared. but he was moving his feet”**.
This establishes visible walking leg motion and disappearance. It also records
remaining lag/sliding. It does not explicitly confirm initial/final idle or feet
on ground in this animation trial. Those details are null in the separate
`human-animation-observation.json`, rather than inferred from older results.

End registered at **20:40:57**, followed by adapter stop and script disposal.
The user exited normally; host shutdown and Exit both logged at **20:41:21**.
Successful CIM inventory confirms no game/bootstrap process, and fresh
RequireReady collection verifies the known executable, Online profile, all
**16 owned files**, GPU/PnP OK/0, unchanged boot and no new grouped dump identity.
Collector: `Build/definitive-diagnostics/20261011-204139-b225ed71f02d4c3fb9266ec106876055/`.
This completed run does not resolve the earlier whole-PC crash cause.

Immutable retry audit:
`peer-203844-06b9006b21c640509bb6f836d597ca80/audit-20261011-204148-451134-final-84ea6f9b/`.
First-attempt audit:
`peer-203737-c9fc42441f244ece800ec129ae277c38/audit-20261011-204207-632632-final-5019d45b/`.
Both are under the current animation plan. Final raw CLEO:
**120389 bytes**, SHA-256
`51bb09f48098b06b8dfe41273c4b84f38fdb3620761e0f4555d08256d6f0b348`.
The full strict reader keeps its sole false `changed_transforms_applied` flag
from the idle-only first attempt. A documented selected-peer projection uses
only original lines for the retry and passes; it does not replace the full log.
Server helper 9 departed with requested LEAVE; game 8 departed by timeout.

### Next movement work, based on source review

Current game-adapter polling uses `wait(50)`, and it writes each changed cached
network transform directly. Helper submissions and server relay are about 20 Hz.
This produces discrete pose steps; a fixed clip does not match its stride to
travel speed. The test circle uses approximately **3.75 game units/second**.
These are plausible contributors to the reported appearance, not proven causes
from one-second telemetry. `TASK_PLAY_ANIM`'s 4.0 argument is blend speed and has
not been established as playback-rate control.

There is also a smaller facing-calibration question. Classic local
`Shared/mods/deathmatch/logic/Utils.h` derives heading as `2π - atan2(dx,dy)`.
For the helper's circle tangent `(-sin(a), cos(a))`, that convention would use
`degrees(a)`, while the helper currently submits `degrees(a)+90`. This is a
possible 90-degree mismatch under classic conventions; DE-facing semantics have
not been visually calibrated. Preserve the verified adapter's received heading
and test helper-facing/speed calibration separately before assuming a gait issue.

For smoothing, keep received target and rendered pose separate. A faster render
tick needs an independent 50 ms network gate so it does not multiply submissions;
interpolating only inside the existing 50 ms loop mostly adds delay. Use bounded
interpolation without unlimited extrapolation, shortest-angle heading handling,
and explicit resets on teleport, clock reversal, stale data, replacement,
departure and stop. Animation inference should use received targets. Position
evidence must distinguish the received target, the requested rendered pose and
actual readback; deliberately delayed poses must not be judged by the old exact-
target comparison. These are next-candidate recommendations, not installed changes.

Recovery remains discoverable through `Build/definitive-recovery/current-checkpoint.json`.
The new archive retains all **37 prototype source files**, current docs, test and
install certificates, both peer attempts, original raw logs, final assessment,
human observation, collector evidence and one-off audit/recovery helpers. Its ZIP
CRC and source/evidence hashes are verified after writing. For a future commit,
include the original goal, motivation for the separate adapter and animation
policy, the 118/34 mock results and actual runtime result, plus lag/sliding and
the interrupted-attempt acceptance distinction. No commit/push was performed.

## 2026-10-12 06:25–06:54: straight-walk calibration and automated control completed

The next change isolates helper-facing and travel speed before altering the
verified installed controller. Added opt-in `straight-walk-calibration` to
`utils/definitive_edition/game_peer.py`, retaining the older orbit and
`idle-walk-idle` behavior. At 30 seconds/radius 5 it fixes the first received game
anchor, places a lane at anchor Y + 3, and uses endpoints at anchor X ± 3 with
the initial CJ Z. Five equal nominal six-second stages are initial idle, outbound
walk, middle idle/turn, return walk and final idle. Outbound heading is 270;
return heading is 90. Default travel speed is **1 unit/second**, versus the older
circle's approximately 3.75. Maximum distance from the anchor is approximately
4.243 units. These headings and speed are calibration candidates; the helper
does not change how the adapter interprets a received pose.

The profile validates parameters and derived speed **0.5–1.5 units/second** before
loading the bridge. It preserves independent task/transport cleanup and records
each successfully submitted stage. Completion requires accepted updates in all
five stages. Waiting for the first game anchor consumes the same bounded overall
deadline, so stage presence alone does not prove full nominal idle duration after
late anchoring; audit actual timestamps and playback separately.

Added **17 peer tests**, bringing peer coverage to **51**. Source-bound
verification passes the full **135 utility tests** and **118 adapter mocks**:
`Build/definitive-calibration-check/20261012-062559-verified-e017e7fc881c4751916e63437244d86f/test-result.json`.
Code/test hashes are unchanged before and after verification. No C++ changed;
the game adapter, native bridge, loader, installed configuration and protocol
were unchanged, with all **16 installed hashes** verified.

| Changed source | SHA-256 |
| --- | --- |
| `utils/definitive_edition/game_peer.py` | `631968c85a36b51d2ca8233828fbbe47d0427faddf40254281ef68cf2475c29e` |
| `utils/definitive_edition/test_game_peer.py` | `5ca70615831993e957ca2841b6ec7e4e5a25b558737d1f2f24b3d01d7ae20ac5` |

The October 11 server had expired normally with zero players. Fresh CIM/process
and UDP ownership checks found it and its endpoint absent before replacement.
The agent started server **8884**, session **ac589aeabda047a1ac1a314450ee06db**, at
**06:28:45**, with one loopback endpoint `127.0.0.1:22040` and scheduled expiry
**07:28:45 local time**. Fresh readiness verifies the known game executable, all
16 owned hashes and GPU/PnP OK/0; boot remains October 11 **19:09:35.5**. The fresh
baseline/plan is at
`Build/definitive-manual-test/20261012-062924-calibration-host-ready/`, with its own
`current-calibration-test.json` pointer. Older position/animation pointers and
their raw evidence remain separate.

### User authorization and observed game controls

The user requested automated launch/input (“you try do it” and “you must do this
things now”), overriding the previous manual-only testing preference for this
run. Steam launch occurred at **06:35:24**; actual game PID **8436** was born at
**06:35:36**. The user confirmed the game was open and responsive.

The computer-use runtime failed before connection during its drive enumeration.
That tool-initialization failure did not involve the accessible game/project
paths and is not evidence of a game failure or storage-encryption cause. After
reading the computer-use skill and fallback guidance, the agent used targeted
Python Windows APIs/PIL against the verified foreground GTA window. Screenshots
and actions are retained under `Build/definitive-game-control/`. Observed Resume
was selected, CJ was moved from the kitchen through the living room to the flat
street outside. The game log records Home activation requested at **06:48:22**
and `adapter_start` at **06:48:23**. This did not
overwrite a save, change the executable or alter devices, drives or security
settings. No new freeze was observed in this completed run.

### Actual peer, playback and visual limits

Owned helper **8352** ran **06:48:55–06:49:25**, with game player **3**, synthetic
player **4**, ped **8450**. Duration was **30.010 seconds**; the helper submitted
**575 updates**, exactly **115 per stage**, and **232 distinct positions**. Exit
was **0**, with no run/cleanup error or game departure. The helper sent requested
LEAVE. The server also records requested LEAVE for game player **3** during
controlled stop; neither departure is inferred from timeout. Peer evidence is under
`peer-064855-d27dd4a390854aac8995ed9ac5a81bbc/` in the current calibration plan.

The same owned ped requested and played **idle → walk → idle → walk → idle**.
All **30 playback observations** report playing=true with phase progression:
**6/7/5/7/5** observations across the five requested tasks. Requested and observed
XYZ errors are **zero**, including stationary observations. Maximum wrapped
heading error is **0.00006103515625 degrees**. Both lane legs are monotonic in
their intended X direction. One outbound endpoint sample already has heading 90
while the walk task remains active during the controller's existing 350 ms motion
hold; this is preserved rather than labeled a missing task transition.

The targeted return-leg screenshot
`Build/definitive-game-control/20261012-064917-841401-observe.png` shows the actor
left of CJ facing left, while contemporaneous transforms move toward decreasing
X with heading **90**. Its feet appear on the ground in that frame. The later
`20261012-065106-746933-observe.png` shows the actor absent after departure.
These images establish one return pose/direction and subsequent absence. They
cannot establish continuous leg motion, gait/stride matching, smoothness, every
stage or outbound facing. No new human smoothness report was received. The
October 11 lag/sliding report remains open, and no render interpolation has been
installed. The log reader's automated rendering flag remains false; visual
evidence is recorded separately. Copying CJ's Z at different XY also remains a
terrain limitation.

### Controlled stop, closure and durable result

The agent held End in active gameplay; the log records controlled stop at
**06:51:18**, followed by cleanup. Normal pause-menu Exit and confirmation produced
host shutdown/Exit at **06:52:49**. Successful CIM inventory verifies the game is
closed. Post-exit RequireReady collection passed at
`Build/definitive-diagnostics/20261012-065303-c802768872fa48a9bc4b16da5b8240eb/`,
checking the known executable, all **16 installed files**, GPU/PnP OK/0,
unchanged boot, no new grouped dump identities and zero collector issues/blockers.
This does not resolve the older whole-PC crash cause or establish
general startup stability.

The post-peer assessment under
`peer-064855-d27dd4a390854aac8995ed9ac5a81bbc/audit-20261012-065014-675330-post-peer-ea96ff9c/assessment.json`
preserves the successful task/playback/position markers separately from the
subsequent stop/exit gate. The reviewed final result is
`assessment-final-reviewed.json` in the calibration plan. The independent final
audit is under
`peer-064855-d27dd4a390854aac8995ed9ac5a81bbc/audit-20261012-065402-661867-final-d41c67fa/assessment.json`.
Both the **unfiltered full-session** and scoped strict readers pass, with
**one completed session, zero active sessions and zero issues/failures**. Final
raw CLEO is **83,092 bytes**, SHA-256
`2a23615ad075b5532e5e77625b0aba54710f7185b19643af4202a47d1c9c88e6`.
The current calibration pointer is **completed** and retains the full sequence
and explicit visual limits. Recovery is
discoverable through `Build/definitive-recovery/current-checkpoint.json`, retaining
all **37 prototype source files**, current docs, helper/test certificates, plan,
raw peer/game/server logs, final assessment and targeted control evidence.

Next work remains bounded render interpolation with independent network cadence,
more visual facing/stride checks, then two real clients. The separate on-foot
multiplayer milestone is still incomplete. Future commits should state the
small-client goal, why facing/speed were isolated in a helper, the **118/135**
verification results (including **51 peer mocks**), actual five-stage/stop/exit
evidence, and the unresolved visual/crash limits. No commit or push was performed.

## 2026-10-12 07:56–08:10: bounded movement smoothing prepared and installed

Implemented the earlier movement-source recommendation in the controller and
Online entry loop. The separate **50 ms network deadline** gates local transform
reads, pump/submission and snapshot work while active Online yields at **16 ms**
for presentation. Missed deadlines skip forward rather than issuing catch-up
bursts. Stop, gameplay readiness, local-ped ownership and existence checks remain
ahead of this gate; faster presentation cannot defer controlled stop. Manual
activation and Smoke retain their existing 50 ms yields.

Each owned remote stores its received network target separately from its last
displayed pose. It interpolates XY over a bounded **50 ms** segment from that
displayed pose, and rotates along the shortest heading arc. Received Z snaps
directly to the body center, preserving the verified height/write convention.
There is **no extrapolation**. Initial creation, height changes, inferred
teleport speed above 15 units/second, network/render gaps above 250 ms and clock
reversal reset presentation. Replacement, departure, loading, loss of local
ownership and stop discard session state using existing cleanup. Animation
inference uses received targets, retaining the same clips and task policy.

Bounded one-second evidence now distinguishes `network_target`, requested
displayed transform and actual readback. `remote_motion_observed` records target
age, smoothing progress, render/write/poll/target counts and interval bounds,
plus discontinuity reason. Deliberately delayed display poses are compared to
their requested display pose, not judged against the newest received target.
These counters can establish adapter opportunities and intermediate writes;
they do not measure GPU frames or prove visible smoothness.

Source-bound verification passed **149 adapter mocks** and **135 utility tests**,
including **51 peer mocks**, without launching GTA, running a server or loading
the native bridge. Certificate:
`Build/definitive-smoothing-check/20261012-080403-verified-fa4b14e38fbb404ebb8d0bf6772c7535/test-result.json`.
The separate audit helper's **47 fixture checks** cover five-stage completeness,
owned task/playback evidence, exact displayed-pose readback, bounded interpolation,
network rate, lifecycle and explicit rejection of malformed/incomplete evidence:
`Build/definitive-smoothing-audit-check/20261012-080335-665579-fixtures-2ce35c3d/verification-result.json`.
Fixture success is not an actual gameplay result.

| Tested source | SHA-256 |
| --- | --- |
| `game-adapter/controller.mjs` | `6fc9eb8164d83d1f36be0a21390839c9fec9889559b7d83041c68cb7cde6edfc` |
| `game-adapter/index.js` | `29a8aad71e75d9424d98f5be633c1d7b5c4d089f6de85e40eb6829f746961074` |
| `Tests/definitive/test_game_adapter.cjs` | `76758760dc3ccd48902c0450f02173b9548652498c320d4d88984ee07cad3972` |

Only `CLEO/de-online/controller.mjs` and `CLEO/de-online/index.js` changed during
installation, recorded at **08:08:42**. Old bytes and recovery mapping are at
`Build/definitive-update-backups/20261012-080743-movement-smoothing/`; install
certificate is beside the source-bound test certificate. All **16 owned hashes**
verify. Native bridge, loader and protocol did not change. No C++ changed, so
there was no C++ build/format step. The fixed straight-walk helper is unchanged.

## 2026-10-12 08:10–11:45: unactivated interruption preserved; fresh trial prepared

The earlier plan was
`Build/definitive-manual-test/20261012-081051-smoothing-host-ready/`. Assistant
usage limits interrupted the preparation. Its recovered CLEO log records the
Online adapter armed and awaiting Home at **08:13:27**, with bridge/game/network
inactive, followed by normal host shutdown and Exit at **08:19:10**. No activation,
bridge load, join or synthetic peer occurred. Boot remains unchanged. This is an
interrupted, unactivated session with normal exit, not a smoothing success or
recorded crash. Recovery record:
`interrupted-assessment.json` in that plan. Preserved CLEO is **3,344 bytes**,
SHA-256 `649aecbc8fea8ca3f48ec87101ad6bf346af2cd30c017a416b1a72719bbf1510`.

After the user resumed work, fresh readiness was collected at
`Build/definitive-diagnostics/20261012-114325-d528cf3088a547e69337d1d8fb644ef3/`.
The agent prepared a distinct baseline/plan at
`Build/definitive-manual-test/20261012-114540-smoothing-host-ready/`; the current
pointer is `current-smoothing-test.json`. Fresh server **6740**, session
**45ad570fff9c44d2ad377e50a04aa6c0**, serves `127.0.0.1:22040` until scheduled
**12:44:16 local expiry**. The agent launched GTA through Steam under the existing
automation authorization; actual game **5816** was observed. The fresh plan calls
for one bounded 30-second straight-lane peer, preserving five submitted stages,
same-ped task progression, displayed-pose readback and independent motion/network
cadence, then controlled End and normal exit. Its actual result was pending at
this preparation step; the completed immutable audit is recorded below.

Visible sliding/stride alignment, general startup stability and the older PC
freeze cause remain open. A synthetic helper does not complete the two-real-client
multiplayer milestone. Future commits should explain the goal and motivation for
separate presentation/network cadence, the 149/135 mock results and 47 audit
fixtures, then report actual trial evidence and remaining limitations separately.

## 2026-10-12 11:45–11:57: actual bounded smoothing and shutdown verified

Under the existing user authorization, the agent launched through Steam at
**11:45:51**. Actual game **5816** was born at **11:46:02**, and targeted controls
resumed the observed save and moved CJ to the previously tested flat street.
Home registered at **11:51:46**. Fresh connection/self-echo and exact game/server
identity were verified before the owned helper. The current completed plan is
`Build/definitive-manual-test/20261012-114540-smoothing-host-ready/`.

Helper **20316** ran **11:52:02–11:52:32**, game player **2**, synthetic player
**3**, ped **6406**. It completed **30.010 seconds**, **575 submitted updates**
(**115 per stage**) and **232 distinct synthetic positions**, with exit 0 and
no run/cleanup error or game departure. The same ped requested and played
idle → walk → idle → walk → idle, then was deleted. The **30 playback samples**
split **7/6/5/7/5** across tasks, all playing=true with phase progression and no
pending/invalid evidence. The lane readback passes for both directions. Received
target, requested displayed pose and readback remain separate; maximum displayed
X readback rounding error is approximately **0.0001172 units**, Y/Z errors are
zero, and maximum heading error is **0.00006103515625 degrees**. Exact target
agreement from the earlier unsmoothed trial is not this candidate's criterion.

### Runtime interpolation and independent cadence

Across the **30 logged motion windows**, totals are **1,745 display ticks**,
**574 display writes**, **582 network polls** and **190 changed network targets**.
These counters exclude the trailing fraction before deletion that has no emitted
summary. There are
**14 bounded windows** with display writes exceeding received-target updates and
**7 sampled intermediate display poses**. Full active-walk windows have
**42–51 writes/second** against **13–17 changed target updates/second**. Display
write intervals during those walking windows range approximately **15–117 ms**;
network intervals are generally near **50 ms**, with an observed **33/66 ms**
window. The **5,967/5,916 ms** setter gaps span stationary idle and are not render
gaps; they do not represent a requirement to write an unchanged pose repeatedly.

These actual game counters and readbacks verify bounded interpolation advances
between network updates without multiplying network calls. They measure adapter
opportunities/setter work, not rendered GPU frames. No unlimited extrapolation,
gait-rate calibration or absence-of-sliding conclusion follows from them.
The reviewed record sets `actual_smoothing_gameplay_verified=true` for this
measured behavior while keeping `visible_smoothness_verified=false` and
`stride_matching_verified=false`.

### Explicit visual observations and control provenance

The separate `agent-visual-observation.json` records three inspected screenshots
under the current plan's `controls/` directory. At helper elapsed **9.35 s**, the
nearby matching model right of CJ faces right in a walking pose while the outbound
lane travels +X/heading 270. At **20.62 s**, the matching character partly behind
CJ faces left in a walking pose while the return lane travels −X/heading 90.
A visible shoe appears at street height in both frames; the return pose is partly
occluded. A later screenshot shows the character absent after logged deletion.
The association uses expected model, lane, timing, facing and absence; there is
no rendered player label.

These are agent observations from still frames, not a new human visual report or
continuous recording. They support sampled outbound/return facing and apparent
foot contact, while leaving all-stage grounding, smooth continuous gait, stride
matching and no sliding unverified. The October 11 human lag/sliding report
remains unresolved. Automated rendering flags remain false in the helper/reader.

`root-control-evidence.json` preserves **18 control/observation records** proving
Resume, movement, Home, End and normal menu exit under explicit authorization.
The prepared helper inherited `gameInputSentByTools=false` from the launch pointer;
that original metadata remains intact, with a separate correction documenting
the actual input records and final true flag. No save was overwritten and no
device, drive, security or executable setting changed.

### Final acceptance, closure and remaining scope

Controlled End registered at **11:53:18**, followed by cleanup. Normal host
shutdown and Exit both logged at **11:54:40**; successful CIM inventory confirms
the game closed. The server records requested LEAVE for both helper **3** and
game player **2**. Independent final audit:
`peer-115202-df8d206b06a04ad0ac58a1a4236286b3/audit-20261012-115442-743475-final-5f10a5f2/assessment.json`.
Both unfiltered full-session and scoped strict readers pass with **one completed
session, zero active sessions and zero issues/failures**. Root-reviewed result:
`assessment-final-reviewed.json` in the current plan. Its current smoothing
pointer records **completed**, with runtime blending true and visual smoothness
false. Final full CLEO is **88,439 bytes**, SHA-256
`45ca5a5adcd611704b748772ef28e87d03d596c856db4832b115db3ee4054773`.
Original full/scoped logs and their provenance remain preserved. The exact server
identity/listening endpoint were reverified after game exit at **11:57:52**,
saved in `server-post-session-status.json` in the plan; scheduled expiry remains
**12:44:16 local time**.

Post-exit RequireReady collection:
`Build/definitive-diagnostics/20261012-115456-8238efc22d404745a9167d470675f7c9/`.
It verifies the known executable, all **16 installed files**, GPU/PnP OK/0,
unchanged October 11 19:09:35.5 boot, no new grouped dump identities and zero
collector issues/blockers. Root's finalizer separately verifies the same five
tested source hashes. No PC crash was observed during this controlled run.
Older PC-freeze causes and general startup stability
remain unresolved.

Recovery remains discoverable through `Build/definitive-recovery/current-checkpoint.json`;
retain the source/install/test certificates, raw logs, both actual and interrupted
plans, targeted controls/images, reviewed result and visual limitations together.
Next: assess visible movement and stride independently, then prepare two real
client tests. One game plus a synthetic peer does not establish playable
multiplayer. Future commits should explain the goal/motivation, independent
cadence and bounded blend, the **149/135** tests and **47 fixtures**, actual
five-stage/stop/exit evidence and unresolved visual/crash limits.

## 2026-10-12 12:13–12:16: bounded gait measurement prepared and installed

The October 11 sliding report is still unresolved. The completed smoothing
trial establishes intermediate display writes, but its one-second animation
readbacks are too sparse to directly calibrate stride. A phase fit suggested
approximately **1.133 seconds per loop**; that is an inference subject to aliasing,
not a measured physical stride or an animation-speed recommendation. Before
changing animation rate, this candidate records a faster bounded sequence of
actual phase/pose observations while the unchanged clip moves at three speeds.

### Primary command-signature research

The installed command definitions remain pinned to Sanny Builder Library commit
`34bedee0be2f57047ed8f0705c81f137ede3d3ce`. The inspected DE definitions establish
the following signatures and constraints; they do not verify behavior in this
game executable. [Pinned SA Unreal definitions](https://raw.githubusercontent.com/sannybuilder/library/34bedee0be2f57047ed8f0705c81f137ede3d3ce/sa_unreal/sa_unreal.json).

| Command | Definition | Consequence for follow-up work |
| --- | --- | --- |
| `0393 SET_CHAR_ANIM_SPEED` | Character, animation name, float speed; no output or animation-file argument | A possible later rate setter needs its own bounded runtime verification. It was not called in this candidate. |
| `0615 OPEN_SEQUENCE_TASK` | Returns a sequence handle | It is not an animation-speed command. |
| `0605 TASK_PLAY_ANIM` | Fourth input is `blendSpeed` | Existing value 4.0 controls blending; it is not a calibrated playback multiplier. |
| `06A6 TASK_PLAY_ANIM_WITH_VELOCITY_EXTRACTION` | Marked NOP | It cannot supply the proposed DE root-motion path from this definition. |
| `0620 TASK_PLAY_ANIM_FOR_TIME` | Marked NOP | It is not a usable alternate timed-animation implementation here. |

No animation-speed getter was found in the pinned DE command set. The CLEO API
documents `native(name, ...inputs)` using definition names and outputs; missing
getter evidence must not be replaced with a fabricated successful readback.
[CLEO Redux native API](https://re.cleo.li/docs/en/api.html#native).

### Probe bounds and measurement semantics

Online enables one read-only probe for the **first owned remote ped in each
connection**. It takes at most one sample per due **100 ms network tick**,
without catch-up bursts, for at most **300 samples/30 seconds**. Shared one-second
position and animation observations avoid duplicate native queries when due.
Samples retain absolute `sample_at_ms`, `probe_elapsed_ms`, current `task_age_ms`,
clip/state/playing/phase and the separate received target, requested display pose
and observed pose. New tasks can already have advanced on their first sample;
phase zero is never injected. The chosen ped must still exist and must not be
CJ. Deletion, loading/reset/stop, lost ownership, clock reversal, render/network
gaps and target discontinuities end the probe; another ped in the same connection
does not silently restart it.

The production change adds no animation task, animation-speed setter, model,
memory hook or native command. It does not modify smoothing, bridge/network
cadence, received Z, loader or C++. Existing task transition and cleanup policies
remain. This is measurement for a later implementation decision, not a claimed
repair of sliding.

### Separate synthetic speed sweep

`stride-speed-sweep` retains the first valid anchor and reuses the tested flat
lane, with X endpoints anchor ±3, Y anchor +3 and copied anchor Z. At the
**30-second/radius-5 defaults**, its nominal stages are:

| Stage | Seconds | Motion | Heading candidate |
| --- | ---: | --- | ---: |
| `idle_start` | 2 | Stationary at −3 X | 270 |
| `walk_slow` | 12 | +X at 0.5 units/s | 270 |
| `idle_turn_one` | 2 | Stationary at +3 X | 90 |
| `walk_medium` | 6 | −X at 1.0 units/s | 90 |
| `idle_turn_two` | 2 | Stationary at −3 X | 270 |
| `walk_fast` | 4 | +X at 1.5 units/s | 270 |
| `idle_end` | 2 | Stationary at +3 X | 270 |

Parameterized sweep speeds are restricted to **0.25–1.5 units/s before DLL
loading**. The lower boundary need not cross the controller's 0.02-unit motion
threshold at a 50 ms sample, so live measurement uses the defaults. Waiting for
an anchor still consumes the fixed deadline. Submitted-stage completeness alone
does not prove each nominal idle duration: the audit must also check full bounded
duration, no game departure and same-ped seven-task progression. Default orbit,
idle/walk/idle and five-stage straight calibration retain their behavior.

### Source/install certificates and pending trial

Root's combined source-bound check passed **180 adapter mocks** and **153 utility
tests**, including **69 peer mocks**, without game/server/native-bridge execution.
The test and install certificates are at
`Build/definitive-stride-check/20261012-121336-verified-8eee296d920b490bbbcbb879f6a57451/`.
The audit's **61 passing fixture checks** are recorded at
`Build/definitive-stride-audit-check/20261012-121609-172817-fixtures-6f2ef817/verification-result.json`.
Mock/fixture success is not actual gameplay or visual evidence.

| Tested source | SHA-256 |
| --- | --- |
| `game-adapter/controller.mjs` | `a2e2fb6a0973100517868481da00e4879387ab0274baa8516b5f689013fca799` |
| `game-adapter/index.js` | `5aa1a8bf3d16b3e09848adea1761dd3cddc5921d1226c51c01d7c9e059ba561a` |
| `Tests/definitive/test_game_adapter.cjs` | `f3b24d925956fb12397be53d96d7eeb29b22f99f08d64495b7f624313d2f27d5` |
| `utils/definitive_edition/game_peer.py` | `2266e1b4295062e2e50c80d10cd48f6d09e2768f21f21f233670f61ad2420621` |
| `utils/definitive_edition/test_game_peer.py` | `55ad42d836bd486d5d2e0d6390e5fb03249a613229d30db6cd5bde62c2541ba5` |

Installation at **12:13:58** changed only `CLEO/de-online/controller.mjs` and
`CLEO/de-online/index.js`. All **16 owned hashes** verify; the other 14 package
files are unchanged. Previous script bytes and restore mapping are in
`Build/definitive-update-backups/20261012-121339-movement-stride/`.
No C++ changed, so no native build or formatting step was needed.

Plan: `Build/definitive-manual-test/20261012-121416-stride-host-ready/`, discoverable
through `current-stride-test.json`. Its baseline preserves the previous final
CLEO log, **88,439 bytes**, SHA-256
`45ca5a5adcd611704b748772ef28e87d03d596c856db4832b115db3ee4054773`.
Fresh readiness is at
`Build/definitive-diagnostics/20261012-121358-cf2be884914746b9829862a849f48993/`.
Root reverified server **6740**/session **45ad570fff9c44d2ad377e50a04aa6c0**,
nonce-matching welcome and requested probe departure, without any UPDATE packet.
Steam launch was **12:14:18**; actual game **14068** was born at **12:14:29**.
The actual seven-stage trial, final lifecycle and post-exit collector were pending
at this preparation step; their result must be added as a separate record.

Future commits should explain the small-client goal, why stride measurement
precedes guessed playback rates, the unchanged smoothing/tasks/native scope,
the **180/153** checks and **61 fixtures**, then actual phase-fit/stop/exit evidence
and explicit visual/crash limits. One game plus a synthetic helper still does
not establish two-real-client playable multiplayer.

## 2026-10-12 12:14–12:32: actual seven-stage gait timing and shutdown verified

The agent launched through Steam at **12:14:18** under the existing authorization.
Actual game **14068** was born at **12:14:29**. Targeted controls resumed the
observed save and returned CJ to the tested flat street. Two one-second Home
attempts at **12:21:49** and **12:22:21** did not produce an activation marker.
The local control helper then used standard Win32 extended-key handling and a
bounded three-second hold, checked that the pinned game window remained focused,
and recorded the down-state high bit for Ctrl/Shift/Home (virtual keys 17/16/36).
All three keys were observed held. Adapter activation registered at **12:25:04**.

Microsoft documents the extended-key flag and the current-down high bit used by
these checks. [KEYBDINPUT](https://learn.microsoft.com/en-us/windows/win32/api/winuser/ns-winuser-keybdinput),
[GetAsyncKeyState](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-getasynckeystate).
The corrected injection and longer duration changed together, so this sequence
does not identify which caused activation to register. The helper changed only
ordinary local input/verification, not the game's memory, installed adapter or
shortcut bindings. The final control certificate preserves **19** records and
true game-launch/input provenance in `root-control-evidence-final.json`.
Helper preparation already recorded the actual input flag correctly before the
peer; no inherited false-input claim is used for this trial.

### Owned helper and probe lifecycle

After fresh game/server identity, connection and self-echo checks, the single
owned helper ran **12:26:01–12:26:32**, for **30.003 seconds**. Game player **5**
displayed synthetic player **6**, ped **6153**. The helper submitted **570 updates**
and **405 distinct positions**, completed every requested stage, exited 0 and
reported no run/cleanup error or game departure. The raw submitted-stage counts
are retained rather than reusing the previous trial's 575-update total:

| Stage | Submitted updates | Valid gait-phase samples |
| --- | ---: | ---: |
| `idle_start` | 38 | 18 |
| `walk_slow` | 229 | 106 |
| `idle_turn_one` | 38 | 15 |
| `walk_medium` | 114 | 58 |
| `idle_turn_two` | 38 | 14 |
| `walk_fast` | 76 | 38 |
| `idle_end` | 37 | 14 |

The one read-only probe recorded **263 valid samples**, with distinct playing
phases in all seven task requests. It finished for reason `deleted` at elapsed
**29,901 ms**, before the corresponding owned-ped deletion. No sampling crossed
the deletion boundary. Existing one-second records separately verify seven
idle/walk tasks, the same owned handle, requested/observed lane and bounded motion.
Smoothing remains measured: **30 motion windows** record **1,738 display ticks**,
**1,046 display writes**, **582 network polls** and **343 changed received targets**,
with **25** faster-write windows and **16** sampled intermediate poses. These
counts exclude the trailing fraction without an emitted summary and do not
measure rendered GPU frames.

### Measured clip cadence and explicit stride limits

The audit unwraps actual observed phases only within contiguous playback of the
same task. It never joins across a task request, missing playback, discontinuity,
clock reset or sample gap over 250 ms, and requires at least two observed wraps.
The three valid walking fits are:

| Walking stage | Commanded speed (units/s) | Samples / observed wraps | Clip period (s/cycle) | Signed average displayed X speed (units/s) | Display translation (units/cycle) |
| --- | ---: | --- | ---: | ---: | ---: |
| Slow, +X | 0.5 | 106 / 10 | 1.1333532744 | 0.493587778 | 0.559409324 |
| Medium, −X | 1.0 | 58 / 5 | 1.1333980419 | −0.944881890 | 1.070927284 |
| Fast, +X | 1.5 | 38 / 4 | 1.1332938114 | 1.405732996 | 1.593108505 |

The approximately **0.8823 cycles/second** cadence stays nearly constant while
commanded translation changes threefold. This is actual evidence that the
unchanged clip's cadence does not scale with this helper's movement speed.
The displayed speed and distance-per-cycle are descriptive **averages of each
owned walking task**, including smoothing and the **350 ms stationary walking
hold** at its endpoint. They are not steady movement speeds, moving-only stride
lengths, extracted root motion or distances between foot contacts. There is no
physical no-slip reference from these values, so they cannot calibrate an
animation multiplier. `animation_speed_was_changed=false`,
`foot_contact_or_no_sliding_verified=false`, `visible_smoothness_verified=false`
and `stride_matching_verified=false` remain explicit in the reviewed result.

### Audit correction retained with original evidence

The initial post-peer audit at
`peer-122601-6d3c3ab7935c4d8ab1499fb5cef0994e/audit-20261012-122642-447902-post-peer-928d7d3b/`
reported incomplete phase progression because three short idle tasks had only
one one-second playback sample. That raw failed/incomplete assessment remains
preserved. The reader was corrected to accept the already validated **100 ms
probe phases from the same owned task request** as phase-progress evidence.
It retains the original one-second statistic as false, the original samples,
additional bounded samples and their source-line provenance. The requirement
for actual phase progression in every task remains; it was not removed.

The final reader passes **66 fixtures**, including rejection of task mismatches
and invalid probe evidence:
`Build/definitive-stride-audit-check/20261012-122940-976800-fixtures-4cdead0a/verification-result.json`.
No installed prototype change or additional game trial occurred after this
reader correction. Final independent audit:
`peer-122601-6d3c3ab7935c4d8ab1499fb5cef0994e/audit-20261012-123058-154265-final-0ffea79a/assessment.json`.
Both the original unfiltered full-session and scoped strict readers pass with
**one completed session, zero active sessions and zero issues/failures**.

### Separate sampled visual observations

`agent-visual-observation.json` records the root's inspected captures. At elapsed
**10.72 s**, the nearby matching character faces right in a walking pose with
shoes apparently near the road. At **19.71 s**, it is largely occluded by CJ;
its head appears left-facing, but its leg pose and foot contact cannot be assessed.
At **26.71 s**, the matching character faces right with an extended walking leg
and a shoe apparently near the road. A later frame shows it absent after logged
owned deletion. Association is inferred from model, lane, timing and absence;
no rendered player label identifies it.

These are still-frame agent observations, not a new human report or continuous
recording. They do not prove every-stage ground contact, visible smoothness,
physical stride, sliding or absence of sliding. The October 11 human lag/sliding
report remains open. The primary measurement above concerns clip phase timing.

### Closure, hash verification and recovery

Controlled End registered at **12:27:44**. Normal host shutdown and Exit both
logged at **12:30:30**, and fresh CIM inventory confirms the game closed. The
server records requested departure for both helper **6** and game player **5**.
Post-exit RequireReady collection:
`Build/definitive-diagnostics/20261012-123055-9f3622d2984d430286025bfdb61f318d/`.
It verifies the known executable, all **16 owned installed hashes**, GPU/PnP
OK/0, unchanged October 11 **19:09:35.5** boot, no new grouped dump identities
and no collector issues/blockers. Root separately verified all **five tested
source hashes**. No PC crash was observed during this controlled trial;
older freeze causes and general runtime stability remain unresolved.

The final full CLEO snapshot is **284,399 bytes**, SHA-256
`f4a5a8cc038fde31a29d6920b96cfeaf16e2a0143458a73e68e50da492f625bd`.
Root-reviewed result: `assessment-final-reviewed.json` in
`Build/definitive-manual-test/20261012-121416-stride-host-ready/`.
`current-stride-test.json` records **completed** and
`actual_stride_probe_verified=true`, with playback rate and visual/stride limits
kept separate. Exact server **6740**/session
**45ad570fff9c44d2ad377e50a04aa6c0** identity and endpoint were reverified at
**12:32:20** after exit; its scheduled expiry remains **12:44:16 local time**.
Keep the reviewed result, raw incomplete audit, final audit, both evidence scopes,
fixtures, all source/install hashes and controls together in recovery, discoverable
through `Build/definitive-recovery/current-checkpoint.json`.

Next work needs a calibrated temporal foot-contact reference before implementing
bounded animation-rate control. Two real game clients remain a separate pending
milestone; one GTA plus a synthetic peer does not establish playable multiplayer.
Future commits should include the goal, why measurement precedes guessed rate
changes, the **180/153** tests (**69 peer mocks**) and **66 audit fixtures**, the
actual **570-update/263-sample** measurements and complete End/exit sequence,
reader-correction provenance and remaining physical-stride/visual/crash limits.

## 2026-10-12 17:05–17:19: recovery completed and native reference installed

The interrupted recovery step was completed before new source edits. The
completed stride checkpoint is
`Build/definitive-recovery/20261012-170540-stride-probe-complete/checkpoint.json`.
Independent verification passed six archives/985 entries, 37 source files, all
five accepted-trial hashes and all 37 saved audit snapshots. Records from 26
historical checkpoint directories and 33 unchanged historical archives remain
preserved. The raw incomplete stride audit and its corrected accepted reader
remain together. Source archive SHA-256:
`676dd1179f34634f6d7351f4ade6d8b9ce27cfbb719114f962995a14be3072a4`.

The new reference is a separate opt-in Smoke controller with no transport. It
uses supported `TASK_GO_STRAIGHT_TO_COORD` (`05D3`) with six inputs and finite
10,000 ms timeout, and `MoveState.Walk=4` from the
[pinned DE enums](https://raw.githubusercontent.com/sannybuilder/library/34bedee0be2f57047ed8f0705c81f137ede3d3ce/sa_unreal/enums.json).
The [pinned command definitions](https://raw.githubusercontent.com/sannybuilder/library/34bedee0be2f57047ed8f0705c81f137ede3d3ce/sa_unreal/sa_unreal.json)
mark the no-stopping walk and animation velocity-extraction alternatives as
NOPs, and the foot-down queries as unsupported. No bone-position getter is
provided. Foot-contact timing therefore needs visual evidence; body-center
height and a moving animation phase alone do not establish it.
[CLEO native semantics](https://re.cleo.li/docs/en/api.html#native) support guarded
boolean playback queries and single-float phase outputs for the observed clip.

The source-bound result is 186 adapter/36 reference/185 utility tests with 32
reader fixtures. A last reader hardening changed two host-side files after the
first certificate; the install preparation correctly rejected those mismatched
hashes before touching the game. The full suites were rerun and all nine final
hashes bound to the accepted 17:18:18 certificate. The Online package was updated
in three owned script paths, followed by explicit Smoke/reference selection.
All 16 installed files were independently snapshotted and hash-verified. The
other 13 files, including native bridge, loader and API definitions, remain the
verified bytes. No C++ changed, so native compilation/formatting was unnecessary.

The user volunteered to perform walking and visual checks; this attempt's input
and gameplay owner is human. Root prepared/installed the code and will collect
and audit the evidence. No agent launch or input occurred. The new plan remains
`awaiting_human_gameplay`; its actual/foot-contact/stride/rate flags are false.
The reader must preserve unavailable clips or incomplete cleanup instead of
calling them a successful physical calibration. A future bounded rate trial
can be based on an accepted native reference and explicit visual observations.

Future commit messages should include the user's small-client goal, why an
engine-driven reference follows the fixed-cadence finding, how task/ownership
bounds were chosen, the 186/36/185 checks and 32 fixtures, actual gameplay and
visual results when collected, and remaining multiplayer/crash/stride limits.

## 2026-10-12 17:19–17:25: human-run natural walk reference completed

The user launched GTA through Steam (actual game3736 born17:19:47), loaded the
flat street and activated the reference. Its single owned ped6658 completed
the native walk and was deleted/stopped at17:20:42, with42 actual pose/phase
samples. The first unavailable clip sample, seven warmup samples and three
samples in the destination stopping region were excluded. The accepted31-sample
steady segment spans4.000seconds and four actual wraps, measuring1.5216103452786847
units/second,0.8823398519979847cycles/second,1.1333501459053263seconds/cycle and
1.7245173068326511units/cycle. No invalid/rejected evidence or adapter failure
occurred; all strict ownership/task/budget/cleanup flags pass.

Human observation is preserved verbatim as “Walked naturally; no obvious sliding”
in `human-visual-observation.json`. This is one finite reference without continuous
recording or measured foot-contact timing. It does not establish absence of
sliding across multiplayer speeds. The identical natural and earlier forced
clip periods now support an experimental rate of horizontal travel speed divided
by1.5216103452786847; a subsequent trial must independently verify that the speed
setter changes actual cadence and improves what the user sees.

The reference had already disposed itself, so no additional End was needed.
The user exited normally; host shutdown/Exit logged17:22:37. The final full log
is30,828bytes/SHA256 `00f0fb08f2bbcdc0430e3bb349a614b6d53874add06ed998f387747c07dbdd88`.
Final audit: `audit-172507-80aa85a3/assessment.json` in the current reference plan.
Root reviewed result: `assessment-final-reviewed.json`; pointer statuscompleted,
engine_reference_measured=true, human_observed_no_obvious_sliding=true, normal
exit/game closure=true. Physical-contact, multiplayer-stride and bounded-rate
implementation flags remainfalse.

Post-exit RequireReady diagnostics:
`Build/definitive-diagnostics/20261012-172313-aa6d6ed5e220426889ddb1d3f678f3de/`.
Known executable, all16owned installed hashes, GPU/PnP OK/0 and no collector
issues/blockers verify; all nine accepted source hashes were separately checked
before finalization. Gameplay/input and visual judgment belonged to the human;
root performed no launch or input. The prepared checkpoint17:20:46 remains
preserved separately, and the completed result needs its own sealed checkpoint
before the next source change.

## 2026-10-12 17:28–17:42: opt-in calibrated gait-rate trial prepared

The accepted native reference was sealed before any rate source edit:
`Build/definitive-recovery/20261012-172840-native-gait-reference-complete-72d1ad7f/checkpoint.json`.
Its independently verified source archive SHA-256 is
`b8a3580b1af05af5be323ff591672ebeb1b64e07f90c0a87f7dbb32ef5fd6e53`.
The prior prepared reference and completed stride records remain distinct.

Online can now opt into a bounded `SET_CHAR_ANIM_SPEED` request on an owned
playing `Walk_civi` association. The baseline comes from the accepted human-run
native fit; it is not a classic-game speed assumption. Changed received XY
endpoints contribute accumulated path and elapsed time over 250–500 ms. Cached
polls do not halve the denominator or double speed. The requested multiplier
is speed/1.5216103452786847, clamped to 0.25–2. A valid window must be no older
than 250 ms. Setter/query throttles are 250 ms, with 0.05 minimum setter delta;
existing same-tick playback observations are shared. Clock, gap, target,
connection and task discontinuities reset inference. Idle/CJ/unowned handles
never get a rate setter. No task replay or phase-zero assumption was introduced.
The 350 ms movement hold and original blend/task policy remain, so stationary
endpoint gait is separately excluded from the timing fit.

Requested-rate events carry task/ownership linkage, actual clock and speed-window
provenance. The 100 ms samples retain the currently requested rate. The new strict
reader requires two observed wraps per stable walking segment and compares real
cadence with baseline cadence times requested rate, within its declared 10%
tolerance. It rejects mismatched, reset, gapped or insufficient evidence and
preserves skipped/failed fit provenance. There is no invented speed getter or
setter-success readback. Foot contact and visual sliding remain human observations.

Final certificate at 17:40:49 binds 13 sources after 230 adapter mocks, 36
unchanged native-reference tests and 215 utility tests, including 69 peer mocks
and 30 cadence-reader fixtures. Source defaults keep both experiment flags
false. Packaging explicitly enabled the Online rate trial; three installed
scripts changed, and all 16 owned files were independently snapshotted and
verified. The 13 other files, native DLL/loader/definitions and C++ remain the
verified bytes. Backup and current plan are linked in Current result above.

Server 15772/session 4f40e691dc0548b5a9d2e4f06ccbf957 started at 17:41:09 and
expires at 18:41:09 local time. Exact process/birth/command/endpoint identity,
nonce-matching welcome and requested probe departure were verified without an
UPDATE. Fresh post-install RequireReady diagnostics passed. The human will
launch/load/activate and judge the three-speed sweep; root will verify fresh
connection/self-echo, start one bounded owned synthetic peer and audit playback,
cleanup and final shutdown. Actual rate cadence and sliding improvement are
pending; one GTA plus a synthetic helper does not prove two-real-client multiplayer.

Future commits should state the small-client goal, why the measured native
reference was used, why cached snapshots need a time window, rate/task/ownership
bounds, the 230/36/215 checks and 30 fixtures, and the actual game/visual results
when available. Retain the scoped human reference observation separately from
any later claim about remote stride or absence of sliding.

## 2026-10-12 17:54–17:58 trial; 22:22–22:28 final regression audit

The actual first rate-enabled peer completed 17:54:01–17:54:31 with exit 0.
Game 11512 (born 17:51:58) displayed peer 4/ped 11777 while connected as player 3.
263 samples cover all seven requested stages; owned cleanup completed. The user
reported backward jumps/chop during repeated walking loops. This is a failed
visual result, despite valid native calls and correct finite helper cleanup.
End and normal shutdown/Exit were verified at 17:56:47 and 17:58:15.

The final immutable log has 342,994 bytes, SHA-256
`30ced91859f17f748ed51030816b4ec59f3aacc5bc368981573d960125b3bfce`.
The original full reader reports invalid evidence from repeated local joins
2/3/5/6. Independent scope copies original lines 1–52 and 64–557 into a separate
546-line file, with complete original-line mapping and no invented markers.
That scope identifies the readiness-matching player 3 through peer 4 deletion.
It has zero issues and rejected samples, but remains incomplete because no
walking stage has the required stable two-wrap fit. Full failures and scope
limitations remain together; scoped selection does not erase the reconnects.

There were 24/12/10 setter requests. Their rate ranges and short stable spans
are recorded above. Across 29 short segments, observed phase cadence agrees
with requested-rate-scaled baseline within 0.73%; independent integration across
changing rates agrees within 0.015%/0.031%/0.051%. These describe real setter
response and do not satisfy the unchanged stable-rate acceptance requirement.
Sampled network X speeds remain 0.499391/−1.003138/1.497102 units/second.
There are no sampled backward target/display/body steps, including around phase
wraps. The human's visible rewind therefore needs an independent rendering/root
investigation; entity-coordinate readback alone cannot dismiss it.

The post-exit collector at
`Build/definitive-diagnostics/20261012-222222-7f18b2673f5a4b9faed5dba115c00cce/`
verifies closed game, known executable, all 16 owned hashes, GPU/PnP OK/0 and
no collector issues/blockers. All 13 accepted source hashes were separately
verified before finalization. No agent game launch or input occurred. The prior
server expired normally; its old expiry is historical, not current readiness.

Official DE definitions expose `TASK_PLAY_ANIM` horizontal-lock arguments;
classic MTA animation code separately controls root X/Y extraction through
animation flags. This supports a bounded one-variable lock trial, but is an
inference about DE behavior until the human observes it. The rate estimator's
confirmed jitter will be reproduced from the preserved trace and repaired
separately. Runtime cadence, stride matching and absence of sliding remain false.

Future commits should explain the original goal, measured reference, why the
first estimator failed under real receipt jitter, source-bound 230/36/215 checks,
the actual failed/scoped evidence, human visual regression, proposed isolated
root test and remaining rendering/stride/multiplayer limits.
