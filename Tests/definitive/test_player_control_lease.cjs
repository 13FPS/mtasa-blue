// SPDX-License-Identifier: GPL-3.0-or-later
// Native booleans below model readback/state transitions only. This suite never
// loads GTA or establishes that its control opcode works on an installed build.
const assert = require('node:assert/strict');

module.exports = function testPlayerControlLease(PlayerControlLease) {
    let tests = 0;
    const test = (name, body) => {
        try { body(); ++tests; } catch (error) { error.message = name + ': ' + error.message; throw error; }
    };
    const fixture = (options = {}) => {
        const calls = [], events = [];
        const native = { ready: true, ped: 10, exists: true, foot: true, focused: true, stop: false, enabled: true, ...options.native };
        const state = options.state || [17, 1, 0, 0, 0, 0, 0, 0];
        let cache = null;
        const game = {
            isReady: () => native.ready, localPed: () => native.ped,
            doesPedExist: () => native.exists, isOnFoot: () => native.foot,
            isFocused: () => native.focused, stopRequested: () => native.stop,
            isControlOn: () => { calls.push(['get', native.enabled]); return native.enabled; },
            setControl: enabled => { calls.push(['set', enabled, native.ped]); native.enabled = enabled; },
            ...options.game
        };
        const bridge = {
            capture: () => { cache = state.slice(); return 1; }, read: field => cache[field],
            ack: generation => { calls.push(['ack', generation]); state[2] = state[3] = 1; return 1; },
            heartbeat: generation => { calls.push(['heartbeat', generation]); return 1; },
            revoke: generation => { calls.push(['revoke', generation]); state[4] = 1; state[3] = 0; return 1; },
            restored: generation => { calls.push(['restored', generation]); assert.equal(state[4], 1); assert.equal(state[5], 1); state[6] = 1; return 1; },
            ...options.bridge
        };
        const owner = new PlayerControlLease(game, bridge, { log: options.log || (event => events.push(event)) });
        const detach = () => { state[4] = state[5] = 1; state[3] = 0; };
        const finish = at => { detach(); owner.advance(at); owner.advance(at + 50); };
        return { owner, native, state, calls, events, game, bridge, detach, finish };
    };
    const writes = f => f.calls.filter(call => call[0] === 'set');
    const called = (f, name) => f.calls.filter(call => call[0] === name);

    test('disable is verified before Ack; heartbeat follows a second false readback', () => {
        const f = fixture(); f.owner.advance(0);
        assert.deepEqual(f.calls, [['get', true], ['set', false, 10], ['get', false], ['ack', 17], ['get', false], ['heartbeat', 17]]);
        assert.equal(f.owner.snapshot().acquired, true); assert.equal(f.native.enabled, false);
        assert.equal(f.owner.snapshot().native_restored_verified, false);
        assert.equal(f.events.find(event => event.action === 'native_disabled').native_disabled, false);
    });
    test('no native player/control reads or writes before a worker request', () => {
        const f = fixture({ state: Array(8).fill(0), game: { isReady: () => { throw Error('premature native'); } } });
        f.owner.advance(0); f.owner.advance(50);
        assert.deepEqual(f.calls, []); assert.equal(f.owner.finishWithoutRequest(), true);
        assert.equal(f.owner.snapshot().native_initial, null); assert.equal(f.owner.snapshot().no_mutation, true);
    });
    test('worker revoke waits for input detachment then restores while GPU/Core may remain pending', () => {
        const f = fixture(); f.owner.advance(0); f.state[4] = 1; f.state[3] = 0; f.owner.advance(50);
        assert.deepEqual(writes(f), [['set', false, 10]]); assert.equal(called(f, 'restored').length, 0);
        f.finish(100);
        assert.deepEqual(writes(f), [['set', false, 10], ['set', true, 10]]);
        assert.equal(f.owner.snapshot().native_restored_verified, true); assert.equal(f.owner.completed, true);
        assert.equal(f.owner.snapshot().full_mta_ready, false); assert.equal(f.owner.snapshot().gameplay_ready, false);
        assert.equal(f.owner.snapshot().camera_capture_verified, false);
        f.owner.advance(200); assert.equal(writes(f).length, 2);
    });
    test('initially disabled controls refuse admission and retire without mutation or native restoration claims', () => {
        const f = fixture({ native: { enabled: false } }); f.owner.advance(0);
        assert.deepEqual(writes(f), []); assert.equal(called(f, 'ack').length, 0);
        f.finish(50);
        const state = f.owner.snapshot();
        assert.equal(state.native_initial, false); assert.equal(state.acquired, false);
        assert.equal(state.native_restored_verified, false); assert.equal(state.restored_acknowledged, true);
        assert.equal(state.no_mutation, true); assert.equal(state.completed, true);
        assert.ok(f.events.some(event => event.action === 'no_mutation_acknowledged'));
    });
    for (const property of ['ready', 'exists', 'foot', 'focused']) test('unavailable ' + property + ' refuses mutation but retires after detach', () => {
        const f = fixture({ native: { [property]: false } }); f.owner.advance(0); f.finish(50);
        assert.deepEqual(writes(f), []); assert.equal(called(f, 'ack').length, 0);
        assert.equal(f.owner.completed, true); assert.equal(f.owner.snapshot().native_restored_verified, false);
    });
    for (const value of [0, 1, undefined, null, 'true']) test('nonboolean native result rejects admission: ' + String(value), () => {
        const f = fixture({ native: { enabled: value } }); f.owner.advance(0); f.finish(50);
        assert.deepEqual(writes(f), []); assert.equal(called(f, 'ack').length, 0); assert.equal(f.owner.completed, true);
    });
    test('setter throwing after a successful disable still retains restoration obligation before Ack', () => {
        const f = fixture();
        f.game.setControl = value => { f.calls.push(['set', value, f.native.ped]); f.native.enabled = value; if (!value) throw Error('return binding failed'); };
        f.owner.advance(0); assert.equal(f.native.enabled, false); assert.equal(called(f, 'ack').length, 0);
        assert.equal(f.owner.snapshot().mutation_attempted, true); assert.equal(f.owner.snapshot().native_disabled_verified, false);
        f.finish(50); assert.equal(f.native.enabled, true); assert.equal(f.owner.completed, true);
        assert.deepEqual(writes(f), [['set', false, 10], ['set', true, 10]]);
    });
    test('failed disable readback records attempted mutation and verifies original state on cleanup', () => {
        const f = fixture(); f.game.setControl = value => f.calls.push(['set', value, 10]);
        f.owner.advance(0); f.finish(50);
        assert.equal(called(f, 'ack').length, 0); assert.equal(f.owner.snapshot().mutation_attempted, true);
        assert.equal(f.owner.snapshot().native_disabled_verified, false); assert.equal(f.owner.snapshot().native_restored_verified, true);
        assert.deepEqual(writes(f), [['set', false, 10]]);
    });
    for (const method of ['ack', 'heartbeat']) test('rejected ' + method + ' never discards the native restoration obligation', () => {
        const f = fixture({ bridge: { [method]: () => 0 } }); f.owner.advance(0); f.finish(50);
        assert.equal(f.native.enabled, true); assert.equal(f.owner.completed, true);
        assert.equal(f.owner.snapshot().native_restored_verified, true);
    });
    for (const mutation of ['focus', 'stop', 'identity', 'ready', 'foot']) test('loss of ' + mutation + ' revokes and retains original identity until safe restoration', () => {
        const f = fixture(); f.owner.advance(0);
        if (mutation === 'focus') f.native.focused = false;
        if (mutation === 'stop') f.native.stop = true;
        if (mutation === 'identity') f.native.ped = 11;
        if (mutation === 'ready') f.native.ready = false;
        if (mutation === 'foot') f.native.foot = false;
        f.owner.advance(50); f.detach(); f.owner.advance(100);
        if (['identity', 'ready'].includes(mutation)) {
            assert.deepEqual(writes(f), [['set', false, 10]]); assert.equal(f.owner.completed, false);
            assert.equal(called(f, 'restored').length, 0);
        }
        f.native.ped = 10; f.native.ready = f.native.foot = true;
        f.owner.advance(150); f.owner.advance(200);
        assert.equal(f.owner.completed, true); assert.equal(f.native.enabled, true);
        assert.ok(writes(f).every(call => call[2] === 10));
    });
    test('same original player may restore from a vehicle while on-foot admission remains required', () => {
        const f = fixture(); f.owner.advance(0); f.native.foot = false; f.owner.advance(50);
        assert.equal(called(f, 'heartbeat').length, 1); f.finish(100);
        assert.equal(f.native.foot, false); assert.equal(f.native.enabled, true); assert.equal(f.owner.completed, true);
        assert.equal(f.owner.snapshot().native_restored_verified, true);
    });
    test('external control enable revokes without repeatedly forcing disable', () => {
        const f = fixture(); f.owner.advance(0); f.native.enabled = true; f.owner.advance(50); f.finish(100);
        assert.deepEqual(writes(f), [['set', false, 10]]); assert.equal(f.owner.completed, true);
    });
    test('tick gap over expiry revokes before renewing a stale lease', () => {
        const f = fixture(); f.owner.advance(0); f.owner.advance(1001);
        assert.equal(f.owner.stopReason, 'heartbeat_gap'); assert.equal(called(f, 'heartbeat').length, 1);
        f.finish(1051); assert.equal(f.native.enabled, true);
    });
    test('ordinary scheduling hitch below expiry is not a false cancellation', () => {
        const f = fixture(); f.owner.advance(0); f.owner.advance(200);
        assert.equal(f.owner.stopReason, null); assert.equal(called(f, 'heartbeat').length, 2);
    });
    test('independent active deadline gives Core its display plus setup margin then revokes', () => {
        const f = fixture(); for (let at = 0; at <= 35000; at += 50) f.owner.advance(at);
        assert.equal(f.owner.stopReason, 'active_deadline'); assert.equal(called(f, 'heartbeat').length, 700);
        f.finish(35050); assert.equal(f.owner.completed, true);
    });
    for (const now of [-1, NaN, 1.5, 0]) test('invalid/regressed clock retains restoration: ' + String(now), () => {
        const f = fixture(); f.owner.advance(50); f.owner.advance(now); f.finish(100);
        assert.equal(f.owner.completed, true); assert.equal(f.native.enabled, true); assert.ok(f.owner.error.includes('clock'));
    });
    test('worker acquisition expiry/refusal avoids native mutation', () => {
        const f = fixture({ state: [17, 1, 0, 0, 1, 0, 0, 0] }); f.owner.advance(5001); f.finish(5051);
        assert.deepEqual(writes(f), []); assert.equal(f.owner.completed, true);
    });
    test('expired acknowledged heartbeat is not renewed', () => {
        const f = fixture(); f.owner.advance(0); f.state[3] = 0; f.owner.advance(50); f.finish(100);
        assert.equal(called(f, 'heartbeat').length, 1); assert.equal(f.owner.completed, true);
    });
    test('restoration failures retry with owner retained; acknowledgement follows verified readback', () => {
        const f = fixture(); f.owner.advance(0); f.detach();
        let failures = 2;
        f.game.setControl = value => { f.calls.push(['set', value, 10]); if (failures-- > 0) throw Error('temporarily unavailable'); f.native.enabled = value; };
        f.owner.advance(50); f.owner.advance(100);
        assert.equal(f.owner.completed, false); assert.equal(called(f, 'restored').length, 0);
        f.owner.advance(150); f.owner.advance(200);
        assert.equal(f.owner.completed, true); assert.equal(f.owner.snapshot().restore_attempts, 3); assert.equal(f.native.enabled, true);
    });
    test('rejected restoration acknowledgement retries without repeated native writes', () => {
        const f = fixture(); f.owner.advance(0); f.detach(); const accepted = f.bridge.restored;
        let attempts = 0; f.bridge.restored = generation => ++attempts < 3 ? 0 : accepted(generation);
        for (let at = 50; at <= 200; at += 50) f.owner.advance(at);
        assert.equal(attempts, 3); assert.equal(writes(f).length, 2); assert.equal(f.owner.completed, true);
    });
    test('rejected restoration Ack cannot reuse saved proof after controls revert', () => {
        const f = fixture(); f.owner.advance(0); f.detach(); const accepted = f.bridge.restored;
        let attempts = 0; f.bridge.restored = generation => {
            ++attempts; assert.equal(f.native.enabled, true);
            return attempts === 1 ? 0 : accepted(generation);
        };
        f.owner.advance(50);
        assert.equal(f.owner.snapshot().native_restored_verified, true);
        assert.equal(f.owner.snapshot().restored_acknowledged, false);
        f.native.enabled = false; f.owner.advance(100);
        assert.equal(attempts, 2); assert.equal(f.native.enabled, true);
        assert.deepEqual(writes(f), [['set', false, 10], ['set', true, 10], ['set', true, 10]]);
        f.owner.advance(150); assert.equal(f.owner.completed, true);
    });
    test('replacement identity between rejected restoration Ack attempts denies both write and Ack', () => {
        const f = fixture(); f.owner.advance(0); f.detach(); const accepted = f.bridge.restored;
        let attempts = 0; f.bridge.restored = generation => ++attempts === 1 ? 0 : accepted(generation);
        f.owner.advance(50); f.native.ped = 11; f.native.enabled = false;
        f.owner.advance(100); f.owner.advance(150);
        assert.equal(attempts, 1); assert.equal(writes(f).length, 2);
        assert.equal(f.owner.snapshot().native_restored_verified, false);
        assert.equal(f.owner.completed, false); assert.equal(f.native.enabled, false);
        f.native.ped = 10; f.owner.advance(200); f.owner.advance(250);
        assert.equal(attempts, 2); assert.equal(f.owner.completed, true); assert.equal(f.native.enabled, true);
        assert.ok(writes(f).every(call => call[2] === 10));
    });
    test('rejected revoke retries before detachment and never restores early', () => {
        const f = fixture(); f.owner.advance(0); f.native.stop = true; const accepted = f.bridge.revoke;
        let attempts = 0; f.bridge.revoke = generation => ++attempts < 3 ? 0 : accepted(generation);
        f.owner.advance(50); f.owner.advance(100); f.owner.advance(150);
        assert.equal(attempts, 3); assert.equal(writes(f).length, 1); assert.equal(f.owner.completed, false);
        f.finish(200); assert.equal(f.owner.completed, true);
    });
    test('generation change never authorizes restoration into another lease; original token can recover', () => {
        const f = fixture(); f.owner.advance(0); f.state[0] = 18; f.detach(); f.owner.advance(50);
        assert.equal(writes(f).length, 1); assert.equal(called(f, 'restored').length, 0);
        assert.equal(f.owner.generation, 17); f.state[0] = 17; f.owner.advance(100); f.owner.advance(150);
        assert.equal(f.owner.completed, true); assert.ok(called(f, 'restored').every(call => call[1] === 17));
    });
    for (const shape of [[17, 1, 0, 0, 0, 0, 0, 2], [17, 1, 0, 0, 0, 0, 1, 0], [0, 1, 0, 0, 0, 0, 0, 0]])
        test('malformed control snapshot cannot authorize native mutation ' + JSON.stringify(shape), () => {
            const f = fixture({ state: shape }); f.owner.advance(0);
            assert.deepEqual(writes(f), []); assert.equal(f.owner.generation, 0); assert.ok(f.owner.error.includes('Invalid'));
        });
    test('snapshot capture failure while acquired revokes and restoration resumes after capture recovers', () => {
        const f = fixture(); f.owner.advance(0); const capture = f.bridge.capture; f.bridge.capture = () => 0;
        f.owner.advance(50); assert.equal(called(f, 'revoke').length, 1); assert.equal(writes(f).length, 1);
        f.bridge.capture = capture; f.finish(100); assert.equal(f.owner.completed, true);
    });
    test('throwing log sink cannot abandon a native change or admit GUI input', () => {
        const f = fixture(); f.owner.advance(0); f.owner.log = () => { throw Error('log unavailable'); };
        f.detach(); f.owner.advance(50); f.owner.advance(100);
        assert.equal(f.native.enabled, true); assert.equal(f.owner.completed, true);
    });
    return tests;
};
