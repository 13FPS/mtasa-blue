// SPDX-License-Identifier: GPL-3.0-or-later
// Model only bridge readbacks, never game input or engine state injection.
const assert = require('node:assert/strict');
const path = require('node:path');
const { pathToFileURL } = require('node:url');
let tests = 0;
function fixture(Controller, options = {}) {
    const calls = [], events = [];
    const state = { ready: true, ped: 10, exists: true, stop: false, bind: 1, capture: 1,
        velocity: { x: 0.1, y: -0.2, z: 0 }, pose: { x: 1, y: 2, z: 3, heading: 270 },
        pads: Array.from({ length: 2 }, (_, pad) => Array.from({ length: 2 }, (_, which) =>
            Array.from({ length: 24 }, (_, index) => (pad * 1000 + which * 100 + index + 1) * (which ? -1 : 1)))) };
    const call = (name, ...args) => calls.push([name, ...args]);
    const game = { stopRequested: () => state.stop, isReady: () => state.ready, localPed: () => state.ped,
        doesPedExist: () => state.exists, readVelocity: () => ({ ...state.velocity }), readTransform: () => ({ ...state.pose }) };
    for (const name of ['createPed', 'deletePed', 'movePed', 'requestModel', 'playRemoteAnimation', 'setRemoteAnimationRate', 'readPadState'])
        game[name] = () => assert.fail('read-only probe must not call ' + name);
    const bridge = { reset: () => call('reset'), bind: () => { call('bind'); return state.bind; },
        capture: () => { call('capture'); return state.capture; }, read: (pad, which, index) => { call('read', pad, which, index); return state.pads[pad][which][index] >>> 0; } };
    const controller = new Controller(game, bridge, { log: event => events.push(event), ...options });
    return { controller, calls, events, state, game, bridge, tick: now => controller.tick(now) };
}
const samples = f => f.events.filter(e => e.event === 'native_pad_sample');
const count = (f, name) => f.calls.filter(c => c[0] === name).length;
const finish = f => f.events.find(e => e.event === 'native_pad_reference_finished');
(async () => {
    const { MtaNativePadReferenceController: Controller } = await import(pathToFileURL(path.resolve(__dirname,
        '../../Client/definitive/game-adapter/native-pad-reference.mjs')).href);
    const test = (name, fn) => { fn(); ++tests; };
    test('construction is inert; first sample captures once and reads all 96 signed fields', () => {
        const f = fixture(Controller); assert.deepEqual(f.calls, []); f.tick(0);
        assert.deepEqual(f.calls.slice(0, 3).map(c => c[0]), ['reset', 'bind', 'capture']);
        assert.equal(count(f, 'read'), 96); assert.equal(count(f, 'capture'), 1);
        assert.deepEqual(samples(f)[0].pads, f.state.pads.map(p => ({ current: p[0], previous: p[1] })));
        assert.equal(samples(f)[0].previous_state, 'engine_OldState'); assert.equal(samples(f)[0].previous_source, 'native_OldState');
        assert.equal(samples(f)[0].provenance, 'mta_game_de_native_pad_reader'); assert.equal(samples(f)[0].input_writes, false);
        assert.equal(samples(f)[0].network_active, false); assert.equal(samples(f)[0].remote_peds_created, false);
        assert.deepEqual(samples(f)[0].position, { x: 1, y: 2, z: 3 }); assert.equal(samples(f)[0].heading, 270);
        assert.deepEqual(samples(f)[0].raw_velocity, f.state.velocity); assert.equal(samples(f)[0].velocity_units, null);
    });
    test('previous data is actual captured OldState rather than preceding probe sample', () => {
        const f = fixture(Controller); f.tick(1000); const prior = samples(f)[0].pads[0].current.slice();
        f.state.pads[0][1] = Array(24).fill(-300); f.tick(1125);
        assert.deepEqual(samples(f)[1].pads[0].previous, Array(24).fill(-300)); assert.notDeepEqual(samples(f)[1].pads[0].previous, prior);
    });
    test('signed extrema decode from uint32 bits without losing sign', () => {
        const f = fixture(Controller); f.state.pads[0][0][0] = -32768; f.state.pads[1][1][23] = 32767; f.tick(0);
        assert.equal(samples(f)[0].pads[0].current[0], -32768); assert.equal(samples(f)[0].pads[1].previous[23], 32767);
    });
    test('cached ticks do not recapture; timely irregular ticks have no catch-up burst', () => {
        const f = fixture(Controller); [0, 0, 50, 124, 125, 125, 200, 260].forEach(f.tick);
        assert.deepEqual(samples(f).map(e => e.at_ms), [0, 125, 260]); assert.equal(count(f, 'capture'), 3);
    });
    test('default trial stops at 30 seconds with 240 samples and cleanup', () => {
        const f = fixture(Controller); for (let t = 0; t <= 30000; t += 125) f.tick(t);
        assert.equal(samples(f).length, 240); assert.equal(finish(f).reason, 'time_budget'); assert.equal(count(f, 'reset'), 2);
    });
    test('hard sample cap prevents more than 300 captures', () => {
        const f = fixture(Controller, { sampleIntervalMs: 100 }); for (let t = 0; t <= 30100; t += 100) f.tick(t);
        assert.equal(samples(f).length, 300); assert.equal(finish(f).reason, 'sample_budget'); assert.equal(count(f, 'capture'), 300);
    });
    test('waiting for first readiness is bounded and does not bind', () => {
        const f = fixture(Controller); f.state.ready = false;
        for (let t = 0; t <= 10000; t += 1000) f.tick(t);
        assert.equal(finish(f).reason, 'readiness_timeout'); assert.equal(count(f, 'bind'), 0); assert.equal(count(f, 'reset'), 1);
    });
    test('first ready tick starts the observation clock after waiting', () => {
        const f = fixture(Controller); f.state.ready = false; f.tick(0); f.state.ready = true; f.tick(1000);
        assert.equal(samples(f)[0].elapsed_ms, 0); assert.equal(count(f, 'bind'), 1);
    });
    for (const [name, change, reason] of [
        ['loading', f => { f.state.ready = false; }, 'loading'], ['ped changed', f => { f.state.ped = 11; }, 'local_ped_changed'],
        ['ped missing', f => { f.state.exists = false; }, 'local_ped_missing'], ['cancel', f => { f.state.stop = true; }, 'Ctrl+Shift+End']
    ]) test(name + ' cleans up without another capture', () => {
        const f = fixture(Controller); f.tick(0); change(f); f.tick(125); f.tick(250);
        assert.equal(samples(f).length, 1); assert.equal(count(f, 'reset'), 2); assert.equal(finish(f).reason, reason); assert.equal(f.controller.running, false);
    });
    for (const [time, reason] of [[2001, 'render_gap'], [-1, 'clock_reversed']]) test(reason + ' stops and keeps nonnegative terminal time', () => {
        const f = fixture(Controller); f.tick(0); f.tick(time); assert.equal(samples(f).length, 1);
        assert.equal(finish(f).reason, reason); assert.ok(finish(f).elapsed_ms >= 0); assert.equal(count(f, 'reset'), 2);
    });
    test('exact two second gap remains allowed', () => { const f = fixture(Controller); f.tick(0); f.tick(2000); assert.equal(samples(f).length, 2); });
    test('published gap is bounded even when cached render ticks straddle deadline', () => {
        const f = fixture(Controller); f.tick(0); f.tick(124); f.tick(2124);
        assert.equal(samples(f).length, 1); assert.equal(finish(f).reason, 'render_gap');
    });
    for (const bad of [-1, 0x100000000, 1.5, NaN, Infinity, '1', null, undefined, 32768, 65535, 0x80000000])
        test('invalid native bits reject without a sample: ' + String(bad), () => {
            const f = fixture(Controller); f.bridge.read = () => bad; assert.throws(() => f.tick(0), /Invalid native pad/);
            assert.equal(samples(f).length, 0); assert.equal(f.controller.running, false); assert.equal(count(f, 'reset'), 2);
        });
    for (const method of ['bind', 'capture']) test('failed ' + method + ' stops and records error', () => {
        const f = fixture(Controller); f.state[method] = 0; assert.throws(() => f.tick(0), /failed/);
        assert.equal(samples(f).length, 0); assert.equal(count(f, 'reset'), 2); assert.match(finish(f).error, /failed/);
        if (method === 'bind') assert.equal(count(f, 'capture'), 0);
    });
    for (const method of ['bind', 'capture', 'read']) test(method + ' exception preserves original failure and cleans up', () => {
        const f = fixture(Controller); const error = new Error('original'); f.bridge[method] = () => { throw error; };
        assert.throws(() => f.tick(0), caught => caught === error); assert.equal(count(f, 'reset'), 2); assert.equal(samples(f).length, 0);
    });
    test('mixed ped sample is discarded after readback', () => {
        const f = fixture(Controller); f.game.readVelocity = () => { f.state.ped = 11; return f.state.velocity; }; f.tick(0);
        assert.equal(samples(f).length, 0); assert.equal(finish(f).reason, 'local_ped_changed'); assert.equal(count(f, 'reset'), 2);
    });
    for (const change of [f => { f.state.velocity.x = NaN; }, f => { f.state.pose.heading = Infinity; }]) test('invalid movement evidence stops before publication', () => {
        const f = fixture(Controller); change(f); assert.throws(() => f.tick(0), /Invalid native pad/); assert.equal(samples(f).length, 0);
    });
    test('stop before ticking is idempotent and resets without game access', () => {
        const f = fixture(Controller); f.controller.stop('cancelled'); f.controller.stop(); f.tick(0);
        assert.equal(count(f, 'reset'), 1); assert.equal(count(f, 'bind'), 0); assert.equal(f.events.filter(e => e.event === 'native_pad_reference_stopped').length, 1);
    });
    test('logger failure still attempts cleanup and preserves original error', () => {
        const error = new Error('logger'); const f = fixture(Controller, { log: () => { throw error; } });
        assert.throws(() => f.tick(0), caught => caught === error); assert.equal(count(f, 'reset'), 2); assert.equal(f.controller.running, false);
    });
    test('cleanup failure is recorded without replacing native read error', () => {
        const f = fixture(Controller); f.tick(0); const original = new Error('read failure'); f.bridge.capture = () => { throw original; };
        f.bridge.reset = () => { throw new Error('reset failure'); }; assert.throws(() => f.tick(125), caught => caught === original);
        assert.ok(f.events.some(e => e.event === 'cleanup_failed')); assert.equal(f.controller.running, false);
    });
    for (const options of [{ durationMs: 30001 }, { durationMs: Infinity }, { sampleIntervalMs: 99 }, { maxSamples: 301 }, { renderGapMs: 2001 }])
        test('unbounded options fail before native access', () => assert.throws(() => fixture(Controller, options), /Invalid native pad option/));
    console.log('Passed ' + tests + ' native pad reference checks');
})().catch(error => { console.error(error); process.exitCode = 1; });
