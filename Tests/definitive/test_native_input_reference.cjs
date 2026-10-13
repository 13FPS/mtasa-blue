// SPDX-License-Identifier: GPL-3.0-or-later
// Fake native reads and canonical bridge history let us verify isolation and
// bounded observation before asking the human to exercise the actual DE APIs.
const assert = require('node:assert/strict');
const path = require('node:path');
const { pathToFileURL } = require('node:url');

let tests = 0;
function fixture(Controller, options = {}) {
    const calls = [], events = [];
    const state = { now: 0, ready: true, ped: 10, exists: true, stop: false,
        controls: Array.from({ length: 20 }, (_, index) => index % 2 ? index * 100 : -index * 100),
        velocity: { x: -0.123, y: 0.456, z: -0.01 }, position: { x: 1, y: -2, z: 3, heading: 270 } };
    let current = null, previous = null, pending = null;
    const record = (name, ...args) => calls.push([name, ...args]);
    const game = {
        stopRequested: () => state.stop,
        isReady: () => { record('ready'); return state.ready; },
        localPed: () => { record('ped'); return state.ped; },
        doesPedExist: ped => { record('exists', ped); return state.exists; },
        readPadState: index => { record('control', index); return state.controls[index]; },
        readVelocity: ped => { record('velocity', ped); return { ...state.velocity }; },
        readTransform: ped => { record('position', ped); return { ...state.position }; }
    };
    const bridge = {
        reset: () => { record('reset'); current = previous = pending = null; },
        begin: () => { record('begin'); pending = []; return 1; },
        set: (index, value) => { record('set', index, value); pending[index] = value; return 1; },
        commit: () => { record('commit'); previous = current; current = pending.slice(); pending = null; return 1; },
        read: (which, index) => { record('bridge_read', which, index); return (which ? previous : current)?.[index] ?? 0; },
        currentMask: () => current ? 0xfffff : 0,
        previousMask: () => previous ? 0xfffff : 0
    };
    for (const name of ['createPed', 'deletePed', 'movePed', 'requestModel', 'walkReferencePed', 'playRemoteAnimation', 'setRemoteAnimationRate'])
        game[name] = () => assert.fail('input reference must never invoke ' + name);
    const controller = new Controller(game, bridge, { log: event => events.push(event), ...options });
    return { state, calls, events, game, bridge, controller,
        tick: now => { state.now = now; controller.tick(now); } };
}
const samples = f => f.events.filter(event => event.event === 'local_controller_sample');
const count = (f, name) => f.calls.filter(call => call[0] === name).length;

(async () => {
    const { NativeInputReferenceController: Controller } = await import(pathToFileURL(
        path.resolve(__dirname, '../../Client/definitive/game-adapter/controller.mjs')).href);
    assert.equal(typeof Controller, 'function', 'the bounded input controller must be exported');
    const test = (name, body) => { body(); ++tests; };

    test('observes all twenty raw signed controls through one canonical transaction and no game writes', () => {
        const f = fixture(Controller);
        assert.deepEqual(f.calls, [], 'construction does not access the game or bridge');
        f.tick(0);
        assert.deepEqual(f.calls.filter(call => call[0] === 'control').map(call => call[1]), Array.from({ length: 20 }, (_, index) => index));
        assert.deepEqual(f.calls.filter(call => call[0] === 'set').map(call => call.slice(1)), f.state.controls.map((value, index) => [index, value]));
        assert.equal(count(f, 'begin'), 1); assert.equal(count(f, 'commit'), 1);
        assert.deepEqual(samples(f)[0].current, f.state.controls);
        assert.equal(samples(f)[0].previous, null);
        assert.equal(samples(f)[0].current_mask, 0xfffff); assert.equal(samples(f)[0].previous_mask, 0);
        assert.equal(samples(f)[0].previous_source, 'previous_probe_sample');
        assert.deepEqual(samples(f)[0].velocity_raw, f.state.velocity);
        assert.equal(samples(f)[0].velocity_units, null);
        assert.deepEqual(samples(f)[0].position, f.state.position);
        assert.deepEqual(samples(f)[0].unknown_fields, ['m_bChatIndicated', 'm_bPedWalk', 'm_bVehicleMouseLook', 'm_bRadioTrackSkip']);
        assert.ok(!Object.hasOwn(f.controller, 'transport'));
        assert.equal(count(f, 'reset'), 1, 'published history starts empty');
    });
    test('previous state comes from the previous probe sample rather than a game OldState claim', () => {
        const f = fixture(Controller); f.tick(1000);
        const first = f.state.controls.slice();
        f.state.controls = first.map(value => value + 1); f.tick(1100);
        assert.deepEqual(samples(f)[1].previous, first);
        assert.deepEqual(samples(f)[1].current, f.state.controls);
        assert.equal(samples(f)[1].previous_mask, 0xfffff);
        assert.equal(samples(f)[1].previous_at_ms, 1000);
        assert.equal(samples(f)[1].at_ms, 1100); assert.equal(samples(f)[1].elapsed_ms, 100);
        assert.equal(samples(f)[1].reference_id, samples(f)[0].reference_id);
    });
    test('duplicate and cached ticks cannot multiply pad reads or bridge publications', () => {
        const f = fixture(Controller);
        for (const now of [0, 0, 25, 50, 99, 100, 100, 199, 200]) f.tick(now);
        assert.equal(samples(f).length, 3); assert.equal(count(f, 'control'), 60); assert.equal(count(f, 'commit'), 3);
    });
    test('irregular timely ticks sample once after each hundred milliseconds without catch-up bursts', () => {
        const f = fixture(Controller);
        for (const now of [0, 70, 140, 200, 250, 400, 470]) f.tick(now);
        assert.deepEqual(samples(f).map(sample => sample.at_ms), [0, 140, 250, 400]);
    });
    test('signed sixteen-bit limits are retained without assuming 128 or 255 ranges', () => {
        const f = fixture(Controller); f.state.controls[0] = -32768; f.state.controls[19] = 32767; f.tick(0);
        assert.equal(samples(f)[0].current[0], -32768); assert.equal(samples(f)[0].current[19], 32767);
    });
    test('sample count is capped at three hundred and finishing resets published bridge state', () => {
        const f = fixture(Controller);
        for (let now = 0; now < 31000 && f.controller.running; now += 100) f.tick(now);
        assert.equal(samples(f).length, 300); assert.equal(count(f, 'commit'), 300);
        assert.equal(f.events.find(event => event.event === 'native_input_reference_finished').reason, 'sample_budget');
        assert.equal(count(f, 'reset'), 2); assert.equal(f.bridge.currentMask(), 0); assert.equal(f.bridge.previousMask(), 0);
        assert.ok(samples(f).every(sample => sample.elapsed_ms < 30000));
    });
    test('elapsed observation budget stops a slower sample cadence at thirty seconds', () => {
        const f = fixture(Controller);
        for (let now = 0; now <= 30000; now += 200) f.tick(now);
        assert.equal(samples(f).length, 150);
        assert.equal(f.events.find(event => event.event === 'native_input_reference_finished').reason, 'time_budget');
        assert.equal(count(f, 'reset'), 2);
    });
    for (const [name, change, reason] of [
        ['loading', f => { f.state.ready = false; }, 'loading'],
        ['missing local ped', f => { f.state.exists = false; }, 'local_ped_missing'],
        ['replaced local ped', f => { f.state.ped = 11; }, 'local_ped_changed'],
        ['invalid local handle', f => { f.state.ped = null; }, 'local_ped_missing'],
        ['End', f => { f.state.stop = true; }, 'stop_key']
    ]) test(name + ' stops without another publication or recreation', () => {
        const f = fixture(Controller); f.tick(0); change(f); f.tick(100); f.tick(200);
        assert.equal(samples(f).length, 1); assert.equal(f.controller.running, false); assert.equal(count(f, 'reset'), 2);
        assert.equal(f.events.find(event => event.event === 'native_input_reference_finished').reason, reason);
    });
    test('stop before the first tick resets canonical history without reading CJ', () => {
        const f = fixture(Controller); f.controller.stop('cancelled');
        assert.equal(count(f, 'reset'), 1); assert.equal(count(f, 'ped'), 0); assert.equal(samples(f).length, 0);
    });
    test('external stop is idempotent and records an observation-only finish', () => {
        const f = fixture(Controller, { stopReason: 'Ctrl+Shift+End' }); f.tick(0); f.state.stop = true; f.tick(50);
        const finish = f.events.find(event => event.event === 'native_input_reference_finished');
        assert.equal(finish.outcome, 'observation_only'); assert.equal(finish.reason, 'Ctrl+Shift+End');
        f.controller.stop(); f.tick(100);
        assert.equal(count(f, 'reset'), 2); assert.equal(f.events.filter(event => event.event === 'native_input_reference_stopped').length, 1);
    });
    for (const [name, time, reason] of [['clock reversal', -1, 'clock_reversed'], ['render gap', 251, 'render_gap']])
        test(name + ' stops before observing controls and retains a nonnegative evidence time', () => {
            const f = fixture(Controller); f.tick(0); f.tick(time);
            assert.equal(samples(f).length, 1); assert.equal(count(f, 'reset'), 2);
            const finish = f.events.find(event => event.event === 'native_input_reference_finished');
            assert.equal(finish.reason, reason); assert.ok(finish.elapsed_ms >= 0);
        });
    test('exactly two hundred fifty milliseconds remains a bounded sample gap', () => {
        const f = fixture(Controller); f.tick(0); f.tick(250);
        assert.equal(samples(f).length, 2); assert.equal(f.controller.running, true);
    });
    test('individually timely ticks cannot publish an observation more than two hundred fifty milliseconds after its prior sample', () => {
        const f = fixture(Controller); f.tick(0); f.tick(99); f.tick(349);
        assert.equal(samples(f).length, 1);
        assert.equal(count(f, 'control'), 20, 'overdue sample stops before native control reads');
        assert.equal(count(f, 'commit'), 1);
        assert.equal(f.events.find(event => event.event === 'native_input_reference_finished').reason, 'render_gap');
        assert.equal(f.controller.running, false);
        assert.equal(count(f, 'reset'), 2);
    });
    for (const bad of [-32769, 32768, 1.5, NaN, Infinity, '1', null, undefined])
        test('invalid signed control stops before beginning publication: ' + String(bad), () => {
            const f = fixture(Controller); f.state.controls[19] = bad;
            assert.throws(() => f.tick(0), /invalid.*control/i);
            assert.equal(count(f, 'begin'), 0); assert.equal(samples(f).length, 0); assert.equal(f.controller.running, false);
            assert.ok(count(f, 'reset') >= 1);
        });
    for (const [name, change] of [
        ['velocity NaN', f => { f.state.velocity.z = NaN; }],
        ['velocity string', f => { f.state.velocity.x = '0'; }],
        ['position infinity', f => { f.state.position.x = Infinity; }],
        ['heading missing', f => { delete f.state.position.heading; }]
    ]) test(name + ' prevents a canonical transaction', () => {
        const f = fixture(Controller); change(f);
        assert.throws(() => f.tick(0), /invalid.*(velocity|transform)/i);
        assert.equal(count(f, 'commit'), 0); assert.equal(samples(f).length, 0); assert.equal(f.controller.running, false);
    });
    test('local ped replacement during reads prevents publication of a mixed sample', () => {
        const f = fixture(Controller); f.game.readVelocity = () => { f.state.ped = 11; return f.state.velocity; }; f.tick(0);
        assert.equal(count(f, 'commit'), 0); assert.equal(samples(f).length, 0);
        assert.equal(f.events.find(event => event.event === 'native_input_reference_finished').reason, 'local_ped_changed');
    });
    for (const failedMethod of ['readPadState', 'readVelocity', 'readTransform'])
        test('native ' + failedMethod + ' exception cleans the bridge and preserves the original error', () => {
            const f = fixture(Controller); const error = new Error('native read failed');
            f.game[failedMethod] = () => { throw error; };
            assert.throws(() => f.tick(0), value => value === error);
            assert.equal(f.controller.running, false); assert.ok(count(f, 'reset') >= 1); assert.equal(samples(f).length, 0);
        });
    for (const failedMethod of ['begin', 'set', 'commit'])
        test('failed canonical ' + failedMethod + ' transaction stops and resets history', () => {
            const f = fixture(Controller); f.bridge[failedMethod] = () => 0;
            assert.throws(() => f.tick(0), /canonical.*transaction/i);
            assert.equal(f.controller.running, false); assert.equal(samples(f).length, 0); assert.equal(f.bridge.currentMask(), 0);
        });
    test('current readback must equal the observed signed raw fields', () => {
        const f = fixture(Controller); f.bridge.read = () => 123;
        assert.throws(() => f.tick(0), /canonical.*readback/i); assert.equal(samples(f).length, 0);
    });
    test('previous readback must equal the prior committed probe sample', () => {
        const f = fixture(Controller); f.tick(0); const original = f.bridge.read;
        f.bridge.read = (which, index) => which ? 12 : original(which, index);
        assert.throws(() => f.tick(100), /canonical.*previous/i); assert.equal(samples(f).length, 1);
    });
    test('canonical current mask must expose exactly the twenty documented inputs', () => {
        const f = fixture(Controller); const original = f.bridge.currentMask;
        f.bridge.currentMask = () => original() ? 0xffffff : 0;
        assert.throws(() => f.tick(0), /canonical.*mask/i); assert.equal(samples(f).length, 0);
    });
    test('first previous mask cannot claim unobserved history', () => {
        const f = fixture(Controller); const original = f.bridge.currentMask;
        f.bridge.previousMask = () => original() ? 0xfffff : 0;
        assert.throws(() => f.tick(0), /canonical.*mask/i); assert.equal(samples(f).length, 0);
    });
    test('cleanup failure cannot hide a preceding native read exception', () => {
        const f = fixture(Controller); const error = new Error('original native error'); f.tick(0);
        f.game.readPadState = () => { throw error; }; f.bridge.reset = () => { throw new Error('cleanup failed'); };
        assert.throws(() => f.tick(100), value => value === error);
        assert.equal(f.controller.running, false); assert.ok(f.events.some(event => event.event === 'cleanup_failed'));
    });
    test('failed external cleanup is reported while the probe remains stopped', () => {
        const f = fixture(Controller); f.tick(0); const error = new Error('cleanup failed'); f.bridge.reset = () => { throw error; };
        assert.throws(() => f.controller.stop(), value => value === error);
        assert.equal(f.controller.running, false); f.tick(100); assert.equal(samples(f).length, 1);
    });
    test('a reset that leaves published masks cannot produce a successful cleanup marker', () => {
        const f = fixture(Controller); f.tick(0); f.bridge.reset = () => {};
        assert.throws(() => f.controller.stop(), /canonical.*reset/i);
        assert.ok(!f.events.some(event => event.event === 'native_input_reference_reset' && event.reason === 'cleanup'));
    });
    test('logging failure after canonical publication still resets state and preserves that error', () => {
        const f = fixture(Controller); const error = new Error('sample log failed');
        f.controller.log = event => { if (event.event === 'local_controller_sample') throw error; f.events.push(event); };
        assert.throws(() => f.tick(0), value => value === error);
        assert.equal(f.controller.running, false); assert.equal(f.bridge.currentMask(), 0); assert.equal(count(f, 'reset'), 2);
    });
    test('bridge read exception after Commit cannot leave published history active', () => {
        const f = fixture(Controller); const error = new Error('readback failed'); f.bridge.read = () => { throw error; };
        assert.throws(() => f.tick(0), value => value === error);
        assert.equal(f.controller.running, false); assert.equal(f.bridge.currentMask(), 0); assert.equal(samples(f).length, 0);
    });
    test('loading during evidence collection discards the sample before Begin', () => {
        const f = fixture(Controller); f.game.readTransform = () => { f.state.ready = false; return f.state.position; };
        f.tick(0); assert.equal(count(f, 'begin'), 0); assert.equal(samples(f).length, 0);
        assert.equal(f.events.find(event => event.event === 'native_input_reference_finished').reason, 'loading');
    });
    console.log('Passed ' + tests + ' native input reference mock checks');
})().catch(error => { console.error(error); process.exitCode = 1; });
