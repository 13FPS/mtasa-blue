// SPDX-License-Identifier: GPL-3.0-or-later
const provenance = 'mta_game_de_scoped_local_movement';
const metricNames = ['bound', 'active', 'callbackCount', 'scopedSteps', 'routedQueries', 'neutralQueries',
    'restoredSteps', 'restoreFailures', 'foreignThreadCallbacks', 'nestedCallbacks', 'failureReason', 'lastMoveState'];
const phases = [
    { until: 3000, code: 0, name: 'neutral' },
    { until: 7000, code: 1, name: 'mta_walk' },
    { until: 9000, code: 0, name: 'pause' },
    { until: 13000, code: 2, name: 'mta_forwards' },
    { until: 15000, code: 0, name: 'pause' },
    { until: 19000, code: 3, name: 'mta_sprint' },
    { until: 22000, code: 0, name: 'final_neutral' }
];
const finiteVector = value => value && ['x', 'y', 'z'].every(key => typeof value[key] === 'number' && Number.isFinite(value[key]));

// The script schedules MTA action frames and observes results. The real Game DE
// module owns native hooks, per-callback writes and restoration; no script timer
// writes engine memory or forces an animation/position.
export class MtaNativeMovementTrialController {
    constructor(game, bridge, { log = () => {}, stopReason = 'Ctrl+Shift+End' } = {}) {
        this.game = game; this.bridge = bridge; this.log = log; this.stopReason = stopReason;
        this.running = true; this.startedAt = null; this.waitStartedAt = null; this.lastTickAt = null;
        this.lastSampleAt = null; this.ped = null; this.phaseIndex = -1; this.samples = 0; this.referenceId = null;
    }

    evidence(now) {
        return { reference_id: this.referenceId, ped: this.ped, at_ms: now,
            elapsed_ms: this.startedAt === null || now === null ? null : now - this.startedAt,
            provenance, adapter_abi: 5, scope: 'local_on_foot_callback', input_writes: true,
            network_active: false, remote_peds_created: false, velocity_units: null };
    }

    readState() {
        const state = {};
        metricNames.forEach((name, index) => {
            const value = this.bridge.read(index);
            if (!Number.isInteger(value) || value < 0 || value > 0xffffffff) throw new Error('Invalid movement metric ' + name);
            state[name] = value;
        });
        if (state.bound > 1 || state.active > 1 || state.failureReason > 17 || state.lastMoveState > 1000 ||
            state.restoredSteps > state.scopedSteps) throw new Error('Invalid movement state');
        return state;
    }

    stop(reason = 'requested', now = this.lastTickAt, error = null) {
        if (!this.running) return;
        this.running = false;
        const at = Number.isSafeInteger(now) && now >= 0 && (this.lastTickAt === null || now >= this.lastTickAt) ? now : this.lastTickAt;
        let failure = null, state = null;
        // End is attempted before terminal logging, even when observations or
        // the logger failed. Its metrics are captured before module teardown.
        try {
            if (this.bridge.end() !== 1) throw new Error('MTA movement cleanup refused; native module retained');
            if (this.startedAt !== null) {
                state = this.readState();
                if (state.bound || state.active || state.restoreFailures || state.restoredSteps !== state.scopedSteps)
                    throw new Error('MTA movement cleanup did not restore every applied scope');
            }
        } catch (caught) { failure = caught; }
        try {
            this.log({ event: 'native_movement_trial_finished', ...this.evidence(at), reason, samples: this.samples,
                cleanup_passed: failure === null, state, ...(error ? { error: String(error) } : {}),
                ...(failure ? { cleanup_error: String(failure) } : {}) });
        } catch (caught) { failure ||= caught; }
        if (failure) throw failure;
    }

    tick(now) {
        if (!this.running) return;
        try { this.advance(now); } catch (error) {
            try { this.stop('error', now, error); } catch (_) {}
            throw error;
        }
    }

    advance(now) {
        if (!Number.isSafeInteger(now) || now < 0) throw new Error('Invalid movement trial clock');
        if (this.lastTickAt !== null && (now < this.lastTickAt || now - this.lastTickAt > 2000)) {
            this.stop(now < this.lastTickAt ? 'clock_reversed' : 'render_gap', now); return;
        }
        this.lastTickAt = now;
        if (this.waitStartedAt === null) this.waitStartedAt = now;
        if (this.game.stopRequested()) { this.stop(this.stopReason, now); return; }
        if (!this.game.isReady()) {
            if (this.startedAt !== null || now - this.waitStartedAt >= 10000) this.stop('loading', now);
            return;
        }
        const ped = this.game.localPed();
        if (!Number.isSafeInteger(ped) || ped <= 0 || ped > 0x7fffffff || !this.game.doesPedExist(ped) ||
            (this.ped !== null && ped !== this.ped)) { this.stop('local_ped_changed', now); return; }
        if (this.startedAt === null) {
            this.ped = ped; this.referenceId = 'mta-movement-' + now;
            if (this.bridge.begin() !== 1) throw new Error('MTA scoped movement binding failed');
            this.startedAt = now;
            this.log({ event: 'native_movement_trial_started', ...this.evidence(now), duration_ms: 22000,
                phases: phases.map(value => ({ until_ms: value.until, phase: value.name, code: value.code })) });
        }
        const elapsed = now - this.startedAt;
        const index = phases.findIndex(value => elapsed < value.until);
        if (index < 0) { this.stop('time_budget', now); return; }
        const phase = phases[index];
        if (this.bridge.submit(phase.code) !== 1) throw new Error('MTA action submission rejected');
        if (index !== this.phaseIndex) {
            this.phaseIndex = index;
            this.log({ event: 'native_movement_trial_phase', ...this.evidence(now), phase: phase.name, code: phase.code });
        }
        if (this.lastSampleAt !== null && now - this.lastSampleAt < 125) return;
        if (this.bridge.capture() !== 1) throw new Error('MTA movement state capture failed');
        const state = this.readState();
        if (!state.bound || !state.active || state.failureReason || state.restoreFailures || state.nestedCallbacks)
            throw new Error('Native movement scope stopped: ' + state.failureReason);
        if (elapsed >= 2000 && state.scopedSteps === 0) throw new Error('No native player control scope observed');
        if (elapsed >= 4000 && state.routedQueries === 0) throw new Error('No native provider route observed');
        const pose = this.game.readTransform(ped), velocity = this.game.readVelocity(ped);
        if (!finiteVector(pose) || !Number.isFinite(pose.heading) || !finiteVector(velocity)) throw new Error('Invalid movement observation');
        this.log({ event: 'native_movement_trial_sample', ...this.evidence(now), phase: phase.name, code: phase.code,
            position: { x: pose.x, y: pose.y, z: pose.z }, heading: pose.heading, raw_velocity: velocity, state });
        ++this.samples; this.lastSampleAt = now;
    }
}
