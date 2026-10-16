// Keep the first installed run read-only until the CLEO host and player commands
// have produced evidence for this exact executable. The native bridge fixes the
// online endpoint to 127.0.0.1:22040 for this local experiment.
// Diagnostic mode logs host metadata and exits before loading the native bridge
// so runtime startup can be investigated while retaining all installed files.
export const MODE = 'smoke';
// Repeated startup freezes occur before our adapter is dispatched. Waiting for
// an explicit in-game chord separates that startup from bridge/network activity
// without implying that this can prevent a loader or CLEO startup failure.
export const ONLINE_ACTIVATION = 'manual';
// The engine must move a separate test NPC without our network pose writes to
// supply a travel-speed reference. Keep that short, offline trial opt-in so
// ordinary Smoke remains read-only and Online keeps its validated behavior.
export const GAIT_REFERENCE = false;
// The accepted native walking reference supplies a measured baseline for a
// separate multiplayer cadence trial. Keep it opt-in until that trial and
// human visual observation establish the effect of the playback-rate setter.
export const GAIT_RATE = false;
// Body-coordinate samples stayed monotone during a visible loop rewind. This
// independent Online experiment tests horizontal animation extraction before
// changing the rate estimator, so the rendered-root hypothesis can be isolated.
export const GAIT_ROOT_LOCK = false;
// Before binding remote MTA player input, observe the local DE pad/velocity
// through the shared canonical controller contract. This read-only Smoke
// probe always needs manual activation and keeps unbound logical flags unknown.
export const INPUT_REFERENCE = false;
// Exercise Game DE's actual read-only pad implementation, including both
// engine NewState/OldState pairs. This is independent of the old 00E2 probe.
export const MTA_NATIVE_PAD_REFERENCE = false;
// Explicit local engine-write trial; ordinary Smoke and Online modes leave it off.
export const MTA_NATIVE_MOVEMENT_TRIAL = false;
// Load the real x64 MTA Core only on explicit activation, observe its unchanged
// incomplete-gameplay gate, then unload it before normal play continues.
export const MTA_CORE_STARTUP_REFERENCE = false;
// Observe the real Core-owned configured binds and their canonical frames.
// This separate reference never submits those frames to the engine.
export const MTA_CORE_INPUT_REFERENCE = false;
// Drive only the bounded eligible local on-foot scope from Core's configured
// bindings. Keep this write trial separate from both read-only Core references.
export const MTA_CORE_MOVEMENT_REFERENCE = false;
// Observe an explicitly selected native viewport callback through the real Core worker.
// This diagnostic draws nothing and must never enable input or full startup.
export const MTA_CORE_RENDER_REFERENCE = false;
// A missing D3D11 callback does not identify the active RHI. Select the reviewed
// route explicitly so a diagnostic never guesses from loaded DLLs or silence.
export const MTA_CORE_RENDER_ROUTE = 'd3d11';
// COM metadata queries need their own explicit admission. They release every
// borrowed frame interface before returning and never submit GPU commands.
export const MTA_CORE_RENDER_GPU_REFERENCE = false;
// Static custom-presenter recognition is a separate diagnostic. Matching a
// reviewed implementation does not establish its frame sink or permit drawing.
export const MTA_CORE_RENDER_CUSTOM_PRESENT_REFERENCE = false;
// Observe the scoped natural Present and CPU buffer association separately.
// This does not verify GPU resource state, a drawing boundary or startup.
export const MTA_CORE_RENDER_FRAME_BOUNDARY_REFERENCE = false;
// Native current-slot dispatch is an independent caller hook, never implicit
// in the older standard Present probe and never permission to issue GPU work.
export const MTA_CORE_RENDER_NATIVE_DISPATCH_REFERENCE = false;
// Core obtains an explicit device lease, ends observation, then renders the
// existing MTA GUI on its own queue/offscreen target. No native buffer is drawn.
export const MTA_CORE_GUI_STARTUP_REFERENCE = false;
// Display the existing GUI in an MTA-owned child composition surface. The
// observed device/window lease is released after our queue and surface drain.
export const MTA_CORE_GUI_SURFACE_REFERENCE = false;
// Real MTA menu interaction needs a separately acknowledged, reversible local
// player-control lease. Ordinary Smoke and the passive GUI trials stay read-only.
export const MTA_CORE_GUI_FRONTEND_REFERENCE = false;
// Core owns a bounded neutral configured-input/native scope alongside its
// actual menu and console. This distinct mode never changes player-control
// flags through CLEO and does not establish camera or gameplay readiness.
export const MTA_CORE_GUI_FRONTEND_INPUT_REFERENCE = false;
// Retain a successfully constructed Core/backend around the same bounded
// frontend input view. This additive lifecycle is separate from InitializeCore
// and never advertises gameplay, networking or complete engine readiness.
export const MTA_CORE_GUI_FRONTEND_STARTUP_REFERENCE = false;
// Keep the same MTA frontend alive until explicit GUI closure or an expired
// independent host/native heartbeat. Historical trials retain their timers.
export const MTA_CORE_PERSISTENT_FRONTEND = false;
