// SPDX-License-Identifier: GPL-3.0-or-later
// These mocks exercise ownership and native argument boundaries. They cannot
// establish whether a CLEO opcode works in a particular installed GTA build.
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');
const { pathToFileURL } = require('node:url');

const adapterDirectory = path.resolve(__dirname, '../../Client/definitive/game-adapter');
let tests = 0;

function fixture(Controller, mode = 'online', options = {}) {
    const calls = [];
    const events = [];
    const state = { ready: true, ped: 10, stop: false, loaded: true, pump: true, id: 1,
        transform: { x: 10, y: 20, z: 3, heading: 90 }, players: [], nextPed: 100 };
    const existing = new Set([10]);
    const remoteTransforms = new Map();
    const game = {
        stopRequested: () => state.stop,
        isReady: () => state.ready,
        localPed: () => state.ped,
        doesPedExist: handle => existing.has(handle),
        readTransform: handle => {
            if (handle === state.ped) return { ...state.transform };
            calls.push(['read_remote', handle]);
            return { ...remoteTransforms.get(handle) };
        },
        requestModel: model => calls.push(['request', model]),
        hasModelLoaded: () => state.loaded,
        createPed: (model, transform) => { const handle = state.nextPed++; existing.add(handle); remoteTransforms.set(handle, { ...transform }); calls.push(['create', handle, model, transform]); return handle; },
        makeRemoteInert: handle => calls.push(['inert', handle]),
        movePed: (handle, transform) => { remoteTransforms.set(handle, { ...transform }); calls.push(['move', handle, { ...transform }]); },
        playRemoteAnimation: (handle, clip) => calls.push(['play_animation', handle, clip]),
        setRemoteAnimationRate: (handle, clip, rate) => calls.push(['set_animation_rate', handle, clip, rate]),
        readRemoteAnimation: (handle, clip) => { calls.push(['read_animation', handle, clip]); return { playing: true, time: 0.42 }; },
        deletePed: handle => { existing.delete(handle); remoteTransforms.delete(handle); calls.push(['delete', handle]); },
        releaseModel: model => calls.push(['release', model])
    };
    const transport = {
        connect: nonce => { calls.push(['connect', nonce]); return true; },
        pump: () => state.pump,
        localId: () => state.id,
        submit: transform => { calls.push(['submit', { ...transform }]); return true; },
        players: () => state.players,
        disconnect: () => calls.push(['disconnect'])
    };
    const controller = new Controller(game, transport, { mode, nonce: 123, ...options, log: event => events.push(event) });
    return { controller, calls, events, state, game, transport, existing };
}

const remote = (id = 2, x = 30, heading = 180) => ({ id, x, y: 20, z: 3, heading });
const count = (test, name) => test.calls.filter(call => call[0] === name).length;

async function main() {
    const { AdapterController, NativeGaitReferenceController, NativeInputReferenceController, nativeGaitReferenceSpeed, PlayerControlLease } = await import(pathToFileURL(path.join(adapterDirectory, 'controller.mjs')).href);
    const { MtaNativePadReferenceController } = await import(pathToFileURL(path.join(adapterDirectory, 'native-pad-reference.mjs')).href);
    const { MtaNativeMovementTrialController } = await import(pathToFileURL(path.join(adapterDirectory, 'native-movement-trial.mjs')).href);
    tests += require('./test_player_control_lease.cjs')(PlayerControlLease);
    const test = (name, run) => { run(); ++tests; console.log('PASS ' + name); };
    test('smoke only reads local state, with bounded evidence logs', () => {
        const f = fixture(AdapterController, 'smoke');
        f.state.players = [remote()];
        f.controller.tick(0); f.controller.tick(50); f.controller.tick(1000);
        assert.equal(f.calls.length, 0);
        assert.equal(f.events.filter(value => value.event === 'local_transform').length, 2);
        assert.equal(f.events[0].online, false);
        assert.ok(!f.events.some(event => event.event === 'online_transform'));
    });
    test('waits for a ready player and valid handle before opening transport', () => {
        const f = fixture(AdapterController);
        f.state.ready = false; f.controller.tick(0);
        f.state.ready = true; f.state.ped = 99; f.controller.tick(1500);
        assert.equal(f.calls.length, 0);
        f.state.ped = 10; f.controller.tick(3000);
        assert.equal(count(f, 'connect'), 1);
    });
    test('joins once and replicates a distinct remote while excluding self', () => {
        const f = fixture(AdapterController);
        f.state.players = [remote(1), remote(2)];
        f.controller.tick(0); f.controller.tick(50);
        assert.equal(count(f, 'connect'), 1);
        assert.equal(count(f, 'submit'), 2);
        assert.equal(count(f, 'create'), 1);
        assert.equal(count(f, 'inert'), 1);
        assert.equal(count(f, 'move'), 1);
        assert.equal(f.controller.remotes.size, 1);
    });
    test('model streaming delays creation without blocking submissions', () => {
        const f = fixture(AdapterController); f.state.loaded = false; f.state.players = [remote()];
        f.controller.tick(0); f.controller.tick(50);
        assert.equal(count(f, 'request'), 1); assert.equal(count(f, 'create'), 0); assert.equal(count(f, 'submit'), 2);
        f.state.loaded = true; f.controller.tick(100);
        assert.equal(count(f, 'create'), 1);
    });
    test('heading-only changes update a remote, identical transforms do not', () => {
        const f = fixture(AdapterController); f.state.players = [remote()];
        f.controller.tick(0); f.controller.tick(50);
        assert.equal(count(f, 'move'), 1);
        f.state.players = [remote(2, 30, 181)]; f.controller.tick(100);
        assert.equal(count(f, 'move'), 2);
        assert.equal(f.calls.filter(call => call[0] === 'move').at(-1)[2].heading, 181);
    });
    test('departure deletes the exact owned ped', () => {
        const f = fixture(AdapterController); f.state.players = [remote()]; f.controller.tick(0);
        f.state.players = []; f.controller.tick(50);
        assert.deepEqual(f.calls.find(call => call[0] === 'delete'), ['delete', 100]);
        assert.equal(f.controller.remotes.size, 0);
    });
    test('missing engine ped is recreated instead of using a stale handle', () => {
        const f = fixture(AdapterController); f.state.players = [remote()]; f.controller.tick(0);
        f.existing.delete(100); f.controller.tick(50);
        assert.equal(count(f, 'create'), 2); assert.equal(f.controller.remotes.get(2).handle, 101);
    });
    test('nonfinite local state never reaches the network', () => {
        const f = fixture(AdapterController); f.state.transform.x = NaN;
        assert.throws(() => f.controller.tick(0), /invalid transform/);
        assert.equal(count(f, 'connect'), 0); assert.equal(count(f, 'submit'), 0); assert.equal(f.controller.running, false);
    });
    test('invalid later snapshot record cannot partially spawn or move players', () => {
        const f = fixture(AdapterController); f.state.players = [remote(2), { ...remote(3), z: Infinity }];
        assert.throws(() => f.controller.tick(0), /Invalid snapshot/);
        assert.equal(count(f, 'create'), 0); assert.equal(count(f, 'move'), 0); assert.equal(count(f, 'disconnect'), 1);
    });
    test('duplicate IDs fail closed', () => {
        const f = fixture(AdapterController); f.state.players = [remote(), remote()];
        assert.throws(() => f.controller.tick(0), /Invalid snapshot/); assert.equal(count(f, 'create'), 0);
    });
    test('oversized snapshot fails before engine mutation', () => {
        const f = fixture(AdapterController); f.state.players = Array.from({ length: 17 }, (_, index) => remote(index + 1));
        assert.throws(() => f.controller.tick(0), /Invalid snapshot player count/); assert.equal(count(f, 'create'), 0);
    });
    test('malformed bridge local identity fails closed', () => {
        const f = fixture(AdapterController); f.state.id = NaN;
        assert.throws(() => f.controller.tick(0), /Invalid local connection identity/); assert.equal(count(f, 'submit'), 0);
        assert.equal(count(f, 'disconnect'), 1);
    });
    test('pump failure removes proxies, releases model, and reconnects after delay', () => {
        const f = fixture(AdapterController); f.state.players = [remote()]; f.controller.tick(0);
        f.state.pump = false; f.controller.tick(50);
        assert.equal(count(f, 'delete'), 1); assert.equal(count(f, 'release'), 1); assert.equal(count(f, 'disconnect'), 1);
        f.state.pump = true; f.controller.tick(500); assert.equal(count(f, 'connect'), 1);
        f.controller.tick(1050); assert.equal(count(f, 'connect'), 2); assert.equal(count(f, 'create'), 2);
    });
    test('player readiness loss clears session and never accesses missing ped', () => {
        const f = fixture(AdapterController); f.state.players = [remote()]; f.controller.tick(0);
        f.state.ready = false; f.game.readTransform = () => { throw new Error('must not read'); }; f.controller.tick(50);
        assert.equal(count(f, 'delete'), 1); assert.equal(count(f, 'disconnect'), 1);
    });
    test('replacement local player clears old proxies before reconnecting', () => {
        const f = fixture(AdapterController); f.state.players = [remote()]; f.controller.tick(0);
        f.state.ped = 11; f.existing.add(11); f.controller.tick(50);
        assert.equal(count(f, 'delete'), 1); assert.equal(count(f, 'disconnect'), 1); assert.equal(count(f, 'connect'), 1);
        f.controller.tick(1050); assert.equal(count(f, 'connect'), 2);
    });
    test('requested stop cleans up once and reports the input-neutral default reason', () => {
        const f = fixture(AdapterController); f.state.players = [remote()]; f.controller.tick(0);
        f.state.stop = true; f.controller.tick(50); f.controller.stop(); f.controller.tick(100);
        assert.equal(f.controller.running, false); assert.equal(count(f, 'delete'), 1); assert.equal(count(f, 'disconnect'), 1);
        assert.equal(f.events.find(event => event.event === 'adapter_stopped').reason, 'stop_key');
    });
    test('online evidence records moving negative coordinates and the own-player echo independently', () => {
        const f = fixture(AdapterController);
        f.state.transform = { x: 2488.25, y: -1666.5, z: 13.75, heading: 90 };
        f.state.players = [{ id: 1, ...f.state.transform }];
        f.controller.tick(0);
        const first = f.events.find(event => event.event === 'online_transform');
        assert.deepEqual(first, { event: 'online_transform', player_id: 1, successful_submissions: 1,
            submitted: { x: 2488.25, y: -1666.5, z: 13.75, heading: 90 },
            echoed: { x: 2488.25, y: -1666.5, z: 13.75, heading: 90 } });
        f.state.transform.y = -1665.25;
        f.state.transform.heading = 120;
        f.controller.tick(1000);
        const second = f.events.filter(event => event.event === 'online_transform').at(-1);
        assert.equal(second.successful_submissions, 2);
        assert.deepEqual(second.submitted, f.state.transform);
        assert.deepEqual(second.echoed, first.echoed);
        assert.equal(first.submitted.y, -1666.5);
        assert.equal(count(f, 'create'), 0);
        assert.equal(count(f, 'move'), 0);
    });
    test('online evidence logs at most once per second while counting all successful submissions', () => {
        const f = fixture(AdapterController);
        f.controller.tick(0); f.controller.tick(50); f.controller.tick(999); f.controller.tick(1000);
        const evidence = f.events.filter(event => event.event === 'online_transform');
        assert.equal(evidence.length, 2);
        assert.deepEqual(evidence.map(event => event.successful_submissions), [1, 4]);
        assert.equal(count(f, 'submit'), 4);
    });
    test('online evidence marks an absent own-player echo as null', () => {
        const f = fixture(AdapterController);
        f.state.players = [remote(2)];
        f.controller.tick(0);
        assert.equal(f.events.find(event => event.event === 'online_transform').echoed, null);
    });
    test('an invalid snapshot produces no online success evidence', () => {
        const f = fixture(AdapterController);
        f.state.players = [{ id: 1, ...f.state.transform }, { ...remote(2), heading: NaN }];
        assert.throws(() => f.controller.tick(0), /Invalid snapshot player state/);
        assert.equal(count(f, 'submit'), 1);
        assert.ok(!f.events.some(event => event.event === 'online_transform'));
    });
    test('a failed submission produces no online success evidence or count', () => {
        const f = fixture(AdapterController);
        f.transport.submit = () => false;
        assert.throws(() => f.controller.tick(0), /Local transform submission failed/);
        assert.equal(f.controller.successfulSubmissions, 0);
        assert.ok(!f.events.some(event => event.event === 'online_transform'));
    });
    test('reconnecting resets the submission count and online evidence timer', () => {
        const f = fixture(AdapterController);
        f.controller.tick(0); f.controller.tick(50);
        assert.equal(f.controller.successfulSubmissions, 2);
        assert.equal(f.controller.nextOnlineLog, 1000);
        f.state.pump = false; f.controller.tick(100);
        assert.equal(f.controller.successfulSubmissions, 0);
        assert.equal(f.controller.nextOnlineLog, 0);
        f.state.pump = true; f.controller.tick(1100);
        const evidence = f.events.filter(event => event.event === 'online_transform');
        assert.deepEqual(evidence.map(event => event.successful_submissions), [1, 1]);
        assert.equal(count(f, 'connect'), 2);
    });
    test('setup exception still deletes newly created ped and closes session', () => {
        const f = fixture(AdapterController); f.state.players = [remote()]; f.game.makeRemoteInert = () => { throw new Error('inert failed'); };
        assert.throws(() => f.controller.tick(0), /inert failed/);
        assert.equal(count(f, 'delete'), 1); assert.equal(count(f, 'disconnect'), 1);
    });
    test('remote evidence separates requested commands from observed placement and height error', () => {
        const f = fixture(AdapterController);
        const requested = { x: -30.5, y: -20.25, z: -3.75, heading: 180 };
        const observed = { ...requested, z: -3.25, heading: 179.5 };
        const read = f.game.readTransform;
        f.game.readTransform = handle => handle === 100 ? { ...observed } : read(handle);
        f.state.players = [{ id: 2, ...requested }];
        f.controller.tick(0);
        const applied = f.events.find(event => event.event === 'remote_transform_applied');
        const result = f.events.find(event => event.event === 'remote_transform_observed');
        assert.deepEqual(applied, { event: 'remote_transform_applied', player_id: 2, ped: 100, ...requested });
        assert.deepEqual(result, { event: 'remote_transform_observed', player_id: 2, ped: 100,
            requested, observed, z_error: 0.5 });
        assert.ok(f.events.indexOf(applied) < f.events.indexOf(result));
        f.state.players[0].z = 99;
        observed.z = 42;
        assert.equal(result.requested.z, -3.75);
        assert.equal(result.observed.z, -3.25);
    });
    test('remote readbacks stay at most once per second while movement continues every changed tick', () => {
        const f = fixture(AdapterController);
        const times = [0, 50, 999, 1000, 1001, 1999, 2000];
        for (const [index, time] of times.entries()) {
            f.state.players = [remote(2, -30 - index)];
            f.controller.tick(time);
        }
        assert.equal(count(f, 'move'), times.length);
        assert.equal(count(f, 'read_remote'), 3);
        assert.equal(f.events.filter(event => event.event === 'remote_transform_observed').length, 3);
        assert.equal(f.events.filter(event => event.event === 'remote_transform_applied').length, 3);
        f.controller.tick(3000);
        assert.equal(count(f, 'read_remote'), 4);
        assert.equal(f.events.filter(event => event.event === 'remote_transform_observed').length, 3);
        assert.equal(f.events.filter(event => event.event === 'remote_animation_observed').length, 4);
    });
    test('readback uses each owned remote handle and excludes the local player', () => {
        const f = fixture(AdapterController);
        f.state.players = [remote(1), remote(2, -30), remote(3, -40)];
        f.controller.tick(0);
        assert.deepEqual(f.calls.filter(call => call[0] === 'read_remote'), [['read_remote', 100], ['read_remote', 101]]);
        assert.deepEqual(f.events.filter(event => event.event === 'remote_transform_observed').map(event =>
            [event.player_id, event.ped, event.requested.x, event.observed.x]), [[2, 100, -30, -30], [3, 101, -40, -40]]);
        f.state.players = [remote(1), remote(3, -41)];
        f.controller.tick(1000);
        assert.deepEqual(f.calls.filter(call => call[0] === 'read_remote').at(-1), ['read_remote', 101]);
        assert.ok(!f.events.some(event => event.event === 'remote_transform_observed' && event.player_id === 1));
    });
    test('a recreated remote reads its new handle without inheriting the old evidence timer', () => {
        const f = fixture(AdapterController);
        f.state.players = [remote()];
        f.controller.tick(0);
        f.existing.delete(100);
        f.controller.tick(50);
        assert.deepEqual(f.calls.filter(call => call[0] === 'read_remote'), [['read_remote', 100], ['read_remote', 101]]);
        assert.deepEqual(f.events.filter(event => event.event === 'remote_transform_observed').map(event => event.ped), [100, 101]);
    });
    test('invalid later snapshot state prevents remote reads as well as world mutation', () => {
        const f = fixture(AdapterController);
        f.state.players = [remote(2), { ...remote(3), x: NaN }];
        assert.throws(() => f.controller.tick(0), /Invalid snapshot player state/);
        assert.equal(count(f, 'create'), 0);
        assert.equal(count(f, 'move'), 0);
        assert.equal(count(f, 'read_remote'), 0);
        assert.ok(!f.events.some(event => event.event === 'remote_transform_observed'));
    });
    test('a remote readback exception cleans every owned ped, model and connection', () => {
        const f = fixture(AdapterController);
        f.state.players = [remote(2), remote(3)];
        f.controller.tick(0);
        const read = f.game.readTransform;
        f.game.readTransform = handle => {
            if (handle === 100) throw new Error('readback failed');
            return read(handle);
        };
        f.state.players = [remote(2, 31), remote(3, 32)];
        assert.throws(() => f.controller.tick(1000), /readback failed/);
        assert.deepEqual(f.calls.filter(call => call[0] === 'delete'), [['delete', 100], ['delete', 101]]);
        assert.equal(count(f, 'release'), 1);
        assert.equal(count(f, 'disconnect'), 1);
        assert.equal(f.controller.running, false);
        assert.equal(f.controller.remotes.size, 0);
        assert.equal(f.events.filter(event => event.event === 'remote_transform_observed').length, 2);
    });
    for (const [name, observed] of [
        ['NaN', { ...remote(), x: NaN }], ['infinity', { ...remote(), y: Infinity }],
        ['out-of-range coordinates', { ...remote(), z: 100001 }], ['out-of-range heading', { ...remote(), heading: -100001 }],
        ['nonnumeric value', { ...remote(), z: '3' }], ['missing heading', { x: 30, y: 20, z: 3 }],
        ['null value', null]
    ]) test('remote readback rejects ' + name + ' before observed evidence and cleans its owned ped', () => {
        const f = fixture(AdapterController);
        const read = f.game.readTransform;
        f.game.readTransform = handle => handle === 100 ? observed : read(handle);
        f.state.players = [remote()];
        assert.throws(() => f.controller.tick(0), /invalid observed transform/);
        assert.equal(count(f, 'delete'), 1);
        assert.equal(count(f, 'release'), 1);
        assert.equal(count(f, 'disconnect'), 1);
        assert.equal(f.controller.running, false);
        assert.ok(!f.events.some(event => event.event === 'remote_transform_observed'));
    });

    test('new remote starts idle and repeated cached snapshots do not replay its task', () => {
        const f = fixture(AdapterController); f.state.players = [remote()];
        for (let now = 0; now <= 2000; now += 50) f.controller.tick(now);
        assert.deepEqual(f.calls.filter(call => call[0] === 'play_animation'), [['play_animation', 100, 'idle_stance']]);
        assert.equal(count(f, 'move'), 1);
        assert.equal(count(f, 'read_animation'), 3);
        assert.equal(count(f, 'read_remote'), 3);
        assert.deepEqual(f.events.filter(event => event.event === 'remote_animation_observed').map(event =>
            [event.state, event.playing, event.time, event.z_error]),
            [['idle', true, 0.42, 0], ['idle', true, 0.42, 0], ['idle', true, 0.42, 0]]);
        assert.equal(f.events.filter(event => event.event === 'remote_transform_applied').length, 1);
    });
    test('horizontal movement starts walking once and cached snapshots expire to idle at 350 ms', () => {
        const f = fixture(AdapterController); f.state.players = [remote()]; f.controller.tick(0);
        f.state.players = [remote(2, 30.1)]; f.controller.tick(50);
        f.controller.tick(100); f.controller.tick(300); f.controller.tick(399);
        assert.deepEqual(f.calls.filter(call => call[0] === 'play_animation').map(call => call[2]), ['idle_stance', 'Walk_civi']);
        f.controller.tick(400);
        assert.deepEqual(f.calls.filter(call => call[0] === 'play_animation').map(call => call[2]), ['idle_stance', 'Walk_civi', 'idle_stance']);
        assert.equal(count(f, 'move'), 2);
    });
    for (const [name, transform] of [
        ['heading only', { ...remote(), heading: 181 }], ['height only', { ...remote(), z: 4 }],
        ['horizontal jitter', remote(2, 30.01)]
    ]) test(name + ' does not start walking', () => {
        const f = fixture(AdapterController); f.state.players = [remote()]; f.controller.tick(0);
        f.state.players = [transform]; f.controller.tick(50);
        assert.deepEqual(f.calls.filter(call => call[0] === 'play_animation').map(call => call[2]), ['idle_stance']);
    });
    for (const [name, elapsed, displacement] of [
        ['minimum plausible speed', 80, 0.02], ['maximum plausible speed', 50, 0.75],
        ['maximum sample interval', 250, 0.0625]
    ]) test('walking accepts ' + name + ' boundary', () => {
        const f = fixture(AdapterController); f.state.players = [remote(2, 0)]; f.controller.tick(0);
        f.state.players = [remote(2, displacement)]; f.controller.tick(elapsed);
        assert.equal(f.calls.filter(call => call[0] === 'play_animation').at(-1)[2], 'Walk_civi');
    });
    for (const [name, nextTime, nextX] of [
        ['same timestamp', 50, 30.2], ['reversed time', 49, 30.2],
        ['long pause', 301, 30.2], ['teleport', 100, 50]
    ]) test(name + ' resets existing walking to idle', () => {
        const f = fixture(AdapterController); f.state.players = [remote()]; f.controller.tick(0);
        f.state.players = [remote(2, 30.1)]; f.controller.tick(50);
        f.state.players = [remote(2, nextX)]; f.controller.tick(nextTime);
        assert.equal(f.calls.filter(call => call[0] === 'play_animation').at(-1)[2], 'idle_stance');
    });
    test('reversed clock resets playback grace and evidence without replaying an unchanged idle task', () => {
        const f = fixture(AdapterController); f.state.players = [remote()]; f.controller.tick(1000);
        f.game.readRemoteAnimation = () => ({ playing: false, time: null });
        f.controller.tick(900);
        const observed = f.events.filter(event => event.event === 'remote_animation_observed').at(-1);
        assert.equal(observed.task_age_ms, 0); assert.equal(observed.pending, true);
        assert.equal(f.events.filter(event => event.event === 'remote_animation_clock_reset').length, 1);
        assert.equal(count(f, 'play_animation'), 1);
        f.controller.tick(1899);
        assert.throws(() => f.controller.tick(1900), /did not start/);
    });
    test('idle evidence detects coordinate drift even without another position setter', () => {
        const f = fixture(AdapterController); f.state.players = [remote()]; f.controller.tick(0);
        const read = f.game.readTransform;
        f.game.readTransform = handle => handle === 100 ? { ...remote(), x: 30.5, z: 3.25 } : read(handle);
        f.controller.tick(1000);
        const observed = f.events.filter(event => event.event === 'remote_animation_observed').at(-1);
        assert.equal(observed.requested_transform.x, 30); assert.equal(observed.observed_transform.x, 30.5);
        assert.equal(observed.z_error, 0.25); assert.equal(count(f, 'move'), 1);
    });
    test('missing animation is pending during scheduling grace and fails with all owned cleanup afterward', () => {
        const f = fixture(AdapterController); f.state.players = [remote(2), remote(3)];
        f.game.readRemoteAnimation = () => ({ playing: false, time: null });
        f.controller.tick(0); f.controller.tick(999);
        assert.ok(f.events.filter(event => event.event === 'remote_animation_observed').every(event => event.pending && event.time === null));
        assert.throws(() => f.controller.tick(1000), /Remote animation did not start/);
        assert.deepEqual(f.calls.filter(call => call[0] === 'delete'), [['delete', 100], ['delete', 101]]);
        assert.equal(count(f, 'release'), 1); assert.equal(count(f, 'disconnect'), 1);
        assert.equal(f.controller.remotes.size, 0); assert.equal(f.controller.running, false);
        assert.equal(count(f, 'play_animation'), 2);
        assert.equal(f.events.filter(event => event.event === 'remote_animation_observed').at(-1).pending, false);
    });
    for (const [name, evidence] of [
        ['NaN phase', { playing: true, time: NaN }], ['infinite phase', { playing: true, time: Infinity }],
        ['negative phase', { playing: true, time: -0.01 }], ['phase over one', { playing: true, time: 1.01 }],
        ['string phase', { playing: true, time: '0.5' }], ['missing phase', { playing: true }],
        ['nonnumeric condition', { playing: 1, time: 0.5 }], ['absent association with a phase', { playing: false, time: 0.5 }]
    ]) test('invalid animation evidence rejects ' + name + ' and cleans its owned session', () => {
        const f = fixture(AdapterController); f.state.players = [remote()]; f.game.readRemoteAnimation = () => evidence;
        assert.throws(() => f.controller.tick(0), /invalid animation evidence/);
        assert.equal(count(f, 'delete'), 1); assert.equal(count(f, 'release'), 1); assert.equal(count(f, 'disconnect'), 1);
        assert.ok(!f.events.some(event => event.event === 'remote_animation_observed'));
    });
    for (const name of ['playRemoteAnimation', 'readRemoteAnimation']) test(name + ' exception still cleans every existing proxy', () => {
        const f = fixture(AdapterController); f.state.players = [remote(2), remote(3)]; f.controller.tick(0);
        f.game[name] = () => { throw new Error('animation native failed'); };
        f.state.players = [remote(2, 30.1), remote(3)];
        assert.throws(() => f.controller.tick(name === 'playRemoteAnimation' ? 50 : 1000), /animation native failed/);
        assert.deepEqual(f.calls.filter(call => call[0] === 'delete'), [['delete', 100], ['delete', 101]]);
        assert.equal(count(f, 'release'), 1); assert.equal(count(f, 'disconnect'), 1);
    });
    test('replacement handle starts its own idle state and evidence timer', () => {
        const f = fixture(AdapterController); f.state.players = [remote()]; f.controller.tick(0);
        f.state.players = [remote(2, 30.1)]; f.controller.tick(50);
        f.existing.delete(100); f.controller.tick(100);
        assert.deepEqual(f.calls.filter(call => call[0] === 'play_animation'),
            [['play_animation', 100, 'idle_stance'], ['play_animation', 100, 'Walk_civi'], ['play_animation', 101, 'idle_stance']]);
        assert.equal(f.events.filter(event => event.event === 'remote_animation_observed').at(-1).ped, 101);
    });
    test('independent remotes keep separate walking, idle and bounded playback observations', () => {
        const f = fixture(AdapterController); f.state.players = [remote(1), remote(2), remote(3, 40)];
        for (let now = 0; now <= 2000; now += 50) {
            f.state.players = [remote(1), remote(2, 30 + now / 1000), remote(3, 40)];
            f.controller.tick(now);
        }
        assert.deepEqual(f.calls.filter(call => call[0] === 'play_animation'),
            [['play_animation', 100, 'idle_stance'], ['play_animation', 101, 'idle_stance'], ['play_animation', 100, 'Walk_civi']]);
        assert.equal(count(f, 'read_animation'), 6);
        assert.ok(f.calls.filter(call => call[0] === 'read_animation').every(call => call[1] !== 10));
        assert.deepEqual(f.events.filter(event => event.event === 'remote_animation_observed' && event.player_id === 2).map(event => event.state), ['idle', 'walk', 'walk']);
    });

    // Display interpolation must be verified against requested engine positions,
    // independently of a transport mock that merely returns the newest target.
    const smooth = () => fixture(AdapterController, 'online', { renderSmoothing: true });
    const moved = (f, handle = 100) => f.calls.filter(call => call[0] === 'move' && call[1] === handle).at(-1)[2];
    const near = (actual, expected) => assert.ok(Math.abs(actual - expected) < 1e-9, `${actual} != ${expected}`);
    test('smoothing keeps the current display pose when a network target arrives, then blends and settles', () => {
        const f = smooth(); f.state.players = [remote(2, 30, 350)]; f.controller.tick(0);
        assert.equal(moved(f).x, 30); assert.equal(moved(f).y, 20);
        assert.equal(moved(f).z, 3); assert.equal(moved(f).heading, 350);
        f.state.players = [remote(2, 30.1, 10)]; f.controller.tick(50);
        assert.equal(moved(f).x, 30);
        f.controller.tick(75);
        near(moved(f).x, 30.05); near(moved(f).heading, 0);
        f.controller.tick(100);
        near(moved(f).x, 30.1); near(moved(f).heading, 10);
    });
    test('smoothing renders between network polls without submitting at the display rate', () => {
        const f = smooth(); f.state.players = [remote()];
        for (const now of [0, 16, 32, 48, 64, 80, 96, 112]) f.controller.tick(now);
        assert.equal(count(f, 'submit'), 3);
        assert.equal(count(f, 'move'), 1);
        assert.equal(count(f, 'play_animation'), 1);
    });
    test('smoothing reaches a stationary target once and never extrapolates beyond it', () => {
        const f = smooth(); f.state.players = [remote()]; f.controller.tick(0);
        f.state.players = [remote(2, 30.1)]; f.controller.tick(50); f.controller.tick(75); f.controller.tick(100);
        const writes = count(f, 'move');
        for (let now = 125; now <= 1000; now += 25) f.controller.tick(now);
        assert.equal(count(f, 'move'), writes); near(moved(f).x, 30.1);
        assert.equal(count(f, 'read_remote'), 2);
    });
    test('repeated cached targets do not restart a blend before its deadline', () => {
        const f = smooth(); f.state.players = [remote()]; f.controller.tick(0);
        f.state.players = [remote(2, 30.1)]; f.controller.tick(50);
        for (const now of [66, 82, 98, 100]) f.controller.tick(now);
        near(moved(f).x, 30.1);
        const writes = count(f, 'move'); f.controller.tick(116); f.controller.tick(150);
        assert.equal(count(f, 'move'), writes);
    });
    test('smoothed heading takes the short wraparound path in either direction', () => {
        for (const [from, to] of [[350, 10], [10, 350]]) {
            const f = smooth(); f.state.players = [remote(2, 30, from)]; f.controller.tick(0);
            f.state.players = [remote(2, 30, to)]; f.controller.tick(50); f.controller.tick(75);
            near(moved(f).heading, 0); f.controller.tick(100); near(moved(f).heading, to);
            assert.deepEqual(f.calls.filter(call => call[0] === 'play_animation').map(call => call[2]), ['idle_stance']);
        }
    });
    test('smoothing snaps teleports rather than interpolating through unrelated world positions', () => {
        const f = smooth(); f.state.players = [remote()]; f.controller.tick(0);
        f.state.players = [remote(2, 31)]; f.controller.tick(50);
        assert.equal(moved(f).x, 31);
        assert.deepEqual(f.calls.filter(call => call[0] === 'play_animation').map(call => call[2]), ['idle_stance']);
    });
    test('smoothing snaps the whole pose when height changes and keeps animation based on horizontal motion', () => {
        const f = smooth(); f.state.players = [remote()]; f.controller.tick(0);
        f.state.players = [{ ...remote(2, 30.1, 190), z: 3.5 }]; f.controller.tick(50);
        assert.equal(moved(f).x, 30.1); assert.equal(moved(f).z, 3.5); assert.equal(moved(f).heading, 190);
        assert.equal(f.calls.filter(call => call[0] === 'play_animation').at(-1)[2], 'Walk_civi');
    });
    test('smoothing snaps a fresh target after a long display gap', () => {
        const f = smooth(); f.state.players = [remote()]; f.controller.tick(0);
        f.state.players = [remote(2, 30.1)]; f.controller.tick(50); f.controller.tick(75);
        f.state.players = [remote(2, 30.2)]; f.controller.tick(400);
        assert.equal(moved(f).x, 30.2);
        assert.equal(f.calls.filter(call => call[0] === 'play_animation').at(-1)[2], 'idle_stance');
    });
    test('smoothing snaps a target with a stale network interval even at the display gap boundary', () => {
        const f = smooth(); f.state.players = [remote()]; f.controller.tick(0);
        f.state.players = [remote(2, 30.1)]; f.controller.tick(50); f.controller.tick(75);
        f.state.players = [remote(2, 30.2)]; f.controller.tick(325);
        assert.equal(moved(f).x, 30.2);
    });
    test('smoothing resets display and network schedules on a reversed clock', () => {
        const f = smooth(); f.state.players = [remote(2, 30, 350)]; f.controller.tick(0);
        f.state.players = [remote(2, 30.1, 10)]; f.controller.tick(50); f.controller.tick(75);
        f.state.players = [remote(2, 30.2, 20)]; f.controller.tick(60);
        assert.equal(moved(f).x, 30.2); assert.equal(moved(f).heading, 20);
        assert.equal(count(f, 'submit'), 3);
        assert.equal(f.calls.filter(call => call[0] === 'play_animation').at(-1)[2], 'idle_stance');
        f.controller.tick(76); assert.equal(count(f, 'submit'), 3);
        f.controller.tick(110); assert.equal(count(f, 'submit'), 4);
    });
    test('smoothing accepts a speed of fifteen units per second at the interpolation boundary', () => {
        const f = smooth(); f.state.players = [remote(2, 0)]; f.controller.tick(0);
        f.state.players = [remote(2, 0.75)]; f.controller.tick(50);
        assert.equal(moved(f).x, 0); f.controller.tick(75); near(moved(f).x, 0.375);
    });
    test('smoothing retains walking inferred from network displacement while display increments are below its threshold', () => {
        const f = smooth(); f.state.players = [remote(2, 0)]; f.controller.tick(0);
        f.state.players = [remote(2, 0.02)]; f.controller.tick(50); f.controller.tick(75);
        near(moved(f).x, 0.01);
        assert.deepEqual(f.calls.filter(call => call[0] === 'play_animation').map(call => call[2]), ['idle_stance', 'Walk_civi']);
        for (const now of [100, 150, 200, 250, 300, 350, 399]) f.controller.tick(now);
        assert.equal(f.calls.filter(call => call[0] === 'play_animation').at(-1)[2], 'Walk_civi');
        f.controller.tick(400);
        assert.equal(f.calls.filter(call => call[0] === 'play_animation').at(-1)[2], 'idle_stance');
    });
    test('smoothing keeps distinct peers on independent display paths', () => {
        const f = smooth(); f.state.players = [remote(2, 30), remote(3, 40)]; f.controller.tick(0);
        f.state.players = [remote(2, 30.1), remote(3, 40)]; f.controller.tick(50); f.controller.tick(75);
        near(moved(f, 100).x, 30.05); assert.equal(moved(f, 101).x, 40);
        assert.equal(f.calls.filter(call => call[0] === 'move' && call[1] === 101).length, 1);
        assert.ok(!f.calls.some(call => call[0] === 'move' && call[1] === 10));
    });
    test('smoothing replaces a missing owned ped without writing or inheriting interpolation from its stale handle', () => {
        const f = smooth(); f.state.players = [remote()]; f.controller.tick(0);
        f.state.players = [remote(2, 30.1)]; f.controller.tick(50); f.controller.tick(75);
        f.existing.delete(100); const offset = f.calls.length; f.controller.tick(100);
        assert.ok(!f.calls.slice(offset).some(call => call[0] === 'move' && call[1] === 100));
        assert.equal(count(f, 'create'), 2); assert.equal(moved(f, 101).x, 30.1);
        assert.equal(f.calls.filter(call => call[0] === 'play_animation').at(-1)[2], 'idle_stance');
    });
    test('smoothing checks stop before another display write or network poll and cleans owned state once', () => {
        const f = smooth(); f.state.players = [remote()]; f.controller.tick(0);
        f.state.players = [remote(2, 30.1)]; f.controller.tick(50);
        const writes = count(f, 'move'), submissions = count(f, 'submit');
        f.state.stop = true; f.controller.tick(75); f.controller.tick(100);
        assert.equal(count(f, 'move'), writes); assert.equal(count(f, 'submit'), submissions);
        assert.deepEqual(f.calls.filter(call => call[0] === 'delete'), [['delete', 100]]);
        assert.equal(count(f, 'disconnect'), 1); assert.equal(f.controller.remotes.size, 0);
    });
    test('smoothing deletes a departed peer instead of continuing its unfinished display blend', () => {
        const f = smooth(); f.state.players = [remote()]; f.controller.tick(0);
        f.state.players = [remote(2, 30.1)]; f.controller.tick(50); f.controller.tick(75);
        const offset = f.calls.length; f.state.players = []; f.controller.tick(100); f.controller.tick(125);
        assert.deepEqual(f.calls.filter(call => call[0] === 'delete'), [['delete', 100]]);
        assert.ok(!f.calls.slice(offset).some(call => call[0] === 'move'));
    });
    test('smoothing validates an entire fresh snapshot before advancing any displayed peer', () => {
        const f = smooth(); f.state.players = [remote(2), remote(3, 40)]; f.controller.tick(0);
        f.state.players = [remote(2, 30.1), remote(3, 40.1)]; f.controller.tick(50); f.controller.tick(75);
        const offset = f.calls.length; f.state.players = [remote(2, 30.2), { ...remote(3, 40.2), x: NaN }];
        assert.throws(() => f.controller.tick(100), /Invalid snapshot player state/);
        assert.ok(!f.calls.slice(offset).some(call => call[0] === 'move'));
        assert.deepEqual(f.calls.filter(call => call[0] === 'delete'), [['delete', 100], ['delete', 101]]);
        assert.equal(count(f, 'release'), 1); assert.equal(count(f, 'disconnect'), 1);
    });
    test('smoothing checks readiness loss between network polls and clears the owned session', () => {
        const f = smooth(); f.state.players = [remote()]; f.controller.tick(0);
        f.state.players = [remote(2, 30.1)]; f.controller.tick(50);
        f.state.ready = false; f.game.readTransform = () => { throw new Error('must not read'); }; f.controller.tick(66);
        assert.equal(count(f, 'delete'), 1); assert.equal(count(f, 'disconnect'), 1); assert.equal(f.controller.remotes.size, 0);
    });
    test('smoothing checks local handle replacement between network polls and clears the old connection', () => {
        const f = smooth(); f.state.players = [remote()]; f.controller.tick(0);
        f.state.ped = 11; f.existing.add(11); f.controller.tick(16);
        assert.equal(count(f, 'delete'), 1); assert.equal(count(f, 'disconnect'), 1); assert.equal(count(f, 'connect'), 1);
    });
    test('smoothing reports requested display placement separately from the network target and actual readback once per second', () => {
        const f = smooth(); const read = f.game.readTransform;
        f.game.readTransform = handle => { const value = read(handle); return handle === 100 ? { ...value, z: value.z + 0.125 } : value; };
        for (let now = 0; now <= 2016; now += 16) {
            f.state.players = [remote(2, 30 + now / 1000)]; f.controller.tick(now);
        }
        const evidence = f.events.filter(event => event.event === 'remote_motion_observed');
        assert.equal(evidence.length, 3); assert.equal(count(f, 'read_remote'), 3);
        assert.equal(f.events.filter(event => event.event === 'remote_animation_observed').length, 3);
        const middle = evidence[1];
        near(middle.network_target.x, 31.008); assert.ok(middle.requested_transform.x < middle.network_target.x);
        assert.equal(middle.observed_transform.x, middle.requested_transform.x);
        assert.equal(middle.observed_transform.z, middle.requested_transform.z + 0.125);
        assert.ok(middle.smoothing_progress >= 0 && middle.smoothing_progress <= 1);
        assert.ok(middle.target_age_ms >= 0);
        assert.ok(middle.display_write_count_since_evidence > middle.network_target_updates_since_evidence);
        assert.equal(middle.network_target_updates_since_evidence, 20);
        assert.equal(middle.display_interval_min_ms, 16); assert.ok(middle.display_interval_max_ms <= 80);
        assert.equal(middle.network_poll_count_since_evidence, 20);
        assert.equal(middle.network_interval_min_ms, 48); assert.equal(middle.network_interval_max_ms, 64);
        assert.equal(middle.evidence_elapsed_ms, 1008);
        const animation = f.events.filter(event => event.event === 'remote_animation_observed')[1];
        assert.deepEqual(animation.network_target, middle.network_target);
        assert.deepEqual(animation.requested_transform, middle.requested_transform);
        assert.deepEqual(animation.observed_transform, middle.observed_transform);
        f.state.players[0].x = 99;
        near(middle.network_target.x, 31.008);
    });
    test('smoothing continues bounded stationary drift evidence without flooding coordinate writes', () => {
        const f = smooth(); f.state.players = [remote()]; f.controller.tick(0);
        const read = f.game.readTransform;
        f.game.readTransform = handle => handle === 100 ? { ...remote(), x: 30.25 } : read(handle);
        for (let now = 16; now <= 1024; now += 16) f.controller.tick(now);
        const evidence = f.events.filter(event => event.event === 'remote_motion_observed');
        assert.equal(evidence.length, 2); assert.equal(count(f, 'move'), 1);
        assert.equal(evidence[1].network_target.x, 30); assert.equal(evidence[1].requested_transform.x, 30);
        assert.equal(evidence[1].observed_transform.x, 30.25);
        assert.equal(evidence[1].display_write_count_since_evidence, 0);
        assert.equal(evidence[1].network_target_updates_since_evidence, 0);
    });
    test('smoothing completes the previous received segment before accepting another target on the same poll', () => {
        const f = smooth(); f.state.players = [remote(2, 30)]; f.controller.tick(0);
        f.state.players = [remote(2, 30.1)]; f.controller.tick(50); f.controller.tick(75);
        f.state.players = [remote(2, 30.2)]; f.controller.tick(100);
        near(moved(f).x, 30.1); f.controller.tick(125); near(moved(f).x, 30.15);
        f.controller.tick(150); near(moved(f).x, 30.2);
    });
    for (const [name, now, target] of [
        ['teleport', 100, remote(2, 31)], ['height change', 100, { ...remote(2, 30.2), z: 4 }],
        ['stale network interval', 325, remote(2, 30.2)], ['long display gap', 400, remote(2, 30.2)]
    ]) test('smoothing writes a ' + name + ' target once without first writing the obsolete segment endpoint', () => {
        const f = smooth(); f.state.players = [remote()]; f.controller.tick(0);
        f.state.players = [remote(2, 30.1)]; f.controller.tick(50); f.controller.tick(75);
        const offset = f.calls.length; f.state.players = [target]; f.controller.tick(now);
        const writes = f.calls.slice(offset).filter(call => call[0] === 'move');
        assert.equal(writes.length, 1);
        assert.equal(writes[0][2].x, target.x); assert.equal(writes[0][2].z, target.z);
    });
    for (const value of [1, 'true', null]) test('smoothing option rejects nonboolean ' + JSON.stringify(value), () => {
        assert.throws(() => fixture(AdapterController, 'online', { renderSmoothing: value }), /smoothing/i);
    });

    const gait = (options = {}) => fixture(AdapterController, 'online', { renderSmoothing: true, gaitProbe: true, ...options });
    const gaitSamples = f => f.events.filter(event => event.event === 'remote_gait_sample');
    const gaitFinished = f => f.events.filter(event => event.event === 'remote_gait_probe_finished');
    test('gait probe is opt-in and smoke never samples a remote', () => {
        for (const f of [fixture(AdapterController), fixture(AdapterController, 'smoke', { gaitProbe: true })]) {
            f.state.players = [remote()]; f.controller.tick(0); f.controller.tick(100);
            assert.equal(gaitSamples(f).length, 0);
            assert.ok(!f.events.some(event => event.event === 'remote_gait_probe_started'));
        }
    });
    test('gait sample preserves actual time, clip identity and nonzero initial phase', () => {
        const f = gait(); f.state.players = [remote()]; f.controller.tick(12345);
        const started = f.events.find(event => event.event === 'remote_gait_probe_started');
        assert.equal(gaitSamples(f).length, 1);
        assert.equal(started.interval_ms, 100); assert.equal(started.budget_ms, 30000); assert.equal(started.max_samples, 300);
        assert.deepEqual(gaitSamples(f)[0], { event: 'remote_gait_sample', player_id: 2, ped: 100,
            sample_at_ms: 12345, probe_elapsed_ms: 0, task_age_ms: 0, state: 'idle', animation_file: 'PED',
            clip: 'idle_stance', playing: true, time: 0.42,
            requested_transform: { x: 30, y: 20, z: 3, heading: 180 },
            observed_transform: { x: 30, y: 20, z: 3, heading: 180 }, network_target: { x: 30, y: 20, z: 3, heading: 180 } });
        assert.equal(count(f, 'read_remote'), 1); assert.equal(count(f, 'read_animation'), 1);
    });
    test('gait probe rejects a nonboolean option', () => {
        assert.throws(() => gait({ gaitProbe: 'yes' }), /gait probe/i);
    });
    test('gait samples only on network polls and adds no transport calls', () => {
        const f = gait(); f.state.players = [remote()];
        for (const now of [0, 16, 32, 48, 64, 80, 96, 112, 128, 144, 160, 176, 192, 208, 224]) f.controller.tick(now);
        assert.deepEqual(gaitSamples(f).map(event => event.sample_at_ms), [0, 112]);
        assert.equal(count(f, 'submit'), 5); assert.equal(count(f, 'read_remote'), 2);
    });
    test('gait probe shares existing one-second readback without duplicate native queries', () => {
        const f = gait(); f.state.players = [remote()];
        for (let now = 0; now <= 1000; now += 50) f.controller.tick(now);
        assert.equal(gaitSamples(f).length, 11); assert.equal(count(f, 'read_remote'), 11); assert.equal(count(f, 'read_animation'), 11);
        assert.equal(f.events.filter(event => event.event === 'remote_animation_observed').length, 2);
    });
    test('gait sample separates the displayed pose from a newer received target', () => {
        const f = gait(); f.state.players = [remote()]; f.controller.tick(0);
        f.state.players = [remote(2, 30.1)]; f.controller.tick(50);
        f.state.players = [remote(2, 30.2)]; f.controller.tick(100);
        const sample = gaitSamples(f).at(-1);
        near(sample.requested_transform.x, 30.1); near(sample.observed_transform.x, 30.1); near(sample.network_target.x, 30.2);
        assert.equal(sample.clip, 'Walk_civi'); assert.equal(sample.task_age_ms, 50); assert.equal(sample.sample_at_ms, 100);
    });
    test('gait probe selects one proxy and never switches after departure', () => {
        const f = gait(); f.state.players = [remote(2), remote(3)]; f.controller.tick(0);
        f.state.players = [remote(3)]; f.controller.tick(100); f.controller.tick(200);
        assert.ok(gaitSamples(f).every(event => event.player_id === 2 && event.ped === 100));
        assert.equal(gaitFinished(f).length, 1); assert.equal(gaitFinished(f)[0].reason, 'deleted');
        assert.equal(f.events.filter(event => event.event === 'remote_gait_probe_started').length, 1);
        assert.ok(f.events.indexOf(gaitFinished(f)[0]) < f.events.findIndex(event => event.event === 'remote_ped_deleted'));
    });
    test('gait probe count budget finishes exactly once and stops extra reads', () => {
        const f = gait(); f.state.players = [remote()];
        for (let now = 0; now <= 29900; now += 100) f.controller.tick(now);
        assert.equal(gaitSamples(f).length, 300); assert.equal(gaitFinished(f).length, 1);
        assert.equal(gaitFinished(f)[0].reason, 'sample_budget'); assert.equal(gaitFinished(f)[0].samples, 300);
        const reads = count(f, 'read_remote'); f.controller.tick(29950); f.controller.tick(30000); f.controller.stop();
        assert.equal(count(f, 'read_remote'), reads + 1); assert.equal(gaitFinished(f).length, 1);
    });
    test('gait probe elapsed budget ends before a sample at thirty seconds', () => {
        const f = gait(); f.state.players = [remote()];
        for (let now = 0; now <= 30000; now += 150) f.controller.tick(now);
        assert.equal(gaitSamples(f).length, 200); assert.equal(gaitFinished(f)[0].reason, 'time_budget');
        assert.equal(gaitFinished(f)[0].probe_elapsed_ms, 30000);
        assert.ok(gaitSamples(f).every(event => event.probe_elapsed_ms < 30000));
    });
    for (const [name, action, reason] of [
        ['stop', f => { f.state.stop = true; }, 'stop'],
        ['loading', f => { f.state.ready = false; }, 'loading'],
        ['transport loss', f => { f.state.pump = false; }, 'transport_lost'],
        ['local replacement', f => { f.state.ped = 11; f.existing.add(11); }, 'local_ped_changed'],
        ['missing engine proxy', f => { f.existing.delete(100); }, 'missing_ped']
    ]) test('gait probe finishes before ' + name + ' and never reads a stale proxy', () => {
        const f = gait(); f.state.players = [remote()]; f.controller.tick(0); const reads = count(f, 'read_remote');
        action(f); f.controller.tick(100);
        assert.equal(gaitSamples(f).length, 1); assert.equal(gaitFinished(f).length, 1); assert.equal(gaitFinished(f)[0].reason, reason);
        if (name !== 'missing engine proxy') assert.equal(count(f, 'read_remote'), reads);
        else assert.ok(!f.calls.slice(f.calls.findIndex(call => call[0] === 'create' && call[1] === 101)).some(call => call[0] === 'read_remote' && call[1] === 100));
    });
    test('gait probe starts a fresh bounded selection after reconnecting', () => {
        const f = gait(); f.state.players = [remote()]; f.controller.tick(0);
        f.state.pump = false; f.controller.tick(100); f.state.pump = true; f.controller.tick(1100);
        assert.equal(f.events.filter(event => event.event === 'remote_gait_probe_started').length, 2);
        assert.deepEqual(gaitSamples(f).map(event => [event.ped, event.probe_elapsed_ms]), [[100, 0], [101, 0]]);
    });
    test('gait probe rejects clock reversal without negative ages or a second selection', () => {
        const f = gait(); f.state.players = [remote()]; f.controller.tick(1000); f.controller.tick(1100); f.controller.tick(1099);
        f.controller.tick(1200);
        assert.equal(gaitFinished(f)[0].reason, 'clock_reversed'); assert.equal(gaitSamples(f).length, 2);
        assert.ok(gaitSamples(f).every(event => event.task_age_ms >= 0 && event.probe_elapsed_ms >= 0));
        assert.equal(f.events.filter(event => event.event === 'remote_gait_probe_started').length, 1);
    });
    for (const [name, now, player] of [
        ['render_gap', 300, remote()], ['teleport', 100, remote(2, 33)], ['height_change', 100, { ...remote(), z: 4 }]
    ]) test('gait probe finishes at ' + name + ' without sampling the discontinuity', () => {
        const f = gait(); f.state.players = [remote()]; f.controller.tick(0); f.state.players = [player]; f.controller.tick(now);
        assert.equal(gaitSamples(f).length, 1); assert.equal(gaitFinished(f)[0].reason, name);
    });
    test('gait probe records absent playback as null without fabricating a phase', () => {
        const f = gait(); f.game.readRemoteAnimation = () => ({ playing: false, time: null });
        f.state.players = [remote()]; f.controller.tick(0); f.controller.tick(100);
        assert.ok(gaitSamples(f).every(event => event.playing === false && event.time === null));
    });
    for (const [name, replacement] of [
        ['animation', f => { f.game.readRemoteAnimation = () => ({ playing: true, time: NaN }); }],
        ['transform', f => { const read = f.game.readTransform; f.game.readTransform = handle => handle === 100 ? { x: 1, y: 2, z: Infinity, heading: 0 } : read(handle); }]
    ]) test('gait probe never logs a partially invalid ' + name + ' sample', () => {
        const f = gait(); f.state.players = [remote()]; f.controller.tick(0); replacement(f);
        assert.throws(() => f.controller.tick(100), /invalid.*evidence|invalid observed transform/i);
        assert.equal(gaitSamples(f).length, 1); assert.equal(gaitFinished(f).length, 1); assert.equal(count(f, 'delete'), 1);
    });
    test('gait probe does not start or emit a negative absolute clock', () => {
        const f = gait(); f.state.players = [remote()];
        assert.throws(() => f.controller.tick(-1), /Invalid adapter clock/);
        assert.equal(count(f, 'create'), 0); assert.equal(gaitSamples(f).length, 0);
    });
    test('gait probe never adopts a game-created handle equal to the local player', () => {
        const f = gait(); f.game.createPed = () => 10; f.state.players = [remote()];
        assert.throws(() => f.controller.tick(0), /Remote ped creation failed/);
        assert.equal(count(f, 'inert'), 0); assert.equal(count(f, 'delete'), 0); assert.equal(gaitSamples(f).length, 0);
    });
    test('gait probe preserves the new clip phase and absolute stamp through an idle transition', () => {
        const f = gait(); f.game.readRemoteAnimation = (handle, clip) => ({ playing: true, time: clip === 'idle_stance' ? 0.72 : 0.26 });
        f.state.players = [remote()]; f.controller.tick(1000);
        f.state.players = [remote(2, 30.1)];
        for (let now = 1050; now <= 1400; now += 50) f.controller.tick(now);
        const samples = gaitSamples(f);
        assert.deepEqual(samples.map(event => [event.sample_at_ms, event.clip, event.time, event.task_age_ms]),
            [[1000, 'idle_stance', 0.72, 0], [1100, 'Walk_civi', 0.26, 50], [1200, 'Walk_civi', 0.26, 150],
                [1300, 'Walk_civi', 0.26, 250], [1400, 'idle_stance', 0.72, 0]]);
        assert.equal(gaitFinished(f).length, 0);
    });
    test('gait probe finishes a vanished ped between network ticks without reading it again', () => {
        const f = gait(); f.state.players = [remote()]; f.controller.tick(0); f.existing.delete(100);
        f.controller.tick(16); f.controller.tick(100);
        assert.equal(gaitFinished(f)[0].reason, 'missing_ped'); assert.equal(gaitSamples(f).length, 1);
        assert.equal(f.calls.filter(call => call[0] === 'read_remote' && call[1] === 100).length, 1);
        assert.equal(f.events.filter(event => event.event === 'remote_gait_probe_started').length, 1);
    });
    test('gait probe takes one irregular sample per tick and does not catch up missed deadlines', () => {
        const f = gait(); f.state.players = [remote()];
        for (const now of [0, 240, 250, 300, 340, 350, 450]) f.controller.tick(now);
        assert.deepEqual(gaitSamples(f).map(event => event.sample_at_ms), [0, 240, 350, 450]);
        assert.equal(gaitFinished(f).length, 0); assert.equal(count(f, 'submit'), 6);
    });
    test('gait probe ends on a long gap without stopping normal gameplay replication', () => {
        const f = gait(); f.state.players = [remote()]; f.controller.tick(0); f.controller.tick(400); f.controller.tick(500);
        assert.equal(gaitFinished(f)[0].reason, 'render_gap'); assert.equal(gaitSamples(f).length, 1);
        assert.equal(f.controller.running, true); assert.equal(count(f, 'delete'), 0); assert.equal(count(f, 'submit'), 3);
    });
    test('gait probe can start at the first remote after a clock reversal before its selection', () => {
        const f = gait(); f.controller.tick(1000); f.controller.tick(999); f.state.players = [remote()]; f.controller.tick(1050);
        assert.equal(gaitSamples(f).length, 1); assert.equal(gaitSamples(f)[0].sample_at_ms, 1050);
        assert.equal(gaitFinished(f).length, 0);
    });

    const rateFixture = (options = {}) => fixture(AdapterController, 'online', { gaitRate: true, ...options });
    const rates = f => f.events.filter(event => event.event === 'remote_animation_rate_applied');
    const rateTick = (f, now, x) => {
        f.state.now = now;
        f.state.players = [remote(2, x)];
        f.controller.tick(now);
    };
    const constantSpeed = (f, speed, until = 1000, step = 50) => {
        for (let now = 0; now <= until; now += step) rateTick(f, now, 30 + speed * now / 1000);
    };

    test('gait rate uses the immutable accepted native reference speed and remains opt-in', () => {
        assert.equal(nativeGaitReferenceSpeed, 1.5216103452786847);
        assert.match(fs.readFileSync(path.join(adapterDirectory, 'controller.mjs'), 'utf8'),
            /export const nativeGaitReferenceSpeed = 1\.5216103452786847;/);
        for (const f of [fixture(AdapterController), fixture(AdapterController, 'online', { gaitRate: false }),
            fixture(AdapterController, 'smoke', { gaitRate: true })]) {
            constantSpeed(f, 1);
            assert.equal(count(f, 'set_animation_rate'), 0);
            assert.equal(rates(f).length, 0);
            assert.ok(!f.events.some(event => Object.hasOwn(event, 'requested_animation_rate') || Object.hasOwn(event, 'requested_at_ms')));
        }
    });
    for (const value of [1, 'true', null, [], {}]) test('gait rate rejects nonboolean option ' + JSON.stringify(value), () => {
        assert.throws(() => rateFixture({ gaitRate: value }), /gait rate/i);
    });
    for (const speed of [0.5, 1, 1.5]) test('gait rate measures constant received XY speed ' + speed, () => {
        const f = rateFixture();
        constantSpeed(f, speed);
        assert.equal(rates(f).length, 1);
        const event = rates(f)[0];
        near(event.measured_speed, speed);
        near(event.requested_rate, speed / nativeGaitReferenceSpeed);
        assert.equal(event.reference_speed, nativeGaitReferenceSpeed);
        assert.equal(event.clip, 'Walk_civi');
        assert.equal(event.state, 'walk');
        assert.equal(event.task_requested_at_ms, 50);
        assert.equal(event.sample_at_ms, 300);
        assert.equal(event.task_age_ms, 250);
        assert.equal(event.speed_window_ms, 250);
        assert.equal(event.speed_window_started_at_ms, 50);
        assert.equal(event.speed_window_finished_at_ms, 300);
        near(event.speed_path_distance, speed * 0.25);
        assert.equal(event.speed_sample_count, 6);
        assert.equal(event.min_speed_window_ms, 250);
        assert.equal(event.max_speed_window_ms, 500);
        assert.equal(event.source, 'received_target_xy');
        assert.equal(event.setter_interval_ms, 250);
        assert.equal(event.min_rate_delta, 0.05);
        assert.equal(event.rate_min, 0.25);
        assert.equal(event.rate_max, 2);
        assert.equal(count(f, 'play_animation'), 2);
        assert.deepEqual(f.calls.find(call => call[0] === 'set_animation_rate'), ['set_animation_rate', 100, 'Walk_civi', event.requested_rate]);
        assert.ok(!Object.hasOwn(event, 'actual_rate') && !Object.hasOwn(event, 'no_slip'));
    });
    for (const multiple of [0.5, 1, 1.5]) test('native travel-speed multiple ' + multiple + ' requests the matching playback ratio', () => {
        const f = rateFixture();
        constantSpeed(f, nativeGaitReferenceSpeed * multiple);
        near(rates(f)[0].requested_rate, multiple);
    });
    test('cached 100 ms target updates observed on 50 ms polls do not double the requested rate', () => {
        const f = rateFixture();
        for (let now = 0; now <= 1000; now += 50) rateTick(f, now, 30 + 1.5 * (Math.floor(now / 100) * 100) / 1000);
        assert.equal(rates(f).length, 1);
        near(rates(f)[0].measured_speed, 1.5);
        near(rates(f)[0].requested_rate, 1.5 / nativeGaitReferenceSpeed);
        assert.equal(rates(f)[0].speed_window_ms, 300);
        assert.equal(rates(f)[0].speed_sample_count, 4);
        assert.equal(rates(f)[0].sample_at_ms, 400);
    });
    test('irregular monotone target arrivals use their accumulated path and actual elapsed window', () => {
        const f = rateFixture();
        for (const now of [0, 50, 120, 170, 240, 310, 380, 450, 530, 610, 710, 810]) rateTick(f, now, 30 + 1.3 * now / 1000);
        near(rates(f)[0].measured_speed, 1.3);
        near(rates(f)[0].requested_rate, 1.3 / nativeGaitReferenceSpeed);
        assert.equal(rates(f)[0].speed_window_ms, 260);
        assert.equal(rates(f)[0].speed_sample_count, 5);
    });
    test('received turns contribute accumulated XY path rather than net start/end displacement', () => {
        const f = rateFixture();
        const pathPoints = [[30, 20], [30.05, 20], [30.10, 20], [30.10, 20.05],
            [30.10, 20.10], [30.05, 20.10], [30, 20.10]];
        pathPoints.forEach(([x, y], index) => {
            f.state.players = [{ ...remote(2, x), y }];
            f.controller.tick(index * 50);
        });
        near(rates(f)[0].measured_speed, 1);
        near(rates(f)[0].requested_rate, 1 / nativeGaitReferenceSpeed);
    });
    test('gait rate clamps low and high measured speeds to the explicit playback limits', () => {
        const low = rateFixture();
        for (let now = 0; now <= 1000; now += 50) rateTick(low, now, 30 + 0.3 * (Math.floor(now / 100) * 100) / 1000);
        near(rates(low)[0].measured_speed, 0.3);
        assert.equal(rates(low)[0].requested_rate, 0.25);
        const high = rateFixture();
        constantSpeed(high, 3.5);
        assert.equal(rates(high)[0].requested_rate, 2);
    });
    test('setter cadence stays at least 250 ms while changing rates do not replay the walking task', () => {
        const f = rateFixture();
        let x = 30;
        for (let now = 0; now <= 1600; now += 50) {
            if (now) x += (now < 500 ? 1 : now < 1000 ? 2 : 0.7) * 0.05;
            rateTick(f, now, x);
        }
        assert.ok(rates(f).length >= 3);
        assert.ok(rates(f).every((event, index, values) => index === 0 || event.sample_at_ms - values[index - 1].sample_at_ms >= 250));
        assert.equal(count(f, 'play_animation'), 2);
        assert.equal(count(f, 'submit'), 33);
        assert.ok(f.calls.filter(call => call[0] === 'set_animation_rate').every(call => call[1] === 100 && call[2] === 'Walk_civi'));
    });
    test('sub-threshold rate changes suppress both setters and extra association queries', () => {
        const f = rateFixture();
        let x = 30;
        for (let now = 0; now <= 900; now += 50) {
            if (now) x += nativeGaitReferenceSpeed * (now <= 300 ? 1 : 1.02) * 0.05;
            rateTick(f, now, x);
        }
        assert.equal(rates(f).length, 1);
        near(rates(f)[0].requested_rate, 1);
        assert.equal(count(f, 'read_animation'), 2);
    });
    test('display smoothing retains bridge cadence and estimates target speed instead of display increments', () => {
        const f = rateFixture({ renderSmoothing: true });
        constantSpeed(f, 1, 960, 16);
        near(rates(f)[0].measured_speed, 1);
        near(rates(f)[0].requested_rate, 1 / nativeGaitReferenceSpeed);
        assert.equal(count(f, 'submit'), 20);
        assert.equal(count(f, 'play_animation'), 2);
        assert.ok(count(f, 'move') > count(f, 'submit'));
        assert.equal(rates(f)[0].speed_window_ms, 288);
    });
    test('pending playback defers setters with association queries bounded to 250 ms', () => {
        const f = rateFixture();
        const reads = [];
        f.game.readRemoteAnimation = (handle, clip) => {
            reads.push([f.state.now, handle, clip]);
            return clip === 'Walk_civi' && f.state.now < 700 ? { playing: false, time: null } : { playing: true, time: 0.42 };
        };
        constantSpeed(f, 1, 900);
        assert.deepEqual(reads.filter(value => value[2] === 'Walk_civi').map(value => value[0]), [300, 550, 800]);
        assert.equal(rates(f).length, 1);
        assert.equal(rates(f)[0].sample_at_ms, 800);
        assert.equal(count(f, 'play_animation'), 2);
    });
    test('a late association cannot apply an average from the final cached walking hold', () => {
        const f = rateFixture({ gaitProbe: true });
        f.game.readRemoteAnimation = (handle, clip) => clip === 'Walk_civi' && f.state.now < 600
            ? { playing: false, time: null } : { playing: true, time: 0.42 };
        constantSpeed(f, 1, 300);
        for (let now = 350; now <= 600; now += 50) rateTick(f, now, 30.3);
        assert.equal(rates(f).length, 0);
        assert.equal(count(f, 'set_animation_rate'), 0);
        assert.equal(f.controller.remotes.get(2).animation.state, 'walk');
        rateTick(f, 650, 30.3);
        assert.equal(f.controller.remotes.get(2).animation.state, 'idle');
    });
    test('gait probe shares actual same-tick playback evidence and records the newly requested rate', () => {
        const f = rateFixture({ gaitProbe: true });
        constantSpeed(f, 1);
        assert.equal(count(f, 'read_animation'), 11);
        const samples = gaitSamples(f);
        assert.ok(samples.filter(event => event.sample_at_ms < 300).every(event => event.requested_animation_rate === null));
        assert.ok(samples.filter(event => event.sample_at_ms >= 300).every(event => Math.abs(event.requested_animation_rate - 1 / nativeGaitReferenceSpeed) < 1e-9));
        const appliedIndex = f.events.findIndex(event => event.event === 'remote_animation_rate_applied');
        const sampledIndex = f.events.findIndex(event => event.event === 'remote_gait_sample' && event.sample_at_ms === 300);
        assert.ok(appliedIndex < sampledIndex);
        assert.equal(samples.at(-1).task_requested_at_ms, 50);
        assert.equal(samples.at(-1).time, 0.42);
        assert.equal(count(f, 'play_animation'), 2);
    });
    test('pending associations can use the probe read without adding rate-specific native queries', () => {
        const f = rateFixture({ gaitProbe: true });
        f.game.readRemoteAnimation = (handle, clip) => {
            f.calls.push(['read_animation', handle, clip]);
            return clip === 'Walk_civi' && f.state.now < 700 ? { playing: false, time: null } : { playing: true, time: 0.42 };
        };
        constantSpeed(f, 1);
        assert.equal(count(f, 'read_animation'), 11);
        assert.equal(rates(f)[0].sample_at_ms, 700);
        assert.ok(gaitSamples(f).filter(event => event.sample_at_ms < 700).every(event => event.requested_animation_rate === null));
    });
    test('absent walking association never receives a rate and retains existing missing-playback grace', () => {
        const f = rateFixture();
        f.game.readRemoteAnimation = (handle, clip) => clip === 'Walk_civi' ? { playing: false, time: null } : { playing: true, time: 0.42 };
        constantSpeed(f, 1, 1950);
        assert.equal(rates(f).length, 0);
        assert.throws(() => rateTick(f, 2000, 32), /Remote animation did not start/);
        assert.equal(count(f, 'delete'), 1);
        assert.equal(count(f, 'disconnect'), 1);
    });
    test('idle, heading-only updates and height-only updates never request playback rates', () => {
        const f = rateFixture();
        for (let now = 0; now <= 1000; now += 50) {
            f.state.players = [{ ...remote(), heading: now, z: 3 + now / 1000 }];
            f.controller.tick(now);
        }
        assert.equal(rates(f).length, 0);
        assert.equal(count(f, 'play_animation'), 1);
    });
    test('walking-to-idle and resumed walking reset rate history without issuing an idle setter', () => {
        const f = rateFixture({ gaitProbe: true });
        constantSpeed(f, 1, 500);
        for (let now = 550; now <= 900; now += 50) rateTick(f, now, 30.5);
        assert.equal(f.controller.remotes.get(2).animation.state, 'idle');
        assert.equal(f.controller.remotes.get(2).gaitRate.window, null);
        assert.equal(f.controller.remotes.get(2).gaitRate.requestedRate, null);
        assert.equal(rates(f).length, 1);
        for (let now = 950; now <= 1250; now += 50) rateTick(f, now, 30.5 + (now - 900) / 1000);
        assert.equal(rates(f).length, 2);
        assert.equal(rates(f)[1].task_requested_at_ms, 950);
        assert.equal(count(f, 'play_animation'), 4);
        assert.ok(gaitSamples(f).filter(event => event.state === 'idle').every(event => event.requested_animation_rate === null));
    });
    for (const reason of ['teleport', 'clock_reversed', 'render_gap', 'height_change'])
        test('gait rate clears measurement and requested-rate history after ' + reason, () => {
            const f = rateFixture({ renderSmoothing: true });
            constantSpeed(f, 1, 500);
            const priorRates = rates(f).length;
            if (reason === 'teleport') rateTick(f, 550, 60);
            else if (reason === 'clock_reversed') rateTick(f, 499, 30.5);
            else if (reason === 'render_gap') rateTick(f, 801, 30.8);
            else {
                f.state.players = [{ ...remote(2, 30.55), z: 3.5 }];
                f.controller.tick(550);
            }
            assert.equal(rates(f).length, priorRates);
            const rate = f.controller.remotes.get(2).gaitRate;
            assert.equal(rate.window, null);
            assert.equal(rate.requestedRate, null);
            assert.ok(rate.points.length <= 1);
        });
    test('network gap resets its rate window even when display ticks remain timely', () => {
        const f = rateFixture({ renderSmoothing: true });
        constantSpeed(f, 1, 500);
        f.controller.nextNetwork = 850;
        for (let now = 550; now <= 850; now += 50) rateTick(f, now, 30 + now / 1000);
        assert.equal(f.controller.remotes.get(2).gaitRate.window, null);
        assert.equal(f.controller.remotes.get(2).gaitRate.requestedRate, null);
        assert.equal(rates(f).length, 1);
    });
    test('a wider-than-500 ms changed-target window is discarded instead of requesting an unbounded average', () => {
        const f = rateFixture();
        for (let now = 0; now <= 950; now += 50) {
            const sample = now < 50 ? 0 : now < 100 ? 50 : now < 450 ? 100 : now < 800 ? 450 : 800;
            rateTick(f, now, 30 + sample / 1000);
        }
        assert.ok(rates(f).every(event => event.speed_window_ms >= 250 && event.speed_window_ms <= 500));
        assert.ok(f.controller.remotes.get(2).gaitRate.points.length <= 2);
    });
    test('reconnect, departure and replacement never inherit a former ped or session rate', () => {
        const f = rateFixture();
        constantSpeed(f, 1, 500);
        const old = f.controller.remotes.get(2);
        f.state.pump = false;
        f.controller.tick(550);
        assert.equal(old.gaitRate.window, null);
        assert.equal(old.gaitRate.requestedRate, null);
        assert.equal(f.controller.lastGaitRateTickAt, null);
        f.state.pump = true;
        for (let now = 1550; now <= 1850; now += 50) rateTick(f, now, 40 + (now - 1550) / 1000);
        assert.equal(rates(f).length, 2);
        assert.equal(rates(f)[1].ped, 101);
        const replaced = f.controller.remotes.get(2);
        f.existing.delete(101);
        rateTick(f, 1900, 40.35);
        assert.equal(replaced.gaitRate.window, null);
        assert.equal(f.controller.remotes.get(2).handle, 102);
        assert.equal(f.controller.remotes.get(2).gaitRate.requestedRate, null);
        f.state.players = [];
        f.controller.tick(1950);
        assert.equal(f.controller.remotes.size, 0);
        assert.equal(rates(f).length, 2);
    });
    test('each owned peer has independent speed, application cadence and task metadata', () => {
        const f = rateFixture();
        for (let now = 0; now <= 1000; now += 50) {
            f.state.players = [remote(1), remote(2, 30 + now / 1000), remote(3, 40 + 1.5 * now / 1000)];
            f.controller.tick(now);
        }
        assert.equal(rates(f).length, 2);
        near(rates(f).find(event => event.player_id === 2).measured_speed, 1);
        near(rates(f).find(event => event.player_id === 3).measured_speed, 1.5);
        assert.ok(rates(f).every(event => event.ped !== 10 && event.player_id !== 1));
        assert.equal(count(f, 'submit'), 21);
    });
    for (const failedMethod of ['setRemoteAnimationRate', 'readRemoteAnimation'])
        test('gait rate native ' + failedMethod + ' failure cleans owned ped, model and transport', () => {
            const f = rateFixture();
            constantSpeed(f, 1, 250);
            const failure = new Error('rate native failure');
            f.game[failedMethod] = () => { throw failure; };
            assert.throws(() => rateTick(f, 300, 30.3), error => error === failure);
            assert.equal(f.controller.running, false);
            assert.equal(count(f, 'delete'), 1);
            assert.equal(count(f, 'release'), 1);
            assert.equal(count(f, 'disconnect'), 1);
            assert.equal(rates(f).length, 0);
        });
    test('malformed rate-only animation readback fails before a setter and preserves cleanup', () => {
        const f = rateFixture();
        constantSpeed(f, 1, 250);
        f.game.readRemoteAnimation = () => ({ playing: true, time: NaN });
        assert.throws(() => rateTick(f, 300, 30.3), /invalid animation evidence/);
        assert.equal(count(f, 'set_animation_rate'), 0);
        assert.equal(count(f, 'delete'), 1);
        assert.equal(count(f, 'release'), 1);
        assert.equal(count(f, 'disconnect'), 1);
    });
    test('an invalid later snapshot cannot partially update or set a rate for the earlier valid player', () => {
        const f = rateFixture();
        constantSpeed(f, 1, 250);
        f.state.players = [remote(2, 30.3), { ...remote(3), x: NaN }];
        assert.throws(() => f.controller.tick(300), /Invalid snapshot/);
        assert.equal(rates(f).length, 0);
        assert.equal(count(f, 'set_animation_rate'), 0);
    });
    test('stop between smoothing polls cleans history before any additional rate command', () => {
        const f = rateFixture({ renderSmoothing: true });
        constantSpeed(f, 1, 512, 16);
        const old = f.controller.remotes.get(2);
        const applied = rates(f).length;
        f.state.stop = true;
        f.controller.tick(528);
        assert.equal(f.controller.running, false);
        assert.equal(old.gaitRate.window, null);
        assert.equal(old.gaitRate.requestedRate, null);
        assert.equal(rates(f).length, applied);
        assert.equal(count(f, 'delete'), 1);
        assert.equal(count(f, 'disconnect'), 1);
    });

    const entry = fs.readFileSync(path.join(adapterDirectory, 'index.js'), 'utf8').replace(/^import .*;\r?$/gm, '');
    function runEntry(overrides = {}) {
        const calls = [];
        const callFrames = [];
        const runtimeAccess = [];
        const waits = [];
        const logs = [];
        const buffer = new ArrayBuffer(4), view = new DataView(buffer);
        const fromFloat = value => { view.setFloat32(0, value, true); return view.getUint32(0, true); };
        const toFloat = value => { view.setUint32(0, value, true); return view.getFloat32(0, true); };
        let frame = 0;
        let inputCurrent = null, inputPrevious = null, inputPending = null;
        let movementActive = 1;
        const keyFrames = overrides.keyFrames || [[], [17, 16, 35]];
        const functions = { DE_ApiVersion: () => 2, DE_CheckGameBuild: () => 1, DE_BindRuntime: () => 1,
            DE_ConnectLocal: () => 1, DE_Pump: () => 1, DE_LocalPlayerId: () => 1, DE_SubmitTransformBits: () => 1,
            DE_PlayerCount: () => 0, DE_ReadPlayerComponent: () => 0, DE_Disconnect: () => 0,
            DE_ControllerReset: () => { inputCurrent = inputPrevious = inputPending = null; },
            DE_ControllerBegin: () => { inputPending = []; return 1; },
            DE_ControllerSet: (index, value) => { inputPending[index] = value; return 1; },
            DE_ControllerCommit: () => { inputPrevious = inputCurrent; inputCurrent = inputPending.slice(); inputPending = null; return 1; },
            DE_ControllerRead: (which, index) => ((which ? inputPrevious : inputCurrent)?.[index] ?? 0) >>> 0,
            DE_ControllerCurrentMask: () => inputCurrent ? 0xfffff : 0,
            DE_ControllerPreviousMask: () => inputPrevious ? 0xfffff : 0,
            DE_NativePadBind: () => 1, DE_NativePadCapture: () => 1, DE_NativePadRead: () => 0, DE_NativeDeviceRead: () => 0, DE_NativePadReset: () => {},
            DE_MtaMovementBegin: () => 1, DE_MtaMovementSubmit: () => 1, DE_MtaMovementCapture: () => 1,
            DE_MtaMovementRead: index => [movementActive, movementActive, 1, 1, 1, 0, 1, 0, 0, 0, 0, 0][index],
            DE_MtaMovementEnd: () => { movementActive = 0; return 1; },
            DE_CoreStartupBegin: () => 1, DE_CoreStartupPoll: () => 1, DE_CoreStartupCancel: () => 1,
            DE_CoreStartupRead: index => [1, 5, 6, 97, 0, 0, 0, 1, 3, 4, 5, 0, 0, 1, 0, overrides.mtaCoreMovementReference || overrides.mtaCoreRenderReference ? 1 : 0, 0, 1, 0][index],
            DE_CoreInputBegin: () => 1, DE_CoreInputCapture: () => 1,
            DE_CoreInputRead: index => coreInputState()[index],
            DE_CoreMovementBegin: () => 1, DE_CoreMovementCapture: () => 1,
            DE_CoreMovementRead: index => coreMovementState()[index],
            DE_CoreRenderApiVersionV2: () => 2, DE_CoreRenderBeginV2: () => 1, DE_CoreRenderCaptureV2: () => 1,
            DE_CoreRenderReadV2: index => coreRenderState()[index],
            ...(overrides.functions || {}) };
        const names = Object.keys(functions).filter(name => typeof functions[name] === 'function');
        const record = (name, ...args) => { calls.push([name, ...args]); callFrames.push({ frame, name }); };
        const invoke = address => (...args) => { const name = names[address - 1]; record(name, ...args); return functions[name](...args); };
        const runtimeValues = { AdapterController, NativeGaitReferenceController, NativeInputReferenceController, MtaNativePadReferenceController, MtaNativeMovementTrialController, PlayerControlLease, DataView, ArrayBuffer,
            Memory: { FromFloat: fromFloat, ToFloat: toFloat, Fn: { X64U32: invoke, X64I32: invoke }, CallFunction: (address) => invoke(address)() },
            DynamicLibrary: { Load: () => { record('DynamicLibrary.Load'); return { getProcedure: name => {
                runtimeAccess.push({ frame, name: 'getProcedure', procedure: name });
                return names.indexOf(name) + 1;
            } }; } } };
        // Existing ABI/ownership checks select automatic explicitly. Manual
        // waiting has separate frame-based checks so it cannot accidentally
        // inherit activation from a controller fixture or a real game DLL.
        const sandbox = { MODE: overrides.mode || 'smoke', ONLINE_ACTIVATION: overrides.activation ?? 'manual',
            GAIT_REFERENCE: overrides.reference ?? false, GAIT_RATE: overrides.gaitRate ?? false,
            GAIT_ROOT_LOCK: overrides.gaitRootLock ?? false, INPUT_REFERENCE: overrides.inputReference ?? false, HOST: overrides.host || 'sa_unreal',
            MTA_NATIVE_PAD_REFERENCE: overrides.mtaNativePadReference ?? false,
            MTA_NATIVE_MOVEMENT_TRIAL: overrides.mtaNativeMovementTrial ?? false,
            MTA_CORE_STARTUP_REFERENCE: overrides.mtaCoreStartupReference ?? false,
            MTA_CORE_INPUT_REFERENCE: overrides.mtaCoreInputReference ?? false,
            MTA_CORE_MOVEMENT_REFERENCE: overrides.mtaCoreMovementReference ?? false,
            MTA_CORE_RENDER_REFERENCE: overrides.mtaCoreRenderReference ?? false,
            MTA_CORE_RENDER_ROUTE: overrides.mtaCoreRenderRoute ?? 'd3d11',
            MTA_CORE_RENDER_GPU_REFERENCE: overrides.mtaCoreRenderGpuReference ?? false,
            MTA_CORE_RENDER_CUSTOM_PRESENT_REFERENCE: overrides.mtaCoreRenderCustomPresentReference ?? false,
            MTA_CORE_RENDER_FRAME_BOUNDARY_REFERENCE: overrides.mtaCoreRenderFrameBoundaryReference ?? false,
            MTA_CORE_RENDER_NATIVE_DISPATCH_REFERENCE: overrides.mtaCoreRenderNativeDispatchReference ?? false,
            MTA_CORE_GUI_STARTUP_REFERENCE: overrides.mtaCoreGuiStartupReference ?? false,
            MTA_CORE_GUI_SURFACE_REFERENCE: overrides.mtaCoreGuiSurfaceReference ?? false,
            MTA_CORE_GUI_FRONTEND_REFERENCE: overrides.mtaCoreGuiFrontendReference ?? false,
            MTA_CORE_GUI_FRONTEND_INPUT_REFERENCE: overrides.mtaCoreGuiFrontendInputReference ?? false,
            MTA_CORE_GUI_FRONTEND_STARTUP_REFERENCE: overrides.mtaCoreGuiFrontendStartupReference ?? false,
            Date: { now: () => overrides.times ? overrides.times[frame] : frame * 50 },
            CLEO: { version: overrides.version || '1.5.1', apiVersion: overrides.definitions || '0.500', hostVersion: '1.0.113.49697469' },
            native: (name, ...args) => { record(name, ...args);
                if (overrides.natives && Object.hasOwn(overrides.natives, name)) return overrides.natives[name](...args);
                if (name === 'IS_KEY_PRESSED') return keyFrames[frame].includes(args[0]);
                if (name === 'IS_PLAYER_PLAYING') return overrides.readyFrames ? overrides.readyFrames[frame] === true : true;
                if (name === 'DOES_CHAR_EXIST') return true;
                if (name === 'GET_PLAYER_CHAR') return 10;
                if (name === 'GET_CHAR_COORDINATES') return { x: 1.25, y: -2.5, z: 3.75 };
                if (name === 'GET_CHAR_HEADING') return 90;
                throw new Error('Unexpected game mutation: ' + name); },
            log: value => { const event = JSON.parse(value); logs.push(event); overrides.observeLog?.(event); }, wait: milliseconds => {
                waits.push({ frame, milliseconds });
                ++frame;
                if (frame >= keyFrames.length) throw new Error('Adapter did not stop on the final key frame');
            } };
        for (const [name, value] of Object.entries(runtimeValues)) {
            Object.defineProperty(sandbox, name, { configurable: true, get: () => {
                runtimeAccess.push({ frame, name });
                return value;
            } });
        }
        if (overrides.forbidRuntimeAccess) {
            // Throwing getters catch even an attempted bridge/helper lookup.
            // Diagnostic must remain finite without relying on mocked native
            // behavior, a working DLL, or a usable transport endpoint.
            for (const name of ['Memory', 'DynamicLibrary', 'AdapterController', 'DataView', 'ArrayBuffer']) {
                Object.defineProperty(sandbox, name, { get: () => { record('forbidden', name); throw new Error('Forbidden diagnostic access: ' + name); } });
            }
            for (const name of ['native', 'wait']) sandbox[name] = () => { record('forbidden', name); throw new Error('Forbidden diagnostic call: ' + name); };
        }
        vm.runInNewContext(entry, sandbox, { timeout: overrides.executionTimeout ?? 1000 });
        return { calls, callFrames, runtimeAccess, waits, logs, fromFloat };
    }
    // This fixture transports reported scalar evidence only. Its opaque lease
    // provenance and pixel facts cannot establish real COM or GPU execution.
    const coreGuiState = (overrides = {}) => Object.assign(Array(64).fill(0), {
        0: 1, 1: 2, 2: 3, 3: 1000, 4: 1, 6: 1, 8: 1, 9: 1, 10: 1, 11: 1,
        12: 1, 14: 7, 16: 7, 18: 2, 19: 1, 20: 1, 21: 1, 24: 1, 25: 42,
        27: 87, 28: 640, 29: 480, 30: 1, 31: 1, 32: 1, 33: 2, 35: 2, 37: 1,
        38: 20000, 39: 1, 40: 2, 46: 2, 47: 8, 53: 1, 54: 1, 55: 1, 56: 1, 57: 1, 58: 1
    }, overrides);
    const coreGui = (overrides = {}) => {
        let polls = 0;
        return runEntry({ mtaCoreGuiStartupReference: true, keyFrames: [[17, 16, 120], [16], [], []], ...overrides,
            functions: {
                DE_CoreGuiStartupApiVersion: () => 1, DE_CoreGuiStartupBegin: () => 1, DE_CoreGuiStartupCapture: () => 1,
                DE_CoreGuiStartupRead: field => coreGuiState(polls < 2 ? {
                    1: 1, 2: 1, 3: 0, 4: 0, 6: 0, 9: 0, 20: 0, 21: 0, 39: 0, 49: 1, 50: 1, 51: 1,
                    ...(overrides.guiPendingState || {})
                } : overrides.guiState)[field],
                DE_CoreStartupPoll: () => ++polls >= 2 ? 1 : 0,
                DE_CoreStartupRead: field => [1, 5, 6, 97, 0, 0, 0, 1, 3, 4, 5, 0, 0, 1, 0, 1, 0, 1, 0][field],
                ...(overrides.functions || {})
            }
        });
    };
    const guiFinish = result => result.logs.find(event => event.event === 'core_gui_startup_reference_finished');
    test('GUI startup uses a separate Core11 GUI1 one-shot POD worker and keeps native drawing claims false', () => {
        const result = coreGui();
        assert.equal(result.callFrames.find(call => call.name === 'DynamicLibrary.Load').frame, 2);
        assert.equal(result.calls.filter(call => call[0] === 'DE_CoreGuiStartupBegin').length, 1);
        assert.equal(result.calls.filter(call => call[0] === 'DE_CoreGuiStartupRead').length, 128);
        assert.ok(!result.calls.some(call => /DE_CoreRender|DE_CoreInput|DE_ConnectLocal|CREATE_CHAR/.test(call[0])));
        const start = result.logs.find(event => event.event === 'adapter_start');
        assert.equal(start.core_api, 11); assert.equal(start.gui_api, 1); assert.equal(start.core_gui_startup_reference, true);
        const samples = result.logs.filter(event => event.event === 'core_gui_startup_reference_sample');
        assert.deepEqual(samples.map(event => event.state.length), [64, 64]);
        const finish = guiFinish(result);
        assert.equal(finish.passed, true); assert.equal(finish.cleanup_passed, true); assert.equal(finish.own_gpu_workload_verified, true);
        for (const claim of ['full_core_ready', 'visible_gta_drawing_verified', 'network_active', 'input_writes',
            'remote_peds_created', 'native_queue_commands_issued', 'native_backbuffer_touched']) assert.equal(finish[claim], false);
    });
    for (const options of [{ mode: 'online' }, { mode: 'diagnostic' }, { mtaCoreGuiStartupReference: 'true' },
        { inputReference: true }, { reference: true }, { gaitRate: true }, { gaitRootLock: true },
        { mtaNativePadReference: true }, { mtaNativeMovementTrial: true }, { mtaCoreStartupReference: true },
        { mtaCoreInputReference: true }, { mtaCoreMovementReference: true }, { mtaCoreRenderReference: true },
        { mtaCoreRenderGpuReference: true }, { mtaCoreRenderCustomPresentReference: true },
        { mtaCoreRenderFrameBoundaryReference: true }, { mtaCoreRenderNativeDispatchReference: true }])
        test('GUI startup rejects conflicting or nonboolean selection before any runtime lookup ' + JSON.stringify(options), () => {
            const result = coreGui({ ...options, forbidRuntimeAccess: true });
            assert.equal(result.calls.length, 0); assert.ok(result.logs.some(event => event.event === 'adapter_failed'));
        });
    for (const name of ['DE_CoreGuiStartupApiVersion', 'DE_CoreGuiStartupBegin', 'DE_CoreGuiStartupCapture', 'DE_CoreGuiStartupRead'])
        test('GUI startup missing export refuses before Begin ' + name, () => {
            const result = coreGui({ functions: { [name]: null } });
            assert.ok(!result.calls.some(call => call[0] === 'DE_CoreGuiStartupBegin'));
            assert.ok(result.logs.some(event => event.event === 'adapter_failed' && event.error.includes(name)));
        });
    test('GUI startup wrong API version never starts or falls back to render observation', () => {
        const result = coreGui({ functions: { DE_CoreGuiStartupApiVersion: () => 2 } });
        assert.ok(!result.calls.some(call => /DE_CoreGuiStartupBegin|DE_CoreRenderBegin/.test(call[0])));
        assert.ok(!guiFinish(result));
    });
    test('GUI startup rejected Begin never polls or cancels an unowned job', () => {
        const result = coreGui({ functions: { DE_CoreGuiStartupBegin: () => 0 } });
        assert.ok(!result.calls.some(call => /DE_CoreStartupPoll|DE_CoreStartupCancel|DE_CoreGuiStartupCapture/.test(call[0])));
        assert.ok(!guiFinish(result));
    });
    for (const [field, value] of [[0, 2], [1, 0], [2, 0], [4, 12], [5, 2], [18, 1], [44, 1], [45, 1],
        [46, 3], [47, 9], [48, 9], [58, 2], [59, 2], [60, 1], [63, 1], [28, NaN], [29, -1], [25, 0x100000000]])
        test('GUI malformed scalar requests cleanup and emits no final proof: field ' + field, () => {
            const result = coreGui({ guiPendingState: { [field]: value } });
            assert.equal(result.calls.filter(call => call[0] === 'DE_CoreStartupCancel').length, 1);
            assert.ok(!guiFinish(result));
            assert.ok(!result.calls.some(call => call[0] === 'DE_CoreStartupRead'));
        });
    for (const field of [5, 43, 49, 50, 51, 59]) test('GUI cleanup-complete cannot hide retained ownership field ' + field, () => {
        const result = coreGui({ guiState: { [field]: 1 } });
        assert.ok(!guiFinish(result));
        assert.ok(result.logs.some(event => event.event === 'adapter_failed' && /retained ownership/.test(event.error)));
    });
    for (const [field, value] of [[7, 1], [8, 0], [10, 0], [12, 0], [14, 0], [16, 0], [18, 0], [21, 0],
        [22, 0x80004005], [24, 2], [27, 24], [32, 0], [35, 1], [37, 0], [38, 10000], [42, 10], [46, 1], [52, 1], [54, 0], [58, 0]])
        test('GUI final partial or refused workload cannot claim success: field ' + field, () => {
            const finish = guiFinish(coreGui({ guiState: { [field]: value } }));
            assert.equal(finish.passed, false); assert.equal(finish.own_gpu_workload_verified, false);
            assert.equal(finish.cleanup_passed, field !== 7);
        });
    test('GUI backend pin is explicit, including unpinned factory refusal with complete cleanup', () => {
        const result = coreGui({ guiState: { 4: 4, 8: 0, 10: 0, 57: 0 }, functions: {
            DE_CoreStartupRead: field => [1, 5, 6, 97, 0, 0, 0, 1, 3, 4, 5, 0, 0, 1, 0, 0, 15, 0, 0][field]
        } });
        assert.equal(guiFinish(result).cleanup_passed, true); assert.equal(guiFinish(result).passed, false);
    });
    test('GUI identical repeated sequences log once and changing repeated sequence cancels', () => {
        for (const changed of [false, true]) {
            let polls = 0;
            const result = coreGui({ keyFrames: [[17, 16, 120], [], [], [], []], functions: {
                DE_CoreStartupPoll: () => ++polls >= 3 ? 1 : 0,
                DE_CoreGuiStartupRead: field => coreGuiState(polls >= 3 ? {} : { 1: 1, 2: 1, 3: changed && polls === 2 ? 1 : 0,
                    4: 0, 6: 0, 9: 0, 39: 0 })[field]
            } });
            if (changed) { assert.ok(!guiFinish(result)); assert.equal(result.calls.filter(call => call[0] === 'DE_CoreStartupCancel').length, 1); }
            else { assert.equal(guiFinish(result).passed, true); assert.equal(guiFinish(result).samples, 2); }
        }
    });
    for (const loading of [false, true]) test('GUI ' + (loading ? 'loading' : 'End') + ' cancellation waits for terminal cleanup and cannot certify success', () => {
        let polls = 0;
        const result = coreGui({ keyFrames: [[17, 16, 120], [], [], [17, 16, 35], [17, 16, 35]],
            readyFrames: loading ? [true, true, true, false, false] : undefined,
            functions: { DE_CoreStartupPoll: () => ++polls >= 3 ? 1 : 0,
                DE_CoreGuiStartupRead: field => coreGuiState(polls < 3 ? { 1: 1, 2: polls, 3: polls * 50, 4: 0,
                    6: 0, 9: 0, 39: 0 } : {})[field] } });
        assert.equal(result.calls.filter(call => call[0] === 'DE_CoreStartupCancel').length, 1);
        assert.equal(polls, 3); assert.equal(guiFinish(result).stop_requested, true);
        assert.equal(guiFinish(result).cleanup_passed, true); assert.equal(guiFinish(result).passed, false);
    });
    test('GUI arming stays dormant for late F9 and requires all modifiers released', () => {
        const result = coreGui({ activation: 'automatic', keyFrames: [[], [], [17, 16, 120], [16], [], []],
            times: [0, 7200000, 14400000, 14400050, 14400100, 14400200] });
        assert.equal(result.callFrames.find(call => call.name === 'DynamicLibrary.Load').frame, 4);
        assert.equal(result.logs.find(event => event.event === 'core_gui_startup_reference_started').at_ms, 14400100);
        assert.equal(guiFinish(result).passed, true);
    });
    for (const options of [{ keyFrames: [[17, 16, 120, 35]] }, { keyFrames: [[17, 16, 120], [120]], times: [0, 10000] }])
        test('GUI cancelled or timed-out arming never touches runtime ' + JSON.stringify(options), () => {
            const result = coreGui(options);
            assert.equal(result.runtimeAccess.length, 0); assert.ok(!guiFinish(result));
        });
    // Surface policy mocks transport reported facts only; they do not render
    // pixels, call DComp, or turn native dispatch into a drawing permission.
    const coreSurfaceState = (overrides = {}) => Object.assign(Array(96).fill(0), {
        0: 1, 1: 2, 2: 3, 3: 8500, 4: 1, 6: 1, 8: 1, 9: 1, 10: 1, 11: 1,
        12: 1, 14: 7, 16: 7, 18: 2, 19: 1, 20: 1, 21: 1, 24: 1, 25: 42,
        27: 87, 28: 640, 29: 480, 30: 1, 31: 1, 32: 1, 33: 4, 35: 4, 37: 1,
        38: 20000, 39: 1, 40: 2, 46: 4, 47: 9, 53: 1, 54: 1, 55: 1, 56: 1, 57: 1,
        58: 1, 60: 1, 61: 1, 62: 1, 63: 1, 64: 1, 65: 1, 66: 1, 67: 3, 68: 3,
        70: 3, 74: 1, 78: 1, 79: 1, 80: 1, 81: 42, 82: 43, 83: 1, 84: 4, 85: 8000, 86: 1
    }, overrides);
    const coreSurface = (overrides = {}) => {
        let polls = 0;
        return runEntry({ mtaCoreGuiSurfaceReference: true, keyFrames: [[17, 16, 120], [16], [], []],
            times: [0, 50, 100, 8600], ...overrides, functions: {
                DE_CoreGuiSurfaceApiVersion: () => 1, DE_CoreGuiSurfaceBegin: () => 1, DE_CoreGuiSurfaceCapture: () => 1,
                DE_CoreGuiSurfaceRead: field => coreSurfaceState(polls < 2 ? {
                    // Production readback finishes before the eight-second display.
                    1: 1, 2: 1, 3: 50, 4: 0, 6: 0, 20: 0, 21: 0, 37: 1, 39: 0, 47: 6, 74: 0,
                    49: 1, 50: 1, 51: 1, 75: 1, 76: 1, 77: 1, 78: 0, 79: 0, 80: 0,
                    ...(overrides.surfacePendingState || {})
                } : overrides.surfaceState)[field],
                DE_CoreStartupPoll: () => ++polls >= 2 ? 1 : 0,
                DE_CoreStartupRead: field => [1, 5, 6, 97, 0, 0, 0, 1, 3, 4, 5, 0, 0, 1, 0, 1, 0, 1, 0][field],
                ...(overrides.functions || {})
            } });
    };
    const surfaceFinish = result => result.logs.find(event => event.event === 'core_gui_surface_reference_finished');
    // These entry fixtures exchange copied protocol values and real boolean
    // providers, not fixture-generated GUI clicks or an actual game window.
    const coreFrontend = (options = {}) => {
        let polls = 0, controlCache = Array(8).fill(0), enabled = options.initialControl ?? true;
        const mailbox = Array(8).fill(0);
        let admitted = false, restoredAt = null;
        const snapshot = () => {
            const complete = polls >= 6 && mailbox[6] === 1;
            const started = admitted && polls >= 3 && !options.createNull;
            const detached = mailbox[5] === 1;
            const values = Object.assign(Array(160).fill(0), coreSurfaceState({
                1: complete ? 2 : (detached ? 3 : 1), 2: polls, 3: polls * 50, 4: complete ? (started ? 1 : 7) : 0,
                5: detached && !complete ? 1 : 0,
                6: complete ? 1 : 0, 19: started || (options.createNull && admitted && polls >= 3) ? 1 : 0,
                20: complete ? 1 : 0, 21: complete && started ? 1 : 0,
                37: started ? 1 : 0,
                39: complete ? 1 : 0, 49: complete ? 0 : 1, 50: complete ? 0 : 1, 51: complete ? 0 : 1,
                53: 0, 54: 0, 74: detached ? 1 : 0, 75: complete ? 0 : 1, 76: complete ? 0 : 1, 77: complete ? 0 : 1,
                78: complete ? 1 : 0, 79: complete ? 1 : 0, 80: complete ? 1 : 0, 85: 30000
            }), { 128: 1, 139: detached ? 1 : 0, 151: 1000 });
            if (started) Object.assign(values, {
                96: 1, 97: detached ? 4 : 2, 98: 42, 99: polls, 100: detached ? 0 : 1,
                102: detached ? 0 : 1, 103: options.quitRequested ? 1 : 0,
                104: options.creditsVisible ? 1 : 0, 105: options.aboutClicks || 0,
                106: 1, 107: 1, 108: 1, 109: 1, 113: detached ? 1 : 0,
                117: detached ? 0 : 1, 118: 1, 121: 1, 124: 1,
                129: 1, 130: detached ? 0 : 1, 131: 1,
                133: options.inputEvents || 0, 153: 30000, 154: 1, 155: detached ? 1 : 0
            });
            if (options.createNull && admitted) Object.assign(values, { 129: 1, 155: detached ? 1 : 0 });
            mailbox.forEach((value, field) => { values[142 + field] = value; });
            return Object.assign(values, options.scalarOverrides?.(polls) || {});
        };
        const result = runEntry({ mtaCoreGuiFrontendReference: true,
            keyFrames: [[17, 16, 120], [16], ...Array.from({ length: 25 }, () => [])],
            ...options.entry,
            functions: {
                DE_CoreGuiFrontendApiVersion: () => 1, DE_CoreGuiFrontendBegin: () => 1,
                DE_CoreGuiFrontendCapture: () => {
                    if (options.captureErrorAt === polls) throw Error('temporary snapshot failure');
                    return options.skipTerminalCapture && polls >= 6 ? 0 : 1;
                },
                DE_CoreGuiFrontendRead: field => snapshot()[field],
                DE_CoreGuiFrontendControlFocused: () => options.focusLostAt === polls ? 0 : 1,
                DE_CoreGuiFrontendControlCapture: () => { controlCache = mailbox.slice(); return 1; },
                DE_CoreGuiFrontendControlRead: field => controlCache[field],
                DE_CoreGuiFrontendControlAck: generation => {
                    assert.equal(generation, 17); mailbox[2] = mailbox[3] = 1; admitted = true; return 1;
                },
                DE_CoreGuiFrontendControlHeartbeat: generation => { assert.equal(generation, 17); return 1; },
                DE_CoreGuiFrontendControlRevoke: generation => { assert.equal(generation, 17); mailbox[4] = 1; mailbox[3] = 0; return 1; },
                DE_CoreGuiFrontendControlRestored: generation => {
                    assert.equal(generation, 17); assert.equal(mailbox[5], 1);
                    assert.equal(enabled, options.initialControl ?? true); mailbox[6] = 1; restoredAt = polls; return 1;
                },
                DE_CoreStartupPoll: () => {
                    ++polls;
                    if (polls === 2) { mailbox[0] = 17; mailbox[1] = 1; }
                    if (polls >= 4) { mailbox[4] = mailbox[5] = 1; mailbox[3] = 0; }
                    if (options.externalControlAt === polls) enabled = true;
                    if (options.controlFaultAt === polls) mailbox[7] = 1;
                    if (options.pollErrorAt === polls) throw Error('temporary poll failure');
                    return polls >= (options.workerTerminalAt ?? 6) && (mailbox[6] === 1 || options.workerTerminalAt) ? 1 : 0;
                },
                DE_CoreStartupCancel: () => 1,
                DE_CoreStartupRead: field => Object.assign([1, 5, 6, 97, 0, 0, 0, 1, 3, 4, 5, 0, 0, 1, 0, 1, 0, 1, 0], options.workerState)[field],
                ...options.functions
            },
            natives: { IS_CHAR_ON_FOOT: ped => { assert.equal(ped, 10); return true; },
                IS_PLAYER_CONTROL_ON: index => { assert.equal(index, 0); return enabled; },
                SET_PLAYER_CONTROL: (index, value) => {
                    assert.equal(index, 0); assert.equal(typeof value, 'boolean');
                    if (!options.restoreDenied || !value) enabled = value;
                    if (options.throwAfterDisable && value === false) throw Error('setter return failure');
                }, ...options.natives }
        });
        return { ...result, enabled, polls, restoredAt };
    };
    const frontendFinish = result => result.logs.find(event => event.event === 'core_gui_frontend_reference_finished');
    const frontendInputState = (polls, overrides = {}) => {
        const done = polls >= 3;
        return Object.assign(Array(256).fill(0), coreSurfaceState({
            1: done ? 2 : 1, 2: polls, 3: polls * 50, 4: done ? 1 : 0, 6: done ? 1 : 0,
            20: done ? 1 : 0, 21: done ? 1 : 0, 39: done ? 1 : 0,
            49: done ? 0 : 1, 50: done ? 0 : 1, 51: done ? 0 : 1,
            53: 0, 54: 0, 75: done ? 0 : 1, 76: done ? 0 : 1, 77: done ? 0 : 1,
            78: done ? 1 : 0, 79: done ? 1 : 0, 80: done ? 1 : 0, 85: 20000
        }), {
            96: 1, 97: done ? 4 : 2, 98: 42, 99: 3, 100: done ? 0 : 1, 102: done ? 0 : 1,
            106: 1, 107: 1, 108: 1, 109: 1, 113: done ? 1 : 0, 117: done ? 0 : 1, 118: 1, 121: 1, 124: 1,
            128: 1, 129: done ? 4 : 2, 130: 42, 131: 1, 134: done ? 0 : 1, 136: done ? 0 : 1,
            137: done ? 0 : 1, 138: done ? 0 : 2, 149: done ? 1 : 0, 150: done ? 0 : 1, 151: done ? 0 : 1, 152: 1, 157: 255, 158: 111,
            160: 1, 161: 1, 162: done ? 0 : 1, 163: 1, 165: 4, 171: done ? 1 : 0,
            176: 1, 177: done ? 4 : 2, 178: 42, 179: 1, 180: 1, 181: 6, 182: 6, 184: 6,
            186: done ? 0 : 1, 187: 3, 188: 1, 189: 1, 190: done ? 1 : 0, 191: done ? 0 : 1, 192: 1, 193: 111,
            208: done ? 0 : 1, 210: done ? 0 : 1, 212: 3, 214: 3, 216: 3, 220: 3, 232: 1,
            236: 1, 237: 1, 238: done ? 1 : 0, 242: 20000, 243: done ? 0 : 1
        }, overrides);
    };
    const coreFrontendInput = (options = {}) => {
        let polls = 0;
        const result = runEntry({ mtaCoreGuiFrontendInputReference: true,
            keyFrames: [[17, 16, 120], [16], [], [], []], times: [0, 50, 100, 300, 20500], ...options.entry,
            functions: {
                DE_CoreGuiFrontendInputApiVersion: () => 1, DE_CoreGuiFrontendInputBegin: () => 1,
                DE_CoreGuiFrontendInputCapture: () => options.skipTerminalCapture && polls >= 3 ? 0 : 1,
                DE_CoreGuiFrontendInputRead: field => frontendInputState(polls, options.scalarOverrides?.(polls) || {})[field],
                DE_CoreStartupPoll: () => { ++polls; if (options.pollErrorAt === polls) throw Error('temporary input worker poll failure'); return polls >= 3 ? 1 : 0; },
                DE_CoreStartupRead: field => Object.assign([1, 5, 6, 97, 0, 0, 0, 1, 3, 4, 5, 0, 0, 1, 0, 1, 0, 1, 0], options.workerState)[field],
                ...options.functions
            }
        });
        return { ...result, polls };
    };
    const frontendInputFinish = result => result.logs.find(event => event.event === 'core_gui_frontend_input_reference_finished');
    // The new report describes one actual completed owner across three
    // boundaries. These mocks do not construct Core or execute a native scope.
    const frontendStartupReport = (overrides = {}) => Object.assign([
        ...Object.assign(Array(32).fill(0), { 0: 1, 1: 2, 2: 42, 3: 1, 4: 1, 5: 1, 7: 1, 16: 33, 23: 1 }),
        ...Object.assign(Array(32).fill(0), { 0: 1, 1: 2, 2: 42, 3: 1, 4: 1, 5: 1, 6: 1, 7: 2, 16: 97, 22: 1, 23: 1 }),
        ...Object.assign(Array(32).fill(0), { 0: 1, 1: 4, 2: 42, 3: 1, 4: 1, 7: 2, 14: 1, 22: 1, 23: 1 })
    ], overrides);
    const coreFrontendStartup = (options = {}) => {
        let polls = 0;
        const result = runEntry({ mtaCoreGuiFrontendStartupReference: true,
            keyFrames: [[17, 16, 120], [16], [], [], []], times: [0, 50, 100, 300, 20500], ...options.entry,
            functions: {
                DE_CoreGuiFrontendStartupApiVersion: () => 1, DE_CoreGuiFrontendStartupBegin: () => 1,
                DE_CoreGuiFrontendStartupCapture: () => options.lifecycleCaptureStatus ?? 1,
                DE_CoreGuiFrontendStartupRead: field => frontendStartupReport(options.lifecycleOverrides)[field],
                DE_CoreGuiFrontendInputCapture: () => options.noInput || (options.skipTerminalInput && polls >= 3) ? 0 : 1,
                DE_CoreGuiFrontendInputRead: field => (options.inputState?.(polls) || frontendInputState(polls, options.scalarOverrides?.(polls) || {}))[field],
                DE_CoreStartupPoll: () => { ++polls; if (options.pollErrorAt === polls) throw Error('temporary startup worker poll failure'); return polls >= 3 ? 1 : 0; },
                DE_CoreStartupRead: field => Object.assign([0, 0, 0, 0, 0, 0, 0, 0, 2, 0, 0, 0, 0, 1, 0, 1, 0, 1, 0], options.workerState)[field],
                ...options.functions
            }
        });
        return { ...result, polls };
    };
    const frontendStartupFinish = result => result.logs.find(event => event.event === 'core_gui_frontend_startup_reference_finished');
    test('Core15 completed startup uses separate96 lifecycle plus unchanged256 input evidence without script control writes', () => {
        const result = coreFrontendStartup(); const finish = frontendStartupFinish(result);
        assert.equal(finish.passed, true); assert.equal(finish.cleanup_passed, true);
        assert.equal(finish.completed_core_lifecycle_verified, true);
        assert.equal(finish.schema, 'mta_core_gui_frontend_startup_v1'); assert.equal(finish.core_api, 15); assert.equal(finish.trial_api, 1);
        assert.equal(finish.started_at_ms, result.logs.find(event => event.event === 'core_gui_frontend_startup_reference_started').at_ms);
        assert.equal(finish.lifecycle_values.length, 96); assert.equal(finish.input_values.length, 256); assert.equal(finish.state.length, 19);
        if (process.env.MTA_GAME_ADAPTER_MOCK_EVIDENCE_DIR) {
            // Export the actual entry's synthetic events for cross-language
            // reader checks. These explicitly named mock files are never live
            // GTA evidence and do not change any production logging behavior.
            const directory = process.env.MTA_GAME_ADAPTER_MOCK_EVIDENCE_DIR;
            fs.mkdirSync(directory, { recursive: true });
            fs.writeFileSync(path.join(directory, 'script-mock-startup-success.jsonl'), result.logs.map(event => JSON.stringify(event)).join('\n') + '\n');
            fs.writeFileSync(path.join(directory, 'core-mock-startup-success.jsonl'), JSON.stringify({ schema: 'mta_core_gui_frontend_input_v1',
                pid: 71, runMs: 100, values: finish.input_values }) + '\n');
        }
        assert.deepEqual(result.calls.filter(call => call[0] === 'DE_CoreGuiFrontendStartupRead').map(call => call[1]), Array.from({ length: 96 }, (_, field) => field));
        assert.deepEqual(result.calls.filter(call => call[0] === 'DE_CoreGuiFrontendInputRead').map(call => call[1]).slice(-256), Array.from({ length: 256 }, (_, field) => field));
        assert.ok(result.logs.filter(event => event.event === 'core_gui_frontend_startup_reference_sample').every(event => event.input_values.length === 256 && !('state' in event)));
        assert.ok(!result.calls.some(call => /SET_PLAYER_CONTROL|IS_PLAYER_CONTROL_ON|DE_CoreGuiFrontendControl|DE_CoreGuiFrontendInputBegin|DE_CoreStartupBegin|DE_ConnectLocal|CREATE_CHAR/.test(call[0])));
        assert.equal(result.callFrames.find(call => call.name === 'DE_CoreGuiFrontendStartupBegin').frame, 2);
        const start = result.logs.find(event => event.event === 'adapter_start');
        assert.equal(start.core_api, 15); assert.equal(start.core_gui_frontend_startup_reference, true); assert.equal(start.native_player_control_lease_planned, false);
        for (const claim of ['full_core_ready', 'full_mta_ready', 'gameplay_ready', 'camera_capture_verified', 'network_active',
            'remote_peds_created', 'native_queue_commands_issued', 'native_backbuffer_touched', 'native_player_control_writes']) assert.equal(finish[claim], false);
    });
    test('Core15 arming waits indefinitely then releases every F9 modifier before bridge lookup', () => {
        const result = coreFrontendStartup({ entry: { keyFrames: [[], [], [17, 16, 120], [120], [], [], []],
            times: [0, 100, 14400000, 14400050, 14400100, 14400150, 14400200] } });
        assert.equal(result.callFrames.find(call => call.name === 'DynamicLibrary.Load').frame, 4);
        assert.equal(result.callFrames.find(call => call.name === 'DE_CoreGuiFrontendStartupBegin').frame, 4);
        assert.equal(frontendStartupFinish(result).passed, true);
    });
    test('Core15 End cancellation drains native, GUI and completed Core ownership without promoting failed workload', () => {
        const result = coreFrontendStartup({ entry: { keyFrames: [[17, 16, 120], [16], [], [17, 16, 35], []] }, workerState: { 16: 12, 17: 0 } });
        const finish = frontendStartupFinish(result);
        assert.equal(result.polls, 3); assert.equal(finish.cancel_requested, true); assert.equal(finish.passed, false); assert.equal(finish.cleanup_passed, true);
        assert.equal(result.calls.filter(call => call[0] === 'DE_CoreStartupCancel').length, 1);
    });
    test('Core15 transient poll error keeps owner cleanup polling until actual terminal', () => {
        const result = coreFrontendStartup({ pollErrorAt: 1, workerState: { 16: 12, 17: 0 } }); const finish = frontendStartupFinish(result);
        assert.equal(result.polls, 3); assert.equal(finish.passed, false); assert.equal(finish.cleanup_passed, true);
        assert.match(finish.error, /temporary startup worker/);
    });
    test('Core15 saved failed view can prove cleanup while preserving worker failure and failed workload', () => {
        const result = coreFrontendStartup({ workerState: { 9: 4, 16: 23, 17: 0 }, lifecycleOverrides: { 53: 6 } }); const finish = frontendStartupFinish(result);
        assert.equal(finish.passed, false); assert.equal(finish.cleanup_passed, true); assert.equal(finish.state[16], 23);
        assert.equal(finish.completed_core_lifecycle_verified, false);
    });
    const noViewReport = { 38: 0, 48: 33, 53: 4, 54: 0, 86: 0 };
    test('Core15 native bind refusal without an attempted view can prove actual completed Core cleanup without256', () => {
        const result = coreFrontendStartup({ noInput: true, lifecycleOverrides: noViewReport, workerState: { 9: 4, 15: 0, 16: 23, 17: 0 } });
        const finish = frontendStartupFinish(result);
        assert.equal(finish.passed, false); assert.equal(finish.cleanup_passed, true); assert.equal(finish.input_values, null);
        assert.equal(finish.lifecycle_values[54], 0); assert.equal(finish.state[16], 23);
    });
    test('Core15 failed Begin with no view retains its historical failure and proves explicit shutdown', () => {
        const overrides = { 1: 5, 5: 0, 7: 0, 16: 0, 21: 2, ...Object.fromEntries(Array.from({ length: 32 }, (_, field) => [32 + field, 0])), 71: 0, 85: 2, 86: 0 };
        const result = coreFrontendStartup({ noInput: true, lifecycleOverrides: overrides, workerState: { 8: 5, 9: 5, 15: 0, 16: 21, 17: 0 } });
        const finish = frontendStartupFinish(result);
        assert.equal(finish.cleanup_passed, true); assert.equal(finish.passed, false); assert.equal(finish.lifecycle_values[21], 2);
    });
    for (const options of [{ lifecycleOverrides: { 86: 1 } }, { lifecycleOverrides: { 54: 1 } }, { lifecycleOverrides: { 55: 0 } },
        { lifecycleOverrides: { 72: 1, 73: 4 } }, { lifecycleOverrides: { 75: 1 } }, { workerState: { 15: 1 } }])
        test('Core15 absent256 cannot hide an attempted view or retained obligation ' + JSON.stringify(options), () => {
            const result = coreFrontendStartup({ noInput: true, lifecycleOverrides: noViewReport, workerState: { 9: 4, 15: 0, 16: 23, 17: 0 },
                ...options, lifecycleOverrides: { ...noViewReport, ...options.lifecycleOverrides }, workerState: { 9: 4, 15: 0, 16: 23, 17: 0, ...options.workerState } });
            const finish = frontendStartupFinish(result); assert.equal(finish.cleanup_passed, false); assert.equal(finish.passed, false);
        });
    for (const [field, value] of [[1, 5], [5, 0], [6, 1], [7, 0], [21, 4], [22, 1], [33, 5], [35, 0], [37, 0], [38, 0], [39, 0], [53, 6], [54, 0], [55, 0], [86, 0], [87, 0]])
        test('Core15 lifecycle workload requires its actual completed boundaries: field ' + field, () => {
            const result = coreFrontendStartup({ lifecycleOverrides: { [field]: value } }); const finish = frontendStartupFinish(result);
            assert.equal(finish.passed, false);
        });
    for (const [field, value] of [[0, 2], [2, 0], [34, 43], [65, 2], [67, 0], [69, 1], [70, 1], [72, 1], [73, 2], [74, 1], [75, 1], [87, 0],
        [76, 1], [77, 1], [78, 0], [79, 1], [80, 33], [81, 1], [82, 1], [83, 1], [84, 1], [88, 1], [95, 1], [55, 2], [53, 7], [3, -1]])
        test('Core15 retained, invalid or uncorrelated lifecycle state cannot certify cleanup: field ' + field, () => {
            const result = coreFrontendStartup({ lifecycleOverrides: { [field]: value } }); const finish = frontendStartupFinish(result);
            assert.equal(finish.cleanup_passed, false); assert.equal(finish.passed, false);
        });
    for (const [field, value] of [[0, 1], [7, 1], [8, 5], [10, 1], [11, 4], [12, 3], [13, 0], [14, 1], [15, 0], [16, 26], [17, 2], [18, 1]])
        test('Core15 worker bookkeeping cannot substitute for actual unload proof: field ' + field, () => {
            const result = coreFrontendStartup({ workerState: { [field]: value } }); const finish = frontendStartupFinish(result);
            assert.equal(finish.cleanup_passed, false); assert.equal(finish.passed, false);
        });
    for (const options of [{ lifecycleCaptureStatus: 0 }, { lifecycleCaptureStatus: 2 }, { skipTerminalInput: true }, { noInput: true }])
        test('Core15 requires fresh terminal lifecycle and attempted-view input captures ' + JSON.stringify(options), () => {
            const finish = frontendStartupFinish(coreFrontendStartup(options)); assert.equal(finish.cleanup_passed, false); assert.equal(finish.passed, false);
        });
    test('Core15 rejects a changed terminal copy that reuses an earlier clean publication sequence', () => {
        const result = coreFrontendStartup({ inputState: polls => frontendInputState(polls >= 2 ? 3 : polls,
            { 2: polls >= 2 ? 2 : polls, 105: polls >= 3 ? 1 : 0 }) });
        const finish = frontendStartupFinish(result);
        assert.equal(finish.passed, false); assert.equal(finish.cleanup_passed, false);
        assert.match(finish.error, /without a new sequence/);
    });
    test('Core15 internally coherent lifecycle owners must still match the actual input256 GUI/Core owner', () => {
        const result = coreFrontendStartup({ lifecycleOverrides: { 2: 43, 34: 43, 66: 43 } });
        const finish = frontendStartupFinish(result);
        assert.equal(finish.cleanup_passed, false); assert.equal(finish.passed, false);
        assert.equal(finish.lifecycle_values[2], 43); assert.equal(finish.input_values[81], 42);
    });
    for (const [field, value] of [[7, 1], [172, 1], [226, 1], [228, 7], [241, 1], [187, 1], [188, 0], [189, 0], [33, 0], [38, 10000], [242, 19999]])
        test('Core15 retains the unchanged native/render input256 workload requirement: field ' + field, () => {
            const result = coreFrontendStartup({ scalarOverrides: polls => polls >= 3 ? { [field]: value } : {} }); const finish = frontendStartupFinish(result);
            assert.equal(finish.passed, false); assert.equal(finish.cleanup_passed, true);
        });
    test('Core15 reports actual console callbacks independently and never invents a Quit click', () => {
        const result = coreFrontendStartup({ scalarOverrides: polls => polls >= 3 ? { 139: 2, 141: 2, 142: 2, 146: 0, 105: 1 } : {} });
        const finish = frontendStartupFinish(result);
        assert.equal(finish.actual_console_commands_verified, true); assert.equal(finish.actual_about_clicks, 1); assert.equal(finish.actual_quit_requested, false);
        assert.equal(finish.passed, true);
    });
    for (const name of ['DE_CoreGuiFrontendStartupApiVersion', 'DE_CoreGuiFrontendStartupBegin', 'DE_CoreGuiFrontendStartupCapture',
        'DE_CoreGuiFrontendStartupRead', 'DE_CoreGuiFrontendInputCapture', 'DE_CoreGuiFrontendInputRead'])
        test('Core15 refuses missing independent worker exports before Begin: ' + name, () => {
            const result = coreFrontendStartup({ functions: { [name]: null } });
            assert.ok(!result.calls.some(call => call[0] === 'DE_CoreGuiFrontendStartupBegin'));
            assert.ok(result.logs.some(event => event.event === 'adapter_failed' && event.error.includes(name)));
        });
    for (const api of [0, 2]) test('Core15 refuses an unsupported lifecycle API before Begin: ' + api, () => {
        const result = coreFrontendStartup({ functions: { DE_CoreGuiFrontendStartupApiVersion: () => api } });
        assert.ok(!result.calls.some(call => call[0] === 'DE_CoreGuiFrontendStartupBegin'));
        assert.ok(result.logs.some(event => event.event === 'adapter_failed' && event.error.includes('API1')));
    });
    for (const options of [{ mode: 'online' }, { mode: 'diagnostic' }, { mtaCoreGuiFrontendStartupReference: 'true' },
        { mtaCoreGuiFrontendInputReference: true }, { mtaCoreGuiFrontendReference: true }, { mtaCoreGuiSurfaceReference: true },
        { mtaCoreGuiStartupReference: true }, { inputReference: true }, { reference: true }, { gaitRate: true }, { gaitRootLock: true },
        { mtaNativePadReference: true }, { mtaNativeMovementTrial: true }, { mtaCoreStartupReference: true }, { mtaCoreInputReference: true },
        { mtaCoreMovementReference: true }, { mtaCoreRenderReference: true }, { mtaCoreRenderGpuReference: true },
        { mtaCoreRenderCustomPresentReference: true }, { mtaCoreRenderFrameBoundaryReference: true }, { mtaCoreRenderNativeDispatchReference: true }])
        test('Core15 refuses competing modes before runtime lookup ' + JSON.stringify(options), () => {
            const result = coreFrontendStartup({ entry: { ...options, forbidRuntimeAccess: true } });
            assert.equal(result.calls.length, 0); assert.ok(result.logs.some(event => event.event === 'adapter_failed'));
        });
    test('Core14 frontend input uses separate256 evidence and no script player-control lease or native writes', () => {
        const result = coreFrontendInput(); const finish = frontendInputFinish(result);
        assert.equal(finish.passed, true); assert.equal(finish.cleanup_passed, true);
        assert.deepEqual(result.calls.filter(call => call[0].startsWith('DE_CoreGuiFrontendInputRead')).map(call => call[1]).slice(-256),
            Array.from({ length: 256 }, (_, field) => field));
        assert.ok(!result.calls.some(call => /SET_PLAYER_CONTROL|IS_PLAYER_CONTROL_ON|DE_CoreGuiFrontendControl|DE_CoreGuiFrontendBegin|DE_ConnectLocal|CREATE_CHAR/.test(call[0])));
        assert.equal(result.callFrames.find(call => call.name === 'DE_CoreGuiFrontendInputBegin').frame, 2);
        const start = result.logs.find(event => event.event === 'adapter_start');
        assert.equal(start.core_api, 14); assert.equal(start.frontend_input_trial_api, 1);
        assert.equal(start.native_player_control_lease_planned, false); assert.equal(start.native_player_control_writes, false);
        assert.equal(finish.native_player_control_writes, false); assert.equal(finish.native_pad_writes, true);
        assert.deepEqual(finish.native_scoped_write_steps, [3, 0]); assert.deepEqual(finish.native_scoped_restored_steps, [3, 0]);
        assert.equal(finish.actual_console_commands_verified, false);
        for (const claim of ['full_core_ready', 'full_mta_ready', 'gameplay_ready', 'camera_capture_verified', 'network_active',
            'remote_peds_created', 'native_queue_commands_issued', 'native_backbuffer_touched']) assert.equal(finish[claim], false);
    });
    test('Core14 neutral-query subset does not have to equal all routed movement queries', () => {
        const result = coreFrontendInput({ scalarOverrides: () => ({ 218: 1 }) });
        assert.equal(frontendInputFinish(result).passed, true);
    });
    test('Core14 deferred hidden frontend is enabled historically while native admission is still pending', () => {
        const result = coreFrontendInput({ scalarOverrides: polls => polls === 1 ? {
            162: 0, 163: 0, 165: 0, 208: 0, 210: 0, 212: 0, 214: 0, 216: 0, 220: 0,
            232: 0, 236: 0, 237: 0, 238: 1, 243: 0
        } : {} });
        const finish = frontendInputFinish(result);
        assert.equal(finish.passed, true); assert.equal(finish.stop_requested, false);
    });
    test('Core14 foreign callbacks forwarded outside the local scope do not invalidate successful ownership', () => {
        const result = coreFrontendInput({ scalarOverrides: () => ({ 224: 4 }) });
        assert.equal(frontendInputFinish(result).passed, true);
    });
    test('Core14 late F9 waits indefinitely and releases all modifiers before binding', () => {
        const result = coreFrontendInput({ entry: { keyFrames: [[], [], [17, 16, 120], [16], [], [], []],
            times: [0, 100, 14400000, 14400050, 14400100, 14400150, 14400200] } });
        assert.equal(result.callFrames.find(call => call.name === 'DE_CoreGuiFrontendInputBegin').frame, 4);
        assert.equal(frontendInputFinish(result).passed, true);
    });
    test('Core14 paused script can observe already completed Core cleanup without a heartbeat or late cancellation', () => {
        const result = coreFrontendInput({ entry: { times: [0, 50, 100, 300, 130000] } });
        const finish = frontendInputFinish(result);
        assert.equal(finish.cleanup_passed, true); assert.equal(finish.passed, true); assert.equal(finish.stop_requested, false);
        assert.ok(!result.calls.some(call => call[0] === 'DE_CoreStartupCancel'));
    });
    test('Core14 End requests cancellation while worker cleanup still drains', () => {
        const result = coreFrontendInput({ entry: { keyFrames: [[17, 16, 120], [16], [], [17, 16, 35], []] }, workerState: { 16: 12, 17: 0 } });
        const finish = frontendInputFinish(result);
        assert.equal(finish.stop_requested, true); assert.equal(finish.passed, false); assert.equal(finish.cleanup_passed, true);
        assert.equal(result.polls, 3); assert.ok(result.logs.some(event => event.event === 'core_gui_frontend_input_reference_stop_requested'));
    });
    test('Core14 transient poll failure requests cleanup and keeps polling to actual terminal', () => {
        const result = coreFrontendInput({ pollErrorAt: 1, workerState: { 16: 12, 17: 0 } });
        const finish = frontendInputFinish(result);
        assert.equal(result.polls, 3); assert.equal(finish.passed, false); assert.equal(finish.cleanup_passed, true);
        assert.match(finish.error, /temporary input worker/);
    });
    for (const [field, value] of [[7, 1], [172, 1], [226, 1], [228, 7], [241, 1], [187, 1], [188, 0], [189, 0],
        [33, 0], [35, 3], [38, 10000], [67, 4], [68, 1], [69, 1], [85, 30000], [99, 1], [181, 0], [182, 0], [184, 0], [242, 19999]])
        test('Core14 workload independently rejects insufficient native/frontend evidence: field ' + field, () => {
            const result = coreFrontendInput({ scalarOverrides: polls => polls >= 3 ? { [field]: value } : {} });
            const finish = frontendInputFinish(result); assert.ok(finish); assert.equal(finish.passed, false);
            assert.equal(finish.cleanup_passed, true);
        });
    for (const [field, value] of [[49, 1], [75, 1], [102, 1], [150, 1], [191, 1], [169, 1], [171, 0],
        [208, 1], [210, 1], [220, 2], [222, 1], [238, 0], [239, 1], [243, 1], [244, 1], [245, 1], [246, 1]])
        test('Core14 never certifies terminal cleanup with retained or invalid state: field ' + field, () => {
            const result = coreFrontendInput({ scalarOverrides: polls => polls >= 3 ? { [field]: value } : {} });
            const finish = frontendInputFinish(result); assert.ok(finish); assert.equal(finish.passed, false); assert.equal(finish.cleanup_passed, false);
        });
    for (const options of [{ skipTerminalCapture: true }, { workerState: { 13: 0 } }, { workerState: { 15: 0 } },
        { workerState: { 11: 2, 12: 3 } }, { workerState: { 16: 12, 17: 0 } }])
        test('Core14 requires fresh final publication and actual worker unload evidence ' + JSON.stringify(options), () => {
            const result = coreFrontendInput(options); const finish = frontendInputFinish(result);
            assert.equal(finish.passed, false); assert.equal(finish.cleanup_passed, false);
        });
    test('Core14 console command evidence is separate from native/render workload and Clear may leave empty output', () => {
        const result = coreFrontendInput({ scalarOverrides: polls => polls >= 3 ? { 139: 2, 141: 2, 142: 2, 146: 0 } : {} });
        const finish = frontendInputFinish(result);
        assert.equal(finish.actual_console_commands_verified, true); assert.equal(finish.passed, true);
    });
    for (const options of [{ mode: 'online' }, { mode: 'diagnostic' }, { mtaCoreGuiFrontendInputReference: 'true' },
        { mtaCoreGuiFrontendReference: true }, { mtaCoreGuiSurfaceReference: true }, { mtaCoreGuiStartupReference: true },
        { inputReference: true }, { reference: true }, { gaitRate: true }, { gaitRootLock: true }, { mtaNativePadReference: true },
        { mtaNativeMovementTrial: true }, { mtaCoreStartupReference: true }, { mtaCoreInputReference: true }, { mtaCoreMovementReference: true },
        { mtaCoreRenderReference: true }, { mtaCoreRenderGpuReference: true }, { mtaCoreRenderCustomPresentReference: true },
        { mtaCoreRenderFrameBoundaryReference: true }, { mtaCoreRenderNativeDispatchReference: true }])
        test('Core14 refuses mode conflicts before runtime lookup ' + JSON.stringify(options), () => {
            const result = coreFrontendInput({ entry: { ...options, forbidRuntimeAccess: true } });
            assert.equal(result.calls.length, 0); assert.ok(result.logs.some(event => event.event === 'adapter_failed'));
        });
    for (const name of ['DE_CoreGuiFrontendInputApiVersion', 'DE_CoreGuiFrontendInputBegin', 'DE_CoreGuiFrontendInputCapture', 'DE_CoreGuiFrontendInputRead'])
        test('Core14 refuses a missing independent export before Begin: ' + name, () => {
            const result = coreFrontendInput({ functions: { [name]: null } });
            assert.ok(!result.calls.some(call => call[0] === 'DE_CoreGuiFrontendInputBegin'));
            assert.ok(result.logs.some(event => event.event === 'adapter_failed' && event.error.includes(name)));
        });
    test('Frontend requires separate F9-release activation and verified real native bool lease', () => {
        const result = coreFrontend(); const finish = frontendFinish(result);
        assert.equal(result.callFrames.find(call => call.name === 'DynamicLibrary.Load').frame, 2);
        assert.equal(result.calls.filter(call => call[0] === 'DE_CoreGuiFrontendBegin').length, 1);
        assert.ok(result.callFrames.find(call => call.name === 'SET_PLAYER_CONTROL').frame > 2);
        assert.deepEqual(result.calls.filter(call => call[0] === 'SET_PLAYER_CONTROL'),
            [['SET_PLAYER_CONTROL', 0, false], ['SET_PLAYER_CONTROL', 0, true]]);
        assert.equal(result.enabled, true); assert.equal(result.restoredAt, 4);
        assert.ok(result.polls > result.restoredAt); assert.equal(finish.cleanup_passed, true); assert.equal(finish.passed, true);
        assert.equal(finish.control.native_initial, true); assert.equal(finish.control.native_disabled_verified, true);
        assert.equal(finish.control.native_restored_verified, true); assert.equal(finish.control.acquired, true);
        assert.ok(result.logs.filter(event => event.event === 'core_gui_frontend_reference_sample').every(event => event.state.length === 160));
        assert.ok(!result.calls.some(call => /DE_CoreGuiSurface|DE_CoreGuiStartup|DE_CoreInput|DE_CoreRender|DE_ConnectLocal|CREATE_CHAR/.test(call[0])));
        const start = result.logs.find(event => event.event === 'adapter_start');
        assert.equal(start.core_api, 13); assert.equal(start.frontend_trial_api, 1);
        assert.equal(start.input_writes, false); assert.equal(finish.input_writes, true);
        for (const claim of ['full_core_ready', 'full_mta_ready', 'gameplay_ready', 'camera_capture_verified', 'network_active',
            'remote_peds_created', 'native_queue_commands_issued', 'native_backbuffer_touched']) assert.equal(finish[claim], false);
    });
    test('Frontend reports actual Core menu callbacks and never fabricates clicks from rendering', () => {
        const result = coreFrontend({ aboutClicks: 2, quitRequested: true, creditsVisible: true, inputEvents: 15 });
        const finish = frontendFinish(result);
        assert.equal(finish.passed, true); assert.equal(finish.actual_about_clicks, 2);
        assert.equal(finish.actual_quit_requested, true); assert.equal(finish.actual_credits_visible, true);
        assert.equal(frontendFinish(coreFrontend()).actual_about_clicks, 0);
    });
    for (const options of [{ captureErrorAt: 3 }, { pollErrorAt: 3 }, { focusLostAt: 3 }, { throwAfterDisable: true }])
        test('Frontend failure retains and drains controls/Core instead of escaping: ' + JSON.stringify(options), () => {
            const result = coreFrontend(options); const finish = frontendFinish(result);
            assert.ok(finish); assert.equal(finish.passed, false); assert.equal(finish.cleanup_passed, true);
            assert.equal(result.enabled, true); assert.equal(finish.control.native_restored_verified, true);
            assert.equal(result.calls.filter(call => call[0] === 'DE_CoreStartupCancel').length, 1);
            assert.ok(result.polls >= 6); assert.ok(result.logs.some(event => event.event === 'adapter_failed'));
        });
    test('Frontend initially-disabled control refuses native writes and reports honest no-owner cleanup', () => {
        const result = coreFrontend({ initialControl: false }); const finish = frontendFinish(result);
        assert.equal(finish.passed, false); assert.equal(finish.cleanup_passed, true); assert.equal(result.enabled, false);
        assert.ok(!result.calls.some(call => call[0] === 'SET_PLAYER_CONTROL' || call[0] === 'DE_CoreGuiFrontendControlAck'));
        assert.equal(finish.control.acquired, false); assert.equal(finish.control.native_initial, false);
        assert.equal(finish.control.native_restored_verified, false); assert.equal(finish.control.no_mutation, true);
    });
    test('Frontend historical control fault fails workload while verified restoration can retire owners', () => {
        const result = coreFrontend({ controlFaultAt: 4 }); const finish = frontendFinish(result);
        assert.equal(finish.passed, false); assert.equal(finish.cleanup_passed, true);
        assert.equal(result.enabled, true); assert.equal(finish.control.native_restored_verified, true);
        assert.equal(finish.control.completed, true);
    });
    test('Frontend native-control divergence fails workload but accepted cancellation can prove completed cleanup', () => {
        const result = coreFrontend({ externalControlAt: 3, workerState: { 11: 2, 12: 2, 16: 12, 17: 0 } });
        const finish = frontendFinish(result);
        assert.equal(finish.passed, false); assert.equal(finish.cleanup_passed, true);
        assert.equal(finish.stop_requested, true); assert.match(finish.error, /Player control changed during ownership/);
        assert.equal(finish.state[16], 12); assert.equal(finish.state[17], 0);
        assert.equal(finish.control.completed, true); assert.equal(finish.control.input_detached, true);
        assert.equal(finish.control.native_restored_verified, true); assert.equal(finish.control.restored_acknowledged, true);
        assert.equal(finish.control.restore_attempts, 0);
        assert.deepEqual(result.calls.filter(call => call[0] === 'SET_PLAYER_CONTROL'), [['SET_PLAYER_CONTROL', 0, false]]);
        assert.equal(result.enabled, true);
    });
    for (const workerState of [{ 13: 0 }, { 14: 1 }, { 15: 0 }, { 7: 0 }, { 18: 5 },
        { 11: 2, 12: 3 }, { 16: 15 }, { 17: 1 }, { 16: 0, 17: 0 }])
        test('Frontend cancellation refuses retained or inconsistent worker cleanup ' + JSON.stringify(workerState), () => {
            const result = coreFrontend({ externalControlAt: 3, workerState: { 16: 12, 17: 0, ...workerState } });
            const finish = frontendFinish(result);
            assert.equal(finish.passed, false); assert.equal(finish.cleanup_passed, false);
            assert.equal(finish.control.completed, true); assert.equal(result.enabled, true);
        });
    test('Frontend CheckCancellation bookkeeping cannot substitute for an accepted script cancellation', () => {
        const result = coreFrontend({ workerState: { 16: 12, 17: 0 } });
        const finish = frontendFinish(result);
        assert.equal(finish.stop_requested, false); assert.equal(finish.passed, false);
        assert.equal(finish.cleanup_passed, false); assert.equal(finish.control.completed, true);
    });
    for (const options of [{ skipTerminalCapture: true }, { scalarOverrides: polls => polls >= 6 ? { 149: 2 } : {} },
        { scalarOverrides: polls => polls >= 6 ? { 49: 1 } : {} }])
        test('Frontend cancelled cleanup needs a valid terminal retirement publication ' + Object.keys(options)[0], () => {
            const result = coreFrontend({ externalControlAt: 3, workerState: { 16: 12, 17: 0 }, ...options });
            const finish = frontendFinish(result);
            assert.equal(finish.cleanup_passed, false); assert.equal(finish.passed, false);
            assert.equal(finish.control.completed, true); assert.equal(result.enabled, true);
        });
    test('Frontend terminal worker cannot finish cleanup while native restoration still fails', () => {
        const events = [];
        // Bound this deliberately unfinished fixture externally: a false worker
        // terminal flag must not discard the script's real native obligation.
        assert.throws(() => coreFrontend({ focusLostAt: 3, restoreDenied: true, workerTerminalAt: 6,
            workerState: { 16: 12, 17: 0 },
            entry: { executionTimeout: 100, observeLog: event => events.push(event) } }), /timed out/);
        assert.ok(events.some(event => event.event === 'core_gui_frontend_control' && event.action === 'cleanup_pending'));
        assert.ok(!events.some(event => event.event === 'core_gui_frontend_reference_finished'));
    });
    for (const field of [7, 140]) test('Frontend historical diagnostic permits proved cleanup, not workload: field ' + field, () => {
        const result = coreFrontend({ scalarOverrides: polls => polls >= 6 ? { [field]: 1, ...(field === 7 ? { 4: 10 } : {}) } : {} });
        const finish = frontendFinish(result);
        assert.equal(finish.passed, false); assert.equal(finish.cleanup_passed, true);
        assert.equal(finish.control.completed, true); assert.equal(finish.control.native_restored_verified, true);
        assert.equal(result.enabled, true);
    });
    test('Frontend permanently malformed final publication exits after actual owner drain without success proof', () => {
        const result = coreFrontend({ scalarOverrides: polls => polls >= 6 ? { 149: 2 } : {} });
        const finish = frontendFinish(result);
        assert.ok(finish); assert.equal(finish.passed, false); assert.equal(finish.cleanup_passed, false);
        assert.equal(finish.control.completed, true); assert.equal(result.enabled, true);
    });
    test('Frontend Create-null callback can retire controls with no fabricated CPU menu owner', () => {
        const result = coreFrontend({ createNull: true }); const finish = frontendFinish(result);
        assert.equal(finish.passed, false); assert.equal(finish.cleanup_passed, true);
        assert.equal(finish.control.native_restored_verified, true); assert.equal(result.enabled, true);
        const final = result.logs.filter(event => event.event === 'core_gui_frontend_reference_sample').at(-1).state;
        assert.ok(final.slice(96, 128).every(value => value === 0));
        assert.equal(final[154], 0); assert.equal(final[155], 1);
    });
    for (const [field, value] of [[33, 0], [35, 3], [38, 10000], [47, 8], [67, 4], [68, 1],
        [69, 1], [70, 2], [81, 0], [82, 0], [83, 0], [84, 0], [85, 8000], [99, 1], [151, 999], [153, 29999],
        [24, 0], [27, 28], [28, 0], [29, 16385], [53, 1], [54, 1], [128, 2]])
        test('Frontend final proof recomputes complete menu ownership: field ' + field, () => {
            const result = coreFrontend({ scalarOverrides: polls => polls >= 6 ? { [field]: value } : {} });
            const finish = frontendFinish(result); assert.ok(finish); assert.equal(finish.passed, false);
            assert.equal(finish.control.completed, true); assert.equal(result.enabled, true);
        });
    for (const options of [{ mode: 'online' }, { mode: 'diagnostic' }, { mtaCoreGuiFrontendReference: 'true' },
        { inputReference: true }, { reference: true }, { gaitRate: true }, { gaitRootLock: true },
        { mtaNativePadReference: true }, { mtaNativeMovementTrial: true }, { mtaCoreStartupReference: true },
        { mtaCoreInputReference: true }, { mtaCoreMovementReference: true }, { mtaCoreRenderReference: true },
        { mtaCoreRenderGpuReference: true }, { mtaCoreRenderCustomPresentReference: true },
        { mtaCoreRenderFrameBoundaryReference: true }, { mtaCoreRenderNativeDispatchReference: true },
        { mtaCoreGuiStartupReference: true }, { mtaCoreGuiSurfaceReference: true }])
        test('Frontend refuses conflicts before runtime lookup ' + JSON.stringify(options), () => {
            const result = coreFrontend({ entry: { ...options, forbidRuntimeAccess: true } });
            assert.equal(result.calls.length, 0); assert.ok(result.logs.some(event => event.event === 'adapter_failed'));
        });
    for (const name of ['DE_CoreGuiFrontendApiVersion', 'DE_CoreGuiFrontendBegin', 'DE_CoreGuiFrontendCapture', 'DE_CoreGuiFrontendRead',
        'DE_CoreGuiFrontendControlCapture', 'DE_CoreGuiFrontendControlRead', 'DE_CoreGuiFrontendControlAck',
        'DE_CoreGuiFrontendControlHeartbeat', 'DE_CoreGuiFrontendControlRevoke', 'DE_CoreGuiFrontendControlRestored', 'DE_CoreGuiFrontendControlFocused'])
        test('Frontend missing export refuses before Begin and native writes: ' + name, () => {
            const result = coreFrontend({ functions: { [name]: null } });
            assert.ok(!result.calls.some(call => call[0] === 'DE_CoreGuiFrontendBegin' || call[0] === 'SET_PLAYER_CONTROL'));
            assert.ok(result.logs.some(event => event.event === 'adapter_failed' && event.error.includes(name)));
        });
    for (const field of [120, 122, 123, 156, 157]) test('Frontend readiness/native assumptions never become success: field ' + field, () => {
        const result = coreFrontend({ scalarOverrides: polls => polls === 3 ? { [field]: 1 } : {} });
        const finish = frontendFinish(result); assert.equal(finish.passed, false); assert.equal(finish.cleanup_passed, true);
        assert.equal(result.enabled, true); assert.ok(finish.error.includes('scalar'));
    });
    test('Surface uses distinct Core12 API1/96 fields after F9 release and certifies only own composition workload', () => {
        const result = coreSurface();
        assert.equal(result.callFrames.find(call => call.name === 'DynamicLibrary.Load').frame, 2);
        assert.equal(result.calls.filter(call => call[0] === 'DE_CoreGuiSurfaceBegin').length, 1);
        assert.equal(result.calls.filter(call => call[0] === 'DE_CoreGuiSurfaceRead').length, 192);
        assert.ok(!result.calls.some(call => /DE_CoreGuiStartup|DE_CoreRender|DE_CoreInput|DE_ConnectLocal|CREATE_CHAR/.test(call[0])));
        const start = result.logs.find(event => event.event === 'adapter_start');
        assert.equal(start.core_api, 12); assert.equal(start.surface_api, 1);
        assert.equal(start.provenance, 'mta_core_owned_gui_surface');
        assert.equal(start.core_gui_surface_reference, true);
        assert.deepEqual(result.logs.filter(event => event.event === 'core_gui_surface_reference_sample').map(event => event.state.length), [96, 96]);
        const finish = surfaceFinish(result);
        assert.equal(finish.passed, true); assert.equal(finish.cleanup_passed, true);
        assert.equal(finish.own_surface_workload_verified, true); assert.equal(finish.composition_present_verified, true);
        for (const claim of ['full_core_ready', 'visible_gta_drawing_verified', 'network_active', 'input_writes',
            'remote_peds_created', 'native_queue_commands_issued', 'native_backbuffer_touched']) assert.equal(finish[claim], false);
    });
    test('Surface early readback survives active polling until full display completion', () => {
        let polls = 0;
        const result = coreSurface({ keyFrames: [[17, 16, 120], [], [], [], []], times: [0, 50, 100, 1000, 8600], functions: {
            DE_CoreStartupPoll: () => ++polls >= 3 ? 1 : 0,
            DE_CoreGuiSurfaceRead: field => coreSurfaceState(polls < 3 ? {
                1: 1, 2: polls, 3: polls === 1 ? 392 : 1000, 4: 0, 6: 0, 20: 0, 21: 0, 37: 1, 39: 0,
                47: 6, 49: 1, 50: 1, 51: 1, 74: 0, 75: 1, 76: 1, 77: 1, 78: 0, 79: 0, 80: 0
            } : {})[field]
        } });
        const samples = result.logs.filter(event => event.event === 'core_gui_surface_reference_sample');
        assert.deepEqual(samples.map(event => [event.state[1], event.state[20], event.state[21], event.state[37]]),
            [[1, 0, 0, 1], [1, 0, 0, 1], [2, 1, 1, 1]]);
        assert.equal(polls, 3);
        assert.ok(!result.calls.some(call => call[0] === 'DE_CoreStartupCancel'));
        assert.equal(surfaceFinish(result).passed, true);
        assert.equal(surfaceFinish(result).samples, 3);
    });
    test('Surface canceled after early readback preserves final cleanup without claiming a full display', () => {
        // PID10304 reached this state after its first pixels, before the display
        // interval completed. Readback is useful evidence even on cancellation.
        const result = coreSurface({ surfaceState: { 3: 392, 4: 2, 20: 0, 21: 0, 37: 1, 67: 2, 68: 2, 70: 2 } });
        const finish = surfaceFinish(result);
        assert.ok(finish);
        assert.equal(finish.cleanup_passed, true);
        assert.equal(finish.passed, false);
        assert.equal(finish.own_surface_workload_verified, false);
        assert.equal(finish.composition_present_verified, false);
        assert.equal(finish.visible_gta_drawing_verified, false);
        assert.ok(!result.calls.some(call => call[0] === 'DE_CoreStartupCancel'));
    });
    test('Surface early readback remains valid while cancellation drains retained resources', () => {
        let polls = 0;
        const result = coreSurface({ functions: {
            DE_CoreStartupPoll: () => ++polls >= 2 ? 1 : 0,
            DE_CoreGuiSurfaceRead: field => coreSurfaceState(polls < 2 ? {
                1: 3, 2: 1, 3: 392, 4: 2, 5: 1, 6: 0, 20: 0, 21: 0, 37: 1, 39: 0, 47: 8,
                49: 1, 50: 1, 51: 1, 75: 1, 76: 1, 77: 1, 78: 0, 79: 0, 80: 0
            } : { 3: 442, 4: 2, 20: 0, 21: 0, 37: 1 })[field]
        } });
        const samples = result.logs.filter(event => event.event === 'core_gui_surface_reference_sample');
        assert.deepEqual(samples.map(event => event.state[1]), [3, 2]);
        assert.equal(samples[0].state[5], 1);
        assert.equal(samples[0].state[6], 0);
        assert.equal(polls, 2);
        assert.ok(!result.calls.some(call => call[0] === 'DE_CoreStartupCancel'));
        assert.equal(surfaceFinish(result).cleanup_passed, true);
        assert.equal(surfaceFinish(result).passed, false);
        assert.equal(surfaceFinish(result).composition_present_verified, false);
    });
    for (const field of [19, 30, 31, 32]) test('Surface readback without owned rendering is rejected field ' + field, () => {
        const result = coreSurface({ surfacePendingState: { [field]: 0, 37: 1 } });
        assert.ok(!surfaceFinish(result));
        assert.equal(result.calls.filter(call => call[0] === 'DE_CoreStartupCancel').length, 1);
        assert.ok(result.logs.some(event => event.event === 'adapter_failed' && /ownership state/.test(event.error)));
    });
    for (const field of [20, 21]) test('Surface Completed reason still requires full workload field ' + field, () => {
        const result = coreSurface({ surfaceState: { [field]: 0, 37: 1 } });
        assert.ok(!surfaceFinish(result));
        assert.ok(result.logs.some(event => event.event === 'adapter_failed' && /ownership state/.test(event.error)));
    });
    for (const options of [{ mode: 'online' }, { mode: 'diagnostic' }, { mtaCoreGuiSurfaceReference: 'true' },
        { inputReference: true }, { reference: true }, { gaitRate: true }, { gaitRootLock: true },
        { mtaNativePadReference: true }, { mtaNativeMovementTrial: true }, { mtaCoreStartupReference: true },
        { mtaCoreInputReference: true }, { mtaCoreMovementReference: true }, { mtaCoreRenderReference: true },
        { mtaCoreRenderGpuReference: true }, { mtaCoreRenderCustomPresentReference: true },
        { mtaCoreRenderFrameBoundaryReference: true }, { mtaCoreRenderNativeDispatchReference: true }, { mtaCoreGuiStartupReference: true }])
        test('Surface refuses conflicts before runtime access ' + JSON.stringify(options), () => {
            const result = coreSurface({ ...options, forbidRuntimeAccess: true });
            assert.equal(result.calls.length, 0); assert.ok(result.logs.some(event => event.event === 'adapter_failed'));
        });
    for (const name of ['DE_CoreGuiSurfaceApiVersion', 'DE_CoreGuiSurfaceBegin', 'DE_CoreGuiSurfaceCapture', 'DE_CoreGuiSurfaceRead'])
        test('Surface missing export refuses without any Begin or fallback ' + name, () => {
            const result = coreSurface({ functions: { [name]: null } });
            assert.ok(!result.calls.some(call => /DE_CoreGuiSurfaceBegin|DE_CoreGuiStartupBegin|DE_CoreRenderBegin/.test(call[0])));
            assert.ok(result.logs.some(event => event.event === 'adapter_failed' && event.error.includes(name)));
        });
    test('Surface wrong API or rejected Begin never owns a poll/cancel job', () => {
        for (const functions of [{ DE_CoreGuiSurfaceApiVersion: () => 2 }, { DE_CoreGuiSurfaceBegin: () => 0 }]) {
            const result = coreSurface({ functions });
            assert.ok(!result.calls.some(call => /DE_CoreStartupPoll|DE_CoreStartupCancel|DE_CoreGuiSurfaceCapture/.test(call[0])));
            assert.ok(!surfaceFinish(result));
        }
    });
    for (const [field, value] of [[0, 2], [1, 0], [2, 0], [4, 12], [5, 2], [18, 1], [23, 7], [41, 5], [42, 11],
        [44, 1], [45, 1], [47, 10], [48, 10], [58, 2], [59, 2], [60, 2], [74, 2], [86, 2], [28, NaN],
        [29, -1], [25, 0x100000000], ...Array.from({ length: 8 }, (_, i) => [88 + i, 1])])
        test('Surface malformed scalar requests cleanup without proof field ' + field, () => {
            const result = coreSurface({ surfacePendingState: { [field]: value } });
            assert.equal(result.calls.filter(call => call[0] === 'DE_CoreStartupCancel').length, 1);
            assert.ok(!surfaceFinish(result)); assert.ok(!result.calls.some(call => call[0] === 'DE_CoreStartupRead'));
        });
    for (const field of [5, 43, 49, 50, 51, 59, 72, 75, 76, 77]) test('Surface complete cleanup cannot hide ownership field ' + field, () => {
        const result = coreSurface({ surfaceState: { [field]: 1 } });
        assert.ok(!surfaceFinish(result));
        assert.ok(result.logs.some(event => event.event === 'adapter_failed' && /retained ownership/.test(event.error)));
    });
    for (const field of [78, 79, 80]) test('Surface terminal cleanup requires window/composition/class cleanup field ' + field, () => {
        const result = coreSurface({ surfaceState: { [field]: 0 } });
        assert.ok(!surfaceFinish(result));
        assert.ok(result.logs.some(event => event.event === 'adapter_failed' && /retained ownership/.test(event.error)));
    });
    for (const [field, value] of [[3, 7999], [7, 1], [22, 1], [23, 1], [24, 0], [27, 24], [28, 0], [29, 16385],
        [35, 3], [38, 0], [46, 0], [47, 8], [48, 1], [52, 1],
        [53, 0], [54, 0], [55, 0], [56, 0], [60, 0], [61, 0], [62, 0], [63, 0], [64, 0], [65, 0], [66, 0],
        [67, 1], [68, 2], [69, 1], [70, 2], [73, 1], [81, 0], [82, 0], [83, 0], [84, 0],
        [85, 7999], [85, 8001], [86, 0]])
        test('Surface partial workload cannot claim own surface or Present proof field ' + field, () => {
            const finish = surfaceFinish(coreSurface({ surfaceState: { [field]: value } }));
            assert.ok(finish); assert.equal(finish.passed, false);
            assert.equal(finish.own_surface_workload_verified, false); assert.equal(finish.composition_present_verified, false);
            assert.equal(finish.cleanup_passed, field !== 7);
        });
    test('Surface successful Presents are counted and normal Shutdown stop is compatible with completion', () => {
        for (const stop of [0, 1]) {
            const finish = surfaceFinish(coreSurface({ surfaceState: { 67: 200, 68: 200, 70: 201, 74: stop, 71: 0xffffffff } }));
            assert.equal(finish.passed, true);
        }
    });
    test('Surface busy Present retries preserve counted successes and do not require another GUI frame', () => {
        assert.equal(surfaceFinish(coreSurface({ surfaceState: { 67: 200, 68: 3, 87: 197, 70: 3 } })).passed, true);
        for (const changes of [{ 67: 199 }, { 68: 1 }, { 87: 196 }, { 69: 0x887a000a }, { 70: 2 }]) {
            const finish = surfaceFinish(coreSurface({ surfaceState: { 67: 200, 68: 3, 87: 197, 70: 3, ...changes } }));
            assert.equal(finish.passed, false);
        }
        assert.equal(surfaceFinish(coreSurface({ surfaceState: { 67: 0xffffffff, 68: 0xffffffff, 87: 1 } })).passed, false);
    });
    test('Surface64 alias, invalid metadata and removed-fence sentinel cannot certify a workload', () => {
        for (const state of [{ 12: 0 }, { 58: 0 }, { 35: 0xffffffff, 36: 0xffffffff }]) {
            const result = coreSurface({ surfaceState: state });
            assert.ok(!surfaceFinish(result)); assert.ok(result.logs.some(event => event.event === 'adapter_failed'));
        }
        const result = coreSurface({ functions: { DE_CoreGuiSurfaceRead: field => coreGuiState()[field] } });
        assert.ok(!surfaceFinish(result));
    });
    test('Surface full 64-bit fence completion preserves high words', () => {
        assert.equal(surfaceFinish(coreSurface({ surfaceState: { 33: 0xffffffff, 34: 1, 35: 0, 36: 2 } })).passed, true);
        assert.equal(surfaceFinish(coreSurface({ surfaceState: { 33: 0, 34: 2, 35: 0xffffffff, 36: 1 } })).passed, false);
    });
    test('Surface backend pin expectation and Core worker state remain independently checked', () => {
        const result = coreSurface({ surfaceState: { 57: 0 } });
        assert.equal(surfaceFinish(result).cleanup_passed, false);
        for (const field of [0, 7, 13, 14, 16, 17, 18]) {
            const state = [1, 5, 6, 97, 0, 0, 0, 1, 3, 4, 5, 0, 0, 1, 0, 1, 0, 1, 0];
            state[field] = state[field] ? 0 : 1;
            const finish = surfaceFinish(coreSurface({ functions: { DE_CoreStartupRead: index => state[index] } }));
            assert.equal(finish.passed, false);
        }
    });
    test('Surface identical repeated sequences log once; changed repeat and regressed sequence cancel', () => {
        for (const mode of ['same', 'changed', 'regressed']) {
            let polls = 0;
            const result = coreSurface({ keyFrames: [[17, 16, 120], [], [], [], []], times: [0, 50, 100, 150, 8600], functions: {
                DE_CoreStartupPoll: () => ++polls >= 3 ? 1 : 0,
                DE_CoreGuiSurfaceRead: field => coreSurfaceState(polls >= 3 ? {} : { 1: 1, 2: mode === 'regressed' && polls === 1 ? 2 : 1,
                    3: mode === 'changed' && polls === 2 ? 51 : 50, 4: 0, 6: 0, 20: 0, 21: 0, 37: 0, 39: 0 })[field]
            } });
            if (mode === 'same') { assert.equal(surfaceFinish(result).passed, true); assert.equal(surfaceFinish(result).samples, 2); }
            else { assert.ok(!surfaceFinish(result)); assert.equal(result.calls.filter(call => call[0] === 'DE_CoreStartupCancel').length, 1); }
        }
    });
    for (const loading of [false, true]) test('Surface ' + (loading ? 'loading' : 'End') + ' cancel waits for final cleanup', () => {
        let polls = 0;
        const result = coreSurface({ keyFrames: [[17, 16, 120], [], [], [17, 16, 35], [17, 16, 35]],
            times: [0, 50, 100, 150, 8600], readyFrames: loading ? [true, true, true, false, false] : undefined, functions: {
                DE_CoreStartupPoll: () => ++polls >= 3 ? 1 : 0,
                DE_CoreGuiSurfaceRead: field => coreSurfaceState(polls < 3 ? { 1: 1, 2: polls, 3: polls * 50, 4: 0,
                    6: 0, 20: 0, 21: 0, 37: 0, 39: 0 } : {})[field]
            } });
        assert.equal(result.calls.filter(call => call[0] === 'DE_CoreStartupCancel').length, 1);
        assert.equal(polls, 3); assert.equal(surfaceFinish(result).stop_requested, true);
        assert.equal(surfaceFinish(result).cleanup_passed, true); assert.equal(surfaceFinish(result).passed, false);
    });
    test('Surface manual activation remains dormant until late F9 and full modifier release', () => {
        const result = coreSurface({ activation: 'automatic', keyFrames: [[], [], [17, 16, 120], [16], [], []],
            times: [0, 7200000, 14400000, 14400050, 14400100, 14408600] });
        assert.equal(result.callFrames.find(call => call.name === 'DynamicLibrary.Load').frame, 4);
        assert.equal(result.logs.find(event => event.event === 'core_gui_surface_reference_started').at_ms, 14400100);
        assert.equal(surfaceFinish(result).passed, true);
    });
    for (const options of [{ keyFrames: [[17, 16, 120, 35]] }, { keyFrames: [[17, 16, 120], [120]], times: [0, 10000] }])
        test('Surface cancelled or timed-out arming never touches runtime ' + JSON.stringify(options), () => {
            const result = coreSurface(options);
            assert.equal(result.runtimeAccess.length, 0); assert.ok(!surfaceFinish(result));
        });
    const coreRenderState = (overrides = {}, route = 1) => {
        const state = Array(44).fill(0);
        Object.assign(state, { 0: 2, 1: 2, 2: 201, 3: 10000, 4: 1, 5: 200, 6: 1, 7: 1, 9: 1, 11: 1,
            12: 1, 14: 1, 15: 1, 17: 600, 19: 600, 25: 42, 26: 1920, 27: 1080, 28: 1, 31: 1, 37: route === 1 ? 15 : 3, 40: route, 41: route, 42: route === 1 ? 15 : 19, 43: route === 2 ? 1 : 0 }, overrides);
        return state;
    };
    const coreRender = (overrides = {}) => {
        let polls = 0;
        return runEntry({ mtaCoreRenderReference: true, keyFrames: [[17, 16, 120], [16], [], []],
            times: [0, 50, 100, 10200], ...overrides,
            functions: { DE_CoreStartupPoll: () => ++polls >= 2 ? 1 : 0,
                DE_CoreRenderReadV2: index => coreRenderState({ ...(polls < 2 ? { 1: 1, 2: 1, 3: 0, 4: 0, 5: 1, 6: 0, 7: 0, 13: 1 } : {}),
                    ...overrides.renderState }, overrides.mtaCoreRenderRoute === 'd3d12' ? 2 : 1)[index],
                ...overrides.functions } });
    };
    const coreGpuState = (overrides = {}) => {
        const state = coreRenderState({}, 2).concat(Array(36).fill(0));
        Object.assign(state, { 0: 3, 44: 600, 46: 600, 50: 19, 52: 1, 53: 1, 58: 1, 59: 123, 61: 7,
            62: 1, 63: 3, 64: 87, 66: 1920, 67: 1080, 68: 1920, 70: 1080, 71: 87, 72: 1, 75: 1, 76: 1, 77: 1 }, overrides);
        return state;
    };
    const coreGpu = (overrides = {}) => {
        let polls = 0;
        return coreRender({ mtaCoreRenderRoute: 'd3d12', mtaCoreRenderGpuReference: true, ...overrides,
            functions: { DE_CoreRenderApiVersionV3: () => 3, DE_CoreRenderBeginV3: () => 1, DE_CoreRenderCaptureV3: () => 1,
                DE_CoreStartupPoll: () => ++polls >= 2 ? 1 : 0,
                DE_CoreRenderReadV3: field => coreGpuState({ ...(polls < 2 ? { 1: 1, 2: 1, 3: 0, 4: 0, 5: 1, 6: 0, 7: 0, 13: 1 } : {}),
                    ...overrides.renderState })[field], ...overrides.functions } });
    };
    const coreCustomState = (overrides = {}) => {
        const state = coreGpuState({ 0: 4, 46: 0, 48: 600, 53: 0, 64: 24, 71: 24, 74: 1, 75: 2 }).concat([2, 1, 127, 15]);
        Object.assign(state, overrides);
        return state;
    };
    const coreCustom = (overrides = {}) => {
        let polls = 0;
        return coreGpu({ mtaCoreRenderCustomPresentReference: true, ...overrides,
            functions: { DE_CoreRenderApiVersionV4: () => 4, DE_CoreRenderBeginV4: () => 1, DE_CoreRenderCaptureV4: () => 1,
                DE_CoreStartupPoll: () => ++polls >= 2 ? 1 : 0,
                DE_CoreRenderReadV4: field => coreCustomState({ ...(polls < 2 ? { 1: 1, 2: 1, 3: 0, 4: 0, 5: 1, 6: 0, 7: 0, 13: 1 } : {}),
                    ...overrides.renderState })[field], ...overrides.functions } });
    };
    const coreBoundaryState = (overrides = {}) => {
        const state = coreCustomState({ 0: 5 }).concat(Array(26).fill(0));
        Object.assign(state, { 84: 2, 85: 1, 86: 255, 88: 1, 89: 1, 91: 1, 92: 600, 104: 42, 108: 9 }, overrides);
        return state;
    };
    const coreBoundary = (overrides = {}) => {
        let polls = 0;
        return coreCustom({ mtaCoreRenderFrameBoundaryReference: true, ...overrides,
            functions: { DE_CoreRenderApiVersionV5: () => 5, DE_CoreRenderBeginV5: () => 1, DE_CoreRenderCaptureV5: () => 1,
                DE_CoreStartupPoll: () => ++polls >= 2 ? 1 : 0,
                DE_CoreRenderReadV5: field => coreBoundaryState({ ...(polls < 2 ? { 1: 1, 2: 1, 3: 0, 4: 0, 5: 1, 6: 0, 7: 0, 13: 1 } : {}),
                    ...overrides.renderState })[field], ...overrides.functions } });
    };
    const coreDispatchState = (overrides = {}) => {
        const state = coreBoundaryState({ 0: 6 }).concat(Array(26).fill(0));
        state.splice(110, 26, ...state.slice(84, 110)); state[111] = 2;
        state.fill(0, 84, 110); state[84] = 1;
        Object.assign(state, overrides); return state;
    };
    const coreDispatch = (overrides = {}) => {
        let polls = 0;
        return coreBoundary({ mtaCoreRenderNativeDispatchReference: true, ...overrides,
            functions: { DE_CoreRenderApiVersionV6: () => 6, DE_CoreRenderBeginV6: () => 1, DE_CoreRenderCaptureV6: () => 1,
                DE_CoreStartupPoll: () => ++polls >= 2 ? 1 : 0,
                DE_CoreRenderReadV6: field => coreDispatchState({ ...(polls < 2 ? { 1: 1, 2: 1, 3: 0, 4: 0, 5: 1, 6: 0, 7: 0, 13: 1 } : {}),
                    ...overrides.renderState })[field], ...overrides.functions } });
    };
    test('API6 transports136 scalars with independent profile2 and preserves refused standard Present', () => {
        const result = coreDispatch();
        assert.deepEqual(result.calls.find(call => call[0] === 'DE_CoreRenderBeginV6'), ['DE_CoreRenderBeginV6', 2]);
        assert.equal(result.calls.filter(call => call[0] === 'DE_CoreRenderReadV6').length, 272);
        assert.ok(!result.calls.some(call => /DE_CoreRender.*V[2345]/.test(call[0])));
        const start = result.logs.find(event => event.event === 'core_render_reference_started');
        assert.equal(start.core_api, 10); assert.equal(start.observation_api, 6);
        const finish = result.logs.find(event => event.event === 'core_render_reference_finished');
        assert.equal(finish.passed, false); assert.equal(finish.cleanup_passed, true); assert.equal(finish.full_core_ready, false);
        for (const { state } of result.logs.filter(event => event.event === 'core_render_reference_sample')) {
            assert.equal(state.length, 136); assert.deepEqual(Array.from(state.slice(84, 110)), [1, ...Array(25).fill(0)]);
            assert.deepEqual(Array.from(state.slice(110, 113)), [2, 2, 255]); assert.equal(state[53], 0);
        }
    });
    for (const override of [{ mtaCoreRenderFrameBoundaryReference: false }, { mtaCoreRenderCustomPresentReference: false },
            { mtaCoreRenderGpuReference: false }, { mtaCoreRenderReference: false }, { mtaCoreRenderRoute: 'd3d11' },
            { mtaCoreRenderNativeDispatchReference: 'true' }])
        test('API6 requires explicit native dispatch prerequisites ' + JSON.stringify(override), () => {
            const result = coreDispatch(override); assert.equal(result.calls.length, 0);
            assert.ok(result.logs.some(event => event.event === 'adapter_failed'));
        });
    for (const value of [undefined, () => 5, () => 7])
        test('API6 missing or mismatched export cannot fall back to API5 ' + String(value), () => {
            const result = coreDispatch({ functions: { DE_CoreRenderApiVersionV6: value } });
            assert.ok(!result.calls.some(call => /DE_CoreRenderBegin/.test(call[0])));
        });
    for (const [field, value] of [[111, 1], [111, 0], [111, 3], [112, 254], [113, 0x80004005], [114, 2],
            [115, 16], [116, 1], [117, 0], [120, 601], [130, 43], [133, 1], [134, 8], [135, 1], [85, 2]])
        test('API6 independently refuses forged dispatch field ' + field + ':' + value, () => {
            const result = coreDispatch({ renderState: { [field]: value } });
            assert.equal(result.calls.filter(call => call[0] === 'DE_CoreStartupCancel').length, 1);
            assert.ok(!result.logs.some(event => event.event === 'core_render_reference_finished'));
        });
    test('API6 accepts pointer-free refusals without inventing readiness', () => {
        for (const [status, profile] of [[1, 0], [10, 2], [8, 2]]) {
            const zero = Object.fromEntries(Array.from({ length: 26 }, (_, i) => [110 + i, 0]));
            const result = coreDispatch({ renderState: { ...zero, 110: status, 111: profile } });
            assert.equal(result.logs.find(event => event.event === 'core_render_reference_finished').passed, false);
        }
    });
    test('API5 carries scalar110 separately and never promotes coherent CPU association to GPU readiness', () => {
        const result = coreBoundary();
        assert.deepEqual(result.calls.find(call => call[0] === 'DE_CoreRenderBeginV5'), ['DE_CoreRenderBeginV5', 2]);
        assert.equal(result.calls.filter(call => call[0] === 'DE_CoreRenderReadV5').length, 220);
        assert.ok(!result.calls.some(call => /DE_CoreRender.*V[234]/.test(call[0])));
        const start = result.logs.find(event => event.event === 'core_render_reference_started');
        assert.equal(start.core_api, 9); assert.equal(start.observation_api, 5); assert.equal(start.requested_route, 2);
        const finish = result.logs.find(event => event.event === 'core_render_reference_finished');
        assert.equal(finish.passed, false); assert.equal(finish.cleanup_passed, true);
        for (const flag of ['full_core_ready', 'rendering_started', 'network_active', 'input_writes', 'remote_peds_created']) assert.equal(finish[flag], false);
        for (const { state } of result.logs.filter(event => event.event === 'core_render_reference_sample')) {
            assert.equal(state.length, 110); assert.equal(state[53], 0); assert.equal(state[80], 2);
            assert.deepEqual(Array.from(state.slice(84, 87)), [2, 1, 255]);
        }
        assert.ok(!result.calls.some(call => /DE_Connect|DE_MtaMovement|DE_CoreInput|DE_CoreMovement|GET_CHAR/.test(call[0])));
    });
    for (const override of [{ mtaCoreRenderCustomPresentReference: false }, { mtaCoreRenderGpuReference: false },
            { mtaCoreRenderReference: false }, { mtaCoreRenderRoute: 'd3d11' }, { mtaCoreRenderFrameBoundaryReference: 'true' }])
        test('API5 requires each explicit independent admission ' + JSON.stringify(override), () => {
            const result = coreBoundary(override); assert.equal(result.calls.length, 0);
            assert.ok(result.logs.some(event => event.event === 'adapter_failed'));
        });
    for (const value of [undefined, () => 4, () => 6])
        test('API5 missing or wrong version cannot fall back to classification ' + String(value), () => {
            const result = coreBoundary({ functions: { DE_CoreRenderApiVersionV5: value } });
            assert.ok(!result.calls.some(call => /DE_CoreRenderBegin/.test(call[0])));
        });
    for (const [field, value] of [[84, 0], [84, 11], [85, 0], [85, 2], [86, 254], [86, 256], [87, 0x80004005],
            [88, 2], [89, 16], [90, 1], [91, 0], [91, 2], [94, 601], [104, 43], [107, 1], [107, 4], [108, 8], [108, 10], [109, 1]])
        test('API5 rejects contradictory complete association ' + field + ':' + value, () => {
            const result = coreBoundary({ renderState: { [field]: value } });
            assert.equal(result.calls.filter(call => call[0] === 'DE_CoreStartupCancel').length, 1);
            assert.ok(!result.logs.some(event => event.event === 'core_render_reference_finished'));
        });
    for (const tail of [
        { 84: 1, 85: 0, 86: 0, 88: 0, 89: 0, 91: 0, 104: 0, 108: 0 },
        { 84: 10, 86: 0, 88: 0, 89: 0, 91: 0, 104: 0, 108: 0 },
        { 84: 3, 86: 25, 91: 0, 94: 600, 108: 4, 109: 1 },
        { 84: 5, 86: 25, 91: 0, 94: 600, 108: 3, 109: 0x80004002 },
        { 84: 5, 94: 600, 109: 0x8000ffff },
        { 84: 6, 86: 127, 94: 600, 108: 8, 109: 1 },
        { 84: 7, 86: 127, 94: 600, 107: 1 },
        { 84: 9, 87: 0x80004005, 94: 600 }])
        test('API5 preserves an incomplete boundary diagnostic and clean lifecycle ' + JSON.stringify(tail), () => {
            const result = coreBoundary({ renderState: tail });
            const finish = result.logs.find(event => event.event === 'core_render_reference_finished');
            assert.equal(finish.passed, false); assert.equal(finish.cleanup_passed, true);
            assert.equal(result.calls.filter(call => call[0] === 'DE_CoreStartupCancel').length, 0);
        });
    test('API5 pre-entry endpoint change preserves zero/current historical counters without fabricating Present', () => {
        for (const cumulative of [0, 600]) {
            const state = { 84: 8, 86: 0, 88: 0, 89: 0, 91: 0, 92: cumulative, 94: cumulative, 104: 0, 108: 0 };
            const result = coreBoundary({ renderState: state });
            assert.equal(result.logs.find(event => event.event === 'core_render_reference_finished').passed, false);
            const forged = coreBoundary({ renderState: { ...state, 87: 1 } });
            assert.ok(!forged.logs.some(event => event.event === 'core_render_reference_finished'));
        }
        const staleRoots = coreBoundary({ renderState: { 84: 7, 94: 600, 107: 1 } });
        assert.ok(!staleRoots.logs.some(event => event.event === 'core_render_reference_finished'));
    });
    test('API5 all split64 counters preserve carry and reject later regression', () => {
        let polls = 0;
        const initial = coreBoundaryState({ 1: 1, 2: 1, 3: 0, 4: 0, 5: 1, 6: 0, 7: 0, 13: 1, 92: 0xffffffff });
        const terminal = coreBoundaryState({ 92: 0, 93: 1 });
        const good = coreBoundary({ functions: { DE_CoreStartupPoll: () => ++polls >= 2 ? 1 : 0,
            DE_CoreRenderReadV5: field => (polls < 2 ? initial : terminal)[field] } });
        assert.equal(good.logs.find(event => event.event === 'core_render_reference_finished').cleanup_passed, true);
        for (const pair of [92, 94, 96, 98, 100, 102]) {
            polls = 0; const prior = coreBoundaryState({ 1: 1, 2: 1, 3: 0, 4: 0, 5: 1, 6: 0, 7: 0, 13: 1, [pair]: pair === 92 ? 601 : 1 });
            const later = coreBoundaryState();
            const bad = coreBoundary({ functions: { DE_CoreStartupPoll: () => ++polls >= 2 ? 1 : 0,
                DE_CoreRenderReadV5: field => (polls < 2 ? prior : later)[field] } });
            assert.ok(!bad.logs.some(event => event.event === 'core_render_reference_finished'));
        }
    });
    test('API5 fatal earlier GPU cleanup may have no boundary attempt or retain historical cached association', () => {
        const fatal = coreBoundaryState({ 13: 0, 30: 10, 50: 18, 51: 0x8000ffff, 52: 0, 75: 6 });
        fatal.fill(0, 84);
        let polls = 0;
        const noAttempt = coreBoundary({ functions: { DE_CoreStartupPoll: () => ++polls >= 2 ? 1 : 0,
            DE_CoreRenderReadV5: field => ({ ...fatal, ...(polls < 2 ? { 1: 1, 2: 1, 3: 0, 4: 0, 5: 1, 6: 0, 7: 0 } : {}) })[field] } });
        assert.equal(noAttempt.logs.find(event => event.event === 'core_render_reference_finished').passed, false);
        polls = 0;
        const initial = coreBoundaryState({ 1: 1, 2: 1, 3: 0, 4: 0, 5: 1, 6: 0, 7: 0, 13: 1 });
        const retained = coreBoundaryState({ 13: 0, 30: 10, 50: 18, 51: 0x8000ffff, 52: 0, 75: 6 });
        const cached = coreBoundary({ functions: { DE_CoreStartupPoll: () => ++polls >= 2 ? 1 : 0,
            DE_CoreRenderReadV5: field => (polls < 2 ? initial : retained)[field] } });
        assert.equal(cached.logs.find(event => event.event === 'core_render_reference_finished').passed, false);
        assert.equal(cached.logs.find(event => event.event === 'core_render_reference_finished').full_core_ready, false);
    });
    test('API4 static recognition retains scalar84 and API3 custom-present refusal without drawing', () => {
        const result = coreCustom();
        assert.deepEqual(result.calls.find(call => call[0] === 'DE_CoreRenderBeginV4'), ['DE_CoreRenderBeginV4', 2]);
        assert.equal(result.calls.filter(call => call[0] === 'DE_CoreRenderReadV4').length, 168);
        assert.ok(!result.calls.some(call => /DE_CoreRender.*V[23]/.test(call[0])));
        const start = result.logs.find(event => event.event === 'core_render_reference_started');
        assert.equal(start.core_api, 8); assert.equal(start.observation_api, 4); assert.equal(start.requested_route, 2);
        const finish = result.logs.find(event => event.event === 'core_render_reference_finished');
        assert.equal(finish.passed, false); assert.equal(finish.cleanup_passed, true);
        for (const { state } of result.logs.filter(event => event.event === 'core_render_reference_sample')) {
            assert.equal(state.length, 84);
            assert.deepEqual(Array.from(state.slice(78)), [0, 0, 2, 1, 127, 15]);
            assert.equal(state[53], 0); assert.equal(state[74], 1); assert.equal(state[75], 2);
        }
        for (const flag of ['full_core_ready', 'rendering_started', 'network_active', 'input_writes', 'remote_peds_created'])
            assert.equal(finish[flag], false);
        assert.ok(result.logs.some(event => event.event === 'adapter_failed' && event.error.includes('renderer observation incomplete')));
        assert.ok(!result.calls.some(call => /DE_Connect|DE_MtaMovement|DE_CoreInput|DE_CoreMovement|GET_CHAR/.test(call[0])));
    });
    for (const override of [{ mtaCoreRenderGpuReference: false }, { mtaCoreRenderReference: false },
            { mtaCoreRenderRoute: 'd3d11' }, { mtaCoreRenderCustomPresentReference: 'true' }])
        test('API4 cannot activate without independent consistent admission ' + JSON.stringify(override), () => {
            const result = coreCustom(override);
            assert.equal(result.calls.length, 0);
            assert.ok(result.logs.some(event => event.event === 'adapter_failed'));
        });
    for (const value of [undefined, () => 3, () => 5])
        test('API4 missing or wrong version cannot fall back to API3 ' + String(value), () => {
            const result = coreCustom({ functions: { DE_CoreRenderApiVersionV4: value } });
            assert.ok(!result.calls.some(call => /DE_CoreRenderBegin/.test(call[0])));
            assert.ok(result.logs.some(event => event.event === 'adapter_failed'));
        });
    for (const [status, mask] of [[3, 0], [4, 0], [4, 63], [5, 0], [5, 127], [6, 127], [7, 0]])
        test('API4 preserves unrecognized/fault outcome ' + status + ':' + mask + ' and cleanup', () => {
            const result = coreCustom({ renderState: { 80: status, 81: 0, 82: mask, 83: 0 } });
            const finish = result.logs.find(event => event.event === 'core_render_reference_finished');
            assert.equal(finish.passed, false); assert.equal(finish.cleanup_passed, true);
            assert.deepEqual(Array.from(result.logs.find(event => event.event === 'core_render_reference_sample').state.slice(80)), [status, 0, mask, 0]);
            assert.equal(result.calls.filter(call => call[0] === 'DE_CoreStartupCancel').length, 0);
        });
    test('API4 absent presenter preserves independent API3 GPU result without recognition', () => {
        const result = coreCustom({ renderState: { 46: 600, 48: 0, 53: 1, 74: 0, 75: 1, 80: 1, 81: 0, 82: 0, 83: 0 } });
        assert.equal(result.logs.find(event => event.event === 'core_render_reference_finished').passed, true);
        assert.equal(result.logs.find(event => event.event === 'core_render_reference_sample').state[80], 1);
    });
    test('API4 known static code remains independent of a failed COM query', () => {
        const result = coreCustom({ renderState: { 50: 8, 51: 0x80004002, 52: 0, 75: 3 } });
        const finish = result.logs.find(event => event.event === 'core_render_reference_finished');
        assert.equal(finish.passed, false); assert.equal(finish.cleanup_passed, true);
        assert.equal(result.logs.find(event => event.event === 'core_render_reference_sample').state[80], 2);
        assert.equal(result.calls.filter(call => call[0] === 'DE_CoreStartupCancel').length, 0);
    });
    for (const [field, value] of [[80, 0], [80, 8], [81, 0], [81, 2], [82, 126], [82, 128], [83, 0], [83, 16], [78, 1], [79, 1]])
        test('API4 rejects contradictory classification or old reserved field ' + field + ':' + value, () => {
            const result = coreCustom({ renderState: { [field]: value } });
            assert.equal(result.calls.filter(call => call[0] === 'DE_CoreStartupCancel').length, 1);
            assert.ok(!result.logs.some(event => event.event === 'core_render_reference_finished'));
        });
    for (const [status, mask] of [[1, 1], [3, 127], [7, 127]])
        test('API4 unrecognized outcome cannot retain signatures ' + status, () => {
            const result = coreCustom({ renderState: { 80: status, 81: 0, 82: mask, 83: 0 } });
            assert.equal(result.calls.filter(call => call[0] === 'DE_CoreStartupCancel').length, 1);
        });
    for (const status of [3, 4, 6])
        test('API4 classification requires custom presence for status ' + status, () => {
            const result = coreCustom({ renderState: { 58: 2, 74: 0, 80: status, 81: 0, 82: 0, 83: 0 } });
            assert.equal(result.calls.filter(call => call[0] === 'DE_CoreStartupCancel').length, 1);
        });
    for (const status of [5, 7])
        test('API4 incomplete root knowledge permits unknown custom presence for status ' + status, () => {
            const result = coreCustom({ renderState: { 58: 2, 74: 0, 80: status, 81: 0, 82: 0, 83: 0 } });
            const finish = result.logs.find(event => event.event === 'core_render_reference_finished');
            assert.equal(finish.passed, false); assert.equal(finish.cleanup_passed, true);
        });
    test('API4 initial zero-attempt state keeps GPU and classification tails independently zero', () => {
        let polls = 0;
        const initial = coreCustomState({ 1: 1, 2: 1, 3: 0, 4: 0, 5: 1, 6: 0, 7: 0, 13: 1, 17: 0, 19: 0, 25: 0, 26: 0, 27: 0,
            28: 0, 31: 0, 37: 0, 41: 0, 42: 0, 43: 0 });
        initial.fill(0, 44);
        const result = coreCustom({ functions: { DE_CoreStartupPoll: () => ++polls >= 2 ? 1 : 0,
            DE_CoreRenderReadV4: field => (polls < 2 ? initial : coreCustomState())[field] } });
        const samples = result.logs.filter(event => event.event === 'core_render_reference_sample');
        assert.equal(samples.length, 2); assert.ok(Array.from(samples[0].state.slice(44)).every(value => value === 0));
        assert.equal(result.logs.find(event => event.event === 'core_render_reference_finished').cleanup_passed, true);
    });
    test('GPU queries require independent API3 admission and preserve scalar80 evidence without drawing', () => {
        const result = coreGpu();
        assert.deepEqual(result.calls.find(call => call[0] === 'DE_CoreRenderBeginV3'), ['DE_CoreRenderBeginV3', 2]);
        assert.equal(result.calls.filter(call => call[0] === 'DE_CoreRenderReadV3').length, 160);
        assert.ok(!result.calls.some(call => /DE_CoreRender.*V2/.test(call[0])));
        assert.equal(result.logs.find(event => event.event === 'core_render_reference_started').observation_api, 3);
        const finish = result.logs.find(event => event.event === 'core_render_reference_finished');
        assert.equal(finish.passed, true); assert.equal(finish.rendering_started, false); assert.equal(finish.input_writes, false);
        assert.equal(result.logs.find(event => event.event === 'core_render_reference_sample').state.length, 80);
    });
    for (const override of [{ mtaCoreRenderRoute: 'd3d11' }, { mtaCoreRenderReference: false }, { mtaCoreRenderGpuReference: 'true' }])
        test('GPU metadata cannot be implicitly enabled by inconsistent configuration ' + JSON.stringify(override), () => {
            const result = coreGpu(override);
            assert.equal(result.calls.length, 0);
            assert.ok(result.logs.some(event => event.event === 'adapter_failed'));
        });
    for (const value of [undefined, () => 2, () => 4])
        test('GPU API3 missing or wrong version has no API2 fallback ' + String(value), () => {
            const result = coreGpu({ functions: { DE_CoreRenderApiVersionV3: value } });
            assert.ok(!result.calls.some(call => /DE_CoreRenderBegin/.test(call[0])));
            assert.ok(result.logs.some(event => event.event === 'adapter_failed'));
        });
    for (const [field, value] of [[44, 599], [45, 1], [46, 599], [48, 1], [50, 0], [50, 20], [51, 1], [52, 2], [53, 0],
            [54, 2], [57, 2], [58, 2], [61, 3], [62, 3], [63, 1], [63, 17], [63, 0xffffffff], [66, 0], [68, 1919], [69, 1], [70, 1079], [71, 28],
            [72, 2], [73, 1], [74, 1], [75, 0], [76, 2], [77, 2], [78, 1], [79, 1]])
        test('GPU success rejects contradictory metadata field ' + field, () => {
            const result = coreGpu({ renderState: { [field]: value } });
            assert.equal(result.calls.filter(call => call[0] === 'DE_CoreStartupCancel').length, 1);
            assert.ok(!result.logs.some(event => event.event === 'core_render_reference_finished' && event.passed));
        });
    test('GPU success cannot agree on an unknown swapchain and backbuffer format', () => {
        const result = coreGpu({ renderState: { 64: 0, 71: 0 } });
        assert.equal(result.calls.filter(call => call[0] === 'DE_CoreStartupCancel').length, 1);
        assert.ok(!result.logs.some(event => event.event === 'core_render_reference_finished' && event.passed));
    });
    test('GPU unsupported multi-node diagnostic retains native callback and actual cleanup without passing', () => {
        const result = coreGpu({ renderState: { 46: 0, 48: 600, 53: 0, 58: 2, 75: 2 } });
        const finish = result.logs.find(event => event.event === 'core_render_reference_finished');
        assert.equal(finish.passed, false); assert.equal(finish.cleanup_passed, true);
        const sample = result.logs.find(event => event.event === 'core_render_reference_sample').state;
        assert.equal(sample[19], 600); assert.equal(sample[48], 600); assert.equal(sample[58], 2);
    });
    for (const fullyQueried of [false, true])
        test('GPU custom presenter preserves ' + (fullyQueried ? 'queried metadata' : 'historical pre-COM guard') + ' without renderer admission', () => {
            // Full COM metadata must survive the unsupported outcome without
            // turning a custom-present route into a renderer or gameplay pass.
            const metadata = fullyQueried ? {} : Object.fromEntries(Array.from({ length: 20 }, (_, index) => [54 + index, 0]));
            const result = coreGpu({ renderState: { ...metadata, 46: 0, 48: 600, 50: fullyQueried ? 19 : 2,
                52: fullyQueried ? 1 : 0, 53: 0, 74: 1, 75: 2 } });
            const finish = result.logs.find(event => event.event === 'core_render_reference_finished');
            assert.ok(finish);
            assert.equal(finish.passed, false); assert.equal(finish.cleanup_passed, true);
            for (const claim of ['full_core_ready', 'rendering_started', 'network_active', 'input_writes', 'remote_peds_created'])
                assert.equal(finish[claim], false);
            assert.equal(finish.state[13], 1); assert.equal(finish.state[15], 1);
            const samples = result.logs.filter(event => event.event === 'core_render_reference_sample');
            assert.equal(samples.length, 2);
            for (const { state } of samples) {
                assert.equal(state.length, 80);
                assert.equal(state[19], 600); assert.equal(state[44], 600); assert.equal(state[46], 0); assert.equal(state[48], 600);
                assert.deepEqual(Array.from(state.slice(50, 54)), [fullyQueried ? 19 : 2, 0, fullyQueried ? 1 : 0, 0]);
                assert.equal(state[61], fullyQueried ? 7 : 0);
                assert.deepEqual(Array.from(state.slice(74, 78)), [1, 2, 1, 1]);
            }
            assert.equal(result.calls.filter(call => call[0] === 'DE_CoreStartupCancel').length, 0);
            assert.ok(result.logs.some(event => event.event === 'adapter_failed' && event.error.includes('renderer observation incomplete')));
            assert.ok(!result.calls.some(call => /DE_Connect|DE_MtaMovement|DE_CoreInput|DE_CoreMovement|GET_CHAR/.test(call[0])));
        });
    test('GPU failed COM query preserves exact HRESULT and stage and reports clean diagnostic failure', () => {
        const result = coreGpu({ renderState: { 46: 0, 48: 600, 50: 8, 51: 0x80004002, 52: 0, 53: 0, 75: 3 } });
        const finish = result.logs.find(event => event.event === 'core_render_reference_finished');
        assert.equal(finish.passed, false); assert.equal(finish.cleanup_passed, true);
        const sample = result.logs.find(event => event.event === 'core_render_reference_sample').state;
        assert.equal(sample[50], 8); assert.equal(sample[51], 0x80004002);
    });
    test('Core render probe waits for F9 release and reads scalar44 snapshots on yielding worker polls', () => {
        const result = coreRender();
        assert.equal(result.callFrames.find(call => call.name === 'DynamicLibrary.Load').frame, 2);
        assert.equal(result.calls.filter(call => call[0] === 'DE_CoreRenderBeginV2').length, 1);
        assert.equal(result.calls.filter(call => call[0] === 'DE_CoreRenderReadV2').length, 88);
        const finish = result.logs.find(event => event.event === 'core_render_reference_finished');
        assert.equal(finish.passed, true); assert.equal(finish.cleanup_passed, true); assert.equal(finish.state[15], 1);
        assert.equal(finish.rendering_started, false); assert.equal(finish.input_writes, false);
        assert.ok(!result.calls.some(call => /DE_Connect|DE_MtaMovement|DE_CoreInput|DE_CoreMovement|GET_CHAR/.test(call[0])));
        assert.equal(result.waits.at(-1).milliseconds, 100);
    });
    test('Core render native false return remains valid scalar observation without a COM Present claim', () => {
        const finish = coreRender().logs.find(event => event.event === 'core_render_reference_finished');
        assert.equal(finish.passed, true);
        assert.equal(finish.full_core_ready, false);
        assert.equal(finish.rendering_started, false);
    });
    test('Core selects D3D12 explicitly and keeps queue separate from unknown COM device/context', () => {
        const result = coreRender({ mtaCoreRenderRoute: 'd3d12' });
        assert.deepEqual(result.calls.find(call => call[0] === 'DE_CoreRenderBeginV2'), ['DE_CoreRenderBeginV2', 2]);
        const sample = result.logs.find(event => event.event === 'core_render_reference_sample').state;
        assert.equal(sample[37], 3); assert.deepEqual(Array.from(sample.slice(40)), [2, 2, 19, 1]);
        assert.equal(result.logs.find(event => event.event === 'core_render_reference_started').requested_route, 2);
        assert.equal(result.logs.find(event => event.event === 'core_render_reference_finished').passed, true);
    });
    test('Core missing D3D12 callbacks leave observed route unknown and still report actual cleanup', () => {
        const zeros = Object.fromEntries([...Array.from({ length: 12 }, (_, n) => n + 17), 31, 33, 34, 35, 36, 37, 41, 42, 43].map(n => [n, 0]));
        const result = coreRender({ mtaCoreRenderRoute: 'd3d12', renderState: zeros });
        const finish = result.logs.find(event => event.event === 'core_render_reference_finished');
        assert.equal(finish.passed, false); assert.equal(finish.cleanup_passed, true);
        assert.equal(result.logs.find(event => event.event === 'core_render_reference_sample').state[41], 0);
        assert.equal(result.calls.filter(call => call[0] === 'DE_CoreRenderBeginV2').length, 1);
        assert.ok(!result.calls.some(call => call[0] === 'DE_CoreRenderBegin'));
    });
    for (const [field, value] of [[0, 1], [37, 15], [40, 1], [41, 1], [42, 15], [42, 23], [43, 0]])
        test('Core D3D12 rejects contradictory route or COM knowledge field ' + field + ':' + value, () => {
            const result = coreRender({ mtaCoreRenderRoute: 'd3d12', renderState: { [field]: value } });
            assert.equal(result.calls.filter(call => call[0] === 'DE_CoreStartupCancel').length, 1);
            assert.ok(!result.logs.some(event => event.event === 'core_render_reference_finished' && event.passed));
        });
    for (const field of [25, 26, 27, 28, 29, 31])
        test('Core unobserved route rejects fabricated callback scalar ' + field, () => {
            const zeros = Object.fromEntries([...Array.from({ length: 12 }, (_, n) => n + 17), 31, 33, 34, 35, 36, 37, 41, 42, 43].map(n => [n, 0]));
            const result = coreRender({ mtaCoreRenderRoute: 'd3d12', renderState: { ...zeros, [field]: 1 } });
            assert.equal(result.calls.filter(call => call[0] === 'DE_CoreStartupCancel').length, 1);
            assert.ok(!result.logs.some(event => event.event === 'core_render_reference_finished'));
        });
    for (const value of [undefined, () => 1, () => 3])
        test('Core API2 admission never falls back to API1 when version/export is unavailable ' + String(value), () => {
            const result = coreRender({ functions: { DE_CoreRenderApiVersionV2: value } });
            assert.equal(result.calls.filter(call => call[0] === 'DE_CoreRenderBeginV2').length, 0);
            assert.ok(result.logs.some(event => event.event === 'adapter_failed'));
        });
    test('Core rejects an unknown renderer route before loading the bridge', () => {
        const result = coreRender({ mtaCoreRenderRoute: 'automatic' });
        assert.equal(result.calls.length, 0);
        assert.ok(result.logs.some(event => event.event === 'adapter_failed'));
    });
    for (const [field, value] of [[5, 0], [7, 0], [8, 1], [9, 0], [10, 1], [11, 0], [12, 0], [13, 1], [14, 0], [15, 0], [17, 0], [19, 0],
            [21, 1], [23, 1], [25, 0], [26, 0], [27, 0], [29, 2], [30, 7], [31, 0], [32, 1], [33, 1], [37, 3], [38, 1], [39, 1]])
        test('Core render cannot pass missing native/cleanup evidence field ' + field, () => {
            const result = coreRender({ renderState: { [field]: value } });
            assert.ok(result.logs.some(event => event.event === 'adapter_failed'));
            assert.ok(!result.logs.some(event => event.event === 'core_render_reference_finished' && event.passed));
        });
    test('Core render cancels on a malformed frozen field and never reads terminal diagnostics while pending', () => {
        const result = coreRender({ functions: { DE_CoreStartupPoll: () => 0, DE_CoreRenderReadV2: index => index === 27 ? NaN : coreRenderState({ 3: 0 })[index] } });
        assert.equal(result.calls.filter(call => call[0] === 'DE_CoreStartupCancel').length, 1);
        assert.equal(result.calls.filter(call => call[0] === 'DE_CoreStartupRead').length, 0);
    });
    test('Core render keeps polling through cleanup pending and only reads final Core after detachment', () => {
        let polls = 0;
        const result = coreRender({ keyFrames: [[17, 16, 120], [16], [], [], []], times: [0, 50, 100, 10150, 10200],
            functions: { DE_CoreStartupPoll: () => ++polls >= 3 ? 1 : 0,
                DE_CoreRenderReadV2: index => coreRenderState({ 2: polls, ...(polls === 1 ?
                    { 1: 1, 3: 0, 4: 0, 5: 1, 6: 0, 7: 0, 13: 1 } : polls === 2 ? { 1: 3, 6: 0, 10: 1, 32: 1 } : {}) })[index] } });
        assert.equal(result.calls.filter(call => call[0] === 'DE_CoreStartupPoll').length, 3);
        assert.ok(result.callFrames.filter(call => call.name === 'DE_CoreStartupRead').every(call => call.frame === 4));
        assert.equal(result.logs.find(event => event.event === 'core_render_reference_finished').passed, true);
    });
    test('Core render missing DLL export fails before starting an unobservable job', () => {
        const result = coreRender({ functions: { DE_CoreRenderReadV2: undefined } });
        assert.equal(result.calls.filter(call => call[0] === 'DE_CoreRenderBeginV2').length, 0);
        assert.ok(result.logs.some(event => event.event === 'adapter_failed'));
    });
    test('Core render impossible publication metrics cancel the worker without certifying observations', () => {
        const result = coreRender({ renderState: { 35: 1 } });
        assert.equal(result.calls.filter(call => call[0] === 'DE_CoreStartupCancel').length, 1);
        assert.equal(result.calls.filter(call => call[0] === 'DE_CoreStartupRead').length, 0);
        assert.ok(!result.logs.some(event => event.event === 'core_render_reference_finished' && event.passed));
    });
    test('Core render is exclusive with input writes and ordinary Online', () => {
        for (const override of [{ mtaCoreMovementReference: true }, { mtaCoreInputReference: true }, { mtaCoreStartupReference: true }, { mode: 'online' }]) {
            const result = coreRender(override);
            assert.equal(result.calls.length, 0);
            assert.ok(result.logs.some(event => event.event === 'adapter_failed'));
        }
    });
    const coreInputState = (overrides = {}) => {
        const state = Array(64).fill(0);
        Object.assign(state, { 0: 1, 1: 2, 2: 1201, 3: 60000, 4: 4, 5: 6, 7: 6, 11: 1200, 14: 1, 15: 1 }, overrides);
        return state;
    };
    const coreInput = overrides => runEntry({ mtaCoreInputReference: true,
        keyFrames: [[17, 16, 120], [16], []], ...overrides });
    const coreMovementState = (overrides = {}) => {
        const state = coreInputState({ 2: 601, 3: 30000, 4: 0, 11: 600 }).concat(Array(32).fill(0));
        for (const [index, value] of [[2, 5], [3, 5], [4, 15], [5, 2], [6, 5], [11, 1]]) state[64 + index * 2] = value;
        Object.assign(state, { 88: 15, 89: 15, 90: 1, 91: 1, 92: 1, 94: 1 }, overrides);
        return state;
    };
    const coreMovement = overrides => runEntry({ mtaCoreMovementReference: true,
        keyFrames: [[17, 16, 120], [16], []], ...overrides });
    test('Core configured movement selects96 snapshot writes and expected retained Game DE pin', () => {
        const result = coreMovement();
        assert.equal(result.callFrames.find(call => call.name === 'DynamicLibrary.Load').frame, 2);
        assert.equal(result.calls.filter(call => call[0] === 'DE_CoreMovementBegin').length, 1);
        assert.deepEqual(result.calls.filter(call => call[0] === 'DE_CoreMovementRead').map(call => call[1]), Array.from({ length: 96 }, (_, i) => i));
        const finish = result.logs.find(e => e.event === 'core_movement_reference_finished');
        assert.equal(finish.passed, true); assert.equal(finish.cleanup_passed, true); assert.equal(finish.state[15], 1);
        assert.equal(finish.input_writes, true); assert.equal(finish.full_core_ready, false);
        assert.ok(!result.calls.some(call => ['DE_CoreInputBegin', 'DE_CoreStartupBegin', 'DE_MtaMovementBegin', 'DE_ConnectLocal'].includes(call[0])));
    });
    test('Core configured movement retains pending cleanup beyond deadline until native safe completion', () => {
        let polls = 0;
        const result = coreMovement({ keyFrames: [[17, 16, 120], [], [], [], []], times: [0, 50, 5000, 9000, 10000],
            functions: { DE_CoreStartupPoll: () => ++polls === 4 ? 1 : 0,
                DE_CoreMovementRead: field => coreMovementState({ 1: polls === 4 ? 2 : 3, 2: polls, 92: polls, 93: 1, 95: polls === 4 ? 0 : 1 })[field] } });
        assert.equal(polls, 4);
        assert.deepEqual(result.logs.filter(e => e.event === 'core_movement_reference_sample').map(e => e.state[1]), [3, 3, 3, 2]);
        assert.equal(result.logs.find(e => e.event === 'core_movement_reference_finished').passed, false);
        assert.ok(result.waits.slice(1).every(call => call.milliseconds === 100));
    });
    for (const [field, value] of [[1, 3], [15, 0], [64, 1], [66, 1], [70, 0], [72, 0], [76, 4], [78, 1], [80, 1],
        [82, 1], [84, 1], [91, 0], [92, 0], [93, 1], [94, 0], [95, 1], [16, 128], [4, 3], [88, 7], [89, 7], [90, 0], [86, 0xffffffff], [87, 1]])
        test('Core configured movement cannot pass misleading terminal field ' + field, () => {
            const result = coreMovement({ functions: { DE_CoreMovementRead: index => coreMovementState({ [field]: value })[index] } });
            assert.equal(result.logs.find(e => e.event === 'core_movement_reference_finished').passed, false);
        });
    for (const options of [{ mtaCoreInputReference: true }, { mtaCoreStartupReference: true }, { mtaNativeMovementTrial: true },
        { mtaNativePadReference: true }, { inputReference: true }, { reference: true }, { gaitRate: true }, { gaitRootLock: true },
        { mode: 'online' }, { mode: 'diagnostic' }]) test('Core configured movement rejects conflicting selection ' + JSON.stringify(options), () => {
            const result = coreMovement({ ...options, forbidRuntimeAccess: true });
            assert.equal(result.calls.length, 0); assert.ok(result.logs.some(e => e.event === 'adapter_failed'));
        });
    test('Core configured input uses the protected64 snapshot and final19 without engine input writes', () => {
        const result = coreInput();
        assert.equal(result.callFrames.find(call => call.name === 'DynamicLibrary.Load').frame, 2);
        assert.equal(result.calls.filter(call => call[0] === 'DE_CoreInputBegin').length, 1);
        assert.deepEqual(result.calls.filter(call => call[0] === 'DE_CoreInputRead').map(call => call[1]), Array.from({ length: 64 }, (_, i) => i));
        const start = result.logs.find(e => e.event === 'core_input_reference_started');
        assert.equal(start.context_source, 'assumed_on_foot');
        assert.ok(Object.values(start.context).every(value => value === false));
        const finish = result.logs.find(e => e.event === 'core_input_reference_finished');
        assert.equal(finish.passed, true); assert.equal(finish.samples, 1); assert.equal(finish.input_writes, false);
        assert.ok(!result.calls.some(call => ['DE_ConnectLocal', 'GET_PLAYER_CHAR', 'DE_MtaMovementBegin', 'DE_CoreStartupBegin'].includes(call[0])));
    });
    test('Core configured input yields while empty then preserves skipped sequences and terminal publication', () => {
        let polls = 0;
        const frames = [null, coreInputState({ 1: 1, 2: 1, 3: 0, 4: 7, 11: 1, 14: 0, 15: 0 }),
            coreInputState({ 1: 1, 2: 5, 3: 200, 4: 7, 11: 5, 14: 0, 15: 0 }), coreInputState()];
        const result = coreInput({ keyFrames: [[17, 16, 120], [], [], [], []], functions: {
            DE_CoreStartupPoll: () => ++polls === 4 ? 1 : 0,
            DE_CoreInputCapture: () => frames[polls - 1] ? 1 : 0,
            DE_CoreInputRead: index => frames[polls - 1][index]
        } });
        assert.deepEqual(result.logs.filter(e => e.event === 'core_input_reference_sample').map(e => e.state[2]), [1, 5, 1201]);
        assert.equal(result.logs.find(e => e.event === 'core_input_reference_finished').passed, true);
        assert.ok(result.waits.slice(1).every(call => call.milliseconds === 100));
    });
    test('Core configured input emits an unchanged sequence once', () => {
        let polls = 0;
        const result = coreInput({ keyFrames: [[17, 16, 120], [], [], []], functions: {
            DE_CoreStartupPoll: () => ++polls === 3 ? 1 : 0
        } });
        assert.equal(result.logs.filter(e => e.event === 'core_input_reference_sample').length, 1);
        assert.equal(result.logs.find(e => e.event === 'core_input_reference_finished').samples, 1);
    });
    for (const kind of ['same_sequence_mutation', 'sequence_reversed', 'elapsed_reversed', 'bad_unsigned', 'bad_version', 'bad_phase'])
        test('Core configured input rejects ' + kind + ' and requests worker cleanup', () => {
            let polls = 0;
            const first = coreInputState({ 1: 1, 2: 2, 3: 100, 14: 0, 15: 0 });
            const second = first.slice(); second[2] = 3;
            if (kind === 'same_sequence_mutation') { second[2] = 2; second[9] = 1; }
            if (kind === 'sequence_reversed') second[2] = 1;
            if (kind === 'elapsed_reversed') second[3] = 99;
            if (kind === 'bad_unsigned') second[16] = -1;
            if (kind === 'bad_version') second[0] = 2;
            if (kind === 'bad_phase') second[1] = 3;
            const result = coreInput({ keyFrames: [[17, 16, 120], [], []], functions: {
                DE_CoreStartupPoll: () => { ++polls; return 0; }, DE_CoreInputRead: index => (polls === 1 ? first : second)[index]
            } });
            assert.ok(result.logs.some(e => e.event === 'adapter_failed'));
            assert.equal(result.calls.filter(call => call[0] === 'DE_CoreStartupCancel').length, 1);
            assert.ok(!result.calls.some(call => call[0] === 'DE_CoreStartupRead'));
        });
    for (const reason of ['Ctrl+Shift+End', 'loading']) test('Core configured input ' + reason + ' cancels once and waits for owner cleanup', () => {
        let polls = 0;
        const result = coreInput({ keyFrames: [[17, 16, 120], [], [], [17, 16, 35], []],
            readyFrames: reason === 'loading' ? [true, true, true, false, false] : undefined,
            functions: { DE_CoreStartupPoll: () => ++polls === 4 ? 1 : 0,
                DE_CoreInputRead: index => coreInputState({ 14: 2 })[index] } });
        assert.equal(result.calls.filter(call => call[0] === 'DE_CoreStartupCancel').length, 1);
        const finish = result.logs.find(e => e.event === 'core_input_reference_finished');
        assert.equal(finish.stop_requested, true); assert.equal(finish.passed, false); assert.equal(finish.cleanup_passed, true);
    });
    for (const options of [{ inputReference: true }, { reference: true }, { mtaNativePadReference: true },
        { mtaNativeMovementTrial: true }, { mtaCoreStartupReference: true }, { gaitRate: true }, { gaitRootLock: true },
        { mode: 'online' }, { mode: 'diagnostic' }]) test('Core configured input rejects combined mode ' + JSON.stringify(options), () => {
            const result = coreInput({ ...options, forbidRuntimeAccess: true });
            assert.equal(result.calls.length, 0); assert.ok(result.logs.some(e => e.event === 'adapter_failed'));
        });
    const coreStartup = overrides => runEntry({ mtaCoreStartupReference: true,
        keyFrames: [[17, 16, 120], [16], []], ...overrides });
    test('real Core startup entry waits for all F9 modifiers to release and records the gated result once', () => {
        const result = coreStartup();
        assert.equal(result.callFrames.find(call => call.name === 'DynamicLibrary.Load').frame, 2);
        assert.equal(result.calls.filter(call => call[0] === 'DE_CoreStartupBegin').length, 1);
        assert.equal(result.calls.filter(call => call[0] === 'DE_CoreStartupPoll').length, 1);
        assert.equal(result.calls.filter(call => call[0] === 'DE_CoreStartupCancel').length, 0);
        assert.deepEqual(result.calls.filter(call => call[0] === 'DE_CoreStartupRead').map(call => call[1]), Array.from({ length: 19 }, (_, i) => i));
        const finish = result.logs.find(event => event.event === 'core_startup_reference_finished');
        assert.equal(finish.passed, true); assert.equal(finish.cleanup_passed, true);
        assert.equal(finish.stop_requested, false); assert.equal(finish.run_result, 1);
        assert.equal(finish.full_core_ready, false); assert.equal(finish.rendering_started, false);
        assert.ok(!result.calls.some(call => ['DE_ConnectLocal', 'GET_PLAYER_CHAR', 'GET_CHAR_COORDINATES', 'GET_CHAR_VELOCITY',
            'CREATE_CHAR', 'DE_MtaMovementBegin', 'DE_NativePadBind'].includes(call[0])));
        const start = result.logs.find(event => event.event === 'adapter_start');
        assert.equal(start.core_startup_reference, true); assert.equal(start.input_writes, false);
        assert.equal(start.core_api, 2);
        assert.equal(start.network_active, false); assert.equal(start.remote_peds_created, false);
    });
    test('Core startup waits indefinitely before activation even with automatic preferences', () => {
        const result = coreStartup({ activation: 'automatic', keyFrames: [[], [], [17, 16, 120], []],
            times: [0, 7200000, 14400000, 14400050] });
        assert.equal(result.callFrames.find(call => call.name === 'DynamicLibrary.Load').frame, 3);
        assert.equal(result.logs.find(e => e.event === 'core_startup_reference_started').at_ms, 14400050);
    });
    test('Core startup End cancellation takes priority before binding', () => {
        const result = coreStartup({ keyFrames: [[17, 16, 120, 35]] });
        assert.equal(result.runtimeAccess.length, 0);
        assert.ok(result.logs.some(e => e.event === 'adapter_activation_cancelled'));
    });
    test('Core startup holding F9 to the release deadline never loads a DLL', () => {
        const result = coreStartup({ keyFrames: [[17, 16, 120], [120]], times: [0, 10000] });
        assert.equal(result.runtimeAccess.length, 0);
        assert.ok(result.logs.some(e => e.event === 'adapter_activation_release_timeout'));
    });
    for (const name of ['DE_CoreStartupBegin', 'DE_CoreStartupPoll', 'DE_CoreStartupCancel', 'DE_CoreStartupRead'])
        test('Core startup rejects missing worker export ' + name + ' before calling Core', () => {
            const result = coreStartup({ functions: { [name]: null } });
            assert.ok(result.logs.some(e => e.event === 'adapter_failed' && e.error.includes(name)));
            assert.ok(!result.calls.some(call => call[0] === 'DE_CoreStartupBegin'));
        });
    test('Core startup rejected Begin never polls or reads an unowned worker', () => {
        const result = coreStartup({ functions: { DE_CoreStartupBegin: () => 0 } });
        assert.ok(result.logs.some(e => e.event === 'adapter_failed'));
        assert.ok(!result.calls.some(call => ['DE_CoreStartupPoll', 'DE_CoreStartupRead', 'DE_CoreStartupCancel'].includes(call[0])));
        assert.ok(!result.logs.some(e => e.event === 'core_startup_reference_finished'));
    });
    test('Core startup yields until terminal without abandoning a slow loader or reading pending state', () => {
        let polls = 0;
        const result = coreStartup({ keyFrames: [[17, 16, 120], [], [], [], [], []],
            times: [0, 50, 3000, 30000, 300000, 3000000], functions: {
                DE_CoreStartupPoll: () => ++polls === 5 ? 1 : 0,
                DE_CoreStartupRead: index => { assert.equal(polls, 5); return [1, 5, 6, 97, 0, 0, 0, 1, 3, 4, 5, 0, 0, 1, 0, 0, 0, 1, 0][index]; }
            } });
        assert.equal(polls, 5);
        assert.equal(result.logs.find(e => e.event === 'core_startup_reference_finished').elapsed_ms, 2999950);
        assert.ok(result.waits.every(call => call.milliseconds === 50));
        assert.equal(result.calls.filter(call => call[0] === 'DE_CoreStartupCancel').length, 0);
    });
    for (const reason of ['Ctrl+Shift+End', 'loading']) test('Core startup ' + reason + ' requests stop once and polls owner cleanup to terminal', () => {
        let polls = 0, cancels = 0;
        const values = [1, 5, 6, 97, 0, 0, 0, 1, 3, 4, 5, 0, 0, 1, 0, 0, 12, 0, 0];
        const result = coreStartup({ keyFrames: reason === 'loading' ? [[17, 16, 120], [], [], [], []] :
                [[17, 16, 120], [], [], [17, 16, 35], [17, 16, 35]],
            readyFrames: reason === 'loading' ? [true, true, true, false, false] : undefined,
            functions: { DE_CoreStartupPoll: () => ++polls === 4 ? 1 : 0,
                DE_CoreStartupCancel: () => { ++cancels; return 1; },
                DE_CoreStartupRead: index => { assert.equal(polls, 4); return values[index]; } } });
        assert.equal(cancels, 1); assert.equal(polls, 4);
        const stop = result.logs.filter(e => e.event === 'core_startup_reference_stop_requested');
        assert.equal(stop.length, 1); assert.equal(stop[0].reason, reason);
        const finish = result.logs.find(e => e.event === 'core_startup_reference_finished');
        assert.equal(finish.stop_requested, true); assert.equal(finish.passed, false);
        assert.equal(finish.cleanup_passed, true); assert.equal(finish.run_result, 0);
        assert.ok(result.logs.some(e => e.event === 'adapter_failed'));
    });
    test('Core startup cancellation cannot certify a worker that finishes success concurrently', () => {
        let polls = 0;
        const result = coreStartup({ keyFrames: [[17, 16, 120], [], [17, 16, 35]],
            functions: { DE_CoreStartupPoll: () => ++polls === 2 ? 1 : 0 } });
        const finish = result.logs.find(e => e.event === 'core_startup_reference_finished');
        assert.equal(finish.run_result, 1); assert.equal(finish.stop_requested, true); assert.equal(finish.passed, false);
    });
    for (const value of [-1, 2, 0.5, NaN]) test('Core startup invalid Poll requests cooperative cleanup without cached reads: ' + value, () => {
        const result = coreStartup({ functions: { DE_CoreStartupPoll: () => value } });
        assert.equal(result.calls.filter(call => call[0] === 'DE_CoreStartupCancel').length, 1);
        assert.ok(!result.calls.some(call => call[0] === 'DE_CoreStartupRead'));
        assert.ok(result.logs.some(e => e.event === 'adapter_failed'));
    });
    test('Core startup a reversed pending clock requests cooperative cleanup without cached reads', () => {
        const result = coreStartup({ keyFrames: [[17, 16, 120], [], []], times: [0, 100, 99],
            functions: { DE_CoreStartupPoll: () => 0 } });
        assert.equal(result.calls.filter(call => call[0] === 'DE_CoreStartupCancel').length, 1);
        assert.ok(!result.calls.some(call => call[0] === 'DE_CoreStartupRead'));
        assert.ok(result.logs.some(e => e.event === 'adapter_failed' && e.error.includes('clock')));
    });
    test('Core startup preserves a rejected native result and cleanup diagnostics', () => {
        const values = [0, 0, 0, 0, 0, 0, 0, 0, 0xffffffff, 0xffffffff, 0xffffffff, 2, 2, 1, 0, 0, 4, 0, 126];
        const result = coreStartup({ functions: { DE_CoreStartupRead: index => values[index] } });
        const finish = result.logs.find(e => e.event === 'core_startup_reference_finished');
        assert.equal(finish.run_result, 0); assert.equal(finish.passed, false); assert.equal(finish.state[18], 126);
        assert.equal(finish.cleanup_passed, false);
        assert.ok(result.logs.some(e => e.event === 'adapter_failed'));
    });
    for (const [field, value] of [[1, 0], [2, 3], [3, 33], [5, 1], [6, 1], [7, 0], [8, 4], [9, 3], [10, 0],
        [11, 4], [12, 1], [13, 0], [14, 1], [15, 1], [16, 8], [17, 0], [18, 126]]) {
        test('Core startup rejects misleading state field ' + field, () => {
            const values = [1, 5, 6, 97, 0, 0, 0, 1, 3, 4, 5, 0, 0, 1, 0, 0, 0, 1, 0]; values[field] = value;
            const result = coreStartup({ functions: { DE_CoreStartupRead: index => values[index] } });
            assert.equal(result.logs.find(e => e.event === 'core_startup_reference_finished').passed, false);
            assert.ok(result.logs.some(e => e.event === 'adapter_failed'));
        });
    }
    for (const value of [-1, 0x100000000, NaN, 1.5]) test('Core startup rejects unsigned readback ' + value, () => {
        const result = coreStartup({ functions: { DE_CoreStartupRead: () => value } });
        assert.ok(result.logs.some(e => e.event === 'adapter_failed' && e.error.includes('invalid unsigned')));
        assert.ok(!result.logs.some(e => e.event === 'core_startup_reference_finished'));
    });
    for (const options of [{ inputReference: true }, { reference: true }, { mtaNativePadReference: true },
        { mtaNativeMovementTrial: true }, { mode: 'online' }, { mode: 'diagnostic' }]) {
        test('Core startup rejects combined mode ' + JSON.stringify(options), () => {
            const result = coreStartup({ ...options, forbidRuntimeAccess: true });
            assert.equal(result.calls.length, 0);
            assert.ok(result.logs.some(e => e.event === 'adapter_failed'));
        });
    }
    test('MTA movement entry waits for F9 release, submits through the real module ABI and cleans up on End', () => {
        const result = runEntry({ mtaNativeMovementTrial: true,
            keyFrames: [[17, 16, 120], [16], [], [17, 16, 35]], times: [0, 50, 100, 150],
            natives: { GET_CHAR_VELOCITY: () => ({ x: 0, y: 0, z: 0 }) } });
        assert.equal(result.callFrames.find(call => call.name === 'DynamicLibrary.Load').frame, 2);
        assert.equal(result.calls.filter(call => call[0] === 'DE_MtaMovementBegin').length, 1);
        assert.equal(result.calls.filter(call => call[0] === 'DE_MtaMovementEnd').length, 1);
        assert.ok(result.logs.some(e => e.event === 'adapter_start' && e.native_movement_trial && e.adapter_abi === 6));
        assert.ok(result.logs.some(e => e.event === 'native_movement_trial_finished' && e.cleanup_passed));
        assert.ok(!result.calls.some(call => ['DE_ConnectLocal', 'GET_PAD_STATE', 'CREATE_CHAR', 'DE_NativePadBind'].includes(call[0])));
    });
    test('MTA movement entry rejects an old bridge before activating the module', () => {
        const result = runEntry({ mtaNativeMovementTrial: true, functions: { DE_MtaMovementSubmit: null },
            keyFrames: [[17, 16, 120], []], times: [0, 50] });
        assert.ok(result.logs.some(e => e.event === 'adapter_failed' && e.error.includes('DE_MtaMovementSubmit')));
        assert.ok(!result.calls.some(call => call[0] === 'DE_MtaMovementBegin'));
    });
    test('MTA movement arming can wait hours without starting its movement timer', () => {
        const result = runEntry({ mtaNativeMovementTrial: true,
            keyFrames: [[], [], [17, 16, 120], [], [17, 16, 35]],
            times: [0, 7200000, 14400000, 14400050, 14400100],
            natives: { GET_CHAR_VELOCITY: () => ({ x: 0, y: 0, z: 0 }) } });
        assert.equal(result.callFrames.find(call => call.name === 'DynamicLibrary.Load').frame, 3);
        assert.equal(result.logs.find(e => e.event === 'native_movement_trial_started').at_ms, 14400050);
        assert.equal(result.logs.find(e => e.event === 'native_movement_trial_finished').elapsed_ms, 50);
    });
    test('MTA movement entry keeps polling until cross-thread cleanup completes after an action failure', () => {
        let ends = 0;
        const result = runEntry({ mtaNativeMovementTrial: true,
            keyFrames: [[17, 16, 120], [], [], []], times: [0, 50, 100, 150],
            functions: { DE_MtaMovementSubmit: () => 0, DE_MtaMovementEnd: () => ++ends >= 3 ? 1 : 0,
                DE_MtaMovementRead: index => [ends >= 3 ? 0 : 1, 0, 1, 1, 1, 0, 1, 0, 0, 0, 0, 0][index] } });
        assert.equal(ends, 3);
        assert.equal(result.callFrames.find(call => call.name === 'DE_MtaMovementSubmit').frame, 1);
        assert.equal(result.calls.filter(call => call[0] === 'DE_MtaMovementSubmit').length, 1);
        const finish = result.logs.find(e => e.event === 'native_movement_trial_finished');
        assert.equal(finish.cleanup_passed, true); assert.equal(finish.elapsed_ms, 100);
        assert.match(result.logs.at(-1).error, /submission/);
    });
    test('MTA movement trial rejects incompatible selections before runtime access', () => {
        for (const overrides of [{ mode: 'online' }, { mode: 'diagnostic' }, { inputReference: true },
            { reference: true }, { mtaNativePadReference: true }, { mtaNativeMovementTrial: 'yes' }]) {
            const result = runEntry({ mtaNativeMovementTrial: true, ...overrides });
            assert.deepEqual(result.runtimeAccess, []);
            assert.ok(result.logs.some(e => e.event === 'adapter_failed'));
        }
    });
    test('MTA native pad entry captures production-reader snapshots without transport or the old pad opcode', () => {
        const result = runEntry({ mtaNativePadReference: true,
            keyFrames: [[17, 16, 120], [], [17, 16, 35]], times: [0, 150, 200],
            natives: { GET_CHAR_VELOCITY: () => ({ x: 1, y: 0, z: 0 }) } });
        assert.ok(result.logs.some(event => event.event === 'native_pad_reference_started'));
        const sample = result.logs.find(event => event.event === 'native_pad_sample');
        assert.equal(sample.pads.length, 2); assert.equal(sample.pads[1].previous.length, 24);
        assert.equal(sample.device_states.length, 127); assert.equal(sample.capture_version, 2);
        assert.equal(result.calls.filter(call => call[0] === 'DE_NativeDeviceRead').length,
            127 * result.logs.filter(event => event.event === 'native_pad_sample').length);
        assert.equal(sample.previous_source, 'native_OldState');
        assert.equal(result.logs.filter(event => event.event === 'native_pad_reference_reset').length, 2);
        assert.ok(!result.calls.some(call => ['DE_ConnectLocal', 'GET_PAD_STATE', 'CREATE_CHAR'].includes(call[0])));
    });
    test('MTA source-state entry rejects an older bridge before native input binding', () => {
        const result = runEntry({ mtaNativePadReference: true, functions: { DE_NativeDeviceRead: null },
            keyFrames: [[17, 16, 120], []], times: [0, 50] });
        assert.ok(result.logs.some(event => event.event === 'adapter_failed' && event.error.includes('DE_NativeDeviceRead')));
        assert.ok(!result.calls.some(call => ['DE_NativePadBind', 'DE_NativePadCapture'].includes(call[0])));
    });
    test('native source reference waits until all activation keys are released before DLL access or recording', () => {
        const result = runEntry({ mtaNativePadReference: true,
            keyFrames: [[17, 16, 120], [17, 16], [16], [120], [], [17, 16, 35]], times: [0, 50, 100, 150, 200, 250],
            natives: { GET_CHAR_VELOCITY: () => ({ x: 0, y: 0, z: 0 }) } });
        assert.equal(result.calls.filter(call => call[0] === 'DynamicLibrary.Load').length, 1);
        assert.equal(result.callFrames.find(call => call.name === 'DynamicLibrary.Load').frame, 4);
        assert.ok(!result.runtimeAccess.some(access => access.frame < 4));
        const released = result.logs.find(event => event.event === 'adapter_activation_released');
        assert.equal(released.mode, 'smoke'); assert.equal(released.activate_key, 'Ctrl+Shift+F9'); assert.equal(released.release_wait_ms, 200);
        assert.ok(result.logs.indexOf(released) < result.logs.findIndex(event => event.phase === 'bridge_load'));
        const first = result.logs.find(event => event.event === 'native_pad_sample');
        assert.equal(first.at_ms, 200); assert.equal(first.elapsed_ms, 0);
        assert.ok(result.waits.every(wait => wait.milliseconds === 50));
    });
    test('native source release wait gives End cancellation priority without DLL access', () => {
        const result = runEntry({ mtaNativePadReference: true, keyFrames: [[17, 16, 120], [17, 16, 120, 35]], times: [0, 10000] });
        assert.equal(result.logs.at(-1).event, 'adapter_activation_cancelled');
        assert.ok(!result.calls.some(call => call[0] === 'DynamicLibrary.Load'));
        assert.ok(!result.logs.some(event => event.event === 'adapter_failed' || event.event === 'native_pad_reference_started'));
    });
    test('native source release wait times out before DLL access even if a single key remains', () => {
        const result = runEntry({ mtaNativePadReference: true,
            keyFrames: [[17, 16, 120], [120], [120]], times: [0, 9999, 10000] });
        assert.ok(result.logs.some(event => event.event === 'adapter_activation_release_timeout'));
        assert.match(result.logs.at(-1).error, /release timed out/);
        assert.ok(!result.calls.some(call => call[0] === 'DynamicLibrary.Load'));
        assert.deepEqual(result.runtimeAccess, []);
    });
    test('native source readiness loss while waiting for release fails before binding', () => {
        const result = runEntry({ mtaNativePadReference: true,
            keyFrames: [[17, 16, 120], []], readyFrames: [true, false], times: [0, 50] });
        assert.match(result.logs.at(-1).error, /readiness lost/);
        assert.ok(!result.calls.some(call => call[0] === 'DynamicLibrary.Load'));
    });
    test('MTA native pad entry rejects incompatible modes before DLL access', () => {
        for (const overrides of [{ mode: 'online' }, { inputReference: true }, { reference: true }]) {
            const result = runEntry({ mtaNativePadReference: true, ...overrides });
            assert.ok(!result.calls.some(call => call[0] === 'DynamicLibrary.Load'));
            assert.ok(result.logs.some(event => event.event === 'adapter_failed'));
        }
    });
    function runRemoteEntry(players, overrides = {}) {
        const view = new DataView(new ArrayBuffer(4));
        const bits = value => { view.setFloat32(0, value, true); return view.getUint32(0, true); };
        const components = players.map(player => [player.id, ...['x', 'y', 'z', 'heading'].map(name => bits(player[name]))]);
        const positions = new Map([[10, { x: 1.25, y: -2.5, z: 3.75, heading: 90 }]]);
        let nextPed = 100;
        const requirePed = handle => {
            assert.ok(positions.has(handle), 'Native used an unowned mock ped');
            return positions.get(handle);
        };
        // These explicit fake native handlers exercise the entry's real game
        // interface. No CLEO command, DLL or installed game is executed here.
        const natives = {
            GET_CHAR_COORDINATES: handle => { const { x, y, z } = requirePed(handle); return { x, y, z }; },
            GET_CHAR_HEADING: handle => requirePed(handle).heading,
            REQUEST_MODEL: () => {}, HAS_MODEL_LOADED: () => true,
            CREATE_CHAR: (type, model, x, y, z) => {
                const handle = nextPed++;
                positions.set(handle, { x, y, z, heading: 0 });
                return handle;
            },
            CLEAR_CHAR_TASKS: requirePed,
            TASK_TOGGLE_PED_THREAT_SCANNER: requirePed,
            FREEZE_CHAR_POSITION: requirePed,
            SET_CHAR_COLLISION: requirePed,
            SET_CHAR_PROOFS: requirePed,
            SET_CHAR_COORDINATES_NO_OFFSET: (handle, x, y, z) => Object.assign(requirePed(handle), { x, y, z }),
            SET_CHAR_HEADING: (handle, heading) => { requirePed(handle).heading = heading; },
            TASK_PLAY_ANIM: requirePed,
            SET_CHAR_ANIM_SPEED: (handle, clip, rate) => {
                requirePed(handle);
                assert.equal(clip, 'Walk_civi');
                assert.ok(typeof rate === 'number' && Number.isFinite(rate) && rate >= 0.25 && rate <= 2);
            },
            IS_CHAR_PLAYING_ANIM: handle => { requirePed(handle); return true; },
            GET_CHAR_ANIM_CURRENT_TIME: handle => { requirePed(handle); return 0.42; },
            DELETE_CHAR: handle => { requirePed(handle); positions.delete(handle); },
            MARK_MODEL_AS_NO_LONGER_NEEDED: () => {},
            ...(overrides.natives || {})
        };
        return runEntry({ ...overrides, mode: 'online', activation: 'automatic', natives,
            functions: { DE_PlayerCount: () => components.length,
                DE_ReadPlayerComponent: (index, component) => components[index][component], ...(overrides.functions || {}) } });
    }
    function runMovingRateEntry(enabled, options = {}) {
        let polls = 0;
        const view = new DataView(new ArrayBuffer(4));
        const bits = value => { view.setFloat32(0, value, true); return view.getUint32(0, true); };
        return runRemoteEntry([remote()], { gaitRate: enabled, ...options,
            keyFrames: [...Array.from({ length: 21 }, () => []), [17, 16, 35]],
            functions: { DE_SubmitTransformBits: () => { ++polls; return 1; },
                DE_ReadPlayerComponent: (index, component) => [2, bits(30 + (polls - 1) * 1.5 * 0.05),
                    bits(20), bits(3), bits(180)][component] } });
    }
    test('online opt-in entry sends the exact owned Walk_civi rate setter without extra bridge traffic or task replay', () => {
        const result = runMovingRateEntry(true);
        assert.ok(!result.logs.some(log => log.event === 'adapter_failed'));
        const requested = result.logs.filter(log => log.event === 'remote_animation_rate_applied');
        assert.equal(requested.length, 1);
        assert.ok(Math.abs(requested[0].requested_rate - 1.5 / nativeGaitReferenceSpeed) < 1e-4);
        assert.deepEqual(result.calls.filter(call => call[0] === 'SET_CHAR_ANIM_SPEED'),
            [['SET_CHAR_ANIM_SPEED', 100, 'Walk_civi', requested[0].requested_rate]]);
        assert.equal(result.calls.filter(call => call[0] === 'TASK_PLAY_ANIM').length, 2);
        assert.equal(result.calls.filter(call => call[0] === 'DE_SubmitTransformBits').length, 21);
        assert.ok(result.calls.findIndex(call => call[0] === 'IS_CHAR_PLAYING_ANIM' && call[2] === 'Walk_civi') <
            result.calls.findIndex(call => call[0] === 'SET_CHAR_ANIM_SPEED'));
        assert.equal(result.logs.find(log => log.event === 'adapter_start').matched_gait_rate, true);
        const control = runMovingRateEntry(false);
        assert.ok(!control.calls.some(call => call[0] === 'SET_CHAR_ANIM_SPEED'));
        assert.equal(control.calls.filter(call => call[0] === 'DE_SubmitTransformBits').length, 21);
        assert.equal(control.calls.filter(call => call[0] === 'TASK_PLAY_ANIM').length, 2);
        assert.ok(!control.logs.some(log => Object.hasOwn(log, 'requested_animation_rate')));
    });
    test('root-lock trial changes only horizontal walking flags on the owned native task', () => {
        const result = runMovingRateEntry(true, { gaitRootLock: true });
        assert.ok(!result.logs.some(log => log.event === 'adapter_failed'));
        assert.deepEqual(result.calls.filter(call => call[0] === 'TASK_PLAY_ANIM'),
            [['TASK_PLAY_ANIM', 100, 'idle_stance', 'PED', 4, true, false, false, false, -1],
             ['TASK_PLAY_ANIM', 100, 'Walk_civi', 'PED', 4, true, true, true, false, -1]]);
        assert.equal(result.calls.filter(call => call[0] === 'CLEAR_CHAR_TASKS').length, 1);
        assert.ok(result.calls.some(call => call[0] === 'SET_CHAR_ANIM_SPEED'));
        assert.ok(!result.calls.some(call => call[0] === 'SET_CHAR_ANIM_CURRENT_TIME'));
    });
    for (const options of [{ mode: 'smoke', gaitRootLock: true }, { mode: 'online', gaitRootLock: 'yes' }])
        test('invalid root-lock experiment fails before bridge or mutation: ' + JSON.stringify(options), () => {
            const result = runEntry(options);
            assert.match(result.logs.at(-1).error, /root lock/i);
            assert.ok(!result.calls.some(call => call[0] === 'DynamicLibrary.Load' || call[0] === 'CREATE_CHAR'));
        });
    for (const options of [{ mode: 'smoke', gaitRate: true }, { mode: 'online', gaitRate: 'yes' },
        { mode: 'online', gaitRate: 1 }, { mode: 'smoke', reference: true, gaitRate: true }])
        test('invalid gait-rate entry option fails before bridge or game mutation: ' + JSON.stringify(options), () => {
            const result = runEntry(options);
            assert.match(result.logs.at(-1).error, /gait rate requires opt-in Online/i);
            assert.ok(!result.calls.some(call => call[0] === 'DynamicLibrary.Load' || call[0] === 'CREATE_CHAR' || call[0] === 'SET_CHAR_ANIM_SPEED'));
            assert.ok(!result.logs.some(log => log.event === 'adapter_armed'));
        });
    test('entry applies received negative body coordinates with the no-offset setter and reads the owned ped', () => {
        const requested = { x: -2488.25, y: -1666.5, z: -3.75, heading: -45.5 };
        const result = runRemoteEntry([{ id: 2, ...requested }]);
        assert.ok(!result.logs.some(log => log.event === 'adapter_failed'));
        assert.deepEqual(result.calls.find(call => call[0] === 'CREATE_CHAR'),
            ['CREATE_CHAR', 4, 7, requested.x, requested.y, requested.z]);
        assert.deepEqual(result.calls.find(call => call[0] === 'SET_CHAR_COORDINATES_NO_OFFSET'),
            ['SET_CHAR_COORDINATES_NO_OFFSET', 100, requested.x, requested.y, requested.z]);
        assert.ok(!result.calls.some(call => call[0] === 'SET_CHAR_COORDINATES'));
        assert.deepEqual(result.calls.find(call => call[0] === 'SET_CHAR_HEADING'), ['SET_CHAR_HEADING', 100, requested.heading]);
        assert.deepEqual(result.calls.filter(call => call[0] === 'GET_CHAR_COORDINATES'),
            [['GET_CHAR_COORDINATES', 10], ['GET_CHAR_COORDINATES', 100]]);
        assert.deepEqual(result.logs.find(log => log.event === 'remote_transform_observed'), {
            event: 'remote_transform_observed', player_id: 2, ped: 100, requested, observed: requested, z_error: 0,
            network_target: requested
        });
        assert.deepEqual(result.calls.find(call => call[0] === 'FREEZE_CHAR_POSITION'), ['FREEZE_CHAR_POSITION', 100, true]);
        assert.deepEqual(result.calls.find(call => call[0] === 'SET_CHAR_COLLISION'), ['SET_CHAR_COLLISION', 100, false]);
        assert.deepEqual(result.calls.find(call => call[0] === 'DELETE_CHAR'), ['DELETE_CHAR', 100]);
        assert.ok(result.calls.some(call => call[0] === 'DE_Disconnect'));
    });
    test('entry uses the exact built-in PED idle task and playback signatures without streaming PED', () => {
        const result = runRemoteEntry([remote()]);
        assert.deepEqual(result.calls.find(call => call[0] === 'TASK_PLAY_ANIM'),
            ['TASK_PLAY_ANIM', 100, 'idle_stance', 'PED', 4, true, false, false, false, -1]);
        assert.deepEqual(result.calls.find(call => call[0] === 'IS_CHAR_PLAYING_ANIM'), ['IS_CHAR_PLAYING_ANIM', 100, 'idle_stance']);
        assert.deepEqual(result.calls.find(call => call[0] === 'GET_CHAR_ANIM_CURRENT_TIME'), ['GET_CHAR_ANIM_CURRENT_TIME', 100, 'idle_stance']);
        assert.ok(!result.calls.some(call => ['REQUEST_ANIMATION', 'REMOVE_ANIMATION'].includes(call[0])));
        assert.ok(!result.logs.some(event => event.event === 'adapter_failed'));
    });
    test('online entry enables the bounded gait probe using only existing read natives', () => {
        const times = [0, 16, 32, 48, 64, 80, 96, 112, 128];
        const result = runRemoteEntry([remote()], { times, keyFrames: [...times.slice(0, -1).map(() => []), [17, 16, 35]] });
        assert.deepEqual(result.logs.filter(event => event.event === 'remote_gait_sample').map(event => event.sample_at_ms), [0, 112]);
        assert.equal(result.calls.filter(call => call[0] === 'GET_CHAR_ANIM_CURRENT_TIME').length, 2);
        assert.equal(result.calls.filter(call => call[0] === 'TASK_PLAY_ANIM').length, 1);
        assert.ok(!result.calls.some(call => ['SET_CHAR_ANIM_SPEED', 'SET_CHAR_ANIM_CURRENT_TIME', 'GET_CHAR_ANIM_TOTAL_TIME'].includes(call[0])));
        assert.equal(result.logs.find(event => event.event === 'remote_gait_probe_finished').reason, 'stop');
        assert.ok(!result.logs.some(event => event.event === 'adapter_failed'));
    });
    test('online entry never publishes an invalid phase from a probe-only read', () => {
        let reads = 0;
        const result = runRemoteEntry([remote()], { times: [0, 100, 150], keyFrames: [[], [], [17, 16, 35]],
            natives: { GET_CHAR_ANIM_CURRENT_TIME: () => ++reads === 1 ? 0.42 : Infinity } });
        assert.equal(result.logs.filter(event => event.event === 'remote_gait_sample').length, 1);
        assert.equal(result.logs.find(event => event.event === 'remote_gait_probe_finished').reason, 'error');
        assert.match(result.logs.find(event => event.event === 'adapter_failed').error, /invalid animation evidence/);
        assert.deepEqual(result.calls.filter(call => call[0] === 'DELETE_CHAR'), [['DELETE_CHAR', 100]]);
    });
    test('entry never reads animation phase for an absent association during initial grace', () => {
        const result = runRemoteEntry([remote()], { natives: { IS_CHAR_PLAYING_ANIM: () => false,
            GET_CHAR_ANIM_CURRENT_TIME: () => { throw new Error('must not query absent animation'); } } });
        const observed = result.logs.find(event => event.event === 'remote_animation_observed');
        assert.equal(observed.playing, false); assert.equal(observed.time, null); assert.equal(observed.pending, true);
        assert.ok(!result.calls.some(call => call[0] === 'GET_CHAR_ANIM_CURRENT_TIME'));
        assert.ok(!result.logs.some(event => event.event === 'adapter_failed'));
    });
    test('entry selects the same exact native signature for a moving remote without clearing its tasks again', () => {
        const view = new DataView(new ArrayBuffer(4));
        const bits = value => { view.setFloat32(0, value, true); return view.getUint32(0, true); };
        let sample = -1;
        const result = runRemoteEntry([remote()], { keyFrames: [[], [], [17, 16, 35]],
            functions: { DE_ReadPlayerComponent: (index, component) => {
                if (component === 0) { ++sample; return 2; }
                return bits([30 + sample * 0.1, 20, 3, 180][component - 1]);
            } } });
        assert.deepEqual(result.calls.filter(call => call[0] === 'TASK_PLAY_ANIM'),
            [['TASK_PLAY_ANIM', 100, 'idle_stance', 'PED', 4, true, false, false, false, -1],
             ['TASK_PLAY_ANIM', 100, 'Walk_civi', 'PED', 4, true, false, false, false, -1]]);
        assert.equal(result.calls.filter(call => call[0] === 'CLEAR_CHAR_TASKS').length, 1);
        assert.ok(!result.logs.some(event => event.event === 'adapter_failed'));
    });
    test('entry cleans owned ped on invalid animation condition before querying phase', () => {
        const result = runRemoteEntry([remote()], { natives: { IS_CHAR_PLAYING_ANIM: () => 1 } });
        assert.ok(result.logs.some(event => event.event === 'adapter_failed' && /Invalid native animation condition/.test(event.error)));
        assert.ok(!result.calls.some(call => call[0] === 'GET_CHAR_ANIM_CURRENT_TIME'));
        assert.deepEqual(result.calls.find(call => call[0] === 'DELETE_CHAR'), ['DELETE_CHAR', 100]);
        assert.ok(result.calls.some(call => call[0] === 'DE_Disconnect'));
    });
    test('entry validates every decoded snapshot before spawning, moving or observing a remote', () => {
        const result = runRemoteEntry([remote(2), { ...remote(3), z: NaN }]);
        assert.match(result.logs.find(log => log.event === 'adapter_failed').error, /Invalid snapshot player state/);
        assert.ok(!result.calls.some(call => ['CREATE_CHAR', 'SET_CHAR_COORDINATES', 'SET_CHAR_COORDINATES_NO_OFFSET'].includes(call[0])));
        assert.deepEqual(result.calls.filter(call => call[0] === 'GET_CHAR_COORDINATES'), [['GET_CHAR_COORDINATES', 10]]);
        assert.ok(!result.logs.some(log => log.event === 'remote_transform_observed'));
        assert.ok(result.calls.some(call => call[0] === 'DE_Disconnect'));
    });
    test('entry reports invalid remote readback after deleting its owned ped and closing transport', () => {
        const result = runRemoteEntry([remote(2)], { natives: {
            GET_CHAR_COORDINATES: handle => handle === 10 ? { x: 1.25, y: -2.5, z: 3.75 } : { x: 30, y: 20, z: Infinity }
        } });
        const failure = result.logs.find(log => log.event === 'adapter_failed');
        assert.match(failure.error, /invalid observed transform/);
        assert.equal(failure.phase, 'controller_run');
        assert.ok(!result.logs.some(log => log.event === 'remote_transform_observed'));
        assert.deepEqual(result.calls.find(call => call[0] === 'DELETE_CHAR'), ['DELETE_CHAR', 100]);
        assert.ok(result.calls.some(call => call[0] === 'MARK_MODEL_AS_NO_LONGER_NEEDED'));
        assert.ok(result.calls.some(call => call[0] === 'DE_Disconnect'));
    });
    test('diagnostic logs host metadata and completes without bridge, game, wait or transport access', () => {
        const result = runEntry({ mode: 'diagnostic', forbidRuntimeAccess: true });
        assert.equal(result.calls.length, 0);
        assert.equal(result.logs.length, 2);
        assert.deepEqual(result.logs[0], { event: 'adapter_phase', phase: 'entry', mode: 'diagnostic', host: 'sa_unreal',
            cleo: '1.5.1', host_version: '1.0.113.49697469', definitions: '0.500' });
        assert.deepEqual(result.logs[1], { event: 'adapter_diagnostic_complete', phase: 'diagnostic_complete', mode: 'diagnostic',
            bridge_loaded: false, game_adapter_active: false, network_active: false });
    });
    test('diagnostic records unsupported host metadata without starting gameplay validation', () => {
        const result = runEntry({ mode: 'diagnostic', host: 'sa', version: '1.4.3', definitions: '0.492', forbidRuntimeAccess: true });
        assert.equal(result.calls.length, 0);
        assert.equal(result.logs[0].host, 'sa');
        assert.equal(result.logs[0].cleo, '1.4.3');
        assert.equal(result.logs[0].definitions, '0.492');
        assert.equal(result.logs[1].event, 'adapter_diagnostic_complete');
    });
    test('unknown mode fails before bridge or engine access', () => {
        const result = runEntry({ mode: 'unknown', forbidRuntimeAccess: true });
        assert.equal(result.calls.length, 0);
        assert.match(result.logs.at(-1).error, /Unknown adapter mode/);
        assert.equal(result.logs.at(-1).phase, 'entry');
    });
    test('entry smoke validates native gates but never connects or spawns', () => {
        const result = runEntry();
        assert.ok(result.calls.some(call => call[0] === 'DE_BindRuntime'));
        assert.ok(result.logs.some(log => log.event === 'local_transform'));
        assert.ok(!result.calls.some(call => call[0] === 'DE_ConnectLocal' || call[0] === 'CREATE_CHAR'));
        assert.ok([17, 16, 35].every(key => result.calls.some(call => call[0] === 'IS_KEY_PRESSED' && call[1] === key)));
        assert.equal(result.logs.find(log => log.event === 'adapter_start').stop_key, 'Ctrl+Shift+End');
    });
    test('active online entry yields at sixteen milliseconds while bridge submissions retain the fifty-millisecond schedule', () => {
        const times = [0, 16, 32, 48, 64, 80, 96, 112, 128];
        const result = runEntry({ mode: 'online', activation: 'automatic', times,
            keyFrames: [...times.slice(0, -1).map(() => []), [17, 16, 35]] });
        assert.ok(!result.logs.some(log => log.event === 'adapter_failed'));
        assert.equal(result.waits.length, 8);
        assert.ok(result.waits.every(wait => wait.milliseconds === 16));
        assert.deepEqual(result.callFrames.filter(call => call.name === 'DE_SubmitTransformBits').map(call => call.frame), [0, 4, 7]);
        assert.equal(result.logs.find(log => log.event === 'adapter_stopped').reason, 'Ctrl+Shift+End');
    });
    test('smoke entry retains its fifty-millisecond yield and never enables online rendering', () => {
        const result = runEntry({ mode: 'smoke', keyFrames: [[], [], [], [17, 16, 35]] });
        assert.equal(result.waits.length, 3);
        assert.ok(result.waits.every(wait => wait.milliseconds === 50));
        assert.ok(!result.logs.some(log => log.event === 'remote_motion_observed'));
    });
    test('entry tolerates numeric API version which omits trailing zeroes', () => {
        const result = runEntry({ definitions: 0.5 });
        assert.ok(result.logs.some(log => log.event === 'local_transform'));
        assert.ok(!result.logs.some(log => log.event === 'adapter_failed'));
    });
    for (const [name, options] of [
        ['wrong host', { host: 'sa' }], ['old runtime', { version: '1.4.3' }], ['malformed runtime', { version: 'garbage' }],
        ['old definitions', { definitions: '0.492' }], ['wrong bridge API', { functions: { DE_ApiVersion: () => 1 } }],
        ['unknown executable', { functions: { DE_CheckGameBuild: () => 0 } }], ['runtime binding failure', { functions: { DE_BindRuntime: () => 0 } }]
    ]) test('entry rejects ' + name + ' before game access or networking', () => {
        const result = runEntry(options);
        assert.ok(result.logs.some(log => log.event === 'adapter_failed'));
        assert.ok(!result.calls.some(call => call[0] === 'IS_PLAYER_PLAYING' || call[0] === 'DE_ConnectLocal'));
    });
    test('startup failure identifies the last boundary without entering later phases', () => {
        const result = runEntry({ functions: { DE_CheckGameBuild: () => 0 } });
        const failed = result.logs.find(log => log.event === 'adapter_failed');
        assert.equal(failed.phase, 'game_build_check');
        assert.equal(result.logs.at(-2).phase, 'game_build_check');
        assert.ok(!result.calls.some(call => call[0] === 'DE_BindRuntime'));
        assert.ok(!result.logs.some(log => log.phase === 'runtime_bind'));
    });
    test('startup phase logging is bounded and precedes controller gameplay', () => {
        const result = runEntry();
        const phases = result.logs.filter(log => log.event === 'adapter_phase');
        assert.equal(phases.length, 9);
        assert.equal(phases[0].phase, 'entry');
        assert.equal(phases.at(-1).phase, 'controller_run');
        assert.ok(result.logs.findIndex(log => log.phase === 'controller_run') < result.logs.findIndex(log => log.event === 'local_transform'));
    });
    test('entry sends IEEE float bits through integer-only bridge and disconnects on Ctrl+Shift+End', () => {
        const result = runEntry({ mode: 'online', activation: 'automatic' });
        const submitted = result.calls.find(call => call[0] === 'DE_SubmitTransformBits');
        assert.deepEqual(submitted.slice(1), [1.25, -2.5, 3.75, 90].map(value => result.fromFloat(value) | 0));
        assert.ok(submitted.slice(1).every(value => Number.isInteger(value) && value >= -0x80000000 && value <= 0x7fffffff));
        assert.ok(result.calls.some(call => call[0] === 'DE_Disconnect'));
        assert.equal(result.logs.find(log => log.event === 'adapter_stopped').reason, 'Ctrl+Shift+End');
    });
    test('entry decodes unsigned negative-coordinate bits into the own-player echo', () => {
        const bits = value => {
            const view = new DataView(new ArrayBuffer(4));
            view.setFloat32(0, value, true);
            return view.getUint32(0, true);
        };
        const expected = { x: 1.25, y: -2.5, z: 3.75, heading: 90 };
        const values = [1, ...Object.values(expected).map(bits)];
        const result = runEntry({ mode: 'online', activation: 'automatic', functions: { DE_PlayerCount: () => 1,
            DE_ReadPlayerComponent: (index, component) => values[component] } });
        const evidence = result.logs.find(log => log.event === 'online_transform');
        assert.deepEqual(evidence.submitted, expected);
        assert.deepEqual(evidence.echoed, expected);
        assert.equal(evidence.player_id, 1);
        assert.equal(evidence.successful_submissions, 1);
        assert.ok(!result.calls.some(call => call[0] === 'CREATE_CHAR'));
    });
    for (const [name, keys] of [
        ['Ctrl', [17]], ['Shift', [16]], ['End', [35]], ['Ctrl+Shift', [17, 16]],
        ['Ctrl+End', [17, 35]], ['Shift+End', [16, 35]], ['F11', [122]], ['F12', [123]]
    ]) test('entry ignores ' + name + ' until the complete stop chord is held', () => {
        const result = runEntry({ keyFrames: [keys, [17, 16, 35]] });
        assert.equal(result.calls.filter(call => call[0] === 'GET_CHAR_COORDINATES').length, 1);
        assert.equal(result.logs.filter(log => log.event === 'adapter_stopped').length, 1);
        assert.equal(result.logs.find(log => log.event === 'adapter_stopped').reason, 'Ctrl+Shift+End');
        assert.ok(!result.calls.some(call => call[0] === 'IS_KEY_DOWN' || call[0] === 'HOLD_KEY' || call[0] === 'RELEASE_KEY'));
    });
    test('entry requires simultaneous modifier keys rather than accumulating separate presses', () => {
        const result = runEntry({ keyFrames: [[17], [16], [35], [17, 16, 35]] });
        assert.equal(result.calls.filter(call => call[0] === 'GET_CHAR_COORDINATES').length, 3);
        assert.equal(result.logs.find(log => log.event === 'adapter_stopped').reason, 'Ctrl+Shift+End');
    });
    test('complete stop chord exits before reading a player or opening transport', () => {
        const result = runEntry({ mode: 'online', activation: 'automatic', keyFrames: [[17, 16, 35]] });
        assert.ok(!result.calls.some(call => call[0] === 'IS_PLAYER_PLAYING' || call[0] === 'GET_CHAR_COORDINATES' || call[0] === 'DE_ConnectLocal'));
        assert.equal(result.logs.find(log => log.event === 'adapter_stopped').reason, 'Ctrl+Shift+End');
    });
    const manual = overrides => runEntry({ mode: 'online', activation: 'manual', ...overrides });
    function inputEntry(overrides = {}) {
        const controls = Array.from({ length: 20 }, (_, index) => index === 0 ? -32768 : index === 19 ? 32767 : index * -7);
        return runEntry({ mode: 'smoke', inputReference: true, activation: 'automatic',
            keyFrames: [[17, 16, 120], [], [], [17, 16, 35]], ...overrides,
            natives: { GET_PAD_STATE: (pad, index) => { assert.equal(pad, 0); return controls[index]; },
                GET_CHAR_VELOCITY: ped => { assert.equal(ped, 10); return { x: -0.1, y: 0.2, z: -0.3 }; },
                ...(overrides.natives || {}) } });
    }
    test('input reference ignores the console Home chord and activates only on Ctrl+Shift+F9', () => {
        // Home opens the Rockstar console on the tested game, interrupting the
        // observation. The input-only shortcut must not enter the DLL on Home.
        const result = inputEntry({ keyFrames: [[17, 16, 36], [17, 16, 36], [], [17, 16, 120], [], [17, 16, 35]] });
        assert.ok(!result.logs.some(event => event.event === 'adapter_failed'));
        assert.ok(result.runtimeAccess.every(access => access.frame >= 3), 'Home must not activate the input reference');
        assert.deepEqual(result.callFrames.filter(call => call.name === 'DynamicLibrary.Load').map(call => call.frame), [3]);
        assert.ok(result.callFrames.filter(call => call.name === 'GET_PAD_STATE').every(call => call.frame >= 3));
        assert.equal(result.logs.find(event => event.event === 'adapter_armed').activate_key, 'Ctrl+Shift+F9');
        assert.equal(result.logs.find(event => event.event === 'adapter_activation_requested').activate_key, 'Ctrl+Shift+F9');
    });
    test('input reference requires manual F9 despite automatic preferences and does not access the DLL while armed', () => {
        const result = inputEntry({ keyFrames: [[], [], [17, 16, 120], [], [17, 16, 35]] });
        assert.ok(!result.logs.some(event => event.event === 'adapter_failed'));
        assert.equal(result.logs.find(event => event.event === 'adapter_armed').mode, 'smoke');
        assert.ok(result.runtimeAccess.every(access => access.frame >= 2));
        assert.ok(result.callFrames.filter(call => call.name === 'GET_PAD_STATE').every(call => call.frame >= 2));
    });
    test('input reference reads pad zero indices zero through nineteen and velocity from the actual CJ handle', () => {
        const result = inputEntry();
        assert.ok(!result.logs.some(event => event.event === 'adapter_failed'));
        const samples = result.logs.filter(event => event.event === 'local_controller_sample');
        assert.equal(samples.length, 2);
        assert.deepEqual(result.calls.filter(call => call[0] === 'GET_PAD_STATE').slice(0, 20),
            Array.from({ length: 20 }, (_, index) => ['GET_PAD_STATE', 0, index]));
        assert.ok(result.calls.filter(call => call[0] === 'GET_CHAR_VELOCITY').every(call => call[1] === 10));
        assert.equal(samples[0].current[0], -32768); assert.equal(samples[0].current[19], 32767);
        assert.equal(samples[0].previous, null); assert.deepEqual(samples[1].previous, samples[0].current);
        assert.deepEqual(samples[0].velocity_raw, { x: -0.1, y: 0.2, z: -0.3 });
        assert.equal(samples[0].velocity_units, null);
        assert.deepEqual(samples[0].unknown_fields, ['m_bChatIndicated', 'm_bPedWalk', 'm_bVehicleMouseLook', 'm_bRadioTrackSkip']);
    });
    test('input reference publishes typed fields only after all native evidence is collected and reads signed canonical results', () => {
        const result = inputEntry();
        const firstBegin = result.calls.findIndex(call => call[0] === 'DE_ControllerBegin');
        const lastControl = result.calls.findIndex(call => call[0] === 'GET_PAD_STATE' && call[2] === 19);
        const firstVelocity = result.calls.findIndex(call => call[0] === 'GET_CHAR_VELOCITY');
        assert.ok(firstBegin > lastControl && firstBegin > firstVelocity);
        assert.deepEqual(result.calls.filter(call => call[0] === 'DE_ControllerSet').slice(0, 20).map(call => call.slice(1)),
            result.logs.find(event => event.event === 'local_controller_sample').current.map((value, index) => [index, value]));
        assert.deepEqual(result.calls.find(call => call[0] === 'DE_ControllerRead'), ['DE_ControllerRead', 0, 0]);
        assert.ok(result.calls.some(call => call[0] === 'DE_ControllerRead' && call[1] === 1));
        assert.equal(result.calls.filter(call => call[0] === 'DE_ControllerReset').length, 2);
    });
    test('input reference never binds transport exports, creates peds, changes controls, or sends networking', () => {
        const result = inputEntry();
        const forbidden = ['DE_ConnectLocal', 'DE_Pump', 'DE_SubmitTransformBits', 'DE_PlayerCount', 'DE_LocalPlayerId', 'DE_Disconnect',
            'CREATE_CHAR', 'CREATE_PLAYER', 'DELETE_CHAR', 'DELETE_PLAYER', 'SET_CHAR_VELOCITY', 'HOLD_KEY', 'RELEASE_KEY', 'TASK_PLAY_ANIM'];
        for (const name of forbidden) {
            assert.ok(!result.calls.some(call => call[0] === name), name);
            assert.ok(!result.runtimeAccess.some(access => access.procedure === name), 'bind ' + name);
        }
        const start = result.logs.find(event => event.event === 'adapter_start');
        assert.equal(start.native_input_reference, true); assert.equal(start.endpoint, null); assert.equal(start.network_active, false);
        assert.equal(result.logs.find(event => event.event === 'native_input_reference_stopped').reason, 'Ctrl+Shift+End');
    });
    test('input reference cancellation before F9 requires no bridge or player sample', () => {
        const result = inputEntry({ keyFrames: [[17, 16, 35, 120]] });
        assert.ok(!result.calls.some(call => ['DynamicLibrary.Load', 'GET_PLAYER_CHAR', 'GET_PAD_STATE'].includes(call[0])));
        assert.equal(result.logs.at(-1).event, 'adapter_activation_cancelled');
    });
    for (const options of [{ mode: 'online' }, { mode: 'diagnostic' }, { reference: true }, { inputReference: 'yes' }])
        test('invalid input-reference selection fails before any access: ' + JSON.stringify(options), () => {
            const result = inputEntry({ ...options, forbidRuntimeAccess: true });
            assert.match(result.logs.at(-1).error, /input reference.*Smoke/i);
            assert.equal(result.calls.length, 0);
        });
    test('input reference requires API revision 0.500 before key, bridge, or player access', () => {
        const result = inputEntry({ definitions: '0.499', forbidRuntimeAccess: true });
        assert.match(result.logs.at(-1).error, /0\.500/); assert.equal(result.calls.length, 0);
        assert.ok(!result.logs.some(event => event.event === 'adapter_armed'));
    });
    test('input reference accepts numeric shorthand 0.5 for the pinned API revision', () => {
        const result = inputEntry({ definitions: 0.5 });
        assert.ok(!result.logs.some(event => event.event === 'adapter_failed'));
        assert.equal(result.logs.filter(event => event.event === 'local_controller_sample').length, 2);
    });
    test('input reference respects readiness before manual activation', () => {
        const result = inputEntry({ keyFrames: [[17, 16, 120], [], [17, 16, 120], [17, 16, 35]], readyFrames: [false, true, true, true] });
        assert.ok(!result.logs.some(event => event.event === 'adapter_failed'));
        assert.ok(result.runtimeAccess.every(access => access.frame >= 2));
    });
    test('input reference handles native read failure through canonical cleanup without transport cleanup', () => {
        const result = inputEntry({ natives: { GET_CHAR_VELOCITY: () => { throw new Error('velocity failure'); } } });
        assert.match(result.logs.find(event => event.event === 'adapter_failed').error, /velocity failure/);
        assert.equal(result.calls.filter(call => call[0] === 'DE_ControllerReset').length, 2);
        assert.ok(!result.calls.some(call => call[0] === 'DE_Disconnect'));
    });
    test('input reference rejects invalid raw controls before canonical Begin', () => {
        const result = inputEntry({ natives: { GET_PAD_STATE: (pad, index) => index === 19 ? 32768 : 0 } });
        assert.match(result.logs.find(event => event.event === 'adapter_failed').error, /invalid signed control/);
        assert.ok(!result.calls.some(call => call[0] === 'DE_ControllerBegin'));
    });
    test('input reference does not silently coerce malformed unsigned bridge readback', () => {
        const result = inputEntry({ functions: { DE_ControllerRead: () => '0' } });
        assert.match(result.logs.find(event => event.event === 'adapter_failed').error, /canonical.*unsigned/i);
        assert.ok(!result.logs.some(event => event.event === 'local_controller_sample'));
    });
    test('input reference refuses a missing canonical export before observing inputs', () => {
        const result = inputEntry({ functions: { DE_ControllerCommit: undefined } });
        assert.ok(result.logs.some(event => event.event === 'adapter_failed'));
        assert.ok(!result.logs.some(event => event.event === 'local_controller_sample'));
        assert.ok(!result.calls.some(call => call[0] === 'GET_PAD_STATE'));
    });
    test('input reference entry ends automatically after at most three hundred samples with fifty-millisecond yields', () => {
        const result = inputEntry({ keyFrames: [[17, 16, 120], ...Array.from({ length: 610 }, () => [])] });
        assert.ok(!result.logs.some(event => event.event === 'adapter_failed'));
        assert.equal(result.logs.filter(event => event.event === 'local_controller_sample').length, 300);
        assert.equal(result.logs.find(event => event.event === 'native_input_reference_finished').reason, 'sample_budget');
        assert.ok(result.waits.every(wait => wait.milliseconds === 50));
        assert.equal(result.calls.filter(call => call[0] === 'DE_ControllerCommit').length, 300);
        assert.equal(result.calls.filter(call => call[0] === 'DE_ControllerReset').length, 2);
    });
    test('input reference entry stops a replaced local player before another velocity read', () => {
        let lookups = 0;
        const result = inputEntry({ natives: { GET_PLAYER_CHAR: () => ++lookups > 2 ? 11 : 10 } });
        assert.ok(!result.logs.some(event => event.event === 'adapter_failed'));
        assert.equal(result.logs.find(event => event.event === 'native_input_reference_finished').reason, 'local_ped_changed');
        assert.equal(result.calls.filter(call => call[0] === 'GET_CHAR_VELOCITY').length, 1);
    });
    test('input reference entry treats a long render gap as an incomplete observation without catch-up reads', () => {
        const result = inputEntry({ times: [0, 300, 350, 400] });
        assert.equal(result.logs.filter(event => event.event === 'local_controller_sample').length, 1);
        assert.equal(result.logs.find(event => event.event === 'native_input_reference_finished').reason, 'render_gap');
        assert.equal(result.calls.filter(call => call[0] === 'GET_PAD_STATE').length, 20);
    });
    test('ordinary Smoke does not bind or call the opt-in canonical controller exports', () => {
        const result = runEntry();
        assert.ok(!result.calls.some(call => call[0].startsWith('DE_Controller') || ['GET_PAD_STATE', 'GET_CHAR_VELOCITY'].includes(call[0])));
        assert.ok(!result.runtimeAccess.some(access => access.procedure?.startsWith('DE_Controller')));
    });
    for (const script of ['package.ps1', 'set-mode.ps1']) {
        for (const [mode, extra, expected] of [['Online', [], /requires Smoke mode/], ['Diagnostic', [], /requires Smoke mode/],
            ['Smoke', ['-NativeGaitReference'], /conflicts with native gait reference/]])
            test(script + ' rejects invalid input selection before file or installed-game access: ' + mode + extra.join(), () => {
                const { spawnSync } = require('node:child_process');
                const arguments = ['-NoProfile', '-File', path.resolve(adapterDirectory, '..', script), '-Mode', mode, '-NativeInputReference', ...extra];
                if (script === 'set-mode.ps1') arguments.push('-GameDirectory', path.resolve(__dirname, '../../Build/this-input-guard-path-does-not-exist'));
                const result = spawnSync('pwsh', arguments, { encoding: 'utf8', timeout: 10000 });
                assert.equal(result.error, undefined);
                assert.notEqual(result.status, 0);
                assert.match(result.stdout + result.stderr, expected);
                assert.ok(!fs.existsSync(path.resolve(__dirname, '../../Build/this-input-guard-path-does-not-exist')));
            });
    }
    function referenceEntry(overrides = {}) {
        const owned = new Set([10]);
        const natives = {
            DOES_CHAR_EXIST: handle => owned.has(handle),
            REQUEST_MODEL: () => {}, HAS_MODEL_LOADED: () => true,
            CREATE_CHAR: () => { owned.add(100); return 100; },
            GET_CHAR_COORDINATES: handle => handle === 10 ? { x: 10, y: 20, z: 3 } : { x: 6, y: 23, z: 3 },
            GET_CHAR_HEADING: () => 270,
            SET_CHAR_COORDINATES_NO_OFFSET: () => {}, SET_CHAR_HEADING: () => {},
            CLEAR_CHAR_TASKS: () => {}, TASK_TOGGLE_PED_THREAT_SCANNER: () => {},
            SET_CHAR_PROOFS: () => {}, SET_CHAR_COLLISION: () => {}, FREEZE_CHAR_POSITION: () => {},
            TASK_GO_STRAIGHT_TO_COORD: () => {},
            IS_CHAR_PLAYING_ANIM: () => true, GET_CHAR_ANIM_CURRENT_TIME: () => 0.42,
            DELETE_CHAR: handle => owned.delete(handle), MARK_MODEL_AS_NO_LONGER_NEEDED: () => {},
            ...(overrides.natives || {})
        };
        return runEntry({ mode: 'smoke', reference: true, activation: 'automatic',
            keyFrames: [[17, 16, 36], ...Array.from({ length: 11 }, () => []), [17, 16, 35]],
            times: Array.from({ length: 13 }, (_, index) => index * 100),
            ...overrides, natives });
    }
    test('native reference waits for Home even with an automatic online preference', () => {
        const result = referenceEntry({ keyFrames: [[], [17, 16, 36], [17, 16, 35]], times: [0, 50, 100] });
        assert.ok(!result.logs.some(event => event.event === 'adapter_failed'));
        assert.equal(result.logs.find(event => event.event === 'adapter_armed').mode, 'smoke');
        assert.ok(result.runtimeAccess.every(access => access.frame >= 1));
    });
    test('native reference dispatches one finite natural walk and never connects transport or forces playback', () => {
        const result = referenceEntry();
        assert.ok(!result.logs.some(event => event.event === 'adapter_failed'));
        assert.deepEqual(result.calls.find(call => call[0] === 'TASK_GO_STRAIGHT_TO_COORD'),
            ['TASK_GO_STRAIGHT_TO_COORD', 100, 14, 23, 3, 4, 10000]);
        assert.equal(result.calls.filter(call => call[0] === 'TASK_GO_STRAIGHT_TO_COORD').length, 1);
        assert.deepEqual(result.calls.find(call => call[0] === 'FREEZE_CHAR_POSITION'), ['FREEZE_CHAR_POSITION', 100, false]);
        assert.deepEqual(result.calls.find(call => call[0] === 'SET_CHAR_COLLISION'), ['SET_CHAR_COLLISION', 100, true]);
        assert.equal(result.calls.filter(call => call[0] === 'SET_CHAR_COORDINATES_NO_OFFSET').length, 1);
        for (const name of ['DE_ConnectLocal', 'DE_Pump', 'DE_SubmitTransformBits', 'TASK_PLAY_ANIM', 'SET_CHAR_ANIM_SPEED'])
            assert.ok(!result.calls.some(call => call[0] === name));
        assert.deepEqual(result.calls.find(call => call[0] === 'DELETE_CHAR'), ['DELETE_CHAR', 100]);
    });
    test('native reference does not query a phase when the natural clip is unavailable', () => {
        const result = referenceEntry({ natives: { IS_CHAR_PLAYING_ANIM: () => false,
            GET_CHAR_ANIM_CURRENT_TIME: () => { throw new Error('Phase queried without association'); } } });
        assert.ok(!result.logs.some(event => event.event === 'adapter_failed'));
        assert.ok(!result.calls.some(call => call[0] === 'GET_CHAR_ANIM_CURRENT_TIME'));
    });
    test('native reference cancellation while armed needs no bridge or game mutation', () => {
        const result = referenceEntry({ keyFrames: [[17, 16, 35]] });
        assert.ok(!result.calls.some(call => call[0] === 'DynamicLibrary.Load' || call[0] === 'CREATE_CHAR'));
        assert.equal(result.logs.at(-1).event, 'adapter_activation_cancelled');
    });
    for (const options of [{ mode: 'online', reference: true }, { mode: 'smoke', reference: 'yes' }])
        test('invalid native-reference selection fails before bridge and mutation: ' + JSON.stringify(options), () => {
            const result = runEntry(options);
            assert.match(result.logs.at(-1).error, /opt-in Smoke/);
            assert.ok(!result.calls.some(call => call[0] === 'DynamicLibrary.Load' || call[0] === 'CREATE_CHAR'));
        });
    const assertDormant = result => {
        assert.deepEqual(result.runtimeAccess, []);
        assert.ok(result.calls.every(call => ['IS_KEY_PRESSED', 'IS_PLAYER_PLAYING'].includes(call[0])));
        assert.ok(!result.logs.some(log => ['adapter_start', 'adapter_activation_requested', 'adapter_failed',
            'local_transform', 'online_transform', 'adapter_joined', 'remote_ped_created'].includes(log.event)));
        assert.ok(!result.logs.some(log => log.phase === 'bridge_load' || log.phase === 'controller_run'));
    };
    const sourceConfig = await import(pathToFileURL(path.join(adapterDirectory, 'config.mjs')).href);
    test('source configuration makes online activation manual by default', () => {
        assert.equal(sourceConfig.ONLINE_ACTIVATION, 'manual');
        assert.equal(sourceConfig.MTA_CORE_RENDER_NATIVE_DISPATCH_REFERENCE, false);
        assert.equal(sourceConfig.MTA_CORE_GUI_STARTUP_REFERENCE, false);
        assert.equal(sourceConfig.MTA_CORE_GUI_SURFACE_REFERENCE, false);
        assert.equal(sourceConfig.MTA_CORE_GUI_FRONTEND_REFERENCE, false);
        assert.equal(sourceConfig.GAIT_REFERENCE, false);
        assert.equal(sourceConfig.GAIT_RATE, false);
        assert.equal(sourceConfig.INPUT_REFERENCE, false);
        assert.equal(sourceConfig.MTA_NATIVE_PAD_REFERENCE, false);
        assert.equal(sourceConfig.MTA_NATIVE_MOVEMENT_TRIAL, false);
        assert.equal(sourceConfig.MTA_CORE_STARTUP_REFERENCE, false);
        assert.equal(sourceConfig.MTA_CORE_INPUT_REFERENCE, false);
        assert.equal(sourceConfig.MTA_CORE_MOVEMENT_REFERENCE, false);
        assert.equal(sourceConfig.MTA_CORE_RENDER_REFERENCE, false);
        assert.equal(sourceConfig.MTA_CORE_RENDER_ROUTE, 'd3d11');
        assert.equal(sourceConfig.MTA_CORE_RENDER_GPU_REFERENCE, false);
        assert.equal(sourceConfig.MTA_CORE_RENDER_CUSTOM_PRESENT_REFERENCE, false);
    });
    test('manual online remains armed with no DLL, procedure, memory, player-handle, coordinate, UDP or remote access', () => {
        const result = manual({ keyFrames: [[], [], [], [], [], [17, 16, 35]] });
        assertDormant(result);
        assert.equal(result.logs.filter(log => log.event === 'adapter_armed').length, 1);
        assert.deepEqual(result.logs.find(log => log.event === 'adapter_armed'), {
            event: 'adapter_armed', mode: 'online', activate_key: 'Ctrl+Shift+Home', stop_key: 'Ctrl+Shift+End',
            bridge_loaded: false, game_adapter_active: false, network_active: false
        });
        assert.deepEqual(result.logs.filter(log => log.event === 'adapter_phase').map(log => log.phase), ['entry', 'awaiting_activation']);
        assert.equal(result.waits.length, 5);
        assert.ok(result.waits.every(wait => wait.milliseconds === 50));
    });
    for (const [name, keys] of [
        ['Ctrl', [17]], ['Shift', [16]], ['Home', [36]], ['Ctrl+Shift', [17, 16]],
        ['Ctrl+Home', [17, 36]], ['Shift+Home', [16, 36]], ['Ctrl+Shift+F9', [17, 16, 120]], ['F11', [122]], ['F12', [123]]
    ]) test('manual online ignores ' + name + ' without starting runtime or transport', () => {
        const result = manual({ keyFrames: [keys, [17, 16, 35]] });
        assertDormant(result);
        assert.equal(result.waits.length, 1);
        assert.equal(result.calls.filter(call => call[0] === 'IS_PLAYER_PLAYING').length, 0);
    });
    test('manual online requires simultaneous activation keys rather than accumulating separate presses', () => {
        const result = manual({ keyFrames: [[17], [16], [36], [17, 16, 35]] });
        assertDormant(result);
        assert.equal(result.waits.length, 3);
    });
    test('full activation chord while player is not ready does not access runtime or transport', () => {
        const result = manual({ keyFrames: [[17, 16, 36], [], [17, 16, 35]], readyFrames: [false, true, true] });
        assertDormant(result);
        assert.equal(result.calls.filter(call => call[0] === 'IS_PLAYER_PLAYING').length, 1);
        assert.equal(result.waits.length, 2);
    });
    test('ready activation begins exactly one bridge load after its marker and repeated held Home does not reload', () => {
        const result = manual({ keyFrames: [[], [17, 16, 36], [17, 16, 36], [17, 16, 35]] });
        assert.equal(result.calls.filter(call => call[0] === 'DynamicLibrary.Load').length, 1);
        assert.equal(result.calls.filter(call => call[0] === 'DE_ConnectLocal').length, 1);
        assert.equal(result.logs.filter(log => log.event === 'adapter_activation_requested').length, 1);
        assert.equal(result.logs.filter(log => log.event === 'adapter_armed').length, 1);
        assert.ok(result.runtimeAccess.every(access => access.frame >= 1));
        assert.ok(result.callFrames.filter(call => call.frame === 0).every(call => call.name === 'IS_KEY_PRESSED'));
        assert.equal(result.callFrames.find(call => call.name === 'DynamicLibrary.Load').frame, 1);
        assert.ok(result.logs.findIndex(log => log.event === 'adapter_activation_requested') < result.logs.findIndex(log => log.phase === 'bridge_load'));
        assert.ok(result.logs.some(log => log.event === 'online_transform'));
        assert.equal(result.logs.find(log => log.event === 'adapter_stopped').reason, 'Ctrl+Shift+End');
        assert.ok(!result.logs.some(log => log.event === 'adapter_failed'));
    });
    test('manual online ignores not-ready Home until another fully held ready chord', () => {
        const result = manual({ keyFrames: [[17, 16, 36], [], [17, 16, 36], [17, 16, 35]], readyFrames: [false, true, true, true] });
        assert.equal(result.callFrames.find(call => call.name === 'DynamicLibrary.Load').frame, 2);
        assert.ok(result.runtimeAccess.every(access => access.frame >= 2));
        assert.ok(result.callFrames.filter(call => call.frame < 2).every(call => ['IS_KEY_PRESSED', 'IS_PLAYER_PLAYING'].includes(call.name)));
        assert.equal(result.logs.filter(log => log.event === 'adapter_activation_requested').length, 1);
    });
    test('manual pre-activation End cancellation exits without bridge access or a controller', () => {
        const result = manual({ keyFrames: [[17, 16, 35]] });
        assertDormant(result);
        assert.deepEqual(result.logs.at(-1), { event: 'adapter_activation_cancelled', reason: 'Ctrl+Shift+End', mode: 'online',
            bridge_loaded: false, game_adapter_active: false, network_active: false });
        assert.equal(result.waits.length, 0);
        assert.equal(result.calls.filter(call => call[0] === 'IS_PLAYER_PLAYING').length, 0);
    });
    test('manual stop has priority over activation when both full chords are held', () => {
        const result = manual({ keyFrames: [[17, 16, 35, 36]] });
        assertDormant(result);
        assert.equal(result.logs.at(-1).event, 'adapter_activation_cancelled');
        assert.equal(result.calls.filter(call => call[0] === 'IS_PLAYER_PLAYING').length, 0);
    });
    for (const [name, options] of [
        ['wrong host', { host: 'sa' }], ['old CLEO runtime', { version: '1.4.3' }],
        ['old definitions', { definitions: '0.492' }], ['invalid activation configuration', { activation: 'later' }]
    ]) test('manual online rejects ' + name + ' before input, waiting or runtime access', () => {
        const result = manual({ ...options, forbidRuntimeAccess: true });
        assert.equal(result.calls.length, 0);
        assert.ok(result.logs.some(log => log.event === 'adapter_failed'));
        assert.ok(!result.logs.some(log => log.event === 'adapter_armed' || log.phase === 'awaiting_activation'));
        if (options.activation) assert.match(result.logs.at(-1).error, /Unknown online activation mode/);
    });
    test('smoke keeps automatic read-only behavior even when online activation is manual', () => {
        const result = runEntry({ mode: 'smoke', activation: 'manual' });
        assert.ok(result.logs.some(log => log.event === 'local_transform'));
        assert.ok(!result.logs.some(log => log.event === 'adapter_armed' || log.event === 'adapter_activation_requested'));
        assert.ok(!result.calls.some(call => call[0] === 'DE_ConnectLocal'));
    });
    test('diagnostic ignores online activation and stays finite without input or runtime access', () => {
        const result = runEntry({ mode: 'diagnostic', activation: 'manual', forbidRuntimeAccess: true });
        assert.equal(result.calls.length, 0);
        assert.equal(result.logs.at(-1).event, 'adapter_diagnostic_complete');
        assert.ok(!result.logs.some(log => log.event === 'adapter_armed'));
    });
    console.log(`Passed ${tests} game-adapter mock tests. No installed game was run.`);
}

main().catch(error => { console.error(error); process.exitCode = 1; });
