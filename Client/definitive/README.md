# Definitive Edition compatibility harness for the MTA port

**The target is the existing MTA client on Definitive Edition.** On October 13
the user clarified that MTA's existing systems should be reused. This prototype
is a compatibility test harness; its custom transport is not the target MTA
network implementation. Further work starts from the existing client/game
interfaces and synchronization code.

**Next live check: actual MTA Core startup.**
`package.ps1 -Mode Smoke -MtaCoreStartupReference` stages 21 owned files,
including the normal opt-in x64 `core.dll` and its `pthread.dll` dependency.
After manual Ctrl+Shift+F9 activation and release, the bridge loads that actual
Core, sets its MTA/game directories, calls `InitializeCore`, reads its startup
diagnostics and releases it. No walking, NPC, network session or input write is
part of this check. The expected result is the unchanged incomplete-gameplay
gate after exact-image binding and a backend pulse, followed by clean unloading.
It does not start the full MTA client or a DE renderer. Arming waits indefinitely.

The x64 Core now delay-loads its legacy D3DX9 dependency so this early startup
path does not load a D3D9 renderer. Real Core entry checks pass 93 cases; the
classic Win32 option/default builds retain their existing exports and behavior.
The in-game Core check is pending. Use `utils/definitive_edition/core_startup_log.py`
to assess its saved log. Source defaults and ordinary mode switches disable it;
it is mutually exclusive with the other Smoke trials. Intact 16/17/19-file
installs upgrade with ownership/collision checks, and the Core dependency pair
is always installed or removed together.
The 21-file ownership suite passes 335 checks across 30 cases, including all
four historical install generations and loader disable/enable round trips.

**ABI5 adds an explicit MTA-owned engine movement trial.** Build the normal
Game DE Release/x64 module and this bridge, then package with
`package.ps1 -Mode Smoke -MtaNativeMovementTrial`. The current package includes
the real `game_de.dll`, loaded through Core's actual module consumer only after
activation. Intact 16/17-file installs can be upgraded; occupied new paths
reject the update before any writes. An ordinary mode switch clears the trial.
The source configuration defaults to disabled.

Launch the game through Steam on a clear flat street, hold Ctrl+Shift+F9 for
three seconds, then release all keys and leave the controls alone for 22 seconds.
The local player receives MTA walk, forwards and sprint inputs separated by
neutral pauses. Ctrl+Shift+End cancels. Arming has no timeout before F9.
JavaScript supplies logical actions and observations; the normal MTA backend
owns native callback routing, scoped pad writes and restoration. This trial
does not create an NPC or network session. Logs establish callback/query and
cleanup evidence; the human checks how movement looks.

The thread-corrected trial has completed successfully in the game. The human
reported movement phases and sprinting working; its 165 samples record 1,317
applied/restored callback scopes, 44,655 routed queries and no recorded failures.
Normal exit and all 19 installed hashes pass. This proves the scoped local
movement slice, with full MTA startup/networking/rendering still required.

**Current development integrates the adapter into MTA.** The normal build now
includes `Client/game_de` and a Core-owned module consumer. MTA controls module
lifecycle through a versioned SDK interface; controller storage moved into that
module, with this harness retaining a compatibility include. The opt-in Core
branch rejects missing gameplay capabilities before its classic startup code.
The module supports caller observations and now reuses Core's existing digital
action synthesis through ABI2. Logical walk/sprint state stays separate from
native observations. ABI3 also provides an explicit build-pinned native pad
reader in `game_de`; the first live trial rejected the proposed controller layout.
ABI4 added raw provider-source readback through the same Core-owned interface.
Both reads expose only diagnostic candidate data. ABI5's scoped local write
candidate leaves full engine input and remote player capabilities unavailable. Classic Core
calls the same helper while retaining its bind callbacks and joystick/native
write policy. Real opt-in Release/x64 Core now builds and links; full
game/multiplayer, launcher and rendering services remain required.
Current build and regression results are recorded in the
[module documentation](../game_de/README.md) and [integration plan](MTA_REUSE_PLAN.md).

**Completed live test of MTA's native reader.**
`-MtaNativePadReference` selects a separate read-only Smoke trial that compiles
the actual `Client/game_de/NativePadReader.cpp` into this diagnostic bridge.
It records both candidates' proposed 24-field current/previous states, position and raw
velocity for 30 seconds. It is separate from the earlier zero-valued
`GET_PAD_STATE` experiment and MTA's synthesized logical actions. The existing
loader schedules observations; it does not host a complete MTA gameplay client.
Ctrl+Shift+F9 starts the trial and Ctrl+Shift+End cancels it. No NPC, transport or
game-input writes are used. Packages now contain 17 owned files; intact older
16-file installs remain valid, with a narrowly checked additive script upgrade.
Use `utils/definitive_edition/native_pad_log.py` for this trial's evidence.
The human reported normal walking and sprinting, and all 225 captures and cleanup
completed. Both candidates' axes, sprint and previous-state fields stayed zero;
the adapter therefore keeps `EngineInputRead` unavailable. The saved result is
linked from `Build/definitive-manual-test/current-native-pad-reference-test.json`.

The next candidate also captures all 127 signed source states used by the active
provider's native axis implementation. Capture version 2 keeps these separate
from the rejected pad layout. Use the log reader's `--require-device-states`
option for this candidate; older pad-only recordings remain readable but cannot
validate the new capture. Physical key/action mapping is still unverified.

The first source capture now confirms the native W/S/A sources. D and sprint
were beyond its 30-second window, so the recorder now starts after all activation
keys are released and allows 60 seconds. It waits indefinitely before F9;
the human can run the test whenever available. Complete controller mapping and
engine writes remain unverified.

The release-started minute trial completed 449 captures, clean disposal and
normal exit. D and W+LeftShift are observed, along with 76 triple-held captures
in the uncertain W/D/LeftShift segment. Key capture is sufficient to continue;
that segment's movement outcome and complete controller mapping remain open.
Core's original control-bind state policy is now shared in
`Shared/sdk/CControlBindState.h`, with its existing callback loop retained.
The extraction passes 12,650 oracle checks on each architecture and classic
Core compile-only checks with the adapter option both enabled and disabled.

The first shared slice extracts MTA's existing incremental ped position
correction into `Shared/sdk/CPedPositionInterpolation.h`. The classic client
calls it, and the same code passes 117 independent checks on both x86 and x64.
Run `Tests/definitive/test_mta_ped_interpolation.ps1`; see the
[reuse plan](MTA_REUSE_PLAN.md). This is source-level reuse verification; a
complete MTA client build and DE player/input backend are still required.

**Completed compatibility test: read-only local observations.**
The unchanged 48-byte `CControllerState` is shared by the classic client and
x64 bridge. Controller-layout checks (219) and snapshot transaction checks
(930) pass on both x86 and x64. The existing Win32 Release deathmatch gameplay
DLL compiles and links; this does not build the full client stack.

`INPUT_REFERENCE` defaults false. `-NativeInputReference` selects Smoke only
and always requires manual Ctrl+Shift+F9 activation. For 30 seconds the human stands,
walks and sprints normally; the harness reads 20 pad fields, raw engine velocity
and position. It publishes and reads back MTA's canonical state, with explicitly
unknown logical extras and velocity units. No network, NPCs or game-input writes
are used. The previous snapshot is the previous probe sample, not an engine
frame. `native_input_log.py` audits the saved evidence. That candidate was installed
in Smoke with all 16 owned hashes verified; the loader and definitions are
unchanged. The final source-bound run passes 47 input, 261 adapter, 36 gait,
250 utility, 33 native integration and 293 protocol/math checks. See
`Build/definitive-manual-test/current-input-reference-test.json`.
End cancels or stops the probe.

The first attempt stopped after 22 idle samples; the human identified Home opening the
Rockstar console. Cleanup passed, and the human reported normal movement; that
interrupted recording did not verify movement input or velocity. F9 avoided the conflict;
the full F9 retry ran for 30 seconds and reset cleanly. It captured 227 valid
samples and changing raw velocity, but every pad field remained zero while the
human used keyboard/mouse. Normal exit is verified. Pad mapping remains
unverified; further backend work follows MTA's own module and input paths.

**The horizontal animation-lock experiment completed its human test.** The
user reported normal walking with no repeated loop/rewind, while some choppiness
remained. The observed stages were very slow, slow and normal walking; no running
animation was tested. All seven tasks and requested root policies, owned cleanup,
End and normal exit were logged. The 264-sample cadence audit remains incomplete
because rate jitter left no stable two-wrap fits. This is a scoped visual
improvement, not a complete movement or MTA compatibility result.

Historical instructions for this completed comparison:
Launch GTA through Steam, stand on the same flat outdoor street, hold
**Ctrl+Shift+Home for three seconds**, then report ready and keep GTA focused.
The agent will start the 30-second NPC sweep at 0.5, 1.0 and 1.5 units/second.
Stay still with a side view, watch for repeated backward jumps and foot sliding, and
wait for the NPC to disappear. **Ctrl+Shift+End** stops the Online adapter;
the agent will inspect cleanup before normal game exit.

The previous rate-enabled trial produced visible backward jumps/chop. Its
100 ms samples showed continuous body coordinates, so this experiment tests
whether the walking animation's rendered horizontal root motion caused those
jumps. Only the `Walk_civi` task's X/Y lock flags change; idle tasks, the rate
estimator, bridge and loader keep their tested behavior. The flags are a
hypothesis to check visually, not proof of rendered root position.

The measured native speed sets an experimental walking multiplier, using
received XY path over 250–500 ms and a 0.25–2 rate clamp. The controller observes
playback before setting the rate, limits setters to one per 250 ms and preserves
smoothing/task transitions/cleanup. The reader must verify actual phase cadence
across all three speeds; requested setter values alone cannot pass. Source
defaults keep `GAIT_RATE=false` and `GAIT_ROOT_LOCK=false`; package/mode selection
explicitly opts in with `-MatchedGaitRate -LockWalkRoot` in Online mode.
At preparation, all 233 adapter, 36 native-reference and 215
utility checks passed, including 30 cadence-reader fixtures; 13 source hashes
and all 16 installed hashes verified. Historical plan:
`Build/definitive-manual-test/current-root-lock-test.json`.
The failed rate trial remains separately discoverable through
`current-gait-rate-test.json`. Its 46 rate changes prevented a stable two-wrap
cadence fit. The root-lock comparison improved the reported repeated rewind,
while rate jitter, accepted cadence, physical stride and two real clients remain
unresolved. Existing MTA synchronization and input-driven locomotion now take
priority over another custom estimator.

## Completed native walking reference

**The offline native walking reference completed its human-run GTA test.**
The measured natural `Walk_civi` speed is **1.5216 units/second** at a **1.13335-second
cycle**. The user reported natural walking with no obvious sliding. All 42
samples, owned cleanup and normal game exit were audited; the steady fit uses
31 samples and four observed wraps. This reference supplies the baseline for
the pending speed-matched remote animation trial.

For this reference, launch through Steam, load the same flat outdoor street and
hold **Ctrl+Shift+Home for three seconds** while GTA is focused. One nearby NPC
settles briefly, walks across a short eight-unit lane under game control, then
disappears on arrival or within ten seconds of starting its walk. Watch its feet
from the side and report natural contact, sliding or failure to appear. Keep
GTA open afterward for log inspection; **Ctrl+Shift+End** can cancel or stop the
reference. The user handles visual observation, and the agent handles the
measurements and audit.

The explicitly selected Smoke reference leaves natural movement and gait to
the engine after one initial pose setup. It takes at most 100 actual pose/phase
samples at 100 ms, owns one NPC and no transport, and performs bounded cleanup.
Source defaults keep the reference disabled; ordinary Smoke remains read-only.
Select it with `set-mode.ps1 -Mode Smoke -NativeGaitReference` while GTA is closed;
an ordinary mode selection clears the flag. The strict `native_gait_log.py`
reader excludes warmup/stopping samples and requires matching ownership, task,
contiguous phase evidence and completed cleanup before reporting an engine
reference. That result does not independently establish visible foot contact.

Checks passed **186 adapter mocks**, **36 reference-controller tests** and
**185 utility tests** including **32 reader fixtures**, with all nine current
source hashes bound. The installed profile's **16 owned hashes** verify.
Current plan is discoverable through
`Build/definitive-manual-test/current-native-reference-test.json`; source/test
certificate is in `Build/definitive-native-reference-check/current-candidate.json`.
The findings tracker retains preparation, rollback and historical evidence.
The completed reference is saved separately from the pending sliding repair
and two-real-client milestone.

## Completed October 12 gait timing

**The installed read-only gait probe completed its actual seven-stage GTA trial.**
The **12:26:01–12:26:32** helper submitted **570 updates** and **405 positions**
in **30.003 seconds**, with all seven stages, exit 0 and no run/cleanup error or
game departure. The unchanged `PED/Walk_civi` clip measured approximately
**1.1333 seconds/cycle** at commanded translation speeds **0.5/1.0/1.5 units/s**.
This establishes fixed playback cadence, while foot stride and sliding remain
unverified. The first owned remote
ped in a connection is sampled at most every **100 ms**, for at most **300
samples/30 seconds**. Existing one-second transform/playback queries are shared.
Samples record actual timestamps, task age, clip, playing flag, phase, received
target, requested displayed pose and actual readback; a task does not assume
phase zero. Cleanup, budgets and clock discontinuities terminate the probe.

Only the two installed adapter scripts changed. Existing tasks, playback rates,
native commands, DLL, loader, protocol, C++ and smoothing remain unchanged.
Current source-bound checks pass **180 adapter mocks**, **153 utility tests**
(including **69 peer mocks**); the stride audit passes **66 fixtures**.
All **16 installed hashes** verify. Certificates:
`Build/definitive-stride-check/20261012-121336-verified-8eee296d920b490bbbcbb879f6a57451/`.
Prior installed bytes:
`Build/definitive-update-backups/20261012-121339-movement-stride/`.

The opt-in `stride-speed-sweep` uses seven stages: idle → slow +X → idle/turn →
medium −X → idle/turn → fast +X → idle. Defaults are **30 seconds/radius 5**,
translation speeds **0.5/1.0/1.5 units per second** and nominal stage durations
**2/12/2/6/2/4/2 seconds**. The fixed six-unit lane copies the outdoor anchor's Z;
it does not query terrain. Older profiles remain available. Current plan:
`Build/definitive-manual-test/20261012-121416-stride-host-ready/`, through
`current-stride-test.json`. Steam launch was **12:14:18**; game **14068** was born
at **12:14:29**; Home registered at **12:25:04**. Game player **5** displayed
synthetic player **6**, ped **6153**. The probe recorded **263 valid samples** over
**29,901 ms**, all seven tasks playing with phase progression, then finished
before owned deletion. End at **12:27:44**, normal shutdown/Exit at **12:30:30**,
game closure, all 16 installed hashes and all five tested source hashes verify.
Both full/scoped strict readers pass with one completed session and zero active
sessions/issues/failures. The completed pointer links `assessment-final-reviewed.json`.
Post-exit diagnostics show unchanged boot, GPU/PnP OK/0 and no new grouped dumps
or collector issues/blockers; no crash was observed in this trial. Slow/fast still
frames show walking poses, while the medium character is occluded; continuous
gait, foot contact and no sliding remain unverified. No animation-speed setter
was applied. Next work needs a calibrated foot-contact reference before bounded
rate control; two real clients remain pending. The tracker preserves command
research, audit correction, controls and prior results.

## Completed October 12 movement smoothing

**Bounded movement smoothing now has actual GTA runtime evidence.** The
**11:52:02–11:52:32** five-stage straight-lane trial completed playback, owned
deletion, End at **11:53:18** and normal shutdown/Exit at **11:54:40**. Both full
and scoped strict readers pass with one completed session and zero active
sessions/issues/failures. All 16 installed files, five tested source hashes and
the known game executable verify after closure. The helper submitted 575 updates
and 232 positions, with exit 0 and no run/cleanup error.

Active Online yields at **16 ms** for presentation
while a separate **50 ms** deadline gates network/bridge work. Each remote keeps
received target and displayed pose separate, blends XY/shortest-arc heading over
50 ms, snaps received Z and resets on discontinuities. It does not extrapolate.
Stop/readiness/ownership checks run before the network gate; animation inference
uses received targets and retains the existing clips/task policy.

Current verification passes **149 adapter mocks**, **135 utility tests**
(including **51 peer mocks**) and **47 smoothing-audit fixtures**. Only the two
owned adapter scripts changed; native DLL, loader, protocol and C++ are unchanged.
All **16 installed hashes** verify. Source/install certificates:
`Build/definitive-smoothing-check/20261012-080403-verified-fa4b14e38fbb404ebb8d0bf6772c7535/`.
Completed plan: `Build/definitive-manual-test/20261012-114540-smoothing-host-ready/`;
pointer: `current-smoothing-test.json`. The agent launched through Steam under
the existing authorization at **11:45:51**; game **5816** was born at
**11:46:02**, and Home registered at **11:51:46**. Server **6740**, session
**45ad570fff9c44d2ad377e50a04aa6c0**, expires **12:44:16 local time**. The current
pointer is completed and links `assessment-final-reviewed.json`. Server identity
and listening endpoint were reverified at **11:57:52** after game exit. Game
player 2 displayed synthetic player 3/ped 6406 through all five stages. The earlier
08:10 attempt exited normally
at **08:19:10** while armed/unactivated, without bridge/network/peer activity;
its interruption record remains in `20261012-081051-smoothing-host-ready/`.

Motion evidence distinguishes network target, requested displayed pose and actual
readback, plus bounded tick/write/poll counters. The 30 logged motion windows
record 1,745 ticks, 574 writes, 582 polls and 190 changed targets, with 14 faster
write windows and 7 intermediate poses; an unsummarized trailing fraction is
excluded. Full walking windows have 42–51 writes against 13–17 target updates
per approximately one second. This verifies real-time blending; GPU frame cadence
and visible smoothness were not measured. Separate still frames show outbound
right-facing and return left-facing walking poses with apparently street-level
shoes, then absence. They do not establish continuous gait or no sliding.
Post-exit diagnostics find unchanged boot, GPU/PnP OK/0 and no new grouped dumps
or collector issues/blockers; no PC crash was observed in this run. Stride
matching/sliding, earlier PC-freeze
causes and two real game clients remain unverified. Next work is independent
visual movement/stride assessment, then two-real-client testing. The root findings
tracker and
`Build/definitive-recovery/current-checkpoint.json` retain history and recovery.

## Completed October 12 calibration

**October 12 straight-walk calibration is complete.** One real GTA client displayed
a synthetic peer through idle → walk out → idle/turn → walk back → idle, then
deleted it. The helper ran **06:48:55–06:49:25**, with **575 updates**, **232 distinct
positions**, exit **0** and no run/cleanup error. All **30 animation readbacks**
report playing=true with phase progression and exact requested/observed XYZ.
End at **06:51:18**, normal shutdown/Exit at **06:52:49**, game closure and all
**16 installed hashes** are verified. Both unfiltered full-session and scoped
strict readers pass with one completed session and zero active sessions,
issues or failures. The server records requested LEAVE for both game player 3
and helper 4. A targeted return-leg screenshot shows
left-facing movement at heading 90 and apparently grounded feet. It proves one
pose/direction frame; smoothness, stride alignment, both directions and every
idle stage are not visually established. October 11 lag/sliding remains open.

The new `straight-walk-calibration` helper profile uses five nominal six-second
stages at the default 30 seconds/radius 5, a fixed first anchor and parallel lane,
six-unit legs at **1 unit/second**, and candidate headings 270 out/90 back. The
installed game adapter, bridge, loader and protocol are unchanged. Current
verification passes **118 adapter mocks** and **135 utility tests**, including
**51 peer mocks**. Certificate:
`Build/definitive-calibration-check/20261012-062559-verified-e017e7fc881c4751916e63437244d86f/test-result.json`.
Default orbit and `idle-walk-idle` behavior remain available. The profile validates
its parameters/speed before loading the DLL, and only reports completion when
all five stages submit updates; late anchoring still requires runtime audit.

Completed plan: `Build/definitive-manual-test/20261012-062924-calibration-host-ready/`;
pointer: `current-calibration-test.json`; reviewed result:
`assessment-final-reviewed.json` in that plan. Targeted game-control images/actions
are under `Build/definitive-game-control/`. The user authorized automated launch
and game input for this test; the agent launched via Steam, resumed the observed
save, moved outside, activated Home and performed controlled shutdown. No save
overwrite or device/drive/security changes occurred. Home was requested at
**06:48:22** and `adapter_start` logged at **06:48:23**. Post-exit collection has
unchanged boot, GPU/PnP OK/0, no new grouped dump identities and no issues/blockers.
Server PID **8884**, session
**ac589aeabda047a1ac1a314450ee06db**, started **06:28:45** with scheduled expiry
**07:28:45 local time** on October 12. Its predecessor expired normally with zero
players. The root findings tracker and `Build/definitive-recovery/current-checkpoint.json`
retain the audit and recovery records.

Next work is bounded render interpolation with a separate network cadence and
further visual facing/stride checks. The existing 50 ms polling loop still writes
target positions directly; interpolation has not been installed. Two real game
clients and playable multiplayer remain unverified. Earlier PC-freeze causes
remain unresolved despite this completed test.

## Preserved October 11 results

The **walking/idle animation candidate is installed**, with **118 adapter mocks**
and **34 peer-helper mocks** passing and all **16 installed hashes** verified.
An actual synthetic trial completed idle → walk → idle at **20:38:45–20:39:15**,
with observed advancing animation phases, zero Z error and owned deletion.
The user confirmed visible moving feet and disappearance, with lag/sliding.
End at 20:40:57 and normal shutdown/Exit at 20:41:21 are verified. All 16 hashes
still match after exit. It keeps the proven no-offset setter and owned
cleanup, adds state-transition-only built-in PED tasks, and observes playback
and position at most once per second, including stationary idle. No PED request
or removal is used. That loopback server was PID **20868**, expiring at
**October 11 21:27:52 local time**. Its live identity/endpoint and welcome/requested-leave
probe passed without any position UPDATE.
Current manual plan/baseline:
`Build/definitive-manual-test/20261011-202954-animation-host-ready/`, discoverable
through `current-animation-test.json`. The completed retry used game player 8,
synthetic player 9/ped 5898, **574 updates**, **385 distinct positions** and exit 0
without run/cleanup errors. An earlier 1.897-second idle-only attempt is preserved:
game player 5 timed out during a seven-second game-log gap; its cause is unknown.
The retry has 30 valid playback observations (5 idle, 21 walk, 4 idle). Current
foot ground height and both idle stages were not explicitly visually confirmed.
Full-session strict acceptance remains incomplete because the first idle-only
ped did not move; the explicitly selected retry passes. Final assessment:
`assessment-final-reviewed.json` in the current animation plan. Preserve both scopes.
Waiting for an anchor can truncate final idle; helper exit 0 alone does not
establish complete playback.

The next work identified then was helper-facing/speed calibration, then bounded render interpolation
with a separate network cadence. The existing 50 ms polling loop writes target
positions directly; the fixed walking clip still has uncalibrated stride. These
follow-ups have not been installed. The findings tracker contains the full audit,
remaining limits and recovery checkpoint. No second real game client was tested.

Experimental separate Windows x64 client and one custom UDP server. On October
11 at 15:15, one real Steam game client displayed a synthetic remote character:
the user confirmed appearance, movement and disappearance. The log records one
creation, 30 changing transforms and deletion. The character floated and had no
animation; the helper deliberately circles the player and the current proxy is
frozen, collision-disabled and moved by coordinate writes. Ground placement and
walking animation were unfinished in that run, as are two real game clients and playable
multiplayer. The game exited normally at 15:24:10, but no End stop was logged;
the strict reader correctly rejects the complete stop/exit sequence for this run.
The user reports pressing End; shortcut registration in this run is unverified.

The completed position candidate has actual game readback evidence. At
**20:00:47-20:01:17**, synthetic player 5/ped 8706 had **29 distinct paired
transforms**, exact requested/observed X/Y/Z agreement and **zero Z error**.
The helper exited 0 with 575 distinct synthetic updates and no errors. The game
logged creation and deletion, End stop at **20:07:25**, and normal shutdown/exit
at **20:08:08**. The strict reader passes without issues or failures. The user
confirmed feet on the ground, sliding around the player, with no floating or
sinking. Grounding is confirmed at this test location; matching body-center
coordinates do not establish general terrain height. That run had no animation.
The position candidate matched 87 passing mocks, with all 16 installed hashes checked
before the run and no source/native change. Current plan/baseline:
`Build/definitive-manual-test/20261011-195314-position-host-ready/`;
final audit: `audit-200818423094-final/`; assessment: `assessment-final-reviewed.json`
in that directory. The current
pointer is `current-position-test.json` in its parent directory.
An earlier manual session activated at 16:51:39 and registered End at 16:53:21,
then exited normally at 16:53:28. No peer ran or join occurred; position was then
untested. The earlier command launcher failed while enumerating locked F:, and
the previous server's scheduled expiry had passed. Evidence is at
`Build/definitive-diagnostics/20261011-165657-position-stop-no-peer/`.
At the 18:23 resume, sandbox CIM/device/event access was denied and a server
command-session fallback failed its UDP welcome probe, then was interrupted.
Another user-reported crash left the log at 18:57:26 boot-script scanning, without
entry/bridge/join markers. Windows confirms an unclean restart at 19:09, but no newly
grouped dump identity; latest 141 records repeat older dumps. Cause remains unknown.
The user reported an undervolt afterward; its gameplay effect is unverified.
With full access, the agent collected fresh readiness, audited source/startup
files, and verified the restarted server PID 5248 by UDP welcome/requested leave
at 19:36. It expired at 20:13:01 local time and its log confirms a stop with zero
players; the process/endpoint were absent at 20:14. The current bounded peer completed.
The agent subsequently started and verified the fresh server described above.

The manual base-game control completed: the user confirmed gameplay and normal
exit with the loader bypassed. No new boot/dump identity or CLEO-log change was
found. The exact `version.dll` was restored at 19:50:56 with all 16 hashes verified;
fresh RequireReady passed. Online/manual and Home/End are available again.
This one control does not identify the earlier crash cause or establish stability.
Control evidence: `Build/definitive-manual-test/20261011-194145-base-game-control/post-control/`.
Current position plan/baseline:
`Build/definitive-manual-test/20261011-195314-position-host-ready/test-plan.json`;
current pointer: `current-position-test.json`. The agent handles commands directly;
manual outdoor/Home readiness and fresh join/self-echo preceded the 30-second peer.
Recovery and diagnostic limits remain in the root findings tracker.

Automated game testing was stopped after two PC freezes/forced restarts, until
the user's October 12 authorization described above. On October
4 at 06:21 +02:00, Windows reported the Radeon RX Vega healthy (status OK, device
code 0). A manual Diagnostic session completed at 07:08:50 and exited normally at
07:12:21. The October 4 Smoke session also passed: bridge guards, 75 local-player
samples and controlled stop, followed by normal exit at 07:26:55. The installed
mode remains Online after the successful one-client submission/echo test. The cause of the
earlier freezes and general startup stability remain unresolved.
The October 11 13:36 manual Online run joined the local server and passed actual
coordinate submission/echo checks: 23 telemetry records, 22 self-echoes and at
least 331 successful submissions, including changing negative Y values. The
signed-bit conversion fix now has joined game-runtime evidence. Ctrl+Shift+End
stopped the adapter, the server recorded requested departure, and the game exited
normally. This earlier session did not test remote peds; the later 15:15 proxy
experiment is described above. Two real game clients remain unverified.
Another manual launch froze on October 11 around 13:29. Windows recorded a fresh
LiveKernelEvent 141. The recovered CLEO log ends during boot-script scanning,
before adapter discovery or bridge loading. The loader/runtime had already loaded,
so their involvement cannot be ruled out. GPU status is healthy after reboot;
the crash trigger is unresolved. Game testing remained manual in those trials.
The remote experiments then started only after a stable outdoor save was reached.

The initial launch was followed by a reported PC crash and Windows graphics-engine
timeout. Its cause is unresolved. The user confirmed normal play worked after
reboot; the post-reboot log also confirms the smoke adapter was active. The mod
remains installed at the user's request. Evidence is in the root findings tracker.

The October 11 launches at 14:18:42 and 14:21:52 froze at the same recorded CLEO
boot-script scanning boundary as 13:29, before adapter entry or a server join.
Both have fresh 141 dump identities; GPU queries after reboot remain healthy.
Online now starts **armed and inactive**. Hold **Ctrl+Shift+Home** once after
loading a save to enable bridge loading and the client connection. Hold
**Ctrl+Shift+End** to cancel while armed or stop afterward. This gate has mock
coverage, installed-hash verification and actual Home/End game-runtime evidence.
It does not delay the ASI loader, CLEO or plugin boot stages.
The manual 14:38-14:43 run now verifies Home and End in GTA: 31 logged seconds
armed before activation, successful bridge guards/joins, changing self-echoed
coordinates, then Ctrl+Shift+End stop and script disposal. Idle timeouts caused
two successful rejoins; their cause is not confirmed. Normal host shutdown/Exit
is verified at 14:58:39, and all 16 owned hashes pass after game closure. Earlier
freeze causes remain unresolved. The later synthetic proxy was visually observed,
but grounded motion and animation were unverified at that point; the later
position and animation trials above retain their separate acceptance evidence.

## Build and verify

PowerShell 7, Visual Studio C++ tools, x64 Python 3.9+ and Node.js:

```powershell
./Client/definitive/build.ps1
python -B -m unittest discover -s Tests/definitive -p 'test_*.py'
node Tests/definitive/test_game_adapter.cjs
python -B -m unittest discover -s utils/definitive_edition -p 'test_*.py'
pwsh -NoProfile -File Tests/definitive/test_loader_toggle.ps1
```

The independent C++17 build uses `/W4 /WX` and runs 293 protocol/math checks.
Outputs are under ignored `Build/definitive-online`. Thirty-three Python tests exercise
server/native behavior; 180 Node mocks check controller/startup/activation/readback/animation/smoothing and bounded gait-probe behavior. Mocks
do not execute GTA commands. Native tests skip when the x64 bridge is not built.
The utility suite passes 153 tests: 17 preflight, 29 Diagnostic-log, 69 mocked
synthetic-peer and 38 remote-log checks. The loader-toggle suite passes 22 fixture
checks. Those two suites do not load the bridge or launch a real game.

## Startup investigation

Collect evidence without starting GTA or capturing its window:

```powershell
pwsh -NoProfile -File ./utils/definitive_edition/collect_diagnostics.ps1 -RequireReady
```

The collector saves JSON/Markdown, the installed manifest, CLEO log and grouped
Windows live-dump event references under `Build/definitive-diagnostics`. It checks
the exact game hash, owned-file hashes and display-device status. Exit 2 means a
prerequisite failed. Exit 0 only means those prerequisites passed; it does not
establish stability. Older queued WER reports are grouped by dump identity so a
replayed event is not counted as a new crash. Protected dumps still need analysis.

The earlier device-code-22 blocker is now cleared, and the manual Diagnostic
stage passed, followed by the current Smoke bridge/local-read stage. F11 also
toggles fullscreen in this game, so the adapter stop shortcut is now
**Ctrl+Shift+End**. It reads held keys without consuming them; F11 alone no longer
stops the adapter. The new shortcut passed its actual game test on October 11.
No game launch is part of collection.

For the next manual comparison, launch through Steam and load an outdoor save,
then play briefly **without pressing Home**. The log should record
`awaiting_activation` and one `adapter_armed` event with `bridge_loaded: false`,
`game_adapter_active: false` and `network_active: false`. Waiting polls held keys
and checks readiness only when the activation chord is held; no bridge, player
coordinate or network operation occurs during that wait.

After the game is stable, hold Ctrl+Shift+Home briefly. Expect
`adapter_activation_requested` before `bridge_load`, then build/runtime, join and
transform markers. Ctrl+Shift+End takes priority if both chords are held.
Cancellation emits `adapter_activation_cancelled`; an armed-only run cannot count
as an Online join or remote-gameplay pass. This is one activation per script
instance; stopping exits the script, and another comparison uses a fresh manual
launch. No hot reload. Record whether a freeze occurs before or after Home, and
preserve the log before another launch. A post-Home failure could involve bridge,
local reads or networking; the key does not isolate server execution alone.

The one-client Online submission/echo stage passed on October 11 at 13:36.
Acceptance required `joined`, bounded `online_transform` events with increasing
successful submission counts and changing server-echoed own-player coordinates,
then `adapter_stopped` and normal exit. A join alone would not prove the float-bit
submission correction. No synthetic peer was used for that session. Startup
isolation remains necessary; the next gameplay gate is remote-ped rendering.

For a startup comparison, a reversible loader bypass is available with GTA closed:

```powershell
pwsh -NoProfile -File ./Client/definitive/set-loader.ps1 -Action Status
pwsh -NoProfile -File ./Client/definitive/set-loader.ps1 -Action Disable -WhatIf
pwsh -NoProfile -File ./Client/definitive/set-loader.ps1 -Action Disable
# After the comparison, close GTA before restoring:
pwsh -NoProfile -File ./Client/definitive/set-loader.ps1 -Action Enable
```

Disable renames only the owned `version.dll` to `version.dll.de-online-disabled`;
Enable restores it. The helper validates all 16 owned hashes, refuses linked paths
and collisions, and saves a durable pre-rename journal. It preserves mod bytes,
the installed mode and manifest. Status reads only; no game is launched. Restore
the loader before packaging changes, updating, changing modes or uninstalling.
The real installation remains **Enabled / Online**; no bypass was applied.

Diagnostic remains available for repeating the earlier isolation stage. It logs when
CLEO dispatches the ordinary script after loading a save/new game; it cannot log
a failure earlier in the loader/runtime. Progress to Smoke and Online separately
after a stable diagnostic session. Resume/load a save, play briefly, and exit
normally; reaching only the title menu does not dispatch the adapter. The log
should contain `adapter_phase` with `phase: entry`, `mode: diagnostic`, followed
by `adapter_diagnostic_complete` with `bridge_loaded: false`,
`game_adapter_active: false` and `network_active: false`. Diagnostic exits by
itself, so no stop shortcut is necessary in this profile.

Check a completed session from its text log, or start a bounded watcher before
manually launching the game:

```powershell
python -B utils/definitive_edition/session_log.py
python -B utils/definitive_edition/session_log.py --watch-seconds 300
```

The reader accepts ordered Diagnostic entry/completion markers with strictly
false activity flags. It reports adapter failures, bridge-loading evidence and
shutdown messages separately. Exit 0 means the script markers passed; exit 2
means incomplete/failed/timed out. No marker proves gameplay stability. Watch
mode excludes old markers even if unrelated data is appended or the header is
rewritten. The one-shot reader describes the whole existing log and makes no
freshness claim. It reads at most 16 MiB and never launches/inspects the game.
Twenty-nine fixture tests cover these acceptance and stale-evidence cases.

```powershell
./Client/definitive/package.ps1 -Mode Diagnostic
./Client/definitive/set-mode.ps1 -Mode Diagnostic
./Client/definitive/update.ps1
```

These updates keep the mod installed and preserve generated data. `uninstall.ps1
-ValidateOnly` performs only path/link/hash checks and returns before removal is
proposed or executed. Smoke/Online now log each bridge load, build check and runtime
binding boundary, to narrow any later startup failure.

## Local game test

The manual Diagnostic and Smoke stages passed on October 4. Online submission
and later remote-player stages still need runtime evidence; the cause of the
earlier freezes remains unresolved.

The only accepted game SHA-256 is
`63e633bb51c35fe35ca98f503b62e7ddb8403abe4c9d501cfa9162892453846f`.
Fixed numeric version: `1.0.113.21181`; display string: `1.0.113.49697469`.

```powershell
./Client/definitive/package.ps1 -Mode Smoke
./Client/definitive/install.ps1 -Mode Smoke -WhatIf
./Client/definitive/install.ps1 -Mode Smoke
```

Packaging verifies pinned official dependency archives and command definitions.
Installation verifies all 16 files and refuses an existing runtime/loader/CLEO
folder. It records each owned destination for removal. Close GTA before installing.
The commercial executable, classic GTA and installed MTA are unchanged.

Launch through Steam and load an outdoor save. Smoke mode only reads and logs CJ's
position/heading in `Gameface/Binaries/Win64/cleo_redux.log`. Hold Ctrl+Shift+End to stop the
adapter cleanly. Smoke does not connect to the server or create remote peds.

For the online experiment, close GTA first:

```powershell
./Client/definitive/set-mode.ps1 -Mode Online
pwsh -NoProfile -File ./utils/definitive_edition/start-server.ps1
```

The helper starts one hidden server bound to `127.0.0.1:22040` for 30 minutes.
It checks the listening log and owning UDP endpoint, refuses an occupied port,
and saves logs/session metadata under `Build/definitive-online-test`. Run it
again after restarting Windows or after the server expires. It does not install
an automatic startup task or launch GTA. Optional `-DurationSeconds 3600` allows
a longer session; `-PythonPath` selects a Python executable.

For an Online test, manually launch through Steam, load an outdoor save,
wait while armed, hold Ctrl+Shift+Home to activate, then walk for about a minute,
hold Ctrl+Shift+End briefly and exit normally. Check
`joined` and changing `online_transform` submitted/echoed values, then the stop
record. Run this stage without the synthetic peer.

After successful one-client submission/echo verification, a separate later test
can add a synthetic peer while the manually launched game is stable outdoors and
the fresh log shows changing submitted/self-echoed coordinates. Keep GTA running
until this experiment finishes:

```powershell
python -B utils/definitive_edition/game_peer.py --seconds 30
```

The helper is a synthetic peer circling five units from the received player. It
is not a second GTA client. Inspect game-side `joined`, `remote_ped_created`,
`remote_transform_applied` and `remote_ped_deleted` records. Visual confirmation
and two real DE clients are separate acceptance gates. Helper output explicitly
sets `synthetic: true` and `game_rendering_verified: false`.
After peer departure, observe whether the ped disappears, then hold
Ctrl+Shift+End briefly and exit normally. Preserve the game and helper logs.
The remote reader excludes pre-run records when given a saved baseline:

```powershell
python -B utils/definitive_edition/remote_session_log.py --baseline <saved-pre-run-cleo-log>
```

Exit 0 requires one joined Online session with matching remote creation, at least
two changed finite transforms, deletion before controlled stop, and normal host
shutdown/exit. Exit 2 means incomplete, invalid or failed evidence; before any
remote experiment, incomplete is expected. A reader pass verifies recorded command
order, not rendering or two-client multiplayer.

The interrupted October 11 preparation remains under
`Build/definitive-manual-test/20261011-141248-remote/` with its startup-interruption
record. A fresh baseline was prepared at `20261011-150105-remote/`; that plan's
30-second peer completed at 15:15:41. Actual server identity, peer outputs, fresh
game log and the user's visual observation are preserved under
`Build/definitive-diagnostics/20261011-152031-remote-audit/`.
`Build/definitive-manual-test/current-remote-test.json` points to the latest result.
Keep historical prepared server metadata separate from the actual restarted
server identity. The final 15:24 snapshot verifies normal exit and all 16 owned
hashes, but no End stop; keep that missing controlled-stop evidence explicit.
The newer body-center trial verifies matching coordinate readback and visible
grounding at one location, with a complete End/exit sequence. Bounded locomotion
animation now has observed idle/walk/idle playback and visible walking leg motion
in the completed 20:38 retry, followed by controlled stop and normal exit.
The October 12 straight-lane profile adds five-stage playback/readback and one
return-leg facing frame. The later smoothing trial verifies bounded interpolation
and includes sampled outbound/return poses. The new read-only gait probe is
prepared separately. Lag/sliding, complete visual facing/stride calibration,
visible smoothness and two real clients remain unfinished.

Ctrl+Shift+End stops the adapter. Close GTA before editing/removing scripts. Forced hot
reload is unsupported because CLEO documents no JavaScript unload-cleanup callback.
The bridge is pinned while CLEO retains its reset callback. Reset disconnects
transport; actual entity/reset cleanup still needs game tests.

Use `set-mode.ps1 -Mode Smoke` after closing GTA to return to read-only operation.
For removal, run `uninstall.ps1 -WhatIf`, then `uninstall.ps1`. It verifies all
installed hashes before deleting owned files and retains generated/unrelated
files. Modified files cause removal to stop for review.

## Scope and limits

- One fixed game endpoint `127.0.0.1:22040`, 16-session limit, 20 Hz snapshots,
  finite/bounded transform validation and five-second idle removal.
- Native UDP transport with explicit serialization, session checks, retries,
  departure handling and stale/wrapped sequence checks.
- Reused MTA math headers. Classic hooks, `netc`, Lua/resources, CEF and the
  existing MTA server protocol are outside this independent client.
- CLEO game-native controller with model streaming, owned proxies and controlled
  cleanup/reconnect logic. Local reads, remote creation/movement/deletion and paired
  remote coordinate readback now have runtime log evidence. Visual grounding and
  two real clients remain separate acceptance gates.
- Remotes are inert model-7 peds with collision disabled. Walking/idle animation
  has observed actual playback and visible walking leg motion. Stride alignment
  and smoothness remain unverified. Bounded interpolation is installed with mock
  coverage and actual intermediate-pose/cadence evidence. Visible smoothness and
  stride matching remain unverified. Vehicles, combat, chat,
  interiors and world sync are unfinished.
  Use outdoor free roam for the first test.

The server relays client-reported transforms. Accounts, encryption, gameplay
authority and anti-cheat are absent. LAN/Internet play is unverified. This protocol
cannot connect to existing MTA servers.
Joined sessions receive snapshots immediately, but their player state is published
only after an accepted position UPDATE. This prevents a temporary remote proxy at
the map origin while a new peer is waiting for its first usable position. An
explicitly submitted zero position remains valid.

Wire version 1 has a little-endian `DES1` header followed by JOIN, WELCOME,
UPDATE, SNAPSHOT or LEAVE payloads. Maximum packet: 512 bytes; full snapshot:
346 bytes. Bridge ABI v2 adds an executable guard, CLEO lifecycle binding and
integer float-bit calls, avoiding game pointers and foreign output allocations.
Call from one owning thread outside the loader lock. See [Bridge.h](Bridge.h).

Repository code/MTA headers retain the repository license. CLEO is separately
licensed freeware; ASI Loader uses MIT. Notices accompany the local bundle. Sanny
definitions have no declared license in the inspected repository; pinned attribution
is preserved. Public redistribution compatibility remains unresolved.

See [DEFINITIVE_EDITION_PORT.md](../../DEFINITIVE_EDITION_PORT.md) for the complete
audit and tracker. Commit messages should state goals, motivation, implementation
reasoning, tests performed and remaining limitations.
