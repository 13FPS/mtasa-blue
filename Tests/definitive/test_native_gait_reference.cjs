// SPDX-License-Identifier: GPL-3.0-or-later
// The simulated engine advances its own NPC after a single walking task. These
// tests protect observation and ownership boundaries; visual foot contact still
// requires the user's in-game walk/reference recording on the installed build.
const assert = require('node:assert/strict');
const path = require('node:path');
const { pathToFileURL } = require('node:url');

const controllerPath = path.resolve(__dirname, '../../Client/definitive/game-adapter/controller.mjs');
let tests = 0;

function fixture(Controller, options = {}) {
    const calls = [];
    const events = [];
    const state = { now: 0, ready: true, localPed: 10, loaded: true, stop: false, nextPed: 100,
        anchor: { x: -30, y: -20, z: 3, heading: 90 }, taskAt: null, start: null, target: null,
        motion: age => Math.min(8, age * 0.0013),
        animation: age => ({ playing: true, time: (0.37 + age / 800) % 1 }), observed: null };
    const existing = new Set([10]);
    const positions = new Map();
    const forbidden = name => () => assert.fail('reference must not call ' + name);
    const game = {
        stopRequested: () => state.stop,
        isReady: () => state.ready,
        localPed: () => { calls.push(['local_ped']); return state.localPed; },
        doesPedExist: handle => { calls.push(['exists', handle]); return existing.has(handle); },
        readTransform: handle => {
            calls.push(['read', handle]);
            if (handle === state.localPed) return { ...state.anchor };
            if (state.observed) return state.observed(handle, state);
            const value = positions.get(handle);
            if (!value) return null;
            return { ...value, x: value.x + (state.taskAt === null ? 0 : state.motion(state.now - state.taskAt)) };
        },
        requestModel: model => calls.push(['request', model]),
        hasModelLoaded: model => { calls.push(['loaded', model]); return state.loaded; },
        createPed: (model, start) => {
            const handle = state.nextPed;
            existing.add(handle);
            positions.set(handle, { ...start });
            calls.push(['create', handle, model, { ...start }]);
            return handle;
        },
        prepareReferencePed: (handle, start) => {
            positions.set(handle, { ...start });
            state.start = { ...start };
            calls.push(['prepare', handle, { ...start }]);
        },
        walkReferencePed: (handle, target) => {
            state.taskAt = state.now;
            state.target = { ...target };
            calls.push(['walk', handle, { ...target }]);
        },
        readRemoteAnimation: (handle, clip) => {
            calls.push(['read_animation', handle, clip]);
            return state.animation(state.now - state.taskAt);
        },
        deletePed: handle => { calls.push(['delete', handle]); existing.delete(handle); positions.delete(handle); },
        releaseModel: model => calls.push(['release', model]),
        movePed: forbidden('movePed'), makeRemoteInert: forbidden('makeRemoteInert'),
        playRemoteAnimation: forbidden('playRemoteAnimation'), setAnimationSpeed: forbidden('setAnimationSpeed'),
        connect: forbidden('transport connect'), submit: forbidden('transport submit'), pump: forbidden('transport pump')
    };
    const controller = new Controller(game, { ...options, log: event => events.push(event) });
    const tick = now => { state.now = now; controller.tick(now); };
    return { controller, game, state, calls, events, existing, positions, tick };
}

const count = (f, name) => f.calls.filter(call => call[0] === name).length;
const eventsOf = (f, event) => f.events.filter(value => value.event === event);
const finish = f => eventsOf(f, 'native_gait_reference_finished').at(-1);
const samples = f => eventsOf(f, 'native_gait_reference_sample');
function advance(f, from, to, step = 50) {
    for (let now = from; now <= to && f.controller.running; now += step) f.tick(now);
}
function beginWalking(f) { advance(f, 0, 1000); }
function assertCleaned(f, reason) {
    assert.equal(f.controller.running, false);
    assert.equal(count(f, 'delete'), 1);
    assert.equal(count(f, 'release'), 1);
    assert.equal(finish(f).reason, reason);
    assert.equal(finish(f).outcome, 'observation_only');
    assert.equal(eventsOf(f, 'native_gait_reference_stopped').at(-1).reason, reason);
    assert.ok(!f.calls.some(call => call[0] === 'delete' && call[1] === 10));
}

async function main() {
    const { NativeGaitReferenceController: Controller } = await import(pathToFileURL(controllerPath).href);
    const test = (name, run) => { run(); ++tests; console.log('PASS ' + name); };

    test('one owned model-7 NPC settles while its actual pose is read before a single walking task', () => {
        const f = fixture(Controller);
        advance(f, 0, 950);
        assert.equal(count(f, 'request'), 1);
        assert.equal(count(f, 'create'), 1);
        assert.equal(count(f, 'prepare'), 1);
        assert.equal(count(f, 'walk'), 0);
        assert.equal(count(f, 'read_animation'), 0);
        assert.ok(f.calls.filter(call => call[0] === 'read' && call[1] === 100).length >= 20);
        assert.deepEqual(f.calls.find(call => call[0] === 'create'), ['create', 100, 7,
            { x: -34, y: -17, z: 3, heading: 270 }]);
        f.tick(1000);
        assert.deepEqual(f.calls.find(call => call[0] === 'walk'), ['walk', 100,
            { x: -26, y: -17, z: 3, heading: 270 }]);
        const task = eventsOf(f, 'native_gait_reference_task_requested')[0];
        assert.equal(task.move_state, 4);
        assert.equal(task.time_limit_ms, 10000);
        assert.equal(task.clip, 'Walk_civi');
        assert.equal(task.task_age_ms, 0);
        assert.deepEqual(task.start_transform, f.state.start);
        assert.deepEqual(task.target_transform, f.state.target);
    });

    test('samples engine-driven position and real nonzero phase without transport or presentation commands', () => {
        const f = fixture(Controller);
        beginWalking(f);
        advance(f, 1050, 1400);
        assert.equal(count(f, 'prepare'), 1);
        assert.equal(count(f, 'walk'), 1);
        assert.deepEqual(samples(f).map(event => event.sample_at_ms), [1000, 1100, 1200, 1300, 1400]);
        assert.equal(samples(f)[0].time, 0.37);
        assert.equal(samples(f)[0].task_age_ms, 0);
        assert.equal(samples(f).at(-1).observed_transform.x, -34 + 0.4 * 1.3);
        assert.equal(samples(f).at(-1).time, (0.37 + 400 / 800) % 1);
        assert.ok(f.calls.filter(call => call[0] === 'read_animation').every(call => call[1] === 100 && call[2] === 'Walk_civi'));
        assert.ok(!Object.hasOwn(f.controller, 'transport'));
        assert.ok(!samples(f).some(event => Object.hasOwn(event, 'requested_transform') || Object.hasOwn(event, 'network_target')));
        const identity = samples(f)[0];
        for (const event of samples(f)) {
            assert.equal(event.reference_id, identity.reference_id);
            assert.equal(event.reference_started_at_ms, 0);
            assert.equal(event.task_id, identity.task_id);
            assert.equal(event.task_requested_at_ms, 1000);
            assert.equal(event.elapsed_ms, event.sample_at_ms);
            assert.equal(event.task_age_ms, event.sample_at_ms - 1000);
            assert.equal(event.local_ped, 10);
            assert.equal(event.ped, 100);
            assert.equal(event.model, 7);
        }
    });

    test('duplicate ticks never restart the engine task or multiply samples', () => {
        const f = fixture(Controller);
        beginWalking(f);
        f.tick(1000); f.tick(1000); f.tick(1050); f.tick(1100); f.tick(1100);
        assert.equal(count(f, 'walk'), 1);
        assert.equal(count(f, 'prepare'), 1);
        assert.equal(samples(f).length, 2);
    });

    test('the first task sample reads the engine pose after task dispatch', () => {
        const f = fixture(Controller);
        const walk = f.game.walkReferencePed;
        f.game.walkReferencePed = (handle, target) => {
            walk(handle, target);
            f.positions.get(handle).x += 0.02;
        };
        beginWalking(f);
        assert.equal(samples(f)[0].observed_transform.x, -33.98);
        assert.equal(samples(f)[0].time, 0.37);
    });

    test('observed arrival cleans up only after the minimum two seconds of task age', () => {
        const f = fixture(Controller);
        f.state.motion = () => 8;
        beginWalking(f);
        advance(f, 1050, 2950);
        assert.equal(f.controller.running, true);
        f.tick(3000);
        assertCleaned(f, 'arrived');
        assert.equal(finish(f).task_age_ms, 2000);
        assert.equal(count(f, 'walk'), 1);
        assert.equal(eventsOf(f, 'native_gait_reference_cleanup_deleted').length, 1);
        const before = f.calls.length;
        f.tick(3100); f.controller.stop();
        assert.equal(f.calls.length, before);
        assert.equal(eventsOf(f, 'native_gait_reference_finished').length, 1);
    });

    test('a stationary reference emits at most 100 samples over at most ten walking seconds', () => {
        const f = fixture(Controller);
        f.state.motion = () => 0;
        beginWalking(f);
        advance(f, 1050, 12000);
        assertCleaned(f, 'sample_budget');
        assert.equal(samples(f).length, 100);
        assert.equal(finish(f).samples, 100);
        assert.equal(finish(f).task_age_ms, 9900);
        assert.equal(count(f, 'walk'), 1);
        assert.equal(count(f, 'prepare'), 1);
    });

    test('slower display ticks stop at the ten-second walking budget without catch-up samples', () => {
        const f = fixture(Controller);
        f.state.motion = () => 0;
        beginWalking(f);
        advance(f, 1250, 11000, 250);
        assertCleaned(f, 'time_budget');
        assert.equal(samples(f).length, 40);
        assert.equal(finish(f).task_age_ms, 10000);
        assert.ok(samples(f).every(event => event.task_age_ms < 10000));
        assert.equal(count(f, 'walk'), 1);
    });

    test('false playback records null phase as evidence and never guesses a walking phase', () => {
        const f = fixture(Controller);
        f.state.animation = () => ({ playing: false, time: null });
        beginWalking(f);
        f.tick(1100);
        assert.equal(f.controller.running, true);
        assert.ok(samples(f).every(event => event.playing === false && event.time === null));
        assert.equal(count(f, 'walk'), 1);
    });

    test('initial readiness has one ten-second deadline and never creates a late NPC', () => {
        const f = fixture(Controller);
        f.state.ready = false;
        advance(f, 0, 9950);
        assert.equal(f.controller.running, true);
        assert.equal(count(f, 'request'), 0);
        f.state.ready = true;
        f.tick(10000);
        assert.equal(f.controller.running, false);
        assert.equal(finish(f).reason, 'readiness_timeout');
        assert.equal(count(f, 'create'), 0);
        assert.equal(count(f, 'request'), 0);
        assert.equal(count(f, 'release'), 0);
        f.tick(10050);
        assert.equal(count(f, 'create'), 0);
    });

    test('model loading shares the activation deadline and releases the requested model on timeout', () => {
        const f = fixture(Controller);
        f.state.ready = false;
        advance(f, 0, 2000);
        f.state.ready = true;
        f.state.loaded = false;
        advance(f, 2050, 9950);
        assert.equal(count(f, 'request'), 1);
        f.state.loaded = true;
        f.tick(10000);
        assert.equal(finish(f).reason, 'model_timeout');
        assert.equal(count(f, 'create'), 0);
        assert.equal(count(f, 'release'), 1);
        assert.equal(count(f, 'delete'), 0);
    });

    test('loading or player replacement during model wait releases once without ever spawning', () => {
        for (const reason of ['loading', 'local_ped_changed']) {
            const f = fixture(Controller);
            f.state.loaded = false;
            f.tick(0);
            if (reason === 'loading') f.state.ready = false;
            else { f.state.localPed = 11; f.existing.add(11); }
            f.tick(50);
            assert.equal(f.controller.running, false);
            assert.equal(finish(f).reason, reason);
            assert.equal(count(f, 'create'), 0);
            assert.equal(count(f, 'delete'), 0);
            assert.equal(count(f, 'release'), 1);
            f.state.loaded = true;
            f.tick(100);
            assert.equal(count(f, 'create'), 0);
        }
    });

    test('an initially invalid/missing player handle waits without native ped reads or model mutation', () => {
        for (const handle of [null, undefined, 0, -1, NaN, 99]) {
            const f = fixture(Controller);
            f.state.localPed = handle;
            advance(f, 0, 10000);
            assert.equal(finish(f).reason, 'readiness_timeout');
            assert.equal(count(f, 'read'), 0);
            assert.equal(count(f, 'request'), 0);
            assert.equal(count(f, 'delete'), 0);
            assert.ok(!f.calls.some(call => call[0] === 'exists' && !Number.isInteger(call[1])));
        }
    });

    for (const phase of ['settling', 'walking']) {
        test('stop key deletes and releases once while ' + phase, () => {
            const f = fixture(Controller, { stopReason: 'Ctrl+Shift+End' });
            if (phase === 'settling') f.tick(0); else beginWalking(f);
            f.state.stop = true;
            f.tick(phase === 'settling' ? 50 : 1050);
            assertCleaned(f, 'Ctrl+Shift+End');
            const before = f.calls.length;
            f.controller.stop('again'); f.tick(1100);
            assert.equal(f.calls.length, before);
        });
        test('readiness loss stops without further transform/animation observation while ' + phase, () => {
            const f = fixture(Controller);
            if (phase === 'settling') f.tick(0); else beginWalking(f);
            const reads = count(f, 'read');
            const animations = count(f, 'read_animation');
            f.state.ready = false;
            f.tick(phase === 'settling' ? 50 : 1050);
            assertCleaned(f, 'loading');
            assert.equal(count(f, 'read'), reads);
            assert.equal(count(f, 'read_animation'), animations);
        });
    }

    test('local handle replacement cancels the attempt without recreation', () => {
        const f = fixture(Controller);
        beginWalking(f);
        f.state.localPed = 11;
        f.existing.add(11);
        f.tick(1050);
        assertCleaned(f, 'local_ped_changed');
        f.tick(1100);
        assert.equal(count(f, 'create'), 1);
        assert.equal(count(f, 'walk'), 1);
        assert.ok(!f.calls.some(call => call[0] === 'delete' && call[1] === 11));
    });

    test('cleanup cannot delete the current CJ if its handle becomes the former owned NPC', () => {
        const f = fixture(Controller);
        f.tick(0);
        f.state.localPed = 100;
        f.controller.stop('external_stop');
        assert.equal(count(f, 'delete'), 0);
        assert.equal(count(f, 'release'), 1);
        assert.equal(f.controller.running, false);
    });

    test('a missing local ped ends the reference before reading its stale transform', () => {
        const f = fixture(Controller);
        f.tick(0);
        f.existing.delete(10);
        const reads = count(f, 'read');
        f.tick(50);
        assertCleaned(f, 'local_ped_missing');
        assert.equal(count(f, 'read'), reads);
    });

    test('a missing owned ped is neither queried for transform/animation nor recreated', () => {
        const f = fixture(Controller);
        beginWalking(f);
        f.existing.delete(100);
        const reads = count(f, 'read');
        const animations = count(f, 'read_animation');
        f.tick(1050);
        assert.equal(finish(f).reason, 'missing_ped');
        assert.equal(f.controller.running, false);
        assert.equal(count(f, 'read'), reads);
        assert.equal(count(f, 'read_animation'), animations);
        assert.equal(count(f, 'create'), 1);
        assert.equal(count(f, 'delete'), 0);
        assert.equal(count(f, 'release'), 1);
    });

    test('a reversed clock preserves the last absolute time and terminates observation', () => {
        const f = fixture(Controller);
        beginWalking(f);
        f.tick(1100);
        f.tick(1099);
        assertCleaned(f, 'clock_reversed');
        assert.equal(finish(f).sample_at_ms, 1100);
        assert.equal(finish(f).task_age_ms, 100);
        assert.equal(samples(f).length, 2);
    });

    test('a display gap above 250 ms ends a settled or waiting reference instead of resuming it', () => {
        for (const walking of [false, true]) {
            const f = fixture(Controller);
            if (walking) beginWalking(f); else f.tick(0);
            f.tick(walking ? 1251 : 251);
            assertCleaned(f, 'render_gap');
            assert.equal(count(f, 'walk'), walking ? 1 : 0);
        }
    });

    test('finite but excessive XY or Z departure stops before invalid lane evidence is logged', () => {
        for (const observed of [
            { x: -21.99, y: -17, z: 3, heading: 270 },
            { x: -34, y: -17, z: 5.01, heading: 270 }
        ]) {
            const f = fixture(Controller);
            beginWalking(f);
            f.state.observed = () => observed;
            f.tick(1100);
            assertCleaned(f, 'lane_departure');
            assert.equal(samples(f).length, 1);
        }
    });

    test('invalid local/derived lane transforms stop before model allocation or creation', () => {
        for (const anchor of [
            { x: NaN, y: -20, z: 3, heading: 90 },
            { x: 99999, y: -20, z: 3, heading: 90 }
        ]) {
            const f = fixture(Controller);
            f.state.anchor = anchor;
            assert.throws(() => f.tick(0), /invalid transform/);
            assert.equal(f.controller.running, false);
            assert.equal(count(f, 'request'), 0);
            assert.equal(count(f, 'create'), 0);
            assert.equal(count(f, 'delete'), 0);
        }
    });

    test('invalid observed transforms end cleanly without logging a fabricated sample', () => {
        const f = fixture(Controller);
        beginWalking(f);
        f.state.observed = () => ({ x: NaN, y: -17, z: 3, heading: 270 });
        assert.throws(() => f.tick(1100), /invalid observed transform/);
        assertCleaned(f, 'error');
        assert.equal(samples(f).length, 1);
    });

    test('malformed animation evidence never enters a sample and cleans up the owned resources', () => {
        for (const value of [null, {}, { playing: 1, time: 0.2 }, { playing: true, time: null },
            { playing: true, time: NaN }, { playing: true, time: Infinity }, { playing: true, time: -0.01 },
            { playing: true, time: 1.01 }, { playing: false, time: 0 }, { playing: false }]) {
            const f = fixture(Controller);
            f.state.animation = () => value;
            assert.throws(() => beginWalking(f), /invalid animation evidence/);
            assertCleaned(f, 'error');
            assert.equal(samples(f).length, 0);
            assert.equal(count(f, 'walk'), 1);
        }
    });

    test('null, invalid or CJ creation handles never receive setup or deletion', () => {
        for (const handle of [null, undefined, 0, -1, NaN, 10]) {
            const f = fixture(Controller);
            f.state.nextPed = handle;
            assert.throws(() => f.tick(0), /ped creation failed/);
            assert.equal(count(f, 'prepare'), 0);
            assert.equal(count(f, 'walk'), 0);
            assert.equal(count(f, 'delete'), 0);
            assert.equal(count(f, 'release'), 1);
            assert.equal(f.controller.running, false);
        }
    });

    test('a native setup exception still deletes ownership recorded before setup', () => {
        const f = fixture(Controller);
        const failure = new Error('prepare native failed');
        f.game.prepareReferencePed = () => { throw failure; };
        assert.throws(() => f.tick(0), error => error === failure);
        assertCleaned(f, 'error');
        assert.equal(count(f, 'walk'), 0);
        assert.equal(eventsOf(f, 'native_gait_reference_spawned').length, 0);
        assert.match(finish(f).error, /prepare native failed/);
    });

    test('a creation verification exception still cleans up ownership before setup', () => {
        const f = fixture(Controller);
        const exists = f.game.doesPedExist;
        const failure = new Error('creation existence native failed');
        let ownedLookups = 0;
        f.game.doesPedExist = handle => {
            if (handle === 100 && ++ownedLookups === 1) throw failure;
            return exists(handle);
        };
        assert.throws(() => f.tick(0), error => error === failure);
        assertCleaned(f, 'error');
        assert.equal(count(f, 'prepare'), 0);
    });

    test('a partially failing model request is released and never retried', () => {
        const f = fixture(Controller);
        const failure = new Error('request native failed');
        f.game.requestModel = model => { f.calls.push(['request', model]); throw failure; };
        assert.throws(() => f.tick(0), error => error === failure);
        assert.equal(count(f, 'release'), 1);
        assert.equal(count(f, 'create'), 0);
        f.tick(50);
        assert.equal(count(f, 'request'), 1);
    });

    test('a native walking request exception deletes and releases without a retry', () => {
        const f = fixture(Controller);
        const failure = new Error('walk native failed');
        f.game.walkReferencePed = handle => { f.calls.push(['walk', handle]); throw failure; };
        assert.throws(() => beginWalking(f), error => error === failure);
        assertCleaned(f, 'error');
        assert.equal(count(f, 'walk'), 1);
        assert.equal(samples(f).length, 0);
        f.tick(1050);
        assert.equal(count(f, 'walk'), 1);
    });

    test('a native observation exception remains primary when both cleanup operations fail', () => {
        const f = fixture(Controller);
        beginWalking(f);
        const primary = new Error('animation native failed');
        f.game.readRemoteAnimation = () => { throw primary; };
        f.game.deletePed = handle => { f.calls.push(['delete', handle]); throw new Error('delete native failed'); };
        f.game.releaseModel = model => { f.calls.push(['release', model]); throw new Error('release native failed'); };
        assert.throws(() => f.tick(1100), error => error === primary);
        assertCleaned(f, 'error');
        assert.match(eventsOf(f, 'cleanup_failed')[0].error, /delete native failed/);
        assert.match(finish(f).error, /animation native failed/);
        assert.equal(eventsOf(f, 'native_gait_reference_stopped').length, 1);
    });

    test('cleanup lookup failure still releases the requested model and reports failure', () => {
        const f = fixture(Controller);
        f.tick(0);
        const failure = new Error('cleanup player lookup failed');
        f.game.localPed = () => { throw failure; };
        assert.throws(() => f.controller.stop('requested'), error => error === failure);
        assert.equal(count(f, 'release'), 1);
        assert.equal(count(f, 'delete'), 0);
        assert.equal(f.controller.running, false);
        assert.equal(eventsOf(f, 'cleanup_failed').length, 1);
        assert.equal(eventsOf(f, 'native_gait_reference_stopped').at(-1).reason, 'requested');
    });

    test('model-release failure occurs after deletion and cannot cause a second cleanup', () => {
        const f = fixture(Controller);
        f.tick(0);
        const failure = new Error('release native failed');
        f.game.releaseModel = model => { f.calls.push(['release', model]); throw failure; };
        assert.throws(() => f.controller.stop('requested'), error => error === failure);
        assert.equal(count(f, 'delete'), 1);
        assert.equal(count(f, 'release'), 1);
        f.controller.stop('again');
        assert.equal(count(f, 'delete'), 1);
        assert.equal(count(f, 'release'), 1);
    });

    test('invalid tick clocks terminate ownership without negative or synthetic sample times', () => {
        for (const invalid of [NaN, Infinity, -1]) {
            const f = fixture(Controller);
            beginWalking(f);
            assert.throws(() => f.tick(invalid), /Invalid native gait reference clock/);
            assertCleaned(f, 'error');
            assert.equal(finish(f).sample_at_ms, 1000);
            assert.equal(finish(f).elapsed_ms, 1000);
            assert.equal(finish(f).task_age_ms, 0);
        }
    });

    test('an external stop before activation touches no game or model handle', () => {
        const f = fixture(Controller);
        f.controller.stop('before_activation');
        f.tick(0);
        assert.equal(f.calls.length, 0);
        assert.equal(f.controller.running, false);
        assert.equal(eventsOf(f, 'native_gait_reference_started').length, 0);
        assert.equal(eventsOf(f, 'native_gait_reference_finished').length, 0);
        assert.equal(eventsOf(f, 'native_gait_reference_stopped').at(-1).reason, 'before_activation');
    });

    console.log(`Passed ${tests} native gait reference mock tests. No installed game was run.`);
}

main().catch(error => { console.error(error); process.exitCode = 1; });
