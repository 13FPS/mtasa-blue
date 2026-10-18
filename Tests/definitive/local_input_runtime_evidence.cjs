// SPDX-License-Identifier: GPL-3.0-or-later
// This verifier consumes copied script events only. Process identity, raw-log
// preservation and human observations belong to the surrounding capture tool.
const fs = require('node:fs');
const path = require('node:path');
const crypto = require('node:crypto');
const vm = require('node:vm');

const indexPath = path.resolve(__dirname, '../../Client/definitive/game-adapter/index.js');
const indexBytes = fs.readFileSync(indexPath);
const indexSource = indexBytes.toString('utf8');
const sha256 = bytes => crypto.createHash('sha256').update(bytes).digest('hex');
const localStart = indexSource.indexOf('function validateMtaLocalInputRuntimeSample(');
const runtimeStart = indexSource.indexOf('\nfunction runFrontendRuntime(', localStart);
const validatorsStart = indexSource.indexOf("    const schema = 'mta_frontend_runtime_v1';", runtimeStart);
const validatorsEnd = indexSource.indexOf("    enterPhase('frontend_runtime');", validatorsStart);
if (localStart < 0 || runtimeStart <= localStart || validatorsStart <= runtimeStart || validatorsEnd <= validatorsStart)
    throw new Error('Cannot locate the pinned production local-input/runtime validators');
const localSource = indexSource.slice(localStart, runtimeStart);
const runtimeValidatorsSource = indexSource.slice(validatorsStart, validatorsEnd);
// Execute only the schema helpers. Loading this module never enters the script,
// resolves a DLL, samples a physical key or accesses an installed game.
const validators = vm.runInNewContext(localSource + '\n(() => {\n' + runtimeValidatorsSource +
    '\nreturn { local: validateMtaLocalInputRuntimeSample, runtime: validate, retiredCore, retiredView };\n})()',
    Object.create(null), { timeout: 1000 });
const validatorSource = Object.freeze({ path: indexPath, bytes: indexBytes.length, sha256: sha256(indexBytes),
    localValidatorSha256: sha256(localSource), runtimeValidatorsSha256: sha256(runtimeValidatorsSource) });
const maximumEvents = 100000;
const runtimeSchema = 'mta_frontend_runtime_v1', localSchema = 'mta_local_input_runtime_v1';
const pair = (values, field) => values[field + 1] + ':' + values[field];
const nonzeroPair = (values, field) => Boolean(values[field] || values[field + 1]);
const unsigned = value => Number.isInteger(value) && value >= 0 && value <= 0xffffffff;
const record = value => value !== null && typeof value === 'object' && !Array.isArray(value);
const copiedWords = (values, length) => {
    if (!Array.isArray(values) || values.length !== length ||
        Array.from({ length }, (_, field) => field).some(field => !Object.hasOwn(values, field)))
        throw new Error('Expected a dense copied ' + length + '-word record');
    return values.slice();
};
const localRetired = values => [1, 4, 6].includes(values[1]) &&
    [4, 8, 9, 20].every(field => values[field] === 0) && values[18] === 1 && values[21] === 1;
const localDisabled = values => values[1] === 1 && [3, 4, 8, 9, 20].every(field => values[field] === 0) &&
    values[18] === 1 && values[21] === 1;

function verifyEvents(events, options = {}) {
    const protocolErrors = [], cleanupErrors = [];
    const protocolError = message => protocolErrors.push(message);
    const cleanupError = message => cleanupErrors.push(message);
    const summary = { eventCount: Array.isArray(events) ? events.length : 0, generation: null, ownerThread: null,
        runtimeSamples: 0, localSamples: 0, acknowledgedViewGenerations: [], retiredViewGenerations: [],
        heldRenewedLocalInputBetweenViews: false, localRetiredBeforeSecondVisible: false,
        activeLocalSamplesOutsideQualifiedInterval: 0, terminalNativeAttempted: null,
        terminalNativeGeneration: null, terminalEndAttempts: null, terminalRuntimePhase: null,
        menuCloseObserved: false, quitCallbackVerified: false, physicalKeyVerified: false,
        nativeHookExternallyVerified: false, humanResultInferred: false, fullMtaReady: false };
    const finish = () => ({ schema: 'mta_local_input_runtime_copied_event_verification_v1',
        protocolPassed: protocolErrors.length === 0, cleanupPassed: cleanupErrors.length === 0,
        protocolErrors, cleanupErrors, errors: { protocol: protocolErrors, cleanup: cleanupErrors },
        summary, validatorSource, limitations: [
            'Copied script evidence only; process, image, module and raw-prefix identity require the capture tool.',
            'Separate local and view caches are ordered observations, not simultaneous snapshots.',
            'GUI close counters identify closing, including Escape; they do not prove a Quit click or a physical key.',
            'No human, networking, remote-input, full CGame or full MTA readiness is inferred.'
        ] });
    if (!Array.isArray(events) || events.length > maximumEvents) {
        protocolError('Expected an event array bounded by ' + maximumEvents);
        cleanupError('No qualified terminal event array');
        return finish();
    }
    if (options.expectedIndexSha256 && options.expectedIndexSha256.toLowerCase() !== validatorSource.sha256) {
        protocolError('Pinned production index SHA256 does not match the requested source');
        cleanupError('Terminal validators are not bound to the requested source');
        return finish();
    }
    const starts = events.map((value, index) => ({ value, index })).filter(row => row.value?.event === 'frontend_runtime_started');
    const finishes = events.map((value, index) => ({ value, index })).filter(row => row.value?.event === 'frontend_runtime_finished');
    if (starts.length !== 1) protocolError('Expected exactly one runtime started event');
    if (finishes.length !== 1) protocolError('Expected exactly one runtime finished event');
    const start = starts.length === 1 ? starts[0] : null, ended = finishes.length === 1 ? finishes[0] : null;
    const generation = start?.value.generation;
    summary.generation = unsigned(generation) && generation !== 0 ? generation : null;
    if (!start || start.value.schema !== runtimeSchema || !summary.generation || start.value.fields !== 416 ||
        start.value.core_fields !== 64 || start.value.view_fields !== 288 || start.value.local_input !== true ||
        start.value.local_input_api !== 1 || start.value.local_input_fields !== 32)
        protocolError('Runtime start does not select API1 local Core32 with Runtime416');
    if (start && ended && ended.index <= start.index) protocolError('Runtime finished before it started');
    if (!ended || ended.value.schema !== runtimeSchema || ended.value.generation !== summary.generation || ended.value.local_input !== true)
        protocolError('Runtime finish is absent, mixed-generation or not local-input enabled');
    const runtimeRows = [], localRows = [], commands = [];
    let previousRuntime = null, expectedOwner = null, expectedSession = null, expectedScriptOwner = null, previousTime = null;
    const relevant = new Set(['frontend_runtime_started', 'frontend_runtime_finished', 'frontend_runtime_sample',
        'frontend_local_input_runtime_sample', 'frontend_runtime_command', 'frontend_runtime_stop_requested',
        'frontend_runtime_stop_rejected', 'frontend_runtime_host_heartbeat_rejected', 'frontend_runtime_hotkey']);
    for (let index = 0; index < events.length; ++index) {
        const event = events[index];
        if (!record(event)) { protocolError('Non-object copied event at ' + index); continue; }
        if (event.event === 'adapter_failed' && start && index > start.index) protocolError('Script recorded adapter failure');
        if (!relevant.has(event.event)) continue;
        if (!start || index < start.index || (ended && index > ended.index)) protocolError('Runtime event outside its single lifecycle at ' + index);
        if (event.generation !== summary.generation) { protocolError('Mixed runtime generation at event ' + index); continue; }
        if (['network_active', 'gameplay_ready', 'full_mta_ready', 'clock_started'].some(field =>
            Object.hasOwn(event, field) && event[field] !== false)) protocolError('Unexpected promoted runtime capability at event ' + index);
        if (Object.hasOwn(event, 'at_ms')) {
            if (!Number.isSafeInteger(event.at_ms) || event.at_ms < 0 || (previousTime !== null && event.at_ms < previousTime))
                protocolError('Invalid or regressing copied event time at ' + index);
            else previousTime = event.at_ms;
        }
        if (event.event === 'frontend_runtime_stop_rejected' || event.event === 'frontend_runtime_host_heartbeat_rejected')
            protocolError('Runtime recorded rejected stop or lost host heartbeat');
        if (event.event === 'frontend_runtime_command') {
            if (!unsigned(event.sequence) || !event.sequence || ![1, 2].includes(event.command) ||
                (commands.length && event.sequence <= commands[commands.length - 1].sequence))
                protocolError('Malformed accepted view command at ' + index);
            else commands.push({ index, sequence: event.sequence, command: event.command });
        }
        try {
            if (event.event === 'frontend_runtime_sample' || event.event === 'frontend_runtime_finished') {
                if (event.schema !== runtimeSchema) throw new Error('Unexpected runtime schema');
                const values = validators.runtime(copiedWords(event.values, 416), previousRuntime, summary.generation);
                previousRuntime = values;
                const owner = values[8] || (values[26] ? values[66] : 0);
                if (owner && expectedOwner && owner !== expectedOwner) throw new Error('Runtime Core owner changed');
                if (owner) expectedOwner = owner;
                if (expectedScriptOwner === null) expectedScriptOwner = values[7];
                if (values[26] && expectedSession === null) expectedSession = pair(values, 99);
                runtimeRows.push({ index, values, event });
            } else if (event.event === 'frontend_local_input_runtime_sample') {
                if (event.schema !== localSchema) throw new Error('Unexpected local-input schema');
                localRows.push({ index, values: validators.local(copiedWords(event.values, 32)), event });
            }
        } catch (error) { protocolError('Invalid copied record at ' + index + ': ' + error.message); }
    }
    summary.ownerThread = expectedOwner;
    summary.runtimeSamples = runtimeRows.length;
    summary.localSamples = localRows.length;
    for (const row of localRows) {
        if (!expectedOwner || row.values[2] !== expectedOwner) protocolError('Local cache does not belong to the retained Core owner at ' + row.index);
        if (row.values[22]) protocolError('Local input recorded workload failure ' + row.values[22]);
    }
    const views = [];
    for (const row of runtimeRows) {
        const values = row.values;
        if (values[47] || values[25]) summary.menuCloseObserved = true;
        if (values[1] !== 4) continue;
        const key = pair(values, 20);
        if (views.some(view => view.key === key)) continue;
        const command = commands.find(value => value.index < row.index && value.command === 1 && value.sequence === values[11]);
        if (!nonzeroPair(values, 20) || values[12] !== 1 || values[13] !== 1 || !command) {
            protocolError('Visible view lacks its actual accepted and acknowledged Show at ' + row.index);
            continue;
        }
        views.push({ key, index: row.index, values });
    }
    summary.acknowledgedViewGenerations = views.map(view => view.key);
    if (views.length < 2) protocolError('Two distinct acknowledged native GUI view generations were not observed');
    for (const view of views) {
        const retired = runtimeRows.find(row => row.index > view.index && [2, 7, 8].includes(row.values[1]) &&
            pair(row.values, 20) === view.key && pair(row.values.slice(128), 257) === view.key &&
            row.values[128 + 1] === 2 && validators.retiredView(row.values.slice(128)));
        if (!retired) protocolError('Acknowledged view ' + view.key + ' has no subsequent matching retirement');
        else { view.retirement = retired; summary.retiredViewGenerations.push(view.key); }
    }
    if (views.length >= 2 && views[0].retirement) {
        const first = views[0], second = views[1];
        const showing = runtimeRows.find(row => row.index > first.retirement.index && row.index < second.index &&
            row.values[1] === 3 && pair(row.values, 20) === second.key);
        const activeCutoff = showing?.index ?? second.index;
        const active = localRows.filter(row => row.index > first.retirement.index && row.index < activeCutoff &&
            row.values[2] === expectedOwner && row.values[1] === 3 && row.values[8] === 1 &&
            nonzeroPair(row.values, 15) && nonzeroPair(row.values, 13));
        summary.heldRenewedLocalInputBetweenViews = active.length > 0;
        if (!active.length) protocolError('No held and renewed active local lease was observed between retired view1 and view2 Show');
        // The command queue is not the GUI admission boundary. A local cleanup
        // sample may follow Showing, but must precede the actual Visible ack.
        const retired = active.length && localRows.find(row => row.index > active[active.length - 1].index && row.index < second.index &&
            row.values[2] === expectedOwner && localRetired(row.values) && row.values[5] === 1 &&
            pair(row.values, 6) === pair(active[active.length - 1].values, 6) &&
            row.values[10] === 1 && row.values[11] > 0 && row.values[27] > 0);
        summary.localRetiredBeforeSecondVisible = Boolean(retired);
        if (!retired) protocolError('No neutral local native retirement was observed before second Visible admission');
        summary.activeLocalSamplesOutsideQualifiedInterval = localRows.filter(row => row.values[8] &&
            !(row.index > first.retirement.index && row.index < activeCutoff)).length;
    }
    const final = ended?.value;
    if (!final || final.passed !== true || final.error || final.host_heartbeat_rejected || final.stop_accepted !== true || final.stop_requested !== true)
        protocolError('Script did not report a successful requested runtime stop');
    let terminalRuntime = null, terminalLocal = null;
    try {
        if (!start || !summary.generation || !ended || ended.index <= start.index || final.generation !== summary.generation)
            throw new Error('No unique matching started/finished identity');
        terminalRuntime = validators.runtime(copiedWords(final.values, 416), null, summary.generation);
        terminalLocal = validators.local(copiedWords(final.local_input_values, 32));
        if (!expectedOwner || terminalRuntime[8] !== expectedOwner || terminalLocal[2] !== expectedOwner)
            throw new Error('Terminal Core/local owner does not match the lifecycle');
        if (terminalRuntime[7] !== expectedScriptOwner || pair(terminalRuntime, 99) !== expectedSession)
            throw new Error('Terminal script/Core session does not match the lifecycle');
        if (![7, 8].includes(terminalRuntime[1]) || !terminalRuntime[34] || terminalRuntime[33] ||
            [30, 31].some(field => terminalRuntime[field]) || ![39, 40, 41].every(field => terminalRuntime[field] === 1) ||
            !validators.retiredCore(terminalRuntime) || !validators.retiredView(terminalRuntime.slice(128)) ||
            terminalRuntime[14] || terminalRuntime[15]) throw new Error('Terminal runtime retains a Core/view/native owner');
        if (!localDisabled(terminalLocal)) throw new Error('Terminal local input is not Disabled, neutral and fully retired');
        if (!localRows.length) throw new Error('No separately captured local Core32 record');
        const lastLocal = localRows[localRows.length - 1];
        if (lastLocal.values.some((value, field) => value !== terminalLocal[field]))
            throw new Error('Final local cache differs from the last separately captured local record');
    } catch (error) { cleanupError(error.message); }
    if (terminalRuntime) {
        summary.terminalRuntimePhase = terminalRuntime[1];
        if (terminalRuntime[1] !== 7 || terminalRuntime[35] || terminalRuntime[36] || terminalRuntime[37] !== 1 ||
            !terminalRuntime[16] || !terminalRuntime[17] || terminalRuntime[9] !== terminalRuntime[17] ||
            terminalRuntime[11] !== terminalRuntime[17] || terminalRuntime[12] !== 3 || terminalRuntime[13] !== 1)
            protocolError('Terminal headers do not acknowledge the final requested Stop');
        if (terminalRuntime[24] < 2 || terminalRuntime[23] < 2) protocolError('Terminal counters do not retire two acknowledged views');
    }
    if (terminalLocal) {
        summary.terminalNativeAttempted = terminalLocal[5];
        summary.terminalNativeGeneration = pair(terminalLocal, 6);
        summary.terminalEndAttempts = terminalLocal[27];
        if (terminalLocal[22]) protocolError('Terminal local input retains historical workload failure ' + terminalLocal[22]);
    }
    if (cleanupErrors.length) protocolError('Required independent terminal cleanup proof failed');
    return finish();
}

module.exports = { verifyEvents, validatorSource, maximumEvents };
if (require.main === module) {
    const input = JSON.parse(fs.readFileSync(process.argv[2], 'utf8'));
    const result = verifyEvents(Array.isArray(input) ? input : input.events,
        process.argv[3] ? { expectedIndexSha256: process.argv[3] } : {});
    process.stdout.write(JSON.stringify(result, null, 2) + '\n');
    process.exitCode = result.protocolPassed && result.cleanupPassed ? 0 : 1;
}
