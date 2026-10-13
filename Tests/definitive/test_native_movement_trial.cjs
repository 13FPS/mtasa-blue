// SPDX-License-Identifier: GPL-3.0-or-later
// Scheduling tests cannot establish native movement or visual quality. They
// verify that every failure still reaches the real module's cleanup boundary.
const assert = require('node:assert/strict');
const { pathToFileURL } = require('node:url');
const path = require('node:path');

async function main() {
    const { MtaNativeMovementTrialController: Controller } = await import(pathToFileURL(path.resolve(__dirname,
        '../../Client/definitive/game-adapter/native-movement-trial.mjs')).href);
    let checks = 0;
    const test = (name, run) => { run(); ++checks; console.log('PASS ' + name); };
    function fixture() {
        const events = [], calls = [];
        const values = [1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0];
        const state = { ready: true, ped: 10, exists: true, stop: false, begin: 1, submit: 1,
            capture: 1, end: 1, advance: true, route: true, badPose: false, failLog: false };
        const bridge = {
            begin: () => { calls.push(['begin']); return state.begin; },
            submit: phase => { calls.push(['submit', phase]); return state.submit; },
            capture: () => { calls.push(['capture']); if (state.advance) {
                ++values[2]; ++values[3]; ++values[6]; if (state.route) values[4] += 2;
            } return state.capture; },
            read: index => values[index],
            end: () => { calls.push(['end']); if (state.end === 1) values[0] = values[1] = 0; return state.end; }
        };
        const game = { stopRequested: () => state.stop, isReady: () => state.ready,
            localPed: () => state.ped, doesPedExist: () => state.exists,
            readTransform: () => ({ x: state.badPose ? NaN : 0, y: 0, z: 0, heading: 0 }),
            readVelocity: () => ({ x: 0, y: 0, z: 0 }) };
        const controller = new Controller(game, bridge, { log: event => {
            calls.push(['log', event.event]); if (state.failLog) throw Error('logger failed'); events.push(event);
        } });
        return { controller, state, values, events, calls };
    }
    test('complete trial submits the independent expected phase schedule and restores before finish', () => {
        const f = fixture();
        for (let now = 0; now <= 22000; now += 50) {
            const before = f.calls.length;
            f.controller.tick(now);
            const submitted = f.calls.slice(before).find(call => call[0] === 'submit');
            const expected = now < 3000 ? 0 : now < 7000 ? 1 : now < 9000 ? 0 : now < 13000 ? 2 : now < 15000 ? 0 : now < 19000 ? 3 : 0;
            if (now < 22000) assert.equal(submitted[1], expected); else assert.equal(submitted, undefined);
        }
        assert.equal(f.controller.running, false);
        assert.deepEqual(f.events.filter(e => e.event.endsWith('_phase')).map(e => [e.elapsed_ms, e.code]),
            [[0, 0], [3000, 1], [7000, 0], [9000, 2], [13000, 0], [15000, 3], [19000, 0]]);
        const finish = f.events.at(-1);
        assert.equal(finish.reason, 'time_budget'); assert.equal(finish.cleanup_passed, true);
        assert.equal(finish.state.scopedSteps, finish.state.restoredSteps);
        assert.deepEqual(f.calls.slice(-2).map(v => v[0]), ['end', 'log']);
        f.controller.stop(); assert.equal(f.calls.filter(v => v[0] === 'end').length, 1);
    });
    test('cancel takes priority over readiness and binding', () => {
        const f = fixture(); f.state.stop = true; f.state.ready = false; f.controller.tick(0);
        assert.ok(!f.calls.some(c => c[0] === 'begin')); assert.equal(f.events.at(-1).reason, 'Ctrl+Shift+End');
    });
    test('readiness timeout has no input submission', () => {
        const f = fixture(); f.state.ready = false;
        for (let now = 0; now <= 10000; now += 1000) f.controller.tick(now);
        assert.equal(f.events.at(-1).reason, 'loading'); assert.ok(!f.calls.some(c => c[0] === 'submit'));
    });
    for (const [name, mutate] of [
        ['binding rejection', f => { f.state.begin = 0; }],
        ['submission rejection', f => { f.state.submit = 0; }],
        ['capture rejection', f => { f.state.capture = 0; }],
        ['nonfinite observation', f => { f.state.badPose = true; }],
        ['invalid metric width', f => { f.values[2] = 0x100000000; }],
        ['native failure', f => { f.values[10] = 14; }],
        ['native restoration failure', f => { f.values[7] = 1; }],
        ['nested callback', f => { f.values[9] = 1; }],
        ['logger failure', f => { f.state.failLog = true; }]
    ]) test(name + ' attempts cleanup and stops', () => {
        const f = fixture(); mutate(f); assert.throws(() => f.controller.tick(0));
        assert.equal(f.controller.running, false); assert.equal(f.calls.filter(c => c[0] === 'end').length, 1);
    });
    test('cleanup refusal remains observable and does not report success', () => {
        const f = fixture(); f.controller.tick(0); f.state.end = 0;
        f.controller.stop('cancelled'); assert.equal(f.controller.running, true);
        assert.throws(() => f.controller.tick(2000), /retained/);
        assert.equal(f.events.at(-1).cleanup_passed, false);
        assert.equal(f.events.at(-1).state.bound, 1);
    });
    test('busy engine cleanup retries on later ticks without more submissions or observations', () => {
        const f = fixture(); f.controller.tick(0); f.state.end = 0;
        f.controller.stop('cancelled', 50);
        assert.equal(f.controller.running, true);
        const submissions = f.calls.filter(c => c[0] === 'submit').length;
        const observations = f.events.filter(e => e.event.endsWith('_sample')).length;
        f.controller.tick(100); f.state.end = 1; f.controller.tick(150);
        assert.equal(f.controller.running, false);
        assert.equal(f.calls.filter(c => c[0] === 'submit').length, submissions);
        assert.equal(f.events.filter(e => e.event.endsWith('_sample')).length, observations);
        assert.equal(f.events.at(-1).reason, 'cancelled'); assert.equal(f.events.at(-1).cleanup_passed, true);
        assert.equal(f.events.filter(e => e.event.endsWith('_finished')).length, 1);
    });
    test('trial error waits for engine cleanup before leaving the script polling loop', () => {
        const f = fixture(); f.controller.tick(0); f.state.submit = 0; f.state.end = 0;
        assert.doesNotThrow(() => f.controller.tick(50)); assert.equal(f.controller.running, true);
        assert.doesNotThrow(() => f.controller.tick(100));
        f.state.end = 1; assert.throws(() => f.controller.tick(150), /submission/);
        assert.equal(f.controller.running, false); assert.equal(f.events.at(-1).reason, 'error');
        assert.match(f.events.at(-1).error, /submission/);
        assert.equal(f.events.at(-1).cleanup_passed, true);
    });
    test('pending cleanup preserves the first stop reason and fails a reversed clock honestly', () => {
        const f = fixture(); f.controller.tick(100); f.state.end = 0;
        f.controller.stop('first', 150); f.controller.stop('second', 160);
        assert.throws(() => f.controller.tick(149), /cleanup clock/);
        assert.equal(f.events.at(-1).reason, 'first'); assert.equal(f.events.at(-1).cleanup_passed, false);
    });
    test('cleanup success exactly at its deadline records the bounded retry evidence', () => {
        const f = fixture(); f.controller.tick(0); f.state.end = 0; f.controller.stop('cancelled', 50);
        f.state.end = 1; f.controller.tick(2050);
        const finish = f.events.at(-1);
        assert.equal(finish.cleanup_passed, true); assert.equal(finish.cleanup_wait_ms, 2000);
        assert.equal(finish.stop_requested_at_ms, 50); assert.equal(finish.cleanup_attempts, 2);
    });
    test('repeated stop without an explicit clock uses the latest cleanup tick', () => {
        const f = fixture(); f.controller.tick(0); f.state.end = 0; f.controller.stop('cancelled');
        f.controller.tick(50); assert.doesNotThrow(() => f.controller.stop('script_exit'));
        assert.equal(f.controller.running, true); f.state.end = 1; f.controller.tick(100);
        assert.equal(f.events.at(-1).cleanup_passed, true); assert.equal(f.events.at(-1).reason, 'cancelled');
    });
    test('late successful End restores first but cannot claim cleanup met its deadline', () => {
        const f = fixture(); f.controller.tick(0); f.state.end = 0; f.controller.stop('cancelled', 50);
        f.state.end = 1; assert.throws(() => f.controller.tick(2051), /deadline/);
        assert.equal(f.values[0], 0); assert.equal(f.values[1], 0);
        assert.equal(f.events.at(-1).cleanup_passed, false); assert.equal(f.events.at(-1).cleanup_wait_ms, 2001);
    });
    for (const [name, next, reason] of [['backward clock', 99, 'clock_reversed'], ['render gap', 2101, 'render_gap']])
        test(name + ' ends before another action submission', () => {
            const f = fixture(); f.controller.tick(100); const before = f.calls.filter(c => c[0] === 'submit').length;
            f.controller.tick(next); assert.equal(f.events.at(-1).reason, reason);
            assert.equal(f.calls.filter(c => c[0] === 'submit').length, before);
        });
    for (const [name, mutate, until] of [['no native callback', f => { f.state.advance = false; }, 2000],
        ['no provider queries', f => { f.state.route = false; }, 4000]]) test(name + ' cannot count as success', () => {
        const f = fixture(); mutate(f);
        assert.throws(() => { for (let now = 0; now <= until; now += 100) f.controller.tick(now); });
        assert.equal(f.events.at(-1).reason, 'error'); assert.equal(f.controller.running, false);
    });
    for (const [name, mutate] of [['ped replacement', f => { f.state.ped = 11; }],
        ['ped deletion', f => { f.state.exists = false; }], ['readiness lost', f => { f.state.ready = false; }]])
        test(name + ' stops an active trial', () => {
            const f = fixture(); f.controller.tick(0); mutate(f); f.controller.tick(50);
            assert.equal(f.controller.running, false); assert.equal(f.values[1], 0);
        });
    console.log(`Passed ${checks} native movement trial checks`);
}
main().catch(error => { console.error(error); process.exitCode = 1; });
