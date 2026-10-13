# Definitive Edition adapter owned by MTA

`Game DE` is a normal project in `MTASA.sln` and produces `Bin/mta/game_de.dll`.
MTA Core owns its loading, initialization, pulse, reset, shutdown and release
through `CGameAdapterModule` and the versioned SDK `game/CGameAdapter.h`.
The module provides no separate transport or script-host lifecycle.

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
apply joystick overrides, or write an engine pad. Core retains key bindings,
context-transition callbacks, focus/cursor policy and joystick merging. The
module's factory ABI is now **2** because its virtual interface grew; ABI1 is
rejected before returning an incompatible vtable. The historical probe's bridge
ABI is independent.

The factory returns no full `CGame`. Engine input read/write and remote player
context are unavailable. `--with-definitive-adapter` enables Core's early startup
check, which rejects these missing capabilities before the CCore constructor's
classic configuration, GUI, hooks and game factories. Full Core/multiplayer/
render startup remains gated even if a future adapter supplies those features.
The default classic startup is unchanged. Existing DLL file hooks and classic
process detection still precede this check; x64 Core remains unported.

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
its only imported DLL is `KERNEL32.dll`. It has not been loaded into GTA.

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
