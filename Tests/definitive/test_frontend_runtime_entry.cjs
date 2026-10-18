// SPDX-License-Identifier: GPL-3.0-or-later
// These copied-value worker fixtures exercise the real script entry and its
// retirement rules. They do not create a Core, GUI, native lease or GTA process.
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');

const scriptPath = path.resolve(__dirname, '../../Client/definitive/game-adapter/index.js');
const runtimeSource = () => {
    const source = fs.readFileSync(scriptPath, 'utf8');
    const first = source.indexOf('function validateMtaLocalInputRuntimeSample(');
    const last = source.indexOf('\ntry {', first);
    assert.ok(first >= 0 && last > first, 'extract the production runtime function without replacing its validator');
    return source.slice(first, last);
};
const copy = value => JSON.parse(JSON.stringify(value));

function viewState(generation, retired = false) {
    if (!generation) return Array(288).fill(0);
    const view = Array(288).fill(0);
    // The actual GUI reference keeps CleanupPending set for the whole live
    // workload. It becomes zero only after the view's owners have retired.
    Object.assign(view, { 0: 1, 1: 1, 5: 1, 68: 12, 70: 12, 96: 1, 97: 2, 98: 77, 99: 1, 100: 1,
        161: 1, 162: 1, 208: 1, 210: 1, 214: generation + 100, 220: generation + 100,
        232: 1, 236: 1, 237: 1, 256: 1, 257: generation, 259: 1, 260: generation + 100,
        262: 1, 265: 1000, 266: 500 });
    if (retired) {
        Object.assign(view, { 1: 2, 5: 0, 6: 1, 9: 1, 39: 1, 78: 1, 79: 1, 80: 1, 97: 4,
            100: 0, 113: 1, 128: 1, 129: 4, 149: 1, 162: 0, 171: 1, 176: 1,
            177: 4, 190: 1, 208: 0, 210: 0, 238: 1, 259: 0, 262: 0, 263: 1, 269: 1 });
    }
    return view;
}

// Publications follow commands, not script time. Core64 is sampled only at
// Initial/BeforeShow/AfterHide/Stopped boundaries. A Visible report therefore
// retains its genuine pre-view Core sample rather than inventing live reads.
function protocolFixture(options = {}) {
    const generation = 7;
    const commands = [], trace = [];
    const viewCommandAttempts = new Map();
    let polls = 0, phase = 2, viewGeneration = 0, viewsRetired = 0, showAcks = 0, hideAcks = 0;
    let accepted = 0, acceptedCommand = 0, acknowledged = 0, acknowledgedCommand = 0;
    let pending = null, stage = 1, coreSamples = 1, viewSamples = 0, guiHides = 0, focusHides = 0;
    let lastCoreGeneration = 0, lastCoreRetired = 0, backendPinned = false, hostExpired = false;
    let snapshot = null, captured = null, mutated = false, heartbeatRejected = false;
    let localSnapshot = null, localCaptured = null;
    let stopSequence = 0, failure = 0, reason = 0, closeRetirementPending = null, viewStopReason = 1;
    const takeCore = nextStage => {
        stage = nextStage;
        ++coreSamples;
        lastCoreGeneration = viewGeneration;
        lastCoreRetired = viewsRetired;
    };
    const retireView = () => {
        if (viewGeneration > viewsRetired) ++viewsRetired;
    };
    const core = () => {
        const stopped = phase === 7 || phase === 8;
        const result = Array(64).fill(0);
        Object.assign(result, { 0: 1, 1: stopped ? 4 : 2, 2: 77, 3: 1, 4: 1, 5: stopped ? 0 : 1,
            6: !stopped && lastCoreGeneration ? 1 : 0, 7: coreSamples, 14: stopped ? 1 : 0,
            16: stopped ? 0 : lastCoreGeneration ? 97 : 33, 22: lastCoreGeneration, 23: 1,
            32: 1, 33: stopped ? 5 : 2, 34: 77, 35: 901, 36: 1, 37: 1,
            38: stopped ? 0 : 1, 39: stopped ? 0 : 1, 40: stopped ? 0 : 1,
            41: stopped ? 0 : 1, 42: stopped ? 0 : 1, 43: 1, 44: 1, 45: 1,
            46: lastCoreGeneration, 47: lastCoreRetired, 48: lastCoreGeneration,
            51: 1, 52: 3, 54: stopped ? 0 : 6, 55: 1, 57: stopped ? 1 : 0 });
        return result;
    };
    const publish = () => {
        const terminal = phase === 7 || phase === 8;
        const retired = viewGeneration <= viewsRetired;
        const values = Array(416).fill(0);
        Object.assign(values, { 0: 1, 1: phase, 2: polls, 4: polls * 50, 5: generation, 7: 66, 8: 77,
            9: accepted, 10: acceptedCommand, 11: acknowledged, 12: acknowledgedCommand,
            13: acknowledged ? 1 : 0, 14: pending ? pending.sequence : 0, 15: pending ? pending.command : 0,
            16: stopSequence ? 1 : 0, 17: stopSequence, 18: hostExpired ? 0 : 1, 19: 1000,
            20: viewGeneration, 22: viewGeneration, 23: viewsRetired, 24: showAcks, 25: hideAcks,
            26: stage, 27: coreSamples, 30: terminal ? 0 : 1, 31: terminal ? 0 : 1,
            32: backendPinned ? 1 : 0, 33: phase === 9 ? 1 : 0, 34: terminal ? 1 : 0,
            36: failure, 37: reason, 38: viewSamples, 39: terminal ? 1 : 0,
            40: terminal ? 1 : 0, 41: terminal ? 1 : 0, 44: retired ? 1 : 0,
            45: hostExpired ? 1 : 0, 46: focusHides, 47: guiHides,
            49: viewsRetired, 50: 500, 51: 500 });
        values.splice(64, 64, ...core());
        const view = viewState(viewGeneration, retired);
        if (retired && viewGeneration) view[269] = viewStopReason;
        if (phase === 9 && !retired) Object.assign(view, { 1: 3, 269: viewStopReason });
        values.splice(128, 288, ...view);
        options.mutateEvery?.(values, { polls, phase, viewGeneration });
        if (!mutated && options.mutate && options.mutateAt === polls) {
            options.mutate(values, { polls, phase, viewGeneration });
            mutated = true;
        }
        snapshot = values;
        // This is a separate copied Core32 cache. It deliberately carries no
        // synthetic native proof into the existing Runtime416 publication.
        const local = Array(32).fill(0);
        Object.assign(local, { 0: 1, 1: 1, 2: 77, 21: 1 });
        if (!options.localNeverConfigured) {
            Object.assign(local, { 1: terminal ? 1 : phase === 9 ? 5 : phase === 4 ? 4 : viewGeneration ? 3 : 2,
                3: terminal ? 0 : 1, 4: !terminal && phase === 2 ? 1 : 0,
                12: 500, 17: 1, 18: 1, 19: terminal ? 0 : 1, 23: 1,
                24: 15, 25: 15, 26: 1, 28: 6, 30: 6 });
            if (!terminal && phase === 2 && viewGeneration) {
                Object.assign(local, { 5: 1, 6: 701, 8: 1, 9: 1, 13: polls,
                    15: 1, 18: 0, 21: 0 });
            }
            if (!terminal && phase === 9) Object.assign(local, { 5: 1, 6: 701, 9: 1, 20: 1, 21: 0 });
            // Attempted and generation are historical facts. Successful End
            // retains them; absence of active/bound/debt proves retirement.
            if (terminal) Object.assign(local, { 5: 1, 6: 701, 10: 1, 11: 5, 13: polls, 27: 1 });
        }
        options.mutateLocalEvery?.(local, { polls, phase, terminal, viewGeneration });
        if (options.mutateLocal && options.mutateLocalAt === polls)
            options.mutateLocal(local, { polls, phase, terminal, viewGeneration });
        localSnapshot = local;
    };
    const functions = {
        DE_FrontendRuntimeApiVersionV1: () => options.apiVersion ?? 1,
        DE_FrontendRuntimeStartV1: () => options.startResult ?? generation,
        DE_LocalInputRuntimeApiVersionV1: () => options.localApiVersion ?? 1,
        DE_LocalInputRuntimeStartV1: () => {
            trace.push({ kind: 'localStart', polls });
            return options.localStartResult ?? generation;
        },
        DE_LocalInputRuntimeCaptureV1: () => {
            trace.push({ kind: 'localCapture', polls });
            if (options.localCaptureThrowAt === polls) throw new Error('owned local capture failure');
            if (options.localCaptureZeroAt?.includes(polls)) return 0;
            if (options.localCaptureStatusAt === polls) return 2;
            localCaptured = localSnapshot.slice();
            return 1;
        },
        DE_LocalInputRuntimeReadV1: field => {
            trace.push({ kind: 'localRead', polls, field });
            if (options.localReadThrowAt === polls) throw new Error('owned local read failure');
            return localCaptured[field];
        },
        DE_FrontendRuntimeCommandV1: (session, sequence, command) => {
            trace.push({ kind: 'command', polls, session, sequence, command });
            commands.push({ session, sequence, command });
            if (session !== generation || sequence <= accepted || command < 1 || command > 3) return 0;
            if (command === 3 && options.rejectStop) { heartbeatRejected = true; return 0; }
            if (command !== 3 && options.viewCommandResult) {
                const key = sequence + ':' + command;
                const attempt = (viewCommandAttempts.get(key) || 0) + 1;
                viewCommandAttempts.set(key, attempt);
                // Refusal has no acceptance side effects: the same unused
                // sequence remains available to a fresh retry or to Stop.
                const result = options.viewCommandResult({ polls, session, sequence, command, attempt });
                if (result !== undefined && result !== 1) return result;
            }
            accepted = sequence; acceptedCommand = command; pending = { sequence, command };
            if (command === 3) stopSequence = sequence;
            return 1;
        },
        DE_FrontendRuntimeHeartbeatV1: session => {
            trace.push({ kind: 'heartbeat', polls, session });
            if (options.heartbeatRejectAt === polls) heartbeatRejected = true;
            return heartbeatRejected ? 0 : 1;
        },
        DE_FrontendRuntimePollV1: () => {
            ++polls;
            trace.push({ kind: 'poll', polls });
            if (options.pollThrowAt === polls) throw new Error('owned poll failure');
            if (closeRetirementPending) {
                // GUI/focus closing is selected before native cleanup completes.
                // A Hide can be accepted during that debt, but its acknowledgement
                // and the one selected close counter wait for actual retirement.
                const retiresAt = closeRetirementPending === 'focus' ? options.focusHideRetiresAt : options.guiHideRetiresAt;
                if (polls >= retiresAt) {
                    retireView(); phase = 2; takeCore(3);
                    if (closeRetirementPending === 'focus') ++focusHides;
                    else ++guiHides;
                    closeRetirementPending = null;
                    if (pending?.command === 2) {
                        acknowledged = pending.sequence; acknowledgedCommand = pending.command;
                        pending = null; ++hideAcks;
                    }
                }
            } else if (pending) {
                const request = pending;
                pending = null;
                acknowledged = request.sequence; acknowledgedCommand = request.command;
                if (request.command === 1) {
                    takeCore(2);
                    ++viewGeneration; ++showAcks; backendPinned = true; phase = 4; viewStopReason = 1;
                } else if (request.command === 2) {
                    retireView(); ++hideAcks; phase = 2; takeCore(3);
                } else {
                    retireView(); reason = 1; phase = options.retainedCleanup ? 9 : 7; takeCore(4);
                }
            } else if (heartbeatRejected && phase !== 7 && phase !== 8) {
                retireView(); hostExpired = true; failure = 12; reason = 2;
                phase = options.retainedCleanup ? 9 : 8; takeCore(4);
            } else if (phase === 4 && (options.guiHideAt === polls || options.focusHideAt === polls)) {
                const focused = options.focusHideAt === polls;
                viewStopReason = focused ? 4 : 1;
                if ((focused ? options.focusHideRetiresAt : options.guiHideRetiresAt) > polls) {
                    closeRetirementPending = focused ? 'focus' : 'gui'; phase = 9;
                } else {
                    retireView(); phase = 2;
                    if (focused) ++focusHides;
                    else ++guiHides;
                    takeCore(3);
                }
            }
            if (phase === 4) ++viewSamples;
            publish();
            return phase === 7 || phase === 8 ? 1 : 0;
        },
        DE_FrontendRuntimeCaptureV1: () => {
            const result = options.captureZeroAt?.includes(polls) ? 0 : 1;
            trace.push({ kind: 'capture', polls, result });
            if (!result) return 0;
            captured = snapshot.slice();
            return 1;
        },
        DE_FrontendRuntimeReadV1: field => captured[field]
    };
    for (const name of options.missingFunctions || []) delete functions[name];
    return { functions, commands, trace, generation, get polls() { return polls; },
        get values() { return snapshot; }, get localValues() { return localSnapshot; } };
}

function execute(options = {}) {
    const fixture = protocolFixture(options);
    const events = [], waits = [];
    let faulted = false, error = null, lastDate = 0;
    const fault = kind => {
        if (!faulted && options.errorKind === kind && fixture.polls === (options.errorAt ?? 3)) {
            faulted = true;
            throw new Error('owned ' + kind + ' failure');
        }
    };
    const keys = () => options.keys?.(fixture.polls) || [];
    const context = {
        __bind: name => { assert.equal(typeof fixture.functions[name], 'function', name); return fixture.functions[name]; },
        __chord: requested => {
            fixture.trace.push({ kind: 'input', polls: fixture.polls }); fault('input');
            return requested.every(key => (fixture.polls >= (options.stopAt ?? 10) ? [17, 16, 35] : keys()).includes(key));
        },
        native: (name, key) => {
            assert.equal(name, 'IS_KEY_PRESSED');
            fixture.trace.push({ kind: 'native', polls: fixture.polls }); fault('native');
            return keys().includes(key);
        },
        Date: { now: () => {
            fixture.trace.push({ kind: 'date', polls: fixture.polls }); fault('date');
            if (options.badDateAt === fixture.polls) return options.badDate ?? NaN;
            const at = fixture.polls * (options.dateStep ?? 50);
            if (options.dateAlwaysFailsAfter && fixture.polls >= options.dateAlwaysFailsAfter) throw new Error('owned persistent date failure');
            lastDate = at;
            return at;
        } },
        wait: milliseconds => { waits.push(milliseconds); fixture.trace.push({ kind: 'wait', polls: fixture.polls }); fault('wait'); },
        emit: event => events.push(copy(event)), enterPhase: () => {}, HOST: 'sa_unreal', MODE: 'smoke', CLEO: { version: '1.5.1' }
    };
    try {
        vm.runInNewContext(runtimeSource() + '\nrunFrontendRuntime(__bind, __chord, "Ctrl+Shift+End", ' +
            JSON.stringify(options.localInput ?? false) + ');', context,
            { timeout: options.timeout ?? 1000 });
    } catch (caught) { error = caught; }
    return { fixture, events, waits, error, lastDate,
        finished: events.find(event => event.event === 'frontend_runtime_finished') };
}

module.exports = function frontendRuntimeEntryTests({ test, runEntry }) {
    const clean = result => {
        assert.equal(result.error, null);
        assert.equal(result.finished?.passed, true);
        assert.equal(result.finished.cleanup_passed, true);
        assert.equal(result.finished.stop_accepted, true);
        assert.equal(result.finished.values.length, 416);
        assert.equal(result.finished.values[1], 7);
        assert.ok(result.events.every(event => !event.network_active && !event.full_mta_ready && !event.clock_started));
    };
    const failedClean = (result, pattern) => {
        assert.equal(result.finished?.passed, false);
        assert.equal(result.finished.cleanup_passed, true);
        if (pattern) assert.match(result.finished.error || '', pattern);
        assert.match(String(result.error), /without a verified clean requested stop/);
        assert.equal(result.fixture.values[41], 1);
    };
    const freshViewCommands = result => {
        const trace = result.fixture.trace;
        const attempted = new Set();
        for (const call of trace.filter(value => value.kind === 'command' && value.command !== 3)) {
            const key = call.sequence + ':' + call.command;
            if (!attempted.has(key)) { attempted.add(key); continue; }
            const capture = trace.find(value => value.kind === 'capture' && value.polls === call.polls && value.result === 1);
            assert.ok(capture && trace.indexOf(capture) < trace.indexOf(call),
                'view retry at poll ' + call.polls + ' follows its own successful fresh capture');
        }
    };
    test('frontend Runtime416 retains one Core/session through debounced hide and fresh show', () => {
        const result = execute({ stopAt: 10, keys: poll => [3, 4, 5, 7, 8].includes(poll) ? [17, 16, 120] : [] });
        clean(result);
        assert.deepEqual(result.fixture.commands.map(value => value.command), [1, 2, 1, 3]);
        assert.deepEqual(result.fixture.commands.map(value => value.sequence), [1, 2, 3, 4]);
        const samples = result.events.filter(event => event.event === 'frontend_runtime_sample');
        assert.ok(samples.some(event => event.values[1] === 2 && event.values[23] === 1));
        assert.ok(samples.some(event => event.values[1] === 4 && event.values[20] === 2));
        assert.ok(samples.every(event => event.values[99] === 901 && event.values[100] === 1 && event.values[66] === 77));
        assert.equal(result.finished.values[110], 2);
        assert.equal(result.finished.values[111], 2);
        assert.equal(result.finished.values[107], 1);
        assert.equal(result.finished.values[108], 1);
        assert.equal(result.finished.values[109], 1);
    });
    test('frontend F9 release rearms the chord but cycling modifiers with F9 held cannot retrigger', () => {
        const result = execute({ stopAt: 10, keys: poll => {
            if ([3, 5, 7, 8].includes(poll)) return [17, 16, 120];
            if (poll === 4) return [120];
            if (poll === 6) return [17, 16];
            return [];
        } });
        clean(result);
        // Poll 5 has no new F9 edge despite a full chord. Poll 7 does, because
        // F9 alone was released at 6 while both modifiers remained held.
        assert.deepEqual(result.fixture.commands.map(value => value.command), [1, 2, 1, 3]);
        assert.deepEqual(result.fixture.commands.map(value => value.sequence), [1, 2, 3, 4]);
        assert.deepEqual(result.fixture.trace.filter(value => value.kind === 'command').map(value => value.polls), [1, 3, 7, 10]);
        assert.equal(result.finished.values[20], 2);
        assert.equal(result.finished.values[23], 2);
        const hotkeys = result.events.filter(event => event.event === 'frontend_runtime_hotkey');
        assert.deepEqual(hotkeys.map(event => event.keys_mask), [0, 7, 4, 7, 3, 7, 0]);
        assert.ok(hotkeys.every(event => Number.isInteger(event.keys_mask) && event.keys_mask >= 0 && event.keys_mask <= 7));
        const modifiersOnlyCycle = hotkeys.find(event => event.at_ms === 5 * 50);
        assert.equal(modifiersOnlyCycle.full_chord, true);
        assert.equal(modifiersOnlyCycle.trigger_down, true);
        assert.equal(modifiersOnlyCycle.toggle_accepted, false);
        const rearmed = hotkeys.find(event => event.at_ms === 6 * 50);
        assert.equal(rearmed.keys_mask, 3);
        assert.equal(rearmed.trigger_down, false);
        const reopened = hotkeys.find(event => event.at_ms === 7 * 50);
        assert.equal(reopened.toggle_accepted, true);
        assert.equal(reopened.desired_visible, true);
        assert.equal(reopened.pending_sequence, 0);
        assert.equal(reopened.worker_phase, 2);
        for (let poll = 1; poll <= 10; ++poll) {
            assert.equal(result.fixture.trace.filter(value => value.kind === 'native' && value.polls === poll).length, 3,
                'hotkey diagnostics reuse the three existing key reads');
        }
    });
    test('frontend runtime keeps the session alive beyond old absolute menu deadlines until explicit Stop', () => {
        const result = execute({ stopAt: 6, dateStep: 40000 });
        clean(result);
        assert.ok(result.lastDate >= 240000);
        const started = result.events.find(event => event.event === 'frontend_runtime_started');
        assert.equal(started.maximum_duration_ms, 0);
        assert.equal(started.display_duration_ms, 0);
        assert.deepEqual(result.fixture.commands.map(value => value.command), [1, 3]);
        assert.ok(result.waits.every(value => value === 50));
    });
    for (const key of ['guiHideAt', 'focusHideAt']) test('frontend ' + key + ' retires to Hidden without automatically reopening', () => {
        const result = execute({ [key]: 3, stopAt: 8 });
        clean(result);
        assert.deepEqual(result.fixture.commands.map(value => value.command), [1, 3]);
        assert.equal(result.finished.values[key === 'guiHideAt' ? 47 : 46], 1);
        assert.equal(result.finished.values[24], 1);
    });
    test('frontend GUI hide can be followed by a new explicit F9 edge on the same Core', () => {
        const result = execute({ guiHideAt: 3, stopAt: 9, keys: poll => [6, 7].includes(poll) ? [17, 16, 120] : [] });
        clean(result);
        assert.deepEqual(result.fixture.commands.map(value => value.command), [1, 1, 3]);
        assert.equal(result.finished.values[20], 2);
    });
    test('frontend late GUI-hide retirement preserves a newer explicit reopen intent', () => {
        const result = execute({ guiHideAt: 3, guiHideRetiresAt: 8, stopAt: 12,
            keys: poll => [3, 6, 7].includes(poll) ? [17, 16, 120] : [] });
        clean(result);
        // Poll 3 begins GUI close before the script's next capture. Its F9
        // edge queues Hide from the last Visible sample. After release at 4,
        // a new edge at 6 requests reopen while that Hide still owes cleanup.
        // Hidden/GuiHideCount arrives at 8 and must not replace that newer wish.
        const samples = result.events.filter(event => event.event === 'frontend_runtime_sample');
        assert.ok(samples.some(event => event.values[2] === 4 && event.values[1] === 9 &&
            event.values[14] === 2 && event.values[23] === 0 && event.values[47] === 0 &&
            event.values[128 + 1] === 3 && event.values[128 + 5] === 1 && event.values[128 + 6] === 0));
        assert.ok(samples.some(event => event.values[2] === 8 && event.values[1] === 2 &&
            event.values[11] === 2 && event.values[14] === 0 && event.values[23] === 1 && event.values[47] === 1));
        assert.deepEqual(result.fixture.commands.map(value => value.command), [1, 2, 1, 3]);
        assert.deepEqual(result.fixture.trace.filter(value => value.kind === 'command').map(value => value.polls), [1, 3, 8, 12]);
        assert.equal(result.finished.values[24], 2);
        assert.equal(result.finished.values[23], 2);
        assert.equal(result.finished.values[47], 1);
    });
    test('frontend late focus-loss retirement overrides queued reopen until a new post-focus edge', () => {
        const result = execute({ focusHideAt: 3, focusHideRetiresAt: 8, stopAt: 14,
            keys: poll => [3, 6, 7, 11, 12].includes(poll) ? [17, 16, 120] : [] });
        clean(result);
        // The newer intent at 6 does not authorize showing after focus loss.
        // Only the separate, rearmed chord at 11 may create another view.
        const samples = result.events.filter(event => event.event === 'frontend_runtime_sample');
        assert.ok(samples.some(event => event.values[2] === 4 && event.values[1] === 9 &&
            event.values[14] === 2 && event.values[23] === 0 && event.values[46] === 0 &&
            event.values[128 + 1] === 3 && event.values[128 + 269] === 4));
        assert.ok(samples.some(event => event.values[2] === 8 && event.values[1] === 2 &&
            event.values[11] === 2 && event.values[14] === 0 && event.values[23] === 1 &&
            event.values[46] === 1 && event.values[47] === 0 && event.desired_visible === false));
        assert.deepEqual(result.fixture.commands.map(value => value.command), [1, 2, 1, 3]);
        assert.deepEqual(result.fixture.trace.filter(value => value.kind === 'command').map(value => value.polls), [1, 3, 11, 14]);
        assert.equal(result.finished.values[24], 2);
        assert.equal(result.finished.values[23], 2);
        assert.equal(result.finished.values[46], 1);
        assert.equal(result.finished.values[47], 0);
    });
    test('frontend Stop has priority over Show or simultaneous F9', () => {
        const result = execute({ stopAt: 1, keys: () => [17, 16, 120] });
        clean(result);
        assert.deepEqual(result.fixture.commands.map(value => value.command), [3]);
        assert.equal(result.finished.view_workload_observed, false);
        const hotkeys = result.events.filter(event => event.event === 'frontend_runtime_hotkey');
        assert.equal(hotkeys.length, 1);
        assert.equal(hotkeys[0].keys_mask, 7);
        assert.equal(hotkeys[0].full_chord, true);
        assert.equal(hotkeys[0].stop_requested, true);
        assert.equal(hotkeys[0].toggle_accepted, false);
        assert.equal(result.fixture.trace.filter(value => value.kind === 'native').length, 3);
    });
    test('frontend transient capture refusal preserves coherent state and still retires', () => {
        const result = execute({ captureZeroAt: [1, 3, 4], stopAt: 8 });
        clean(result);
        assert.ok(result.fixture.trace.filter(value => value.kind === 'capture').length > result.finished.samples);
        assert.deepEqual(result.fixture.commands.map(value => value.command), [1, 3]);
    });
    test('frontend retries a transient Show refusal with the same unused sequence and fresh capture', () => {
        const result = execute({ stopAt: 7, viewCommandResult: ({ command, attempt }) => command === 1 && attempt === 1 ? 0 : 1 });
        clean(result);
        assert.deepEqual(result.fixture.commands.map(value => [value.sequence, value.command]), [[1, 1], [1, 1], [2, 3]]);
        assert.deepEqual(result.fixture.trace.filter(value => value.kind === 'command').map(value => value.polls), [1, 2, 7]);
        assert.equal(result.finished.values[24], 1);
        assert.equal(result.events.filter(value => value.event === 'frontend_runtime_command').length, 1);
        freshViewCommands(result);
    });
    test('frontend retries a transient Hide refusal without replacing the accepted Show sequence', () => {
        const result = execute({ stopAt: 8, keys: poll => poll === 3 ? [17, 16, 120] : [],
            viewCommandResult: ({ command, attempt }) => command === 2 && attempt === 1 ? 0 : 1 });
        clean(result);
        assert.deepEqual(result.fixture.commands.map(value => [value.sequence, value.command]), [[1, 1], [2, 2], [2, 2], [3, 3]]);
        assert.deepEqual(result.fixture.trace.filter(value => value.kind === 'command').map(value => value.polls), [1, 3, 4, 8]);
        assert.equal(result.finished.values[25], 1);
        assert.equal(result.finished.values[23], 1);
        freshViewCommands(result);
    });
    test('frontend bounds permanent Show refusal to three attempts then reuses the unused sequence for Stop', () => {
        const result = execute({ viewCommandResult: () => 0 });
        failedClean(result, /view command was rejected/);
        assert.deepEqual(result.fixture.commands.map(value => [value.sequence, value.command]), [[1, 1], [1, 1], [1, 1], [1, 3]]);
        assert.deepEqual(result.fixture.trace.filter(value => value.kind === 'command').map(value => value.polls), [1, 2, 3, 3]);
        assert.equal(result.finished.values[17], 1);
        assert.equal(result.finished.values[11], 1);
        assert.equal(result.finished.values[24], 0);
        freshViewCommands(result);
    });
    test('frontend bounds permanent Hide refusal to three attempts without skipping the next Stop sequence', () => {
        const result = execute({ keys: poll => poll === 3 ? [17, 16, 120] : [],
            viewCommandResult: ({ command }) => command === 2 ? 0 : 1 });
        failedClean(result, /view command was rejected/);
        assert.deepEqual(result.fixture.commands.map(value => [value.sequence, value.command]),
            [[1, 1], [2, 2], [2, 2], [2, 2], [2, 3]]);
        assert.equal(result.finished.values[17], 2);
        assert.equal(result.finished.values[11], 2);
        assert.equal(result.finished.values[25], 0);
        assert.equal(result.finished.values[23], 1);
        freshViewCommands(result);
    });
    test('frontend End during deferred Show has priority over every later retry', () => {
        const result = execute({ stopAt: 2, viewCommandResult: () => 0 });
        clean(result);
        assert.deepEqual(result.fixture.commands.map(value => [value.sequence, value.command]), [[1, 1], [1, 3]]);
        assert.equal(result.finished.values[24], 0);
        assert.equal(result.finished.values[17], 1);
        assert.equal(result.events.filter(value => value.event === 'frontend_runtime_command').length, 0);
        freshViewCommands(result);
    });
    test('frontend invalid Show admission status requests Stop immediately without a retry', () => {
        const result = execute({ viewCommandResult: () => 2 });
        failedClean(result, /view command status unavailable/);
        assert.deepEqual(result.fixture.commands.map(value => [value.sequence, value.command]), [[1, 1], [1, 3]]);
        assert.deepEqual(result.fixture.trace.filter(value => value.kind === 'command').map(value => value.polls), [1, 1]);
        assert.equal(result.finished.values[24], 0);
        freshViewCommands(result);
    });
    test('frontend deferred Show never retries a cached publication after capture refuses freshness', () => {
        const result = execute({ stopAt: 8, captureZeroAt: [2, 3],
            viewCommandResult: ({ command, attempt }) => command === 1 && attempt === 1 ? 0 : 1 });
        clean(result);
        assert.deepEqual(result.fixture.commands.map(value => [value.sequence, value.command]), [[1, 1], [1, 1], [2, 3]]);
        assert.deepEqual(result.fixture.trace.filter(value => value.kind === 'command').map(value => value.polls), [1, 4, 8]);
        assert.deepEqual(result.fixture.trace.filter(value => value.kind === 'capture' && [2, 3].includes(value.polls))
            .map(value => [value.polls, value.result]), [[2, 0], [3, 0]]);
        assert.equal(result.finished.values[24], 1);
        freshViewCommands(result);
    });
    for (const errorKind of ['date', 'input', 'native', 'wait']) test('frontend ' + errorKind + ' failure keeps polling until actual retirement', () => {
        const result = execute({ errorKind });
        failedClean(result, new RegExp('owned ' + errorKind + ' failure'));
        const trace = result.fixture.trace;
        assert.ok(trace.some(value => value.kind === 'poll' && value.polls > 3));
        assert.deepEqual(result.fixture.commands.map(value => value.command), [1, 3]);
        for (const sample of trace.filter(value => ['date', 'input', 'native'].includes(value.kind))) {
            const first = trace.findIndex(value => value.kind === 'poll' && value.polls === sample.polls);
            assert.ok(first >= 0 && first < trace.indexOf(sample), 'retirement poll precedes script sampling');
        }
    });
    for (const badDate of [NaN, -1, 1.5, 0]) test('frontend rejects invalid or regressing Date ' + String(badDate) + ' after saving cleanup', () => {
        failedClean(execute({ badDateAt: 3, badDate }), /script clock/);
    });
    test('frontend terminal ownership is captured before a persistently broken Date source', () => {
        const result = execute({ dateAlwaysFailsAfter: 3 });
        failedClean(result, /persistent date failure/);
        assert.equal(result.finished.values[1], 7);
    });
    const malformed = [
        ['unsigned word', 2, value => { value[52] = -1; }, /ownership report/],
        ['fractional word', 2, value => { value[52] = 0.5; }, /ownership report/],
        ['unknown field', 2, value => { value[63] = 1; }, /ownership report/],
        ['publication regression', 4, value => { value[2] = 1; }, /moved backwards/],
        ['same-publication mutation', 4, value => { value[2] = 2; }, /without a publication/],
        ['script owner replacement', 2, value => { value[7] = 67; }, /changed owner/],
        ['worker owner replacement', 2, value => { value[8] = 78; }, /changed owner/],
        ['Core session replacement', 2, value => { ++value[99]; }, /replaced its retained Core/],
        ['view generation mismatch', 2, value => { value[128 + 257] = 9; }, /view sample/],
        ['active view without retirement ownership', 2, value => { value[128 + 5] = 0; }, /before input\/cursor\/render/],
        ['active view claiming cleanup complete', 2, value => { value[128 + 6] = 1; }, /before input\/cursor\/render/],
        ['unsafe active native scope', 2, value => { value[128 + 162] = 0; }, /before input\/cursor\/render/],
        ['unsafe active cursor', 2, value => { value[128 + 210] = 0; }, /before input\/cursor\/render/],
        ['unsafe active rendering', 2, value => { value[128 + 70] = 0; }, /before input\/cursor\/render/],
        ['unsafe active GUI', 2, value => { value[128 + 97] = 4; }, /before input\/cursor\/render/],
        ['Hidden service loss', 1, value => { value[64 + 42] = 0; }, /Hidden before view retirement/],
        ['Hidden live borrower', 1, value => { value[64 + 50] = 1; }, /Hidden before view retirement/],
        ['Hidden false view retirement', 4, value => { value[64 + 47] = 0; }, /Hidden before view retirement/],
        ['Core generation regression', 8, value => { value[64 + 48] = 0; }, /Core view history moved backwards/],
        ['header generation regression', 8, value => { value[20] = 0; }, /moved backwards/]
    ];
    for (const [name, mutateAt, mutate, pattern] of malformed) test('frontend refuses ' + name + ' and requests retirement', () => {
        const options = { mutateAt, mutate, stopAt: 10,
            keys: poll => [3, 4, 5, 7, 8].includes(poll) ? [17, 16, 120] : [] };
        failedClean(execute(options), pattern);
    });
    test('frontend cursor provider PIN cannot disappear from the runtime ownership header', () => {
        failedClean(execute({ mutateAt: 2, mutate: values => { values[32] = 0; } }), /view sample/);
    });
    test('frontend retains the previous backend PIN while the next Show clears its old view publication', () => {
        const result = execute({ stopAt: 10, keys: poll => [3, 4, 5, 7, 8].includes(poll) ? [17, 16, 120] : [],
            mutateAt: 8, mutate: values => {
                // A fresh Showing sample legitimately has no new view yet.
                // Clearing that view cannot revoke the earlier provider PIN.
                values.fill(0, 128);
                Object.assign(values, { 1: 3, 11: 2, 12: 2, 14: 3, 15: 1, 24: 1, 32: 0 });
            } });
        failedClean(result, /moved backwards/);
    });
    test('frontend Failed terminal unload flags cannot replace the actual stopped Core service proof', () => {
        const result = execute({ mutateAt: 11, mutate: values => {
            values[1] = 8;
            values[36] = 5;
            values[37] = 3;
            // Preserve the independently asserted terminal unload flags but
            // return a stale Hidden Core with its real manager/XML still owned.
            Object.assign(values, { 65: 2, 69: 1, 70: 1, 78: 0, 80: 97, 97: 2,
                102: 1, 103: 1, 104: 1, 105: 1, 106: 1, 118: 6, 121: 0 });
        } });
        assert.equal(result.fixture.values[1], 8);
        assert.equal(result.fixture.values[41], 1);
        assert.equal(result.fixture.values[64 + 33], 2);
        assert.equal(result.finished?.passed, false);
        assert.equal(result.finished.cleanup_passed, false);
        assert.ok(result.error);
    });
    for (const [name, mutate] of [
        ['Core not stopped', values => { values[64 + 1] = 2; }],
        ['retained Core services', values => { values[64 + 38] = 1; }],
        ['wrong Core sample stage', values => { values[26] = 3; }],
        ['Core module not unloaded', values => { values[41] = 0; }],
        ['native scope not retired', values => { values[128 + 162] = 1; }]
    ]) test('frontend terminal refusal does not promote ' + name + ' to cleanup', () => {
        const result = execute({ mutateAt: 11, mutate });
        assert.equal(result.finished?.passed, false);
        assert.equal(result.finished.cleanup_passed, false);
        assert.match(result.finished.error, /Stopped before owner retirement/);
        assert.ok(result.error);
    });
    test('frontend missing terminal capture cannot reuse an earlier clean-looking Hidden report', () => {
        const result = execute({ captureZeroAt: [11] });
        assert.equal(result.finished?.passed, false);
        assert.equal(result.finished.cleanup_passed, false);
        assert.match(result.finished.error, /terminal evidence unavailable/);
    });
    test('frontend host expiry retires safely without claiming a requested successful Stop', () => {
        const result = execute({ heartbeatRejectAt: 3 });
        failedClean(result);
        assert.equal(result.finished.host_heartbeat_rejected, true);
        assert.equal(result.finished.stop_accepted, false);
        assert.equal(result.finished.values[1], 8);
        assert.equal(result.finished.values[36], 12);
        assert.equal(result.finished.values[37], 2);
        assert.deepEqual(result.fixture.commands.map(value => value.command), [1]);
    });
    test('frontend rejected Stop ceases heartbeats and waits for lease retirement', () => {
        const result = execute({ rejectStop: true, stopAt: 3 });
        failedClean(result);
        assert.equal(result.finished.stop_accepted, false);
        assert.equal(result.finished.host_heartbeat_rejected, true);
        assert.ok(result.events.some(value => value.event === 'frontend_runtime_stop_rejected'));
    });
    test('frontend RetainedCleanup never becomes falsely unloaded after a bounded script wait', () => {
        const result = execute({ retainedCleanup: true, stopAt: 3, timeout: 30 });
        assert.match(String(result.error), /Script execution timed out/);
        assert.equal(result.finished, undefined);
        assert.equal(result.fixture.values[1], 9);
        assert.equal(result.fixture.values[30], 1);
        assert.equal(result.fixture.values[31], 1);
        assert.equal(result.fixture.values[33], 1);
        assert.equal(result.fixture.values[34], 0);
        assert.equal(result.fixture.values[41], 0);
    });
    test('frontend legacy direct entry never looks up or publishes the optional local-input group', () => {
        const result = execute({ missingFunctions: ['DE_LocalInputRuntimeApiVersionV1', 'DE_LocalInputRuntimeStartV1',
            'DE_LocalInputRuntimeCaptureV1', 'DE_LocalInputRuntimeReadV1'] });
        clean(result);
        assert.ok(!result.fixture.trace.some(value => value.kind.startsWith('local')));
        assert.ok(!result.events.some(value => value.event === 'frontend_local_input_runtime_sample'));
        assert.equal(result.finished.local_input, false);
        assert.equal(result.finished.local_input_values, null);
        assert.equal(result.finished.local_input_samples, 0);
        assert.equal(result.finished.local_input_active_observed, false);
        assert.equal(result.finished.local_input_cleanup_passed, true);
    });
    test('frontend opted-in client keeps local Core32 separate and retires active hidden input before Stop', () => {
        const result = execute({ localInput: true, stopAt: 10,
            keys: poll => [3, 4, 5, 7, 8].includes(poll) ? [17, 16, 120] : [] });
        clean(result);
        assert.deepEqual(result.fixture.commands.map(value => value.command), [1, 2, 1, 3]);
        assert.equal(result.fixture.trace.filter(value => value.kind === 'localStart').length, 1);
        const started = result.events.find(value => value.event === 'frontend_runtime_started');
        assert.equal(started.local_input, true);
        assert.equal(started.local_input_api, 1);
        assert.equal(started.local_input_fields, 32);
        const local = result.events.filter(value => value.event === 'frontend_local_input_runtime_sample');
        assert.ok(local.some(value => value.values[1] === 3 && value.values[8] === 1 && value.values[15] === 1));
        assert.ok(local.some(value => value.values[1] === 4 && value.values[8] === 0));
        assert.ok(local.every(value => value.schema === 'mta_local_input_runtime_v1' && value.generation === 7 &&
            value.values.length === 32 && value.values[2] === 77 && !value.network_active &&
            !value.gameplay_ready && !value.full_mta_ready));
        assert.equal(result.finished.local_input, true);
        assert.equal(result.finished.local_input_active_observed, true);
        assert.equal(result.finished.local_input_cleanup_passed, true);
        assert.equal(result.finished.local_input_values[1], 1);
        assert.ok([3, 4, 8, 9, 20].every(field => result.finished.local_input_values[field] === 0));
        assert.equal(result.finished.local_input_values[5], 1);
        assert.equal(result.finished.local_input_values[27], 1);
        assert.equal(result.finished.local_input_values[18], 1);
        assert.equal(result.finished.local_input_values[21], 1);
        assert.ok(result.finished.local_input_samples >= local.length);
        for (const read of result.fixture.trace.filter(value => value.kind === 'localRead')) {
            const trace = result.fixture.trace;
            assert.ok(trace.findIndex(value => value.kind === 'poll' && value.polls === read.polls) < trace.indexOf(read));
            assert.ok(trace.findIndex(value => value.kind === 'localCapture' && value.polls === read.polls) < trace.indexOf(read));
        }
    });
    test('frontend local-input opt-in does not invent active gameplay when only the menu was shown', () => {
        const result = execute({ localInput: true });
        clean(result);
        assert.equal(result.finished.local_input_active_observed, false);
        assert.equal(result.finished.local_input_cleanup_passed, true);
        assert.ok(result.events.some(value => value.event === 'frontend_local_input_runtime_sample'));
    });
    test('frontend never-configured local input may retire Disabled without a synthetic neutral producer sample', () => {
        const result = execute({ localInput: true, localNeverConfigured: true });
        clean(result);
        assert.equal(result.finished.local_input_values[3], 0);
        assert.equal(result.finished.local_input_values[18], 0);
        assert.equal(result.finished.local_input_cleanup_passed, true);
    });
    test('frontend local-input retirement preserves its successful native attempt and revoked historical token', () => {
        const result = execute({ localInput: true });
        clean(result);
        const values = result.finished.local_input_values;
        assert.equal(values[5], 1);
        assert.equal(values[6], 701);
        assert.equal(values[7], 0);
        assert.equal(values[10], 1);
        assert.ok(values[11] > 0);
        assert.ok(values[27] > 0);
        assert.equal(values[18], 1);
        assert.equal(values[21], 1);
        assert.ok([8, 9, 20].every(field => values[field] === 0));
        assert.equal(result.finished.local_input_cleanup_passed, true);
    });
    test('frontend qualified provider Reset may retire an attempted local Begin without inventing a native token', () => {
        const result = execute({ localInput: true, mutateLocalAt: 11, mutateLocal: values => {
            Object.assign(values, { 5: 1, 6: 0, 7: 0, 10: 0, 11: 0, 18: 1, 21: 1, 27: 3 });
        } });
        clean(result);
        const values = result.finished.local_input_values;
        assert.equal(values[5], 1);
        assert.equal(values[6], 0);
        assert.equal(values[7], 0);
        assert.equal(values[10], 0);
        assert.equal(values[11], 0);
        assert.equal(values[27], 3);
        assert.equal(result.finished.local_input_cleanup_passed, true);
    });
    test('frontend local-input API mismatch rejects before starting either runtime variant', () => {
        const result = execute({ localInput: true, localApiVersion: 2 });
        assert.match(String(result.error), /local-input runtime API1/);
        assert.equal(result.fixture.polls, 0);
        assert.ok(!result.fixture.trace.some(value => value.kind === 'localStart'));
        assert.equal(result.finished, undefined);
    });
    for (const name of ['DE_LocalInputRuntimeApiVersionV1', 'DE_LocalInputRuntimeStartV1',
        'DE_LocalInputRuntimeCaptureV1', 'DE_LocalInputRuntimeReadV1'])
        test('frontend local-input opt-in refuses missing optional export ' + name + ' before admission', () => {
            const result = execute({ localInput: true, missingFunctions: [name] });
            assert.ok(result.error);
            assert.equal(result.fixture.polls, 0);
            assert.ok(!result.fixture.trace.some(value => value.kind === 'localStart'));
            assert.equal(result.finished, undefined);
        });
    test('frontend local-input failed Start does not fall back to the menu-only owner', () => {
        const result = execute({ localInput: true, localStartResult: 0 });
        assert.match(String(result.error), /could not start/);
        assert.equal(result.fixture.trace.filter(value => value.kind === 'localStart').length, 1);
        assert.equal(result.fixture.polls, 0);
        assert.equal(result.finished, undefined);
    });
    for (const [name, mutate, pattern] of [
        ['malformed unsigned word', values => { values[28] = -1; }, /local-input runtime report/],
        ['unknown failure code', values => { values[22] = 12; }, /local-input runtime report/],
        ['replaced Core owner', values => { values[2] = 78; }, /replaced its retained owner/],
        ['unqualified active lease', values => { values[8] = 1; }, /unqualified active input/],
        ['cleanup owner erased', values => { values[1] = 5; values[21] = 0; }, /pending cleanup owner/]
    ]) test('frontend local-input ' + name + ' requests Stop and keeps polling until real retirement', () => {
        const result = execute({ localInput: true, mutateLocalAt: 2, mutateLocal: mutate });
        failedClean(result, pattern);
        assert.ok(result.fixture.trace.some(value => value.kind === 'poll' && value.polls > 2));
        assert.equal(result.finished.local_input_cleanup_passed, true);
        assert.equal(result.finished.local_input_values[1], 1);
        assert.deepEqual(result.fixture.commands.map(value => value.command), [1, 3]);
    });
    for (const [name, options, pattern] of [
        ['invalid capture status', { localCaptureStatusAt: 2 }, /local-input capture status/],
        ['capture exception', { localCaptureThrowAt: 2 }, /owned local capture failure/],
        ['read exception', { localReadThrowAt: 2 }, /owned local read failure/]
    ]) test('frontend local-input ' + name + ' does not unwind a live runtime owner', () => {
        const result = execute({ localInput: true, ...options });
        failedClean(result, pattern);
        assert.ok(result.fixture.polls > 2);
        assert.equal(result.finished.local_input_cleanup_passed, true);
    });
    test('frontend local-input transient capture refusal preserves the last valid copied cache', () => {
        const result = execute({ localInput: true, localCaptureZeroAt: [2, 4, 6, 8] });
        clean(result);
        assert.ok(result.finished.local_input_samples > 0);
        assert.ok(result.fixture.trace.filter(value => value.kind === 'localCapture').length > result.finished.local_input_samples);
    });
    test('frontend local-input owner must match the retained Core even on its first publication', () => {
        const result = execute({ localInput: true, mutateLocalAt: 1, mutateLocal: values => { values[2] = 78; } });
        failedClean(result, /replaced its retained owner/);
        assert.deepEqual(result.fixture.commands.map(value => value.command), [3]);
    });
    for (const [name, mutate] of [
        ['Suspended is not Disabled', values => { values[1] = 4; }],
        ['configuration retained', values => { values[3] = 1; }],
        ['enablement retained', values => { values[4] = 1; }],
        ['attempted cleanup without End or Reset attempt', values => { values[27] = 0; }],
        ['historical native token not revoked', values => { values[10] = 0; }],
        ['historical native token lacks retirement reason', values => { values[11] = 0; }],
        ['zero-token attempted cleanup without End or Reset attempt', values => {
            Object.assign(values, { 6: 0, 7: 0, 10: 0, 11: 0, 27: 0 });
        }],
        ['native binding retained', values => { values[9] = 1; }],
        ['cleanup still pending', values => { values[20] = 1; values[21] = 0; }],
        ['cleanup incomplete', values => { values[21] = 0; }],
        ['configured canonical frame not neutral', values => { values[18] = 0; }]
    ]) test('frontend local-input terminal ' + name + ' cannot borrow clean Runtime416 unload flags', () => {
        const result = execute({ localInput: true, mutateLocalAt: 11, mutateLocal: mutate });
        assert.equal(result.finished?.passed, false);
        assert.equal(result.finished.cleanup_passed, false);
        assert.equal(result.finished.local_input_cleanup_passed, false);
        assert.equal(result.finished.values[41], 1);
        assert.ok(result.error);
    });
    test('frontend missing final local-input cache cannot reuse its previous Suspended sample', () => {
        const result = execute({ localInput: true, localCaptureZeroAt: [11] });
        assert.equal(result.finished?.passed, false);
        assert.equal(result.finished.cleanup_passed, false);
        assert.equal(result.finished.local_input_cleanup_passed, false);
        assert.match(result.finished.error, /local-input terminal evidence unavailable/);
        assert.ok(result.error);
    });
    test('frontend local retained cleanup keeps polling without publishing a final safe unload', () => {
        const result = execute({ localInput: true, retainedCleanup: true, stopAt: 3, timeout: 30 });
        assert.match(String(result.error), /Script execution timed out/);
        assert.equal(result.finished, undefined);
        assert.equal(result.fixture.values[1], 9);
        assert.equal(result.fixture.localValues[1], 5);
        assert.equal(result.fixture.localValues[20], 1);
        assert.equal(result.fixture.localValues[21], 0);
    });
    test('frontend full entry activates local input only after F9 release and uses no clock or transport route', () => {
        const fixture = protocolFixture();
        const result = runEntry({ mtaCoreFrontendRuntime: true, functions: fixture.functions,
            keyFrames: [[17, 16, 120], ...Array.from({ length: 9 }, () => []), [17, 16, 35], []] });
        const finished = result.logs.find(value => value.event === 'frontend_runtime_finished');
        assert.equal(finished?.passed, true);
        assert.ok(!result.logs.some(value => value.event === 'adapter_failed'));
        assert.equal(result.calls.filter(value => value[0] === 'DE_LocalInputRuntimeStartV1').length, 1);
        assert.equal(result.calls.filter(value => value[0] === 'DE_FrontendRuntimeStartV1').length, 0);
        assert.equal(result.calls.filter(value => value[0] === 'DE_LocalInputRuntimeApiVersionV1').length, 1);
        assert.equal(finished.local_input, true);
        assert.equal(finished.local_input_cleanup_passed, true);
        assert.equal(finished.local_input_active_observed, false);
        assert.equal(finished.local_input_values.length, 32);
        assert.ok(result.logs.some(value => value.event === 'frontend_local_input_runtime_sample'));
        assert.ok(result.runtimeAccess.every(value => value.frame >= 1));
        assert.ok(!result.calls.some(value => /Clock|DE_ConnectLocal|DE_Controller|DE_CoreStartupBegin|CREATE_CHAR/.test(value[0])));
        assert.deepEqual(fixture.commands.map(value => value.command), [1, 3]);
    });
    for (const overrides of [{ mtaCoreFrontendRuntime: 'yes' }, { mtaCoreFrontendRuntime: true, mtaCoreClockReference: true },
        { mode: 'online', mtaCoreFrontendRuntime: true }]) test('frontend full entry rejects invalid runtime selection before bridge lookup ' + JSON.stringify(overrides), () => {
        const result = runEntry(overrides);
        assert.equal(result.runtimeAccess.length, 0);
        assert.ok(result.logs.some(value => value.event === 'adapter_failed'));
    });
    test('frontend full entry cancelled while armed never starts a runtime owner', () => {
        const fixture = protocolFixture();
        const result = runEntry({ mtaCoreFrontendRuntime: true, functions: fixture.functions, keyFrames: [[17, 16, 35]] });
        assert.equal(result.runtimeAccess.length, 0);
        assert.equal(result.logs.at(-1).event, 'adapter_activation_cancelled');
        assert.equal(fixture.polls, 0);
    });
    test('frontend full entry rejects an incompatible runtime API before Start', () => {
        const fixture = protocolFixture({ apiVersion: 2 });
        const result = runEntry({ mtaCoreFrontendRuntime: true, functions: fixture.functions, keyFrames: [[17, 16, 120], []] });
        assert.equal(fixture.polls, 0);
        assert.ok(!result.calls.some(value => value[0] === 'DE_FrontendRuntimeStartV1'));
        assert.ok(!result.calls.some(value => value[0] === 'DE_LocalInputRuntimeStartV1'));
        assert.match(result.logs.at(-1).error, /runtime API1/);
    });
    test('frontend main runtime selection refuses incompatible local API without starting the menu-only variant', () => {
        const fixture = protocolFixture({ localApiVersion: 2 });
        const result = runEntry({ mtaCoreFrontendRuntime: true, functions: fixture.functions,
            keyFrames: [[17, 16, 120], []] });
        assert.equal(fixture.polls, 0);
        assert.ok(!result.calls.some(value => /(?:Frontend|LocalInput)RuntimeStartV1/.test(value[0])));
        assert.match(result.logs.at(-1).error, /local-input runtime API1/);
        assert.ok(!result.logs.some(value => value.event === 'frontend_runtime_started'));
    });
    for (const missing of ['DE_LocalInputRuntimeApiVersionV1', 'DE_LocalInputRuntimeStartV1',
        'DE_LocalInputRuntimeCaptureV1', 'DE_LocalInputRuntimeReadV1'])
        test('frontend main runtime selection rejects absent local service ' + missing, () => {
            const fixture = protocolFixture({ missingFunctions: [missing] });
            const result = runEntry({ mtaCoreFrontendRuntime: true, functions: fixture.functions,
                keyFrames: [[17, 16, 120], []] });
            assert.equal(fixture.polls, 0);
            assert.ok(!result.calls.some(value => /(?:Frontend|LocalInput)RuntimeStartV1/.test(value[0])));
            assert.equal(result.logs.at(-1).event, 'adapter_failed');
            assert.match(result.logs.at(-1).error, new RegExp(missing));
            assert.ok(result.runtimeAccess.every(value => value.frame >= 1));
        });
};
