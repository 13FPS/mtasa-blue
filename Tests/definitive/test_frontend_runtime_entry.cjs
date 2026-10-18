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
    const first = source.indexOf('function runFrontendRuntime(');
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
    let polls = 0, phase = 2, viewGeneration = 0, viewsRetired = 0, showAcks = 0, hideAcks = 0;
    let accepted = 0, acceptedCommand = 0, acknowledged = 0, acknowledgedCommand = 0;
    let pending = null, stage = 1, coreSamples = 1, viewSamples = 0, guiHides = 0, focusHides = 0;
    let lastCoreGeneration = 0, lastCoreRetired = 0, backendPinned = false, hostExpired = false;
    let snapshot = null, captured = null, mutated = false, heartbeatRejected = false;
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
    };
    const functions = {
        DE_FrontendRuntimeApiVersionV1: () => options.apiVersion ?? 1,
        DE_FrontendRuntimeStartV1: () => options.startResult ?? generation,
        DE_FrontendRuntimeCommandV1: (session, sequence, command) => {
            trace.push({ kind: 'command', polls, session, sequence, command });
            commands.push({ session, sequence, command });
            if (session !== generation || sequence <= accepted || command < 1 || command > 3) return 0;
            if (command === 3 && options.rejectStop) { heartbeatRejected = true; return 0; }
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
            trace.push({ kind: 'capture', polls });
            if (options.captureZeroAt?.includes(polls)) return 0;
            captured = snapshot.slice();
            return 1;
        },
        DE_FrontendRuntimeReadV1: field => captured[field]
    };
    return { functions, commands, trace, generation, get polls() { return polls; },
        get values() { return snapshot; } };
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
        vm.runInNewContext(runtimeSource() + '\nrunFrontendRuntime(__bind, __chord, "Ctrl+Shift+End");', context,
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
    test('frontend full entry activates only after F9 release and uses no clock or transport route', () => {
        const fixture = protocolFixture();
        const result = runEntry({ mtaCoreFrontendRuntime: true, functions: fixture.functions,
            keyFrames: [[17, 16, 120], ...Array.from({ length: 9 }, () => []), [17, 16, 35], []] });
        const finished = result.logs.find(value => value.event === 'frontend_runtime_finished');
        assert.equal(finished?.passed, true);
        assert.ok(!result.logs.some(value => value.event === 'adapter_failed'));
        assert.equal(result.calls.filter(value => value[0] === 'DE_FrontendRuntimeStartV1').length, 1);
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
        assert.match(result.logs.at(-1).error, /runtime API1/);
    });
};
