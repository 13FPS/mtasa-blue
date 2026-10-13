# Definitive Edition adapter owned by MTA

`Game DE` is a normal project in `MTASA.sln` and produces `Bin/mta/game_de.dll`.
MTA Core owns its loading, initialization, pulse, reset, shutdown and release
through `CGameAdapterModule` and the versioned SDK `game/CGameAdapter.h`.
The module provides no separate transport or script-host lifecycle.

The current factory contract is **ABI5**. An explicit local on-foot movement
trial now sends MTA's shared synthesized current/previous controller state
through the verified DE player-control callback. Real repository Detours scopes
the pad replacement and action-provider membership, availability and value
queries to that callback. All other callers retain the original engine path.
The saved 96 pad bytes are restored before returning or propagating an original
exception. Physical input-source tables and their event queues are never written.
The trial is limited to 35 seconds with a two-second input freshness limit;
it disarms on changed identity/context and retains its module if cleanup fails.
Its separate `NativeMovementWriteCandidate` capability does not enable the
full `EngineInputWrite`, remote-player or `CGame` gameplay requirements.

`package.ps1 -Mode Smoke -MtaNativeMovementTrial` stages the real normal-build
`game_de.dll` beside the diagnostic bridge. The bridge uses Core's actual
`CGameAdapterModule` consumer; JavaScript schedules logical MTA actions and
observes position/velocity. It does not force positions or animations. Arming
waits indefinitely for Ctrl+Shift+F9, then waits for all three keys to release.
The 22-second sequence is neutral, walk, pause, forwards, pause, sprint, neutral.
Ctrl+Shift+End cancels. The human evaluates actual movement in the game; the log
checks callback/query activity and restoration independently of visual quality.
Ordinary packaging and mode switches leave this write experiment disabled.

The current offline checks pass 146 production callback cases, 7,413 routing
cases per architecture, 1,512 module cases and 76 x64 Core-consumer cases.
Normal Release/x64 Core/Game DE builds, the real Core entry fixture (26 checks)
and classic Win32 compiles with the option enabled/disabled pass. The package
passes 135 ownership/mode checks. Script and saved-log tests include logs emitted
by the actual controller simulation; these are explicitly not live movement
evidence. Assess the human trial with
`utils/definitive_edition/native_movement_log.py --log <saved-log> --output <assessment>`;
add `--require-exit` after normal game shutdown. The current installed trial
and preserved prior log are linked from
`Build/definitive-manual-test/current-native-movement-test.json`.

The module supports **observation ingestion**: it validates and stores
caller-provided position, raw velocity and explicitly covered controller state.
The controller snapshot implementation lives here; the earlier probe includes it
through a compatibility header. Storage acceptance does not establish an engine binding.

**MTA input synthesis** reuses the digital action policy extracted from
`CKeyBinds::DoPostFramePulse` into `Shared/sdk/CControlStateSynthesis.h`.
The classic caller and this module use the same implementation. The existing
44 action identifiers retain their values in `Shared/sdk/CControllerActions.h`.
`SubmitControlActions` accepts MTA's configured action state and player context;
`GetMtaInputState` returns current/previous canonical controller state, including
MTA's logical walk and radio fields. Sparse enum holes are rejected atomically.
Reset and shutdown clear both frames and the opposing-direction history.

This is logical MTA input, separate from native controller observations and
their coverage mask. It does not poll hardcoded keys, infer sprint from velocity,
apply joystick overrides, or itself write an engine pad. Core retains key bindings,
context-transition callbacks, focus/cursor policy and joystick merging. The
action interface was introduced in ABI2. Native binding/read methods arrived in
ABI3, raw device-source readback in ABI4, and the bounded write trial in ABI5.
ABI1 through ABI4 are rejected before returning an incompatible
vtable. The historical probe's bridge
ABI is independent.

`BindNativeInput` explicitly binds the read-only `NativePadReader`. It reuses
[Plugin-SDK's SA Unreal pad signature and layout](https://github.com/DK22Pac/plugin-sdk/blob/15f15b60bbf74c106e1b496ff92c98764abf4605/plugin_sa_unreal/game_sa_unreal/CPad.cpp).
The exact local executable was scanned offline: the signature occurs once in
executable code and resolves to the proposed two-pad candidate at RVA `0x53e8810`.
The reader checks the known executable SHA-256, AMD64/PE32+ image bounds, live
instruction bytes and relative target, section attributes and readable committed
pages owned by the main image. It copies the proposed NewState/OldState offsets on the binding
thread and preserves caller output on failed reads. It installs no hooks.

Only a successful explicit binding adds `NativePadCandidateRead`; failed reads, reset
and shutdown discard that binding. Raw native state is separate from synthesized
MTA actions and caller observations. Keyboard aggregation, update timing and
actual controller semantics remain unverified. The test host rejects binding because
its executable is not the pinned game. A read-only reference now compiles this
same reader into the existing diagnostic bridge for a human-run game trial.
That trial exercises backend code; it does not construct a full MTA Core.

The native-reader source/build milestone passes 1,474 module checks, 59 x64
consumer checks, three x86 rejections and 1,325 reader checks per architecture.
The normal module and host builds and both classic Core compile-only
configurations pass. All 84 certified source hashes remain unchanged.
Certificate:
`Build/mta-owned-adapter-check/20261013-123815-061c097f099547aabb949f9e5694cf5a/test-result.json`.
The module SHA-256 is
`063cd5bc16c0900b498747aba226674f72bca6049145e18cd72b8225354abf7f`;
its imports are `bcrypt.dll` and `KERNEL32.dll`. Live native input remains unverified.

The live reference is selected with `package.ps1 -Mode Smoke -MtaNativePadReference`.
It captures both pads and their 24-field NewState/OldState pairs once per sample.
The proposed previous state comes from the SDK's OldState offset, independent of the previous
125 ms probe sample. The existing CLEO script only schedules this read-only
test and supplies position/velocity observations. It creates no NPCs or network
session and sends no input writes. Ctrl+Shift+F9 starts its 30-second budget;
Ctrl+Shift+End cancels. The human remains responsible for walking and sprinting.

The completed trial is recorded under
`Build/definitive-manual-test/current-native-pad-reference-test.json`.
Its bridge SHA-256 is
`a35be5c2ccb0ed34060874df04c4353c13bda32ff44cdda5d862fe669132e7f4`.
Build/protocol, recorder, log-reader, real bridge ABI, transport and installer
compatibility checks pass. A disposable 16-to-17-file upgrade also rejects an
occupied new script path without changing user bytes, then upgrades and verifies
the complete new install.

The first live trial completed **225 captures in 30,057 ms**, with cleanup and
all 17 installed hashes intact. The human reported normal walking and sprinting.
However, both proposed pads' axes, sprint field and complete OldState arrays
stayed zero. Pad 0's last byte pairs decode to constant floats 960/540; pad 1's
changing pair forms a monotone 32-bit value, not plausible button values.
The capture is preserved; this controller layout is rejected for input use.
`EngineInputRead` is **not advertised** by this reader. Its binding exposes only
raw candidate data for diagnosis, never engine writes or gameplay readiness.

Bounded disassembly shows the static address can be replaced by a pointer at
RVA `0x5067940`, followed by queries to separate action providers. That explains
why the signature alone cannot identify usable controller state. The live
alternate pointer and provider contract are not yet validated. The trial's
result and exact raw-log fingerprint are linked by
`Build/definitive-manual-test/current-native-pad-reference-test.json`.

The capability correction passes the normal module/host builds, 1,474 module
checks, 59 x64 host checks and three x86 rejections. Reader, snapshot and shared
MTA synthesis checks pass on both architectures; opt-in/default classic Core
compile-only checks pass. All 89 certified source hashes remain stable.
Certificate:
`Build/mta-owned-adapter-check/20261013-131359-179b301cb5fd43b4a095241b1fe834d4/test-result.json`.
The corrected module SHA-256 is
`0f9f42bb6ce3536f019528278ed96ac6f0b9394f09228fae91790cef6c086d85`.

ABI4 adds `ReadNativeDeviceState` through the same owning Core consumer. It
captures 127 raw signed 32-bit source values from RVA `0x5709A40`. This table
comes from the observed provider's native axis implementation, rather than the
rejected SDK controller layout. Exact live image-base/load instructions, PE
sections and committed main-image pages are checked before the complete copy.
Failures preserve caller output and discard the binding. A successful read adds
only `NativeDeviceStateCandidateRead`; physical key/action mapping, held/previous
semantics and `EngineInputRead` remain unavailable.

The same backend code is compiled into the diagnostic bridge. One capture
publishes pad and device caches together, and the additive `DE_NativeDeviceRead`
export retains bridge ABI2. The existing 30-second recorder now emits capture
version 2 with all 127 sources. Use `native_pad_log.py --require-complete
--require-device-states` when assessing this trial: an older pad-only capture
cannot satisfy that requirement. No game method is invoked by the new reader.

The ABI4 checkpoint passes 1,484 module checks, 64 x64 host checks/three x86
rejections and 1,331 reader checks per architecture. Normal module/host builds,
shared input regression and opt-in/default classic Core compilation pass.
All 91 source hashes remain stable. Certificate:
`Build/mta-owned-adapter-check/20261013-134635-a3a9288303824beb8307a81bee4f038e/test-result.json`.
The installed source-table trial is linked from the current native-pad reference
pointer; its physical mapping remains pending the human test.

The first source-table trial completed 225 captures and normal shutdown. Native
key-name initialization corroborates W=29, S=43 and A=42 in their recorded spans.
The native state writer distinguishes press/held (2/3) from release/rest (0/1).
D and sprint were beyond the 30-second window. The next recorder waits for all
activation keys to release, then allows 60 seconds. Arming before F9 has no
deadline. This low-rate diagnostic still does not establish a complete controller
or lossless input-event source; the adapter capabilities remain diagnostic only.

The release-started minute trial completed 449 captures with zero issues and
normal shutdown. It records D alone, W+LeftShift and 76 captures with W/D/LeftShift
held together. The human reported normal movement and uncertainty in the combined
segment; that segment's movement outcome remains unverified. Source 44's native
key initializer independently identifies it as D. Activation release preceded
DLL access, and all 127 sources were released in the first capture.

The next reuse slice extracts Core's existing bind-state decision into
`Shared/sdk/CControlBindState.h`. Classic `CallGTAControlBind` uses that helper
while retaining its original callback loop. This prepares the configured-input
bridge; it does not yet load MTA's bindings or write input in DE.

The extracted policy passes 12,650 frozen-original comparisons per architecture;
the callback loop remains token-identical. Classic Release/Win32 Core compiles
with the adapter option on/off. Formatting and source-hash stability pass.
Certificates: `Build/definitive-control-bind-state/20261013-145232-4e1b86db67ca420ba8ba8f3269c54d17/test-result.json`
and `Build/mta-core-control-bind-check/20261013-145446-ae11adf5769745c387d3ba95b8fba81a/test-result.json`.

The factory returns no full `CGame`. Engine input writes and remote player
context are unavailable. `--with-definitive-adapter` enables Core's early startup
check, which rejects these missing capabilities before the CCore constructor's
classic configuration, GUI, hooks and game factories. Full Core/multiplayer/
render startup remains gated even if a future adapter supplies those features.
The default classic startup is unchanged. In an opt-in x64 Core, the entry
point now recognizes the `SanAndreas.exe` basename before classic process
detection and skips classic DLL file-hook installation for that candidate.
This classification performs no file hashing or module loading under loader
lock. The constructor explicitly calls `BindNativeInput` outside `DllMain`,
where the backend verifies the exact executable and live instructions before
any engine access. A matching filename alone cannot enable gameplay.

The three existing Core stack walkers now initialize native PC/SP/FP registers
and select AMD64 on x64. Their no-symbol fallback uses the process's runtime
unwind metadata. The existing x86 crash-report/register SDK stays unchanged.
Focused checks pass 47 x86 / 48 x64 stack cases and 14 host-entry cases on each
architecture. The real classic Core translation units compile in Release/Win32
with the adapter option enabled and disabled; full x64 Core remains unported.
Certificates:
`Build/mta-core-stack-trace/20261013-151215-5409d36d1de64394a34b22bda3a87d0c/test-result.json`,
`Build/mta-core-definitive-entry/20261013-151228-a8b10ae865f04dce985b92cf2263fe51/test-result.json`
and `Build/mta-core-startup-check/20261013-153512-c674dfff31844486af06d93fa743a344/test-result.json`.

The subsequent real opt-in x64 compile passes all **99 Core C++ translation
units**, using an isolated generated project with its exclusion cleared for
diagnosis. This removes concrete classic animation layouts from the shared
multiplayer SDK, uses pointer-sized window procedures and widens stored CVars
for `size_t` consumers. Classic animation consumers include their concrete
backend headers explicitly. Native crash text retains full addresses; the old
DWORD register/registry schema and classic pool diagnostics are explicitly
unavailable on x64. The original native minidump is retained. This initial
diagnostic compile did not establish linkage or DE startup/rendering.
Source-bound compile evidence:
`Build/mta-x64-core-diagnostic/20261013-151313-6a609396/final-02/test-result.json`.
Native crash-text checks pass 7 x86 / 8 x64 cases:
`Build/mta-core-native-context/20261013-154019-93fc8f54e0a94d00b87863c5703bc908/test-result.json`.

The normal opt-in **Release/x64 Core now builds and links** all its real vendor
dependencies. Generate with `utils/premake5.exe --with-definitive-adapter vs2026`,
then build `Build/Client Core.vcxproj` in Release/x64. Configure `DXSDK_DIR` with
the D3DX headers and matching x64 import libraries. The output is
`Bin/mta/x64/core.dll`, with the real `game_de.dll` and `pthread.dll` staged
adjacent. Default classic project generation remains unchanged. Other Core
architectures/configurations retain their exclusions. The resource uses the
Windows SDK rather than requiring optional MFC headers, and libpng outputs
are separated by architecture/configuration for this opt-in build.
Certificate:
`Build/mta-x64-core-diagnostic/20261013-151313-6a609396/normal-build/normal-build-certificate.json`.

When building individual `.vcxproj` files, build `Build/Game DE.vcxproj` in
Release/x64 **before** Core so Core's post-build staging copies the current
adapter ABI. The normal solution carries this build-order dependency; invoking
Core's project alone does not build solution-only dependencies. Verify that
`Bin/mta/game_de.dll` and `Bin/mta/x64/game_de.dll` have the same hash before
testing the staged bundle.

`Tests/definitive/test_mta_core_entry.ps1` loads those actual DLLs in a separate
non-game fixture named `SanAndreas.exe`. It passes 26 checks: directory checks
return 3/4, exact image rejection returns 5 twice, the backend unloads after
both failed constructions, classic gameplay modules remain unloaded, and
Core unload returns normally. The log verifies rejection by the engine-build
gate rather than a missing DLL. Native memory access is rejected before it
starts. This executes the real Core entry; it does not launch GTA or establish
full DE startup/rendering. The Core DLL SHA-256 is
`80d7b32b3da3a73fe8c627f66d99ed4a9d3bf5a2de4e6696b5dee7607cbb5ca8`.
Certificate:
`Build/mta-core-entry/20261013-160014-38ecc56c59884f87b0c5a90cf3a68e68/test-result.json`.

The next engine trace finds that the pinned executable updates pads `0x188`
bytes apart, copies current/previous 48-byte blocks at `+0x38` / `+0x68`, and
stores the game timer at `+0x164`. This explains the old candidate's apparent
changing buttons: that old second-pad offset read the first pad's timer.
The raw keyboard states and pending edge queues are process-global. Neither
the mouse-oriented `BaseUpdatePad` hook nor a saved keyboard-table copy proves
a remote-player input scope. Field interpretation and the simulation boundary
must be established before enabling engine writes or advertising readiness.

The diagnostic reader now uses that corrected stride and block offsets, with
eight exact live caller/updater/getter instruction guards. A poisoned-prefix
regression fails with the rejected old layout. The corrected reader passes
1,527 checks on each architecture; the normal module passes 1,484 checks and
the real x64 Core consumer passes 64. Its DLL SHA-256 is
`ba0e134fdb7c33b2fde4ac491c44acb69bb2abcd5b986193a8249192a1afecc7`.
Backend certificate:
`Build/mta-owned-adapter-check/20261013-152908-dcdb17980528448ba321f8600eb72fac/test-result.json`.
The later crash-handling review fixes are covered by the newer Core compile
certificate above; the backend sources and module stayed unchanged.

The corrected-layout human trial completes 440 captures in 60,049 ms with no
issues, clean disposal and normal shutdown. The human reports normal movement.
W/S/A/D and W+LeftShift sources are active while raw velocity changes, but all
corrected current/previous pad fields stay zero. This supports the observed
provider-query path; copying the pad blocks does not establish keyboard input
materialization. No input writes ran. The raw log SHA-256 is
`f0200560bfc824657ef9199027be973aeee2db5f59f5b482b9640fdf50c11502`;
the trial is linked from `Build/definitive-manual-test/current-native-pad-reference-test.json`.

A bounded native trace identifies a player-control class candidate whose
vtable is at RVA `0x4319730`, slot `+0x50` targeting `0x124EAF0`. Its constructor
selects indexed player information and its control callback saves/restores the
active input provider. A bounded passive read during the human trial also
confirms the live local player's vtable, callback slot, player-information
pointer and pad selector against these exact-build values. Remote ownership,
callback thread/ordering and engine writes remain unverified. Scalar input methods can bypass
the pad block through action providers, so an override must also route those
queries inside the same owned simulation scope. The 127 physical sources and
pending edge queues must remain independent of remote input.

The on-foot trace further maps provider actions `0x1E` / `0x1F` to horizontal /
vertical movement and action `2` to sprint. The movement object scales provider
floats by 128. Native modifier action `0x6D` applies a 0.25 walk scale; the
canonical `m_bPedWalk` is also read directly by the on-foot task. The intended
write port must route membership (`0xC09750`), gated availability (`0xC097D0`)
and state (`0xC09990`) inside the owned player-control scope (`0x124EAF0`).
Returning unavailable alone permits widget queries to fall through to physical
mouse/touch input. These mappings are exact-build static evidence, not installed
hooks or a runtime write claim. Full control coverage, callback ordering and
restoration still need implementation and live validation.

Generate normally with `utils/premake5.exe vs2026`, then build `Game DE` and
`Game DE Host Tests` in **Release/x64**. Run the host executable with the absolute
path to `Bin/mta/game_de.dll`. It loads the actual module through the same Core
consumer, using an opaque owner fixture; it does not construct full Core or GTA.

The preceding observation-only milestone passed 1,079 module checks, 45 x64 consumer checks, three x86 rejection
checks, and 930 controller checks per architecture. The classic Release/Win32
Core translation units compile with the option both enabled and disabled.
Core has not been linked or launched by these integration checks.
Its certificate is preserved under
`Build/mta-owned-adapter-check/20261013-113152-f54e411c11b540eb9e6ec7f8b57577cc/`.
The current build and input checks are recorded by
`Build/mta-owned-adapter-check/current-candidate.json`.

`Tests/definitive/test_mta_control_state_synthesis.ps1` compares all controller
fields and six direction-history flags with the original committed keybind
implementation on x86 and x64. The real-DLL host also exercises action
submission, walk/sprint history, reset, foreign-thread rejection and unload.

The October 13 input milestone passes **298,447 differential frames** on each
architecture, comparing all controller/history values. Normal `Game DE`,
`Game DE Host Tests` and `MTA Control Synthesis Tests` builds pass. The module
passes 1,412 checks, its Core consumer passes 54 x64 checks and three x86
rejections, and controller snapshots pass 930 checks per architecture. The
Release/Win32 Core compile-only targets pass with the option enabled/disabled.
All 80 certified source hashes remain unchanged across these checks.
Certificate:
`Build/mta-owned-adapter-check/20261013-121621-e64f282ab4ee4f2ba5158847fc82482f/test-result.json`.
The actual module SHA-256 is
`1df11645e19a3562368a1bca3f30b89653e7e22c2662ec4d070cd62a77b7a02e`;
that input-milestone DLL imports only `KERNEL32.dll`. It has not been loaded into GTA.

For the classic Core compile, this workspace used Microsoft's signed
[D3DX package](https://www.nuget.org/packages/Microsoft.DXSDK.D3DX/9.29.952.8)
headers/import libraries in ignored Build, the repository's pinned
`install_discord` action, and its language-header generator. No downloaded
runtime DLL was extracted or executed. Build dependency hashes and signature
verification are preserved with the certificate.

The actual DE reference trial recorded changing raw velocity while all 20 pad
fields remained zero during keyboard/mouse movement. It does not verify input
mapping. Further work belongs in MTA's backend/startup/input services. See the
[port plan](../definitive/MTA_REUSE_PLAN.md) for the remaining engine boundaries.

Commit messages should explain the adapter-in-MTA goal, lifecycle and capability
decisions, and the build/offline checks separately from unverified gameplay.
