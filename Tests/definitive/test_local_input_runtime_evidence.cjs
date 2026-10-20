// SPDX-License-Identifier: GPL-3.0-or-later
// Controlled copied events exercise the verifier; no installed game is run.
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');
const { verifyEvents, validatorSource, maximumEvents } = require('./local_input_runtime_evidence.cjs');

// Reuse the existing copied-worker fixture without importing its test runner or
// adding an injection seam to the shipped script. Its VM executes the real JS.
const fixturePath = path.join(__dirname, 'test_frontend_runtime_entry.cjs');
const fixtureSource = fs.readFileSync(fixturePath, 'utf8');
const fixtureEnd = fixtureSource.indexOf('\nmodule.exports =');
assert.ok(fixtureEnd > 0);
const { execute } = vm.runInNewContext(fixtureSource.slice(0, fixtureEnd) + '\n({ execute });',
    { require, __dirname }, { timeout: 1000 });
const copy = value => JSON.parse(JSON.stringify(value));
const runtimeSample = event => event.event === 'frontend_runtime_sample';
const localSample = event => event.event === 'frontend_local_input_runtime_sample';
const finished = events => events.find(event => event.event === 'frontend_runtime_finished');
const terminalLocalCopies = events => [...events.filter(localSample).slice(-1).map(event => event.values), finished(events).local_input_values];
const firstActive = events => events.find(event => localSample(event) && event.values[8]);
const secondVisible = events => events.find(event => runtimeSample(event) && event.values[1] === 4 && event.values[20] === 2);
const validRun = execute({ localInput: true, stopAt: 14,
    keys: poll => [3, 4, 9, 10].includes(poll) ? [17, 16, 120] : [],
    // This actual copied-state change forces a sample after the first Hidden
    // report. A renewal-only counter may be rate limited by the real script.
    mutateLocalEvery: (values, { polls, phase, viewGeneration }) => {
        if (phase === 2 && polls === 6) values[15] = 2;
        if (phase === 4 && viewGeneration === 2)
            Object.assign(values, { 5: 1, 6: 701, 10: 1, 11: 5, 13: 8, 27: 1 });
    }
});
assert.equal(validRun.error, null);
assert.equal(validRun.finished.passed, true);
const validEvents = copy(validRun.events);
let tests = 0;
const test = (name, run) => { run(); ++tests; console.log('PASS ' + name); };
const rejectedWorkload = events => {
    const result = verifyEvents(events);
    assert.equal(result.protocolPassed, false);
    assert.equal(result.cleanupPassed, true, result.cleanupErrors.join('; '));
    assert.ok(result.protocolErrors.length);
    return result;
};

test('copied verifier qualifies two real acknowledged views, held renewable local input and independent cleanup', () => {
    const result = verifyEvents(validEvents);
    assert.equal(result.protocolPassed, true, result.protocolErrors.join('; '));
    assert.equal(result.cleanupPassed, true, result.cleanupErrors.join('; '));
    assert.deepEqual(result.summary.acknowledgedViewGenerations, ['0:1', '0:2']);
    assert.deepEqual(result.summary.retiredViewGenerations, ['0:1', '0:2']);
    assert.equal(result.summary.heldRenewedLocalInputBetweenViews, true);
    assert.equal(result.summary.localRetiredBeforeSecondVisible, true);
    assert.equal(result.summary.ownerThread, 77);
    assert.equal(result.summary.generation, 7);
    assert.equal(result.summary.terminalNativeAttempted, 1);
    assert.equal(result.summary.fullMtaReady, false);
});
test('copied verifier reconstructs saved-binding validator scope across reload and Apply', () => {
    const run = execute({ localInput: true, bindingPersistence: true, stopAt: 14,
        keys: poll => [3, 4, 9, 10].includes(poll) ? [17, 16, 120] : [],
        bindingCount: poll => poll >= 6 ? 3 : 2,
        mutateLocalEvery: (values, { polls, phase, viewGeneration }) => {
            if (phase === 2 && polls === 6) values[15] = 2;
            if (phase === 4 && viewGeneration === 2)
                Object.assign(values, { 5: 1, 6: 701, 10: 1, 11: 5, 13: 8, 27: 1 });
        }
    });
    assert.equal(run.error, null);
    const result = verifyEvents(copy(run.events));
    assert.equal(result.protocolPassed, true, result.protocolErrors.join('; '));
    assert.equal(result.cleanupPassed, true, result.cleanupErrors.join('; '));
    assert.equal(result.summary.bindingPersistence, true);
    const missingSelection = copy(run.events);
    delete missingSelection.find(event => event.event === 'frontend_runtime_started').binding_persistence;
    assert.equal(verifyEvents(missingSelection).protocolPassed, false);
});
test('copied verifier rejects malformed saved-binding selection without changing terminal cleanup', () => {
    const events = copy(validEvents);
    events.find(event => event.event === 'frontend_runtime_started').binding_persistence = 'true';
    rejectedWorkload(events);
});
test('separate caches do not turn an Active event before observed Hidden into a simultaneous-view assertion', () => {
    const result = verifyEvents(validEvents);
    assert.equal(result.protocolPassed, true);
    assert.ok(result.summary.activeLocalSamplesOutsideQualifiedInterval > 0);
    assert.equal(result.summary.nativeHookExternallyVerified, false);
});
test('verifier pins both extracted production validators and never mutates copied input', () => {
    const before = JSON.stringify(validEvents);
    const result = verifyEvents(validEvents, { expectedIndexSha256: validatorSource.sha256 });
    assert.equal(result.protocolPassed, true);
    assert.equal(JSON.stringify(validEvents), before);
    assert.match(result.validatorSource.sha256, /^[a-f0-9]{64}$/);
    assert.match(result.validatorSource.localValidatorSha256, /^[a-f0-9]{64}$/);
    assert.match(result.validatorSource.runtimeValidatorsSha256, /^[a-f0-9]{64}$/);
});
test('verifier refuses a mismatched production source pin', () => {
    const result = verifyEvents(validEvents, { expectedIndexSha256: '0'.repeat(64) });
    assert.equal(result.protocolPassed, false);
    assert.equal(result.cleanupPassed, false);
});
for (const [name, mutate] of [
    ['malformed32', events => { firstActive(events).values[28] = -1; }],
    ['missing native active evidence', events => {
        for (const event of events.filter(localSample)) if (event.values[8]) {
            Object.assign(event.values, { 1: 2, 5: 0, 6: 0, 8: 0, 9: 0, 13: 0, 15: 0, 18: 1, 21: 1 });
        }
    }],
    ['missing held actions', events => { for (const event of events.filter(localSample)) event.values[15] = event.values[16] = 0; }],
    ['missing explicit renewal', events => { for (const event of events.filter(localSample)) if (event.values[8]) event.values[13] = event.values[14] = 0; }],
    ['missing second acknowledged Show', events => { events.splice(events.indexOf(secondVisible(events)), 1); }],
    ['second Visible without Show acknowledgement', events => { secondVisible(events).values[12] = 2; }],
    ['second Show without accepted command evidence', events => {
        const index = events.findIndex(event => event.event === 'frontend_runtime_command' && event.command === 1 && event.sequence === 3);
        events.splice(index, 1);
    }],
    ['mixed runtime generation', events => { firstActive(events).generation = 8; }],
    ['foreign local owner', events => { firstActive(events).values[2] = 78; }],
    ['Runtime416 reserved word', events => { secondVisible(events).values[63] = 1; }],
    ['stale Runtime416 publication', events => { secondVisible(events).values[2] = 0; }],
    ['native historical failure', events => { firstActive(events).values[22] = 9; }]
]) test('copied verifier keeps workload failed but terminal cleanup separate for ' + name, () => {
    const events = copy(validEvents); mutate(events);
    rejectedWorkload(events);
});
test('no active sample outside the observed hidden interval can replace held input between views', () => {
    const events = copy(validEvents);
    const hiddenIndex = events.findIndex(event => runtimeSample(event) && event.values[1] === 2 && event.values[20] === 1);
    const nextVisible = events.indexOf(secondVisible(events));
    for (let index = nextVisible - 1; index > hiddenIndex; --index)
        if (localSample(events[index]) && events[index].values[8]) events.splice(index, 1);
    const result = rejectedWorkload(events);
    assert.equal(result.summary.heldRenewedLocalInputBetweenViews, false);
});
test('worker Showing may precede local cleanup while actual Visible admission must follow retirement', () => {
    const events = copy(validEvents);
    const visible = secondVisible(events);
    const showing = copy(visible);
    showing.at_ms -= 25;
    Object.assign(showing.values, { 1: 3, 2: 9, 11: 2, 12: 2, 14: 3, 15: 1, 24: 1 });
    showing.values.fill(0, 128);
    const cleanupIndex = events.findIndex(event => localSample(event) && event.at_ms === visible.at_ms && event.values[1] === 4);
    assert.ok(cleanupIndex >= 0);
    events.splice(cleanupIndex, 0, showing);
    const result = verifyEvents(events);
    assert.equal(result.protocolPassed, true, result.protocolErrors.join('; '));
    assert.equal(result.cleanupPassed, true);
});
test('missing local retirement before the second view is not supplied by final Stop cleanup', () => {
    const events = copy(validEvents), visible = secondVisible(events);
    const cleanupIndex = events.findIndex(event => localSample(event) && event.at_ms === visible.at_ms && event.values[1] === 4);
    assert.ok(cleanupIndex >= 0); events.splice(cleanupIndex, 1);
    const result = rejectedWorkload(events);
    assert.equal(result.summary.localRetiredBeforeSecondVisible, false);
});
for (const [name, mutate] of [
    ['never-attempted zero cache', values => { Object.assign(values, { 5: 0, 6: 0, 7: 0, 10: 0, 11: 0, 27: 0 }); }],
    ['another native generation', values => { values[6] = 702; }]
]) test('pre-view cleanup from ' + name + ' cannot retire the observed active lease', () => {
    const events = copy(validEvents), visible = secondVisible(events);
    const cleanup = events.find(event => localSample(event) && event.at_ms === visible.at_ms && event.values[1] === 4);
    assert.ok(cleanup); mutate(cleanup.values);
    const result = rejectedWorkload(events);
    assert.equal(result.summary.localRetiredBeforeSecondVisible, false);
});
test('historical Attempted and a revoked token survive qualified terminal native cleanup', () => {
    const result = verifyEvents(validEvents);
    assert.equal(result.cleanupPassed, true);
    assert.equal(result.summary.terminalNativeAttempted, 1);
    assert.equal(result.summary.terminalNativeGeneration, '0:701');
    assert.ok(result.summary.terminalEndAttempts > 0);
});
test('actual provider Reset can retire a tokenless attempted Begin without an invented revoke reason', () => {
    const events = copy(validEvents);
    for (const values of terminalLocalCopies(events)) Object.assign(values, { 6: 0, 7: 0, 10: 0, 11: 0, 27: 3 });
    const result = verifyEvents(events);
    assert.equal(result.protocolPassed, true, result.protocolErrors.join('; '));
    assert.equal(result.cleanupPassed, true);
    assert.equal(result.summary.terminalNativeAttempted, 1);
    assert.equal(result.summary.terminalNativeGeneration, '0:0');
});
for (const [name, mutate] of [
    ['retained native owner', events => {
        for (const values of terminalLocalCopies(events)) Object.assign(values, { 1: 5, 9: 1, 20: 1, 21: 0 });
    }],
    ['non-neutral canonical terminal', events => { for (const values of terminalLocalCopies(events)) values[18] = 0; }],
    ['no End or Reset attempt', events => { for (const values of terminalLocalCopies(events)) values[27] = 0; }],
    ['unrevoked historical token', events => { for (const values of terminalLocalCopies(events)) values[10] = 0; }],
    ['foreign final owner', events => { for (const values of terminalLocalCopies(events)) values[2] = 78; }],
    ['mixed terminal Core session', events => { finished(events).values[99] = 902; }],
    ['stale final local cache', events => { finished(events).local_input_values[27] = 2; }],
    ['native view still retained', events => { finished(events).values[128 + 162] = 1; }]
]) test('copied verifier refuses independent cleanup for ' + name, () => {
    const events = copy(validEvents); mutate(events);
    const result = verifyEvents(events);
    assert.equal(result.protocolPassed, false);
    assert.equal(result.cleanupPassed, false);
    assert.ok(result.cleanupErrors.length);
});
test('a script workload error remains failed despite independently clean final native/Core ownership', () => {
    const events = copy(validEvents);
    Object.assign(finished(events), { passed: false, error: 'owned telemetry failure' });
    const result = rejectedWorkload(events);
    assert.ok(result.protocolErrors.some(error => /successful requested/.test(error)));
});
test('a failed terminal label may prove cleanup but never a successful requested workload', () => {
    const events = copy(validEvents);
    // Both copies of this same final publication must agree; only the label,
    // failure and reason change, not the actually stopped Core/native owners.
    for (const event of events.filter(event => runtimeSample(event) && event.values[1] === 7 || event.event === 'frontend_runtime_finished'))
        Object.assign(event.values, { 1: 8, 36: 5, 37: 3 });
    finished(events).passed = false;
    const result = rejectedWorkload(events);
    assert.equal(result.summary.terminalRuntimePhase, 8);
});
for (const [name, mutate] of [
    ['missing start', events => events.splice(events.findIndex(event => event.event === 'frontend_runtime_started'), 1)],
    ['duplicate start', events => events.push(copy(events.find(event => event.event === 'frontend_runtime_started')))],
    ['missing finish', events => events.splice(events.findIndex(event => event.event === 'frontend_runtime_finished'), 1)],
    ['duplicate finish', events => events.push(copy(finished(events)))],
    ['mixed final runtime generation', events => { finished(events).generation = 8; }]
]) test('copied verifier rejects lifecycle identity ambiguity from ' + name, () => {
    const events = copy(validEvents); mutate(events);
    const result = verifyEvents(events);
    assert.equal(result.protocolPassed, false);
    assert.equal(result.cleanupPassed, false);
});
test('closed-menu counters and caller human text never become Quit or physical-key proof', () => {
    const events = copy(validEvents);
    finished(events).humanQuitClicked = true;
    finished(events).humanReport = 'Everything worked';
    const result = verifyEvents(events);
    assert.equal(result.protocolPassed, true);
    assert.equal(result.summary.menuCloseObserved, true);
    assert.equal(result.summary.quitCallbackVerified, false);
    assert.equal(result.summary.physicalKeyVerified, false);
    assert.equal(result.summary.humanResultInferred, false);
});
test('copied verifier rejects an input larger than its bounded event contract', () => {
    const result = verifyEvents(Array(maximumEvents + 1));
    assert.equal(result.protocolPassed, false);
    assert.equal(result.cleanupPassed, false);
});
console.log('Passed ' + tests + ' local-input copied-evidence tests. No installed game was run.');
