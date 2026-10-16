// SPDX-License-Identifier: GPL-3.0-or-later
// All entity handles stay behind the game interface so lifecycle behavior can
// be checked without launching GTA or guessing the Definitive Edition layout.

export function validTransform(value) {
    return value && ['x', 'y', 'z', 'heading'].every(name =>
        typeof value[name] === 'number' && Number.isFinite(value[name]) && Math.abs(value[name]) <= 100000);
}

// This owner runs only on the CLEO script thread: the Core worker requests a
// scalar lease, while the existing game opcodes perform and verify each write.
// An attempted disable creates a restoration obligation even when it throws;
// losing a heartbeat must never discard that obligation or target another ped.
export class PlayerControlLease {
    constructor(game, bridge, options = {}) {
        this.game = game; this.bridge = bridge; this.log = options.log || (() => {});
        this.generation = 0; this.originalPed = null; this.nativeInitial = null;
        this.mutationAttempted = false; this.disabledVerified = false;
        this.acknowledged = false; this.revoked = false; this.inputDetached = false;
        this.restoredVerified = false; this.restoredAcknowledged = false;
        this.completed = false; this.stopReason = null; this.error = null;
        this.requestedAt = null; this.acknowledgedAt = null; this.lastAt = null;
        this.heartbeatCount = 0; this.disableAttempts = 0; this.restoreAttempts = 0;
        this.revokeAttempts = 0; this.lastPendingReason = null;
    }

    snapshot() {
        return { generation: this.generation, original_ped: this.originalPed,
            native_initial: this.nativeInitial, mutation_attempted: this.mutationAttempted,
            native_disabled_verified: this.disabledVerified, acquired: this.acknowledged,
            revoked: this.revoked, input_detached: this.inputDetached,
            native_restored_verified: this.restoredVerified, restored_acknowledged: this.restoredAcknowledged,
            no_mutation: !this.mutationAttempted, completed: this.completed,
            stop_reason: this.stopReason, error: this.error, heartbeats: this.heartbeatCount,
            disable_attempts: this.disableAttempts, restore_attempts: this.restoreAttempts,
            revoke_attempts: this.revokeAttempts, gameplay_ready: false, full_mta_ready: false,
            camera_capture_verified: false };
    }

    record(action, extra = {}) {
        // A log sink failure is a reason to stop admission, not to abandon a
        // native state change that still needs restoration on the next tick.
        try { this.log({ event: 'core_gui_frontend_control', action, ...this.snapshot(), ...extra }); }
        catch (error) { this.stopReason = this.stopReason || 'log_error'; this.error = this.error || String(error); }
    }

    stop(reason = 'cancelled') {
        if (!this.stopReason) { this.stopReason = reason; this.record('stop_requested'); }
    }

    fail(error) {
        this.error = this.error || String(error);
        this.stop('error');
    }

    identity(requireOnFoot = true) {
        if (this.game.isReady() !== true) throw new Error('Player is not playing');
        const ped = this.game.localPed();
        if (!Number.isInteger(ped) || ped <= 0 || ped > 0x7fffffff || this.game.doesPedExist(ped) !== true ||
            (requireOnFoot && this.game.isOnFoot(ped) !== true))
            throw new Error('Player is not a stable eligible character');
        if (this.originalPed !== null && ped !== this.originalPed)
            throw new Error('Original player identity changed');
        return ped;
    }

    control() {
        const value = this.game.isControlOn();
        if (typeof value !== 'boolean') throw new Error('Player-control native did not return a boolean');
        return value;
    }

    readState() {
        const captured = this.bridge.capture();
        if (captured !== 0 && captured !== 1) throw new Error('Control capture status unavailable');
        if (!captured) return null;
        const state = Array.from({ length: 8 }, (_, field) => this.bridge.read(field));
        if (state.some(value => !Number.isInteger(value) || value < 0 || value > 0xffffffff) ||
            state.slice(1).some(value => value > 1) || (state[1] && !state[0]) ||
            (state[2] && !state[1]) || (state[3] && !state[2]) ||
            (state[6] && (!state[4] || !state[5]))) throw new Error('Invalid player-control lease state');
        if (this.generation && state[0] !== this.generation)
            throw new Error('Player-control lease generation changed while owned');
        return state;
    }

    pending(reason) {
        if (this.lastPendingReason !== reason) { this.lastPendingReason = reason; this.record('cleanup_pending', { reason }); }
    }

    cleanup(state) {
        if (!this.revoked) {
            ++this.revokeAttempts;
            if (this.bridge.revoke(this.generation) !== 1) { this.pending('revoke_rejected'); return; }
            this.record('revoke_accepted');
        }
        // The worker detaches input first, without waiting for its GPU/Core
        // owners to drain. Restoring here prevents cleanup latency locking CJ.
        if (!state[4] || !state[5]) { this.pending('input_detachment'); return; }
        if (this.mutationAttempted && !this.restoredAcknowledged) {
            // A rejected/busy acknowledgement can outlive this native read.
            // Every retry needs fresh proof for the same original player;
            // another script or engine transition may have changed it again.
            this.restoredVerified = false;
            try {
                // Admission is on foot, but restoration must also work if the
                // same original ped entered a vehicle during cancellation.
                this.identity(false);
                const before = this.control();
                this.identity(false);
                if (before !== this.nativeInitial) {
                    ++this.restoreAttempts;
                    this.game.setControl(this.nativeInitial);
                }
                const after = this.control();
                this.identity(false);
                if (after !== this.nativeInitial) throw new Error('Player-control restoration readback differs');
                this.restoredVerified = true;
                this.record('native_restored', { native_restored: after });
            } catch (error) { this.error = this.error || String(error); this.pending(String(error)); return; }
        }
        if (!this.restoredAcknowledged) {
            if (this.bridge.restored(this.generation) !== 1) { this.pending('restoration_ack_rejected'); return; }
            this.restoredAcknowledged = true;
            this.record(this.mutationAttempted ? 'restoration_acknowledged' : 'no_mutation_acknowledged');
        }
        if (state[6]) { this.completed = true; this.lastPendingReason = null; this.record('completed'); }
    }

    advance(now) {
        if (this.completed) return;
        if (!Number.isSafeInteger(now) || now < 0 || (this.lastAt !== null && now < this.lastAt)) {
            this.fail(new Error('Invalid player-control clock'));
            now = this.lastAt === null ? 0 : this.lastAt;
        }
        if (this.lastAt !== null && this.acknowledged && now - this.lastAt > 1000) this.stop('heartbeat_gap');
        this.lastAt = now;
        try {
            if (this.game.stopRequested()) this.stop('Ctrl+Shift+End');
            const state = this.readState();
            if (state === null) {
                if (this.generation) throw new Error('Player-control snapshot unavailable while owned');
                return;
            }
            if (!this.generation && state[1]) {
                this.generation = state[0]; this.requestedAt = now;
                this.record('requested');
            }
            if (!this.generation) return;
            this.revoked = state[4] === 1; this.inputDetached = state[5] === 1;
            if (state[7]) this.stop('control_fault');
            if (this.revoked) this.stop('worker_revoked');
            if (!this.acknowledged && now - this.requestedAt >= 5000) this.stop('acquire_deadline');
            // Core starts its 30-second display after the handoff/GUI setup;
            // a separate five-second margin keeps this watchdog independent.
            if (this.acknowledged && now - this.acknowledgedAt >= 35000) this.stop('active_deadline');
            if (!this.stopReason) {
                if (this.game.isFocused() !== true) throw new Error('GTA lost foreground focus');
                const ped = this.identity();
                if (!this.acknowledged) {
                    this.originalPed = ped;
                    this.nativeInitial = this.control();
                    this.record('native_initial', { native_initial: this.nativeInitial });
                    if (this.nativeInitial !== true) { this.stop('control_initially_disabled'); }
                    else if (!this.stopReason) {
                        this.identity();
                        // Mark BEFORE invoking the opcode: a binding can mutate
                        // successfully and then fail while returning to script.
                        this.mutationAttempted = true; ++this.disableAttempts;
                        this.game.setControl(false);
                        if (this.control() !== false) throw new Error('Player-control disable readback differs');
                        this.identity();
                        this.disabledVerified = true; this.record('native_disabled', { native_disabled: false });
                        if (this.stopReason) return;
                        if (this.game.isFocused() !== true) throw new Error('GTA lost foreground focus during acquisition');
                        if (this.bridge.ack(this.generation) !== 1) throw new Error('Player-control acknowledgement rejected');
                        this.acknowledged = true; this.acknowledgedAt = now;
                        this.record('acquired');
                    }
                }
                if (this.acknowledged && !this.stopReason) {
                    if (this.control() !== false) throw new Error('Player control changed during ownership');
                    this.identity();
                    if (this.game.isFocused() !== true) throw new Error('GTA lost foreground focus before heartbeat');
                    if (state[2] && !state[3]) throw new Error('Player-control heartbeat expired');
                    if (this.bridge.heartbeat(this.generation) !== 1) throw new Error('Player-control heartbeat rejected');
                    ++this.heartbeatCount;
                }
            }
            if (this.stopReason) this.cleanup(state);
        } catch (error) {
            this.fail(error);
            // Retry a rejected revoke/restoration later; a foreign generation
            // or unreadable detachment state cannot authorize restoration.
            if (this.generation) {
                try { ++this.revokeAttempts; this.bridge.revoke(this.generation); }
                catch (_) {}
            }
        }
    }

    finishWithoutRequest() {
        if (this.generation || this.mutationAttempted) return false;
        this.completed = true; this.record('completed_without_request'); return true;
    }
}

const remoteAnimations = { idle: 'idle_stance', walk: 'Walk_civi' };
const networkInterval = 50;
const gaitProbeInterval = 100;
const gaitProbeBudget = 30000;
const gaitProbeMaxSamples = 300;
// The accepted model-7 Walk_civi native reference measured this travel speed
// on the pinned DE build. It scales requested playback, not verified cadence
// or visual foot contact; the original recording remains the calibration source.
export const nativeGaitReferenceSpeed = 1.5216103452786847;
const gaitRateInterval = 250;
const gaitRateMaxWindow = 500;
const gaitRateMin = 0.25;
const gaitRateMax = 2;
const gaitRateDelta = 0.05;
const transformNames = ['x', 'y', 'z', 'heading'];
const copyTransform = value => ({ x: value.x, y: value.y, z: value.z, heading: value.heading });
const transformChanged = (previous, value) => !previous || transformNames.some(name => previous[name] !== value[name]);
const headingDelta = (from, to) => ((to - from) % 360 + 540) % 360 - 180;

export class AdapterController {
    constructor(game, transport, { mode = 'smoke', nonce = 1, log = () => {}, stopReason = 'stop_key', renderSmoothing = false,
        gaitProbe = false, gaitRate = false } = {}) {
        if (mode !== 'smoke' && mode !== 'online') throw new Error('Unknown adapter mode');
        if (!Number.isInteger(nonce) || nonce < 1 || nonce > 0xffffffff) throw new Error('Invalid connection nonce');
        if (typeof renderSmoothing !== 'boolean') throw new Error('Invalid render smoothing option');
        if (typeof gaitProbe !== 'boolean') throw new Error('Invalid gait probe option');
        if (typeof gaitRate !== 'boolean') throw new Error('Invalid gait rate option');
        this.game = game;
        this.transport = transport;
        this.mode = mode;
        this.nonce = nonce;
        this.log = log;
        // The entry owns the physical key binding, while the controller keeps
        // its cleanup behavior independent of any particular keyboard shortcut.
        this.stopReason = stopReason;
        // Presentation can advance between snapshots without multiplying the
        // bridge calls. Existing non-smoothed clients retain their tick cadence.
        this.renderSmoothing = renderSmoothing && mode === 'online';
        // A short read-only probe measures this build's actual gait before a
        // playback-rate calibration is guessed from classic-game behavior.
        this.gaitProbeEnabled = gaitProbe && mode === 'online';
        this.gaitRateEnabled = gaitRate && mode === 'online';
        this.lastGaitRateTickAt = null;
        this.gaitProbe = null;
        this.gaitProbeSelected = false;
        this.nextNetwork = null;
        this.lastRenderAt = null;
        this.running = true;
        this.open = false;
        this.localId = 0;
        this.localPed = null;
        this.nextConnect = 0;
        this.nextSmokeLog = 0;
        this.nextOnlineLog = 0;
        this.successfulSubmissions = 0;
        this.modelRequested = false;
        this.modelId = 7;
        this.remotes = new Map();
    }

    clearSession(reason = 'session_cleared') {
        this.finishGaitProbe(reason);
        let failure = null;
        // Continue cleanup even if one engine command fails. Leaving a socket
        // or another proxy alive would hide the original lifecycle failure.
        for (const [id, remote] of this.remotes) {
            if (this.gaitRateEnabled) this.resetGaitRate(remote, true);
            try {
                if (this.game.doesPedExist(remote.handle)) this.game.deletePed(remote.handle);
                this.log({ event: 'remote_ped_deleted', player_id: id });
            } catch (error) { failure = failure || error; }
        }
        this.remotes.clear();
        if (this.modelRequested) {
            try { this.game.releaseModel(this.modelId); } catch (error) { failure = failure || error; }
        }
        this.modelRequested = false;
        if (this.open) {
            try { this.transport.disconnect(); } catch (error) { failure = failure || error; }
        }
        this.open = false;
        this.localId = 0;
        this.localPed = null;
        this.nextOnlineLog = 0;
        this.successfulSubmissions = 0;
        this.nextNetwork = null;
        this.lastRenderAt = null;
        this.lastGaitRateTickAt = null;
        this.gaitProbe = null;
        this.gaitProbeSelected = false;
        if (failure) throw failure;
    }

    stop(reason = 'requested') {
        if (!this.running) return;
        this.running = false;
        this.clearSession('stop');
        this.log({ event: 'adapter_stopped', reason });
    }

    tick(now) {
        if (!this.running) return;
        try {
            this.advance(now);
        } catch (error) {
            this.running = false;
            try { this.clearSession('error'); } catch (cleanupError) {
                this.log({ event: 'cleanup_failed', error: String(cleanupError) });
            }
            throw error;
        }
    }

    animateRemote(remote, player, previous, now, renderClockReset = false) {
        const elapsed = remote.sampleAt === null ? 0 : now - remote.sampleAt;
        // Cached snapshots carry no receipt timestamp. Infer only plausible
        // horizontal movement, and let the short hold bridge unchanged polls
        // without leaving a stationary or teleported proxy walking indefinitely.
        // A display tick can reverse after the last network poll while still
        // being later than that poll. Reset the animation clock as well so a
        // resumed blend cannot preserve walking or shorten playback's grace.
        const clockReset = renderClockReset || (remote.sampleAt !== null && now < remote.sampleAt);
        if (clockReset) {
            remote.nextEvidence = now;
            remote.evidenceAt = now;
            if (remote.animation) remote.animation.requestedAt = now;
            this.log({ event: 'remote_animation_clock_reset', player_id: player.id, ped: remote.handle });
        }
        if (!previous || elapsed <= 0 || elapsed > 250 || clockReset) {
            remote.lastMovedAt = null;
        } else {
            const distance = Math.hypot(player.x - previous.x, player.y - previous.y);
            const speed = distance * 1000 / elapsed;
            if (speed > 15) remote.lastMovedAt = null;
            else if (distance >= 0.02 && speed >= 0.25) remote.lastMovedAt = now;
        }
        remote.sampleAt = now;
        const state = remote.lastMovedAt !== null && now - remote.lastMovedAt < 350 ? 'walk' : 'idle';
        if (!remote.animation || remote.animation.state !== state) {
            const clip = remoteAnimations[state];
            // A task is dispatched only on a state transition. Replaying it
            // for every coordinate write would continually restart its pose.
            this.game.playRemoteAnimation(remote.handle, clip);
            remote.animation = { state, clip, requestedAt: now };
            if (this.gaitRateEnabled) this.resetGaitRate(remote);
            this.log({ event: 'remote_animation_requested', player_id: player.id, ped: remote.handle,
                state, animation: clip, animation_file: 'PED',
                ...(this.gaitRateEnabled ? { requested_at_ms: now } : {}) });
        }
    }

    resetGaitRate(remote, clockReset = false) {
        const previous = remote.gaitRate;
        remote.gaitRate = { points: [], window: null, requestedRate: null,
            lastAppliedAt: clockReset ? null : previous?.lastAppliedAt ?? null,
            lastQueryAt: clockReset ? null : previous?.lastQueryAt ?? null, evidence: null };
    }

    trackGaitRate(remote, player, now, elapsed, discontinuity) {
        if (!this.gaitRateEnabled) return;
        if (discontinuity || elapsed <= 0 || elapsed > 250) this.resetGaitRate(remote, discontinuity === 'clock_reversed');
        if (remote.animation.state !== 'walk') return;
        const rate = remote.gaitRate;
        const last = rate.points[rate.points.length - 1];
        if (last && last.x === player.x && last.y === player.y) return;
        // Cached polls have no receipt timestamp. Only changed XY endpoints
        // contribute elapsed time and accumulated path, preventing a 100 ms
        // update observed on a 50 ms poll from requesting twice its real rate.
        rate.points.push({ x: player.x, y: player.y, at: now,
            path: last ? last.path + Math.hypot(player.x - last.x, player.y - last.y) : 0 });
        while (rate.points.length > 2 && rate.points[1].at <= now - gaitRateInterval) rate.points.shift();
        const first = rate.points[0];
        const duration = now - first.at;
        if (duration > gaitRateMaxWindow) {
            rate.points = [rate.points[rate.points.length - 1]];
            rate.window = null;
            return;
        }
        if (duration < gaitRateInterval) return;
        const distance = rate.points[rate.points.length - 1].path - first.path;
        rate.window = { startedAt: first.at, finishedAt: now, duration, distance,
            samples: rate.points.length, speed: distance * 1000 / duration };
    }

    applyGaitRate(remote, player, now, sharedAnimation = null) {
        if (!this.gaitRateEnabled || remote.animation?.state !== 'walk' || remote.handle === this.localPed ||
            this.remotes.get(player.id) !== remote) return;
        const rate = remote.gaitRate;
        const window = rate?.window;
        if (!window || now - window.finishedAt > gaitRateInterval ||
            (rate.lastAppliedAt !== null && now - rate.lastAppliedAt < gaitRateInterval)) return;
        const requested = Math.max(gaitRateMin, Math.min(gaitRateMax, window.speed / nativeGaitReferenceSpeed));
        if (rate.requestedRate !== null && Math.abs(requested - rate.requestedRate) + 1e-12 < gaitRateDelta) return;
        const cached = rate.evidence;
        let animation = sharedAnimation || (cached?.at === now && cached.taskAt === remote.animation.requestedAt ? cached.value : null);
        if (!animation) {
            if (rate.lastQueryAt !== null && now - rate.lastQueryAt < gaitRateInterval) return;
            rate.lastQueryAt = now;
            animation = this.readAnimationEvidence(remote, now);
        }
        // The native setter requires a live association. A pending task gets
        // no guessed rate or replay; existing playback grace still owns failure.
        if (!animation.playing || !this.game.doesPedExist(remote.handle)) return;
        this.game.setRemoteAnimationRate(remote.handle, remote.animation.clip, requested);
        rate.requestedRate = requested;
        rate.lastAppliedAt = now;
        this.log({ event: 'remote_animation_rate_applied', player_id: player.id, ped: remote.handle,
            state: remote.animation.state, clip: remote.animation.clip, sample_at_ms: now,
            task_requested_at_ms: remote.animation.requestedAt, task_age_ms: now - remote.animation.requestedAt,
            reference_speed: nativeGaitReferenceSpeed, measured_speed: window.speed, requested_rate: requested,
            rate_min: gaitRateMin, rate_max: gaitRateMax, setter_interval_ms: gaitRateInterval, min_rate_delta: gaitRateDelta,
            min_speed_window_ms: gaitRateInterval, max_speed_window_ms: gaitRateMaxWindow,
            speed_window_ms: window.duration, speed_window_started_at_ms: window.startedAt,
            speed_window_finished_at_ms: window.finishedAt, speed_window_age_ms: now - window.finishedAt,
            max_speed_window_age_ms: gaitRateInterval, speed_path_distance: window.distance,
            speed_sample_count: window.samples, source: 'received_target_xy' });
    }

    readAnimationEvidence(remote, now) {
        const value = this.game.readRemoteAnimation(remote.handle, remote.animation.clip);
        if (!value || typeof value.playing !== 'boolean' ||
            (value.playing ? typeof value.time !== 'number' || !Number.isFinite(value.time) || value.time < 0 || value.time > 1
                           : value.time !== null)) throw new Error('Remote ped returned invalid animation evidence');
        if (this.gaitRateEnabled && remote.gaitRate) {
            remote.gaitRate.evidence = { at: now, taskAt: remote.animation.requestedAt, value };
            // Existing probe/playback reads also satisfy the query budget, so
            // a pending association does not acquire redundant rate-only reads
            // between the probe's more frequent observations.
            remote.gaitRate.lastQueryAt = now;
        }
        return value;
    }

    observeRemoteAnimation(remote, player, observed, now) {
        const value = this.readAnimationEvidence(remote, now);
        const age = now - remote.animation.requestedAt;
        const pending = !value.playing && age < 1000;
        const presentation = this.renderSmoothing ? this.presentationEvidence(remote, now) : {};
        this.log({ event: 'remote_animation_observed', player_id: player.id, ped: remote.handle,
            state: remote.animation.state, animation: remote.animation.clip, animation_file: 'PED',
            playing: value.playing, time: value.time, pending, task_age_ms: age,
            requested_transform: { ...remote.transform }, observed_transform: observed, ...presentation,
            z_error: observed.z - remote.transform.z });
        // Native tasks can begin on a later frame. After that scheduling grace,
        // absent playback is a failed candidate, not a reason to replay forever.
        if (!value.playing && !pending) throw new Error('Remote animation did not start: ' + remote.animation.clip);
        return value;
    }

    startGaitProbe(remote, player, now) {
        if (!this.gaitProbeEnabled || this.gaitProbeSelected) return;
        this.gaitProbeSelected = true;
        this.gaitProbe = { playerId: player.id, handle: remote.handle, startedAt: now, lastClockAt: now,
            nextSampleAt: now, samples: 0, finished: false };
        this.log({ event: 'remote_gait_probe_started', player_id: player.id, ped: remote.handle,
            sample_at_ms: now, interval_ms: gaitProbeInterval, budget_ms: gaitProbeBudget, max_samples: gaitProbeMaxSamples });
    }

    finishGaitProbe(reason, now) {
        const probe = this.gaitProbe;
        if (!probe || probe.finished) return;
        probe.finished = true;
        // Preserve the last valid absolute clock on reversal rather than
        // fabricating a negative elapsed time or a reset animation phase.
        const finishedAt = Number.isFinite(now) && now >= probe.lastClockAt ? now : probe.lastClockAt;
        this.log({ event: 'remote_gait_probe_finished', player_id: probe.playerId, ped: probe.handle,
            reason, sample_at_ms: finishedAt, probe_elapsed_ms: finishedAt - probe.startedAt, samples: probe.samples });
    }

    observeGaitProbe(remote, player, now, shared) {
        const probe = this.gaitProbe;
        if (!probe || probe.finished || probe.handle !== remote.handle || probe.playerId !== player.id ||
            now < probe.nextSampleAt) return;
        if (now - probe.startedAt >= gaitProbeBudget) {
            this.finishGaitProbe('time_budget', now);
            return;
        }
        // Only the chosen owned proxy is queried, after the validated network
        // snapshot. Share the existing one-second readback whenever it is due;
        // a delayed tick takes one sample, never a catch-up burst.
        if (remote.handle === this.localPed || !this.game.doesPedExist(remote.handle)) {
            this.finishGaitProbe('missing_ped', now);
            return;
        }
        const observedValue = shared ? shared.observed : this.game.readTransform(remote.handle);
        if (!validTransform(observedValue)) throw new Error('Remote ped returned an invalid observed transform');
        const animation = shared ? shared.animation : this.readAnimationEvidence(remote, now);
        const age = now - remote.animation.requestedAt;
        if (age < 0 || !Number.isFinite(age)) throw new Error('Remote ped returned invalid gait evidence clock');
        // Absolute timestamps and each task's own age remain separate. A new
        // clip can already have advanced when observed; phase zero is never
        // assumed or injected into calibration evidence.
        this.applyGaitRate(remote, player, now, animation);
        this.log({ event: 'remote_gait_sample', player_id: player.id, ped: remote.handle,
            sample_at_ms: now, probe_elapsed_ms: now - probe.startedAt, task_age_ms: age,
            state: remote.animation.state, animation_file: 'PED', clip: remote.animation.clip,
            playing: animation.playing, time: animation.time, requested_transform: copyTransform(remote.transform),
            observed_transform: copyTransform(observedValue),
            network_target: copyTransform(this.renderSmoothing ? remote.target : player),
            ...(this.gaitRateEnabled ? { task_requested_at_ms: remote.animation.requestedAt,
                requested_animation_rate: remote.gaitRate?.requestedRate ?? null } : {}) });
        ++probe.samples;
        probe.nextSampleAt = now + gaitProbeInterval;
        if (probe.samples >= gaitProbeMaxSamples) this.finishGaitProbe('sample_budget', now);
    }

    presentationEvidence(remote, now) {
        return { network_target: { ...remote.target },
            smoothing_progress: !transformChanged(remote.renderFrom, remote.target) ? 1
                : Math.max(0, Math.min(1, (now - remote.targetAt) / networkInterval)),
            target_age_ms: Math.max(0, now - remote.targetAt) };
    }

    writeDisplayed(remote, value, now) {
        if (!transformChanged(remote.transform, value)) return false;
        this.game.movePed(remote.handle, value);
        remote.transform = copyTransform(value);
        const interval = remote.displayAt === null ? null : now - remote.displayAt;
        if (interval !== null && interval > 0) {
            remote.motion.displayMin = remote.motion.displayMin === null ? interval : Math.min(remote.motion.displayMin, interval);
            remote.motion.displayMax = remote.motion.displayMax === null ? interval : Math.max(remote.motion.displayMax, interval);
        }
        remote.displayAt = now;
        ++remote.motion.writes;
        return true;
    }

    renderRemote(remote, now, skipBlend = false) {
        ++remote.motion.ticks;
        if (!remote.target || skipBlend) return false;
        if (!transformChanged(remote.renderFrom, remote.target)) return this.writeDisplayed(remote, remote.target, now);
        const progress = Math.max(0, Math.min(1, (now - remote.targetAt) / networkInterval));
        if (progress === 1) return this.writeDisplayed(remote, remote.target, now);
        const from = remote.renderFrom;
        if (progress === 0) return this.writeDisplayed(remote, from, now);
        // Interpolate only within the received segment. Heading takes the
        // shortest turn; Z stays at the received body center so terrain/step
        // changes cannot invent an airborne path between heights.
        return this.writeDisplayed(remote, { x: from.x + (remote.target.x - from.x) * progress,
            y: from.y + (remote.target.y - from.y) * progress, z: remote.target.z,
            heading: from.heading === remote.target.heading ? from.heading
                : ((from.heading + headingDelta(from.heading, remote.target.heading) * progress) % 360 + 360) % 360 }, now);
    }

    targetResetReason(remote, player, previous, now, resetReason) {
        if (!previous) return 'initial';
        if (resetReason) return resetReason;
        const elapsed = now - remote.sampleAt;
        if (elapsed > 250) return 'network_gap';
        if (previous.z !== player.z) return 'height_change';
        if (elapsed > 0 && Math.hypot(player.x - previous.x, player.y - previous.y) * 1000 / elapsed > 15) return 'teleport';
        return null;
    }

    acceptTarget(remote, player, previous, now, resetReason) {
        const elapsed = remote.sampleAt === null ? 0 : now - remote.sampleAt;
        ++remote.motion.polls;
        if (elapsed > 0) {
            remote.motion.networkMin = remote.motion.networkMin === null ? elapsed : Math.min(remote.motion.networkMin, elapsed);
            remote.motion.networkMax = remote.motion.networkMax === null ? elapsed : Math.max(remote.motion.networkMax, elapsed);
        }
        const changed = transformChanged(previous, player);
        if (!changed && !resetReason) return false;
        if (changed) ++remote.motion.targets;
        remote.target = copyTransform(player);
        remote.targetAt = now;
        remote.renderFrom = remote.transform ? { ...remote.transform } : { ...remote.target };
        if (!resetReason) return false;
        // A discontinuity is authoritative, not a segment to travel through.
        // Reset from the newly received pose and do not extrapolate on resumes.
        remote.renderFrom = { ...remote.target };
        remote.motion.resetReason = resetReason;
        return this.writeDisplayed(remote, remote.target, now);
    }

    observeRemote(remote, player, changed, now) {
        if (now < remote.nextEvidence) return;
        const target = this.renderSmoothing ? { network_target: { ...remote.target } } : {};
        if (changed)
            this.log({ event: 'remote_transform_applied', player_id: player.id, ped: remote.handle, ...remote.transform, ...target });
        const value = this.game.readTransform(remote.handle);
        if (!validTransform(value)) throw new Error('Remote ped returned an invalid observed transform');
        const observed = copyTransform(value);
        if (changed)
            this.log({ event: 'remote_transform_observed', player_id: player.id, ped: remote.handle,
                requested: { ...remote.transform }, observed, z_error: observed.z - remote.transform.z, ...target });
        const animation = this.observeRemoteAnimation(remote, player, observed, now);
        if (this.renderSmoothing) {
            const motion = remote.motion;
            // One bounded summary establishes that display writes advance
            // between polls without adding a log record for every game tick.
            this.log({ event: 'remote_motion_observed', player_id: player.id, ped: remote.handle,
                ...this.presentationEvidence(remote, now), requested_transform: { ...remote.transform }, observed_transform: observed,
                evidence_elapsed_ms: remote.evidenceAt === null ? 0 : Math.max(0, now - remote.evidenceAt),
                display_tick_count_since_evidence: motion.ticks, display_write_count_since_evidence: motion.writes,
                network_poll_count_since_evidence: motion.polls, network_target_updates_since_evidence: motion.targets,
                display_interval_min_ms: motion.displayMin, display_interval_max_ms: motion.displayMax,
                network_interval_min_ms: motion.networkMin, network_interval_max_ms: motion.networkMax,
                reset_reason: motion.resetReason });
            remote.motion = { ticks: 0, writes: 0, polls: 0, targets: 0, displayMin: null, displayMax: null,
                networkMin: null, networkMax: null, resetReason: null };
        }
        remote.evidenceAt = now;
        remote.nextEvidence = now + 1000;
        return { observed, animation };
    }

    advance(now) {
        if (!Number.isFinite(now) || ((this.gaitProbeEnabled || this.gaitRateEnabled) && now < 0)) throw new Error('Invalid adapter clock');
        const probe = this.gaitProbe;
        if (probe && !probe.finished) {
            if (now < probe.lastClockAt) this.finishGaitProbe('clock_reversed', now);
            else if (now - probe.lastClockAt > 250) this.finishGaitProbe('render_gap', now);
            else if (now - probe.startedAt >= gaitProbeBudget) this.finishGaitProbe('time_budget', now);
            if (!probe.finished) probe.lastClockAt = now;
        }
        if (this.game.stopRequested()) {
            this.stop(this.stopReason);
            return;
        }
        if (!this.game.isReady()) {
            if (this.open || this.remotes.size) this.clearSession('loading');
            this.nextConnect = now + 1000;
            return;
        }
        const ped = this.game.localPed();
        if (!this.game.doesPedExist(ped)) {
            if (this.open || this.remotes.size) this.clearSession('local_ped_missing');
            this.nextConnect = now + 1000;
            return;
        }
        if (this.mode === 'smoke') {
            const local = this.game.readTransform(ped);
            if (!validTransform(local)) throw new Error('Local player returned an invalid transform');
            if (now >= this.nextSmokeLog) {
                this.log({ event: 'local_transform', ped, ...local, online: false, game_adapter_active: true });
                this.nextSmokeLog = now + 1000;
            }
            return;
        }

        // A replaced player handle can mark death/restart/loading. Never keep
        // proxies from that session or use its connection identity afterwards.
        if (this.open && this.localPed !== ped) {
            this.clearSession('local_ped_changed');
            this.nextConnect = now + 1000;
        }
        let resetReason = null;
        if (this.gaitRateEnabled) {
            const previousTick = this.lastGaitRateTickAt;
            const reason = previousTick !== null && now < previousTick ? 'clock_reversed'
                : previousTick !== null && now - previousTick > 250 ? 'render_gap' : null;
            this.lastGaitRateTickAt = now;
            if (reason) {
                for (const remote of this.remotes.values()) this.resetGaitRate(remote, reason === 'clock_reversed');
                resetReason = reason;
            }
        }
        if (this.renderSmoothing) {
            if (this.lastRenderAt !== null && now < this.lastRenderAt) {
                resetReason = 'clock_reversed';
                this.nextNetwork = now;
                this.nextOnlineLog = now;
            } else if (this.lastRenderAt !== null && now - this.lastRenderAt > 250) resetReason = 'render_gap';
            this.lastRenderAt = now;
            if (this.nextNetwork !== null && now < this.nextNetwork) {
                for (const [id, remote] of this.remotes) {
                    // A handle can disappear between network polls. The next
                    // validated snapshot owns recreation; no stale handle is
                    // sent another coordinate or animation command meanwhile.
                    if (!this.game.doesPedExist(remote.handle)) {
                        if (this.gaitProbe && this.gaitProbe.handle === remote.handle) this.finishGaitProbe('missing_ped', now);
                        continue;
                    }
                    const changed = this.renderRemote(remote, now);
                    this.observeRemote(remote, { id }, changed, now);
                    this.applyGaitRate(remote, { id }, now);
                }
                return;
            }
            // Preserve a 50 ms schedule despite 16 ms display yields. A slow
            // tick advances directly to the next deadline, never a burst of
            // catch-up bridge submissions in one game frame.
            this.nextNetwork = this.nextNetwork === null ? now + networkInterval
                : this.nextNetwork + (Math.floor((now - this.nextNetwork) / networkInterval) + 1) * networkInterval;
        }
        const local = this.game.readTransform(ped);
        if (!validTransform(local)) throw new Error('Local player returned an invalid transform');
        if (!this.open) {
            if (now < this.nextConnect) return;
            if (!this.transport.connect(this.nonce)) throw new Error('Local transport could not open');
            this.open = true;
            this.localPed = ped;
            this.log({ event: 'connecting', endpoint: '127.0.0.1:22040' });
        }
        if (!this.transport.pump()) {
            this.clearSession('transport_lost');
            this.nextConnect = now + 1000;
            this.log({ event: 'transport_lost', retry_after_ms: 1000 });
            return;
        }
        const ownId = this.transport.localId();
        if (!Number.isInteger(ownId) || ownId < 0 || ownId > 0xffffffff) throw new Error('Invalid local connection identity');
        if (!ownId) return;
        if (this.localId && this.localId !== ownId) throw new Error('Connection identity changed unexpectedly');
        if (!this.localId) this.log({ event: 'joined', player_id: ownId });
        this.localId = ownId;
        if (!this.transport.submit(local)) throw new Error('Local transform submission failed');
        ++this.successfulSubmissions;

        const players = this.transport.players();
        if (!Array.isArray(players) || players.length > 16) throw new Error('Invalid snapshot player count');
        const seen = new Set();
        // Validate the whole snapshot before spawning or moving anything; an
        // invalid later record must not partially change the displayed world.
        for (const player of players) {
            if (!player || !Number.isInteger(player.id) || player.id < 1 || player.id > 0xffffffff || seen.has(player.id) || !validTransform(player))
                throw new Error('Invalid snapshot player state');
            seen.add(player.id);
        }
        // A single-client run needs evidence that signed coordinate bits
        // reached transport and returned in a valid snapshot. Keep the echo
        // separate because an asynchronous snapshot can contain an older state.
        if (now >= this.nextOnlineLog) {
            const own = players.find(player => player.id === ownId);
            const transform = value => ({ x: value.x, y: value.y, z: value.z, heading: value.heading });
            this.log({ event: 'online_transform', player_id: ownId, successful_submissions: this.successfulSubmissions,
                submitted: transform(local), echoed: own ? transform(own) : null });
            this.nextOnlineLog = now + 1000;
        }
        for (const [id, remote] of this.remotes) {
            if (!seen.has(id)) {
                if (this.gaitProbe && this.gaitProbe.handle === remote.handle) this.finishGaitProbe('deleted', now);
                if (this.gaitRateEnabled) this.resetGaitRate(remote, true);
                if (this.game.doesPedExist(remote.handle)) this.game.deletePed(remote.handle);
                this.remotes.delete(id);
                this.log({ event: 'remote_ped_deleted', player_id: id });
            }
        }
        if (!players.some(player => player.id !== ownId)) return;
        if (!this.modelRequested) {
            this.game.requestModel(this.modelId);
            this.modelRequested = true;
        }
        // Model streaming runs asynchronously. Do not block the game loop or
        // create a ped using a model which has not become available yet.
        if (!this.game.hasModelLoaded(this.modelId)) return;
        for (const player of players) {
            if (player.id === ownId) continue;
            let remote = this.remotes.get(player.id);
            if (remote && !this.game.doesPedExist(remote.handle)) {
                if (this.gaitProbe && this.gaitProbe.handle === remote.handle) this.finishGaitProbe('missing_ped', now);
                if (this.gaitRateEnabled) this.resetGaitRate(remote, true);
                this.remotes.delete(player.id);
                remote = null;
            }
            if (!remote) {
                const handle = this.game.createPed(this.modelId, player);
                if (handle === ped || !this.game.doesPedExist(handle)) throw new Error('Remote ped creation failed');
                // Record ownership before setup so an error in inert-ped
                // configuration still removes the newly created character.
                remote = { handle, transform: null, nextEvidence: 0, sampleAt: null, lastMovedAt: null, animation: null,
                    target: null, renderFrom: null, targetAt: null, displayAt: null, evidenceAt: null,
                    motion: { ticks: 0, writes: 0, polls: 0, targets: 0, displayMin: null, displayMax: null,
                        networkMin: null, networkMax: null, resetReason: null } };
                this.remotes.set(player.id, remote);
                this.game.makeRemoteInert(handle);
                this.log({ event: 'remote_ped_created', player_id: player.id, ped: handle, model: this.modelId });
            }
            const previous = this.renderSmoothing ? remote.target : remote.transform;
            const rateElapsed = remote.sampleAt === null ? 0 : now - remote.sampleAt;
            const discontinuity = this.targetResetReason(remote, player, previous, now, resetReason);
            if (discontinuity && discontinuity !== 'initial' && this.gaitProbe && this.gaitProbe.handle === remote.handle)
                this.finishGaitProbe(discontinuity, now);
            let changed;
            if (this.renderSmoothing) {
                const targetReset = this.targetResetReason(remote, player, previous, now, resetReason);
                // Decide discontinuities before advancing the prior segment.
                // A fresh authoritative pose needs only one setter this tick,
                // rather than briefly applying an obsolete endpoint first.
                changed = this.renderRemote(remote, now, Boolean(targetReset));
                changed = this.acceptTarget(remote, player, previous, now, targetReset) || changed;
            } else {
                changed = transformChanged(previous, player);
                if (changed) {
                    this.game.movePed(remote.handle, player);
                    remote.transform = copyTransform(player);
                }
            }
            this.animateRemote(remote, player, previous, now, resetReason === 'clock_reversed');
            this.trackGaitRate(remote, player, now, rateElapsed, discontinuity);
            this.startGaitProbe(remote, player, now);
            // Share one bounded position read between transform and animation
            // evidence. Reading during unchanged idle snapshots also exposes
            // root-motion drift that a setter-only check would miss.
            const observation = this.observeRemote(remote, player, changed, now);
            this.observeGaitProbe(remote, player, now, observation);
            this.applyGaitRate(remote, player, now);
        }
    }
}

const referenceReadyBudget = 10000;
const referenceSettleTime = 1000;
const referenceWalkBudget = 10000;
const referenceSampleInterval = 100;
const referenceMaxSamples = 100;
const referenceClip = 'Walk_civi';
const validPedHandle = handle => Number.isInteger(handle) && handle > 0 && handle <= 0x7fffffff;

// This independent Smoke controller owns no transport. A native walking task
// provides a reference for the engine's actual motion and animation phase;
// coordinate replication or a forced clip would contaminate that evidence.
export class NativeGaitReferenceController {
    constructor(game, { log = () => {}, stopReason = 'stop_key' } = {}) {
        this.game = game;
        this.log = log;
        this.stopReason = stopReason;
        this.running = true;
        this.modelId = 7;
        this.modelRequested = false;
        this.localPed = null;
        this.currentLocalPed = null;
        this.referencePed = null;
        this.startedAt = null;
        this.lastTickAt = null;
        this.referenceId = null;
        this.spawnedAt = null;
        this.taskAt = null;
        this.taskId = null;
        this.nextSampleAt = null;
        this.samples = 0;
        this.anchor = null;
        this.start = null;
        this.target = null;
        this.finished = false;
    }

    evidence(now) {
        return { reference_id: this.referenceId, reference_started_at_ms: this.startedAt,
            task_id: this.taskId, task_requested_at_ms: this.taskAt,
            ped: this.referencePed, local_ped: this.localPed, model: this.modelId,
            sample_at_ms: now, elapsed_ms: this.startedAt === null ? null : now - this.startedAt,
            task_age_ms: this.taskAt === null ? null : now - this.taskAt };
    }

    finish(reason, now, error = null) {
        if (this.finished || this.startedAt === null) return;
        this.finished = true;
        this.log({ event: 'native_gait_reference_finished', ...this.evidence(now), reason,
            // Reaching the endpoint or a sampling budget only ends observation.
            // Neither establishes stride length, visual contact, or no-slip.
            outcome: 'observation_only', samples: this.samples, ...(error ? { error: String(error) } : {}) });
    }

    cleanup(now) {
        let failure = null;
        const handle = this.referencePed;
        if (validPedHandle(handle) && handle !== this.localPed) {
            try {
                // Recheck the player at cleanup, including an external stop:
                // a changed player handle must never turn an owned NPC deletion
                // into a command against the current CJ.
                const current = this.game.localPed();
                this.currentLocalPed = validPedHandle(current) ? current : null;
                if (handle !== this.currentLocalPed && this.game.doesPedExist(handle)) {
                    this.game.deletePed(handle);
                    this.log({ event: 'native_gait_reference_cleanup_deleted', ...this.evidence(now) });
                }
            } catch (error) { failure = error; }
        }
        // Model release is independent of ped lookup/deletion. A failed native
        // cleanup must not prevent the other owned resource from being released.
        if (this.modelRequested) {
            try { this.game.releaseModel(this.modelId); } catch (error) { failure = failure || error; }
        }
        this.modelRequested = false;
        if (failure) throw failure;
    }

    stop(reason = 'requested', now = this.lastTickAt, error = null) {
        if (!this.running) return;
        this.running = false;
        // A reversed clock cannot manufacture a negative elapsed/task age in
        // the final evidence. Keep the most recent validated absolute timestamp.
        const finishedAt = Number.isFinite(now) && now >= (this.lastTickAt ?? now) ? now : this.lastTickAt;
        let failure = null;
        try { this.finish(reason, finishedAt, error); } catch (caught) { failure = caught; }
        try { this.cleanup(finishedAt); } catch (caught) {
            failure = failure || caught;
            try { this.log({ event: 'cleanup_failed', ...this.evidence(finishedAt), error: String(caught) }); } catch (_) {}
        }
        try {
            this.log({ event: 'native_gait_reference_stopped', ...this.evidence(finishedAt), reason,
                game_adapter_active: false, network_active: false });
        } catch (caught) { failure = failure || caught; }
        if (failure) throw failure;
    }

    tick(now) {
        if (!this.running) return;
        try {
            this.advance(now);
        } catch (error) {
            // A partial native setup or task request can already own resources.
            // Stop once, attempt all cleanup, and retain the original exception
            // so a secondary deletion/release failure cannot hide its cause.
            if (this.running) {
                try { this.stop('error', now, error); } catch (_) {}
            }
            throw error;
        }
    }

    readReferencePose(now) {
        if (!this.game.doesPedExist(this.referencePed)) {
            this.stop('missing_ped', now);
            return null;
        }
        const observed = this.game.readTransform(this.referencePed);
        if (!validTransform(observed)) throw new Error('Native gait reference ped returned an invalid observed transform');
        if (Math.hypot(observed.x - this.start.x, observed.y - this.start.y) > 12 || Math.abs(observed.z - this.start.z) > 2) {
            this.stop('lane_departure', now);
            return null;
        }
        return observed;
    }

    advance(now) {
        if (!Number.isFinite(now) || now < 0) throw new Error('Invalid native gait reference clock');
        if (this.startedAt === null) {
            this.startedAt = now;
            this.referenceId = 'native-gait-' + now;
            this.lastTickAt = now;
            this.log({ event: 'native_gait_reference_started', ...this.evidence(now), mode: 'smoke',
                interval_ms: referenceSampleInterval, readiness_budget_ms: referenceReadyBudget,
                settle_ms: referenceSettleTime, budget_ms: referenceWalkBudget, max_samples: referenceMaxSamples,
                min_arrival_age_ms: 2000, arrival_radius: 0.5, lane_radius: 12, max_z_drift: 2 });
        }
        if (this.game.stopRequested()) {
            this.stop(this.stopReason, now);
            return;
        }
        if (now < this.lastTickAt) {
            this.stop('clock_reversed', now);
            return;
        }
        if (now - this.lastTickAt > 250) {
            this.stop('render_gap', now);
            return;
        }
        this.lastTickAt = now;
        // The readiness deadline belongs to this activation, not to each model
        // poll. There is no retry or late creation after an incomplete attempt.
        if (this.spawnedAt === null && now - this.startedAt >= referenceReadyBudget) {
            this.stop(this.modelRequested ? 'model_timeout' : 'readiness_timeout', now);
            return;
        }
        if (!this.game.isReady()) {
            if (this.localPed !== null) this.stop('loading', now);
            return;
        }
        const local = this.game.localPed();
        this.currentLocalPed = validPedHandle(local) ? local : null;
        if (this.localPed !== null && local !== this.localPed) {
            this.stop('local_ped_changed', now);
            return;
        }
        if (!validPedHandle(local) || !this.game.doesPedExist(local)) {
            if (this.localPed !== null) this.stop('local_ped_missing', now);
            return;
        }
        if (this.localPed === null) {
            const anchor = this.game.readTransform(local);
            if (!validTransform(anchor)) throw new Error('Native gait reference player returned an invalid transform');
            this.localPed = local;
            this.anchor = copyTransform(anchor);
            this.start = { x: anchor.x - 4, y: anchor.y + 3, z: anchor.z, heading: 270 };
            this.target = { x: anchor.x + 4, y: anchor.y + 3, z: anchor.z, heading: 270 };
            if (!validTransform(this.start) || !validTransform(this.target))
                throw new Error('Native gait reference lane returned an invalid transform');
        }
        if (!this.modelRequested) {
            // Track even a partially successful streaming request so its model
            // can be released if the native wrapper then throws.
            this.modelRequested = true;
            this.game.requestModel(this.modelId);
        }
        if (this.spawnedAt === null) {
            if (!this.game.hasModelLoaded(this.modelId)) return;
            const handle = this.game.createPed(this.modelId, this.start);
            if (!validPedHandle(handle) || handle === local) throw new Error('Native gait reference ped creation failed');
            // Ownership precedes both existence verification and setup: either
            // native can fail after CREATE_CHAR has already allocated the NPC.
            this.referencePed = handle;
            if (!this.game.doesPedExist(handle)) throw new Error('Native gait reference ped creation failed');
            this.game.prepareReferencePed(handle, this.start);
            this.spawnedAt = now;
            this.log({ event: 'native_gait_reference_spawned', ...this.evidence(now),
                anchor_transform: copyTransform(this.anchor), start_transform: copyTransform(this.start),
                target_transform: copyTransform(this.target) });
        }
        let observed = this.readReferencePose(now);
        if (!observed) return;
        if (this.taskAt === null) {
            if (now - this.spawnedAt < referenceSettleTime) return;
            this.taskAt = now;
            this.taskId = 'walk-' + now;
            // Exactly one engine task owns walking after the pose has settled.
            // Observation below never moves, freezes, replays, or retimes it.
            this.game.walkReferencePed(this.referencePed, this.target);
            this.nextSampleAt = now;
            this.log({ event: 'native_gait_reference_task_requested', ...this.evidence(now),
                move_state: 4, time_limit_ms: referenceWalkBudget, clip: referenceClip,
                start_transform: copyTransform(this.start), target_transform: copyTransform(this.target) });
            // The first sample must pair animation readback with a pose observed
            // after task dispatch, rather than the preceding settling read.
            observed = this.readReferencePose(now);
            if (!observed) return;
        }
        if (now - this.taskAt >= referenceWalkBudget) {
            this.stop('time_budget', now);
            return;
        }
        if (now >= this.nextSampleAt) {
            const animation = this.game.readRemoteAnimation(this.referencePed, referenceClip);
            if (!animation || typeof animation.playing !== 'boolean' ||
                (animation.playing ? typeof animation.time !== 'number' || !Number.isFinite(animation.time) ||
                    animation.time < 0 || animation.time > 1 : animation.time !== null))
                throw new Error('Native gait reference ped returned invalid animation evidence');
            // The first actual association may already be partway through its
            // clip. Record its readback without assuming or injecting phase zero.
            this.log({ event: 'native_gait_reference_sample', ...this.evidence(now),
                clip: referenceClip, playing: animation.playing, time: animation.time,
                observed_transform: copyTransform(observed) });
            ++this.samples;
            this.nextSampleAt = now + referenceSampleInterval;
            if (this.samples >= referenceMaxSamples) {
                this.stop('sample_budget', now);
                return;
            }
        }
        if (now - this.taskAt >= 2000 && Math.hypot(observed.x - this.target.x, observed.y - this.target.y) <= 0.5)
            this.stop('arrived', now);
    }
}

const inputReferenceInterval = 100;
const inputReferenceBudget = 30000;
const inputReferenceMaxSamples = 300;
const inputReferenceMask = 0xfffff;
const inputReferenceUnknownFields = ['m_bChatIndicated', 'm_bPedWalk', 'm_bVehicleMouseLook', 'm_bRadioTrackSkip'];
const validControlValue = value => typeof value === 'number' && Number.isInteger(value) && value >= -32768 && value <= 32767;
const finiteInputVector = value => value && ['x', 'y', 'z'].every(name => typeof value[name] === 'number' && Number.isFinite(value[name]));

// Observe the local player without transport, owned NPCs, or input injection.
// Raw DE pad values pass through MTA's canonical C++ state before logging;
// unbound logical flags and velocity units must remain explicitly unknown.
export class NativeInputReferenceController {
    constructor(game, bridge, { log = () => {}, stopReason = 'stop_key' } = {}) {
        this.game = game;
        this.bridge = bridge;
        this.log = log;
        this.stopReason = stopReason;
        this.running = true;
        this.localPed = null;
        this.startedAt = null;
        this.lastTickAt = null;
        this.nextSampleAt = null;
        this.referenceId = null;
        this.previous = null;
        this.previousAt = null;
        this.samples = 0;
        this.finished = false;
    }

    evidence(now) {
        return { reference_id: this.referenceId, ped: this.localPed, at_ms: now,
            elapsed_ms: this.startedAt === null || now === null ? null : now - this.startedAt };
    }

    reset(reason, now) {
        this.bridge.reset();
        const currentMask = this.bridge.currentMask();
        const previousMask = this.bridge.previousMask();
        if (currentMask !== 0 || previousMask !== 0) throw new Error('Canonical controller reset did not clear published masks');
        this.log({ event: 'native_input_reference_reset', ...this.evidence(now), reason,
            current_mask: currentMask, previous_mask: previousMask });
    }

    stop(reason = 'requested', now = this.lastTickAt, error = null) {
        if (!this.running) return;
        this.running = false;
        const finishedAt = Number.isSafeInteger(now) && (this.lastTickAt === null || now >= this.lastTickAt) ? now : this.lastTickAt;
        let failure = null;
        if (!this.finished && this.startedAt !== null) {
            this.finished = true;
            try {
                this.log({ event: 'native_input_reference_finished', ...this.evidence(finishedAt), reason,
                    outcome: 'observation_only', samples: this.samples, ...(error ? { error: String(error) } : {}) });
            } catch (caught) { failure = caught; }
        }
        // Canonical snapshots are the only owned resource. Always clear them,
        // even if observation or logging failed, while preserving its first error.
        try { this.reset('cleanup', finishedAt); } catch (caught) {
            failure = failure || caught;
            try { this.log({ event: 'cleanup_failed', ...this.evidence(finishedAt), error: String(caught) }); } catch (_) {}
        }
        this.previous = null;
        this.previousAt = null;
        try {
            this.log({ event: 'native_input_reference_stopped', ...this.evidence(finishedAt), reason,
                samples: this.samples, network_active: false, input_writes: false });
        } catch (caught) { failure = failure || caught; }
        if (failure) throw failure;
    }

    tick(now) {
        if (!this.running) return;
        try { this.advance(now); } catch (error) {
            try { this.stop('error', now, error); } catch (_) {}
            throw error;
        }
    }

    validLocal(now) {
        const ready = this.game.isReady();
        if (typeof ready !== 'boolean') throw new Error('Native input reference returned invalid readiness');
        if (!ready) { this.stop('loading', now); return null; }
        const local = this.game.localPed();
        if (!validPedHandle(local) || !this.game.doesPedExist(local)) { this.stop('local_ped_missing', now); return null; }
        if (this.localPed !== null && local !== this.localPed) { this.stop('local_ped_changed', now); return null; }
        return local;
    }

    advance(now) {
        if (!Number.isSafeInteger(now)) throw new Error('Native input reference returned invalid time');
        if (this.lastTickAt !== null && now < this.lastTickAt) { this.stop('clock_reversed', now); return; }
        if (now < 0) throw new Error('Native input reference returned invalid time');
        if (this.lastTickAt !== null && now - this.lastTickAt > 250) { this.stop('render_gap', now); return; }
        this.lastTickAt = now;
        if (this.game.stopRequested()) { this.stop(this.stopReason, now); return; }
        const local = this.validLocal(now);
        if (local === null) return;
        if (this.startedAt === null) {
            this.localPed = local;
            this.startedAt = now;
            this.referenceId = 'input-' + now + '-' + local;
            this.nextSampleAt = now;
            this.reset('start', now);
            this.log({ event: 'native_input_reference_started', ...this.evidence(now),
                sample_interval_ms: inputReferenceInterval, budget_ms: inputReferenceBudget,
                max_samples: inputReferenceMaxSamples, known_field_count: 20,
                unknown_fields: inputReferenceUnknownFields.slice(), velocity_units: null,
                network_active: false, input_writes: false, remote_peds_created: false });
        }
        if (now - this.startedAt >= inputReferenceBudget) { this.stop('time_budget', now); return; }
        if (now < this.nextSampleAt) return;
        // Timely render ticks can straddle the sampling deadline. Bound the
        // actual observation interval too, so a cached early tick cannot turn
        // a later sample into a phase/control gap the strict reader rejects.
        if (this.previousAt !== null && now - this.previousAt > 250) { this.stop('render_gap', now); return; }

        const raw = [];
        for (let index = 0; index < 20; ++index) {
            const value = this.game.readPadState(index);
            if (!validControlValue(value)) throw new Error('Native input reference returned invalid signed control at index ' + index);
            raw.push(value);
        }
        const velocity = this.game.readVelocity(local);
        if (!finiteInputVector(velocity)) throw new Error('Native input reference returned invalid velocity');
        const position = this.game.readTransform(local);
        if (!finiteInputVector(position) || typeof position.heading !== 'number' || !Number.isFinite(position.heading))
            throw new Error('Native input reference returned invalid transform');
        // Avoid publishing a sample gathered across a load or local-ped change.
        if (this.validLocal(now) === null) return;

        if (this.bridge.begin() !== 1) throw new Error('Canonical controller transaction begin failed');
        for (let index = 0; index < raw.length; ++index)
            if (this.bridge.set(index, raw[index]) !== 1) throw new Error('Canonical controller transaction set failed');
        if (this.bridge.commit() !== 1) throw new Error('Canonical controller transaction commit failed');
        const currentMask = this.bridge.currentMask(), previousMask = this.bridge.previousMask();
        if (currentMask !== inputReferenceMask || previousMask !== (this.previous === null ? 0 : inputReferenceMask))
            throw new Error('Canonical controller returned invalid sample masks');
        const current = [], previous = this.previous === null ? null : [];
        for (let index = 0; index < 20; ++index) {
            const value = this.bridge.read(0, index);
            if (!validControlValue(value) || value !== raw[index]) throw new Error('Canonical controller current readback differs at index ' + index);
            current.push(value);
            if (previous !== null) {
                const old = this.bridge.read(1, index);
                if (!validControlValue(old) || old !== this.previous[index]) throw new Error('Canonical controller previous readback differs at index ' + index);
                previous.push(old);
            }
        }
        this.log({ event: 'local_controller_sample', ...this.evidence(now), current, previous,
            current_mask: currentMask, previous_mask: previousMask, previous_source: 'previous_probe_sample',
            previous_at_ms: this.previousAt, unknown_fields: inputReferenceUnknownFields.slice(),
            velocity_raw: { x: velocity.x, y: velocity.y, z: velocity.z }, velocity_units: null,
            position: copyTransform(position) });
        this.previous = current.slice();
        this.previousAt = now;
        ++this.samples;
        this.nextSampleAt = now + inputReferenceInterval;
        if (this.samples >= inputReferenceMaxSamples) this.stop('sample_budget', now);
    }
}
