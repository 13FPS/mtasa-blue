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
