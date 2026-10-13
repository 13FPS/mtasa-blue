// SPDX-License-Identifier: GPL-3.0-or-later
const provenance = 'mta_game_de_native_pad_reader';
const finiteVector = value => value && ['x', 'y', 'z'].every(key => typeof value[key] === 'number' && Number.isFinite(value[key]));
const validPed = value => Number.isSafeInteger(value) && value > 0 && value <= 0x7fffffff;

// The public SDK pad interpretation failed the first live trial. Retain those
// bytes for comparison and observe the provider's source table separately;
// neither representation establishes physical keys or previous-frame semantics.
export class MtaNativePadReferenceController {
    constructor(game, bridge, { log = () => {}, stopReason = 'Ctrl+Shift+End', durationMs = 60000,
        sampleIntervalMs = 125, maxSamples = 600, renderGapMs = 2000, readinessBudgetMs = 10000 } = {}) {
        for (const [name, value, low, high] of [
            ['durationMs', durationMs, 1, 60000], ['sampleIntervalMs', sampleIntervalMs, 100, 2000],
            ['maxSamples', maxSamples, 1, 600], ['renderGapMs', renderGapMs, 1, 2000], ['readinessBudgetMs', readinessBudgetMs, 1, 10000]
        ]) if (!Number.isSafeInteger(value) || value < low || value > high) throw new Error('Invalid native pad option ' + name);
        this.game = game; this.bridge = bridge; this.log = log; this.stopReason = stopReason;
        this.durationMs = durationMs; this.sampleIntervalMs = sampleIntervalMs; this.maxSamples = maxSamples;
        this.renderGapMs = renderGapMs; this.readinessBudgetMs = readinessBudgetMs;
        this.running = true; this.localPed = null; this.activatedAt = null; this.startedAt = null;
        this.lastTickAt = null; this.lastSampleAt = null; this.nextSampleAt = null; this.referenceId = null; this.samples = 0;
    }

    evidence(now) {
        return { reference_id: this.referenceId, ped: this.localPed, at_ms: now,
            elapsed_ms: this.startedAt === null || now === null ? null : now - this.startedAt,
            provenance, field_count: 24, pad_count: 2, previous_state: 'engine_OldState', previous_source: 'native_OldState',
            capture_version: 2, device_state_source: 'native_provider_source_states', device_state_count: 127, controller_layout_verified: false,
            network_active: false, input_writes: false, remote_peds_created: false, velocity_units: null };
    }

    reset(reason, now) {
        this.bridge.reset();
        this.log({ event: 'native_pad_reference_reset', ...this.evidence(now), reason });
    }

    stop(reason = 'requested', now = this.lastTickAt, error = null) {
        if (!this.running) return;
        this.running = false;
        const at = Number.isSafeInteger(now) && now >= 0 && (this.lastTickAt === null || now >= this.lastTickAt) ? now : this.lastTickAt;
        let failure = null;
        try {
            this.log({ event: 'native_pad_reference_finished', ...this.evidence(at), reason, outcome: 'observation_only', samples: this.samples,
                ...(error ? { error: String(error) } : {}) });
        } catch (caught) { failure = caught; }
        // Cleanup must run even after malformed native data or a logger failure.
        // Preserve the first failure while still attempting the terminal record.
        try { this.reset('cleanup', at); } catch (caught) {
            failure ||= caught;
            try { this.log({ event: 'cleanup_failed', ...this.evidence(at), error: String(caught) }); } catch (_) {}
        }
        try { this.log({ event: 'native_pad_reference_stopped', ...this.evidence(at), reason, samples: this.samples }); }
        catch (caught) { failure ||= caught; }
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
        if (typeof ready !== 'boolean') throw new Error('Invalid native pad readiness');
        if (!ready) {
            if (this.startedAt !== null) this.stop('loading', now);
            return null;
        }
        const ped = this.game.localPed();
        if (!validPed(ped) || !this.game.doesPedExist(ped)) { this.stop('local_ped_missing', now); return null; }
        if (this.localPed !== null && ped !== this.localPed) { this.stop('local_ped_changed', now); return null; }
        return ped;
    }

    advance(now) {
        if (!Number.isSafeInteger(now)) throw new Error('Invalid native pad clock');
        if (this.lastTickAt !== null && now < this.lastTickAt) { this.stop('clock_reversed', now); return; }
        if (now < 0) throw new Error('Invalid native pad clock');
        if (this.lastTickAt !== null && now - this.lastTickAt > this.renderGapMs) { this.stop('render_gap', now); return; }
        this.lastTickAt = now;
        if (this.activatedAt === null) { this.activatedAt = now; this.referenceId = 'native-pad-' + now; }
        if (this.game.stopRequested()) { this.stop(this.stopReason, now); return; }
        if (this.startedAt === null && now - this.activatedAt >= this.readinessBudgetMs) { this.stop('readiness_timeout', now); return; }
        const ped = this.validLocal(now);
        if (ped === null) return;
        if (this.startedAt === null) {
            this.localPed = ped; this.startedAt = now; this.nextSampleAt = now;
            this.reset('start', now);
            if (this.bridge.bind() !== 1) throw new Error('MTA native pad binding failed');
            this.log({ event: 'native_pad_reference_started', ...this.evidence(now), duration_ms: this.durationMs,
                sample_interval_ms: this.sampleIntervalMs, max_samples: this.maxSamples, render_gap_ms: this.renderGapMs,
                readiness_budget_ms: this.readinessBudgetMs });
        }
        if (now - this.startedAt >= this.durationMs) { this.stop('time_budget', now); return; }
        if (now < this.nextSampleAt) return;
        if (this.lastSampleAt !== null && now - this.lastSampleAt > this.renderGapMs) { this.stop('render_gap', now); return; }
        // One bridge capture fixes candidate pad bytes and provider source
        // states before the FFI getters; cached ticks cannot add fresh samples.
        if (this.bridge.capture() !== 1) throw new Error('MTA native pad capture failed');
        const pads = [];
        for (let pad = 0; pad < 2; ++pad) {
            const states = [];
            for (let which = 0; which < 2; ++which) {
                const values = [];
                for (let index = 0; index < 24; ++index) {
                    const bits = this.bridge.read(pad, which, index);
                    if (!Number.isInteger(bits) || bits < 0 || bits > 0xffffffff) throw new Error('Invalid native pad uint32 readback');
                    const value = bits | 0;
                    if (value < -32768 || value > 32767) throw new Error('Invalid native pad signed16 readback');
                    values.push(value);
                }
                states.push(values);
            }
            pads.push({ current: states[0], previous: states[1] });
        }
        const deviceStates = [];
        for (let index = 0; index < 127; ++index) {
            const bits = this.bridge.readDevice(index);
            if (!Number.isInteger(bits) || bits < 0 || bits > 0xffffffff) throw new Error('Invalid native device uint32 readback');
            deviceStates.push(bits | 0);
        }
        const velocity = this.game.readVelocity(ped);
        if (!finiteVector(velocity)) throw new Error('Invalid native pad raw velocity');
        const pose = this.game.readTransform(ped);
        if (!finiteVector(pose) || typeof pose.heading !== 'number' || !Number.isFinite(pose.heading)) throw new Error('Invalid native pad transform');
        if (this.validLocal(now) === null) return;
        this.log({ event: 'native_pad_sample', ...this.evidence(now), position: { x: pose.x, y: pose.y, z: pose.z }, heading: pose.heading,
            raw_velocity: { x: velocity.x, y: velocity.y, z: velocity.z }, pads, device_states: deviceStates });
        ++this.samples; this.lastSampleAt = now; this.nextSampleAt = now + this.sampleIntervalMs;
        if (this.samples >= this.maxSamples) this.stop('sample_budget', now);
    }
}
