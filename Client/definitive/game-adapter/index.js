// SPDX-License-Identifier: GPL-3.0-or-later
import { MODE, ONLINE_ACTIVATION, GAIT_REFERENCE, GAIT_RATE, GAIT_ROOT_LOCK, INPUT_REFERENCE, MTA_NATIVE_PAD_REFERENCE, MTA_NATIVE_MOVEMENT_TRIAL, MTA_CORE_STARTUP_REFERENCE, MTA_CORE_INPUT_REFERENCE, MTA_CORE_MOVEMENT_REFERENCE, MTA_CORE_RENDER_REFERENCE, MTA_CORE_RENDER_ROUTE, MTA_CORE_RENDER_GPU_REFERENCE, MTA_CORE_RENDER_CUSTOM_PRESENT_REFERENCE, MTA_CORE_RENDER_FRAME_BOUNDARY_REFERENCE, MTA_CORE_RENDER_NATIVE_DISPATCH_REFERENCE, MTA_CORE_GUI_STARTUP_REFERENCE, MTA_CORE_GUI_SURFACE_REFERENCE, MTA_CORE_GUI_FRONTEND_REFERENCE } from './config.mjs';
import { AdapterController, NativeGaitReferenceController, NativeInputReferenceController, PlayerControlLease } from './controller.mjs';
import { MtaNativePadReferenceController } from './native-pad-reference.mjs';
import { MtaNativeMovementTrialController } from './native-movement-trial.mjs';

let controller = null;
let phase = 'entry';
const emit = event => log(JSON.stringify(event));
const enterPhase = value => { phase = value; emit({ event: 'adapter_phase', phase, mode: MODE }); };

function runAdapter() {
    emit({ event: 'adapter_phase', phase, mode: MODE, host: HOST, cleo: String(CLEO.version),
        host_version: String(CLEO.hostVersion), definitions: String(CLEO.apiVersion) });
    if (!['diagnostic', 'smoke', 'online'].includes(MODE)) throw new Error('Unknown adapter mode');
    if (typeof MTA_CORE_GUI_FRONTEND_REFERENCE !== 'boolean' ||
        (MTA_CORE_GUI_FRONTEND_REFERENCE && (MODE !== 'smoke' || [INPUT_REFERENCE, GAIT_REFERENCE, GAIT_RATE, GAIT_ROOT_LOCK,
            MTA_NATIVE_PAD_REFERENCE, MTA_NATIVE_MOVEMENT_TRIAL, MTA_CORE_STARTUP_REFERENCE, MTA_CORE_INPUT_REFERENCE,
            MTA_CORE_MOVEMENT_REFERENCE, MTA_CORE_RENDER_REFERENCE, MTA_CORE_RENDER_GPU_REFERENCE,
            MTA_CORE_RENDER_CUSTOM_PRESENT_REFERENCE, MTA_CORE_RENDER_FRAME_BOUNDARY_REFERENCE,
            MTA_CORE_RENDER_NATIVE_DISPATCH_REFERENCE, MTA_CORE_GUI_STARTUP_REFERENCE, MTA_CORE_GUI_SURFACE_REFERENCE].some(Boolean))))
        throw new Error('MTA Core GUI frontend requires separate opt-in Smoke mode');
    if (typeof MTA_CORE_GUI_STARTUP_REFERENCE !== 'boolean' ||
        (MTA_CORE_GUI_STARTUP_REFERENCE && (MODE !== 'smoke' || [INPUT_REFERENCE, GAIT_REFERENCE, GAIT_RATE, GAIT_ROOT_LOCK,
            MTA_NATIVE_PAD_REFERENCE, MTA_NATIVE_MOVEMENT_TRIAL, MTA_CORE_STARTUP_REFERENCE, MTA_CORE_INPUT_REFERENCE,
            MTA_CORE_MOVEMENT_REFERENCE, MTA_CORE_RENDER_REFERENCE, MTA_CORE_RENDER_GPU_REFERENCE,
            MTA_CORE_RENDER_CUSTOM_PRESENT_REFERENCE, MTA_CORE_RENDER_FRAME_BOUNDARY_REFERENCE,
            MTA_CORE_RENDER_NATIVE_DISPATCH_REFERENCE, MTA_CORE_GUI_SURFACE_REFERENCE].some(Boolean))))
        throw new Error('MTA Core GUI startup requires separate opt-in Smoke mode');
    // A child composition surface is a separate Core-owned workload. It must
    // never inherit offscreen startup, input writes, or a native render trial.
    if (typeof MTA_CORE_GUI_SURFACE_REFERENCE !== 'boolean' ||
        (MTA_CORE_GUI_SURFACE_REFERENCE && (MODE !== 'smoke' || [INPUT_REFERENCE, GAIT_REFERENCE, GAIT_RATE, GAIT_ROOT_LOCK,
            MTA_NATIVE_PAD_REFERENCE, MTA_NATIVE_MOVEMENT_TRIAL, MTA_CORE_STARTUP_REFERENCE, MTA_CORE_INPUT_REFERENCE,
            MTA_CORE_MOVEMENT_REFERENCE, MTA_CORE_RENDER_REFERENCE, MTA_CORE_RENDER_GPU_REFERENCE,
            MTA_CORE_RENDER_CUSTOM_PRESENT_REFERENCE, MTA_CORE_RENDER_FRAME_BOUNDARY_REFERENCE,
            MTA_CORE_RENDER_NATIVE_DISPATCH_REFERENCE, MTA_CORE_GUI_STARTUP_REFERENCE].some(Boolean))))
        throw new Error('MTA Core GUI surface requires separate opt-in Smoke mode');
    if (typeof MTA_CORE_RENDER_REFERENCE !== 'boolean' ||
        (MTA_CORE_RENDER_REFERENCE && (MODE !== 'smoke' || INPUT_REFERENCE || GAIT_REFERENCE || GAIT_RATE || GAIT_ROOT_LOCK ||
            MTA_NATIVE_PAD_REFERENCE || MTA_NATIVE_MOVEMENT_TRIAL || MTA_CORE_STARTUP_REFERENCE || MTA_CORE_INPUT_REFERENCE || MTA_CORE_MOVEMENT_REFERENCE)))
        throw new Error('MTA Core renderer observation requires separate opt-in Smoke mode');
    if (MTA_CORE_RENDER_REFERENCE && !['d3d11', 'd3d12'].includes(MTA_CORE_RENDER_ROUTE))
        throw new Error('MTA Core renderer observation requires an explicit supported route');
    if (typeof MTA_CORE_RENDER_GPU_REFERENCE !== 'boolean' ||
        (MTA_CORE_RENDER_GPU_REFERENCE && (!MTA_CORE_RENDER_REFERENCE || MTA_CORE_RENDER_ROUTE !== 'd3d12')))
        throw new Error('GPU ownership queries require explicit D3D12 Core renderer observation');
    if (typeof MTA_CORE_RENDER_NATIVE_DISPATCH_REFERENCE !== 'boolean' ||
        (MTA_CORE_RENDER_NATIVE_DISPATCH_REFERENCE && (!MTA_CORE_RENDER_FRAME_BOUNDARY_REFERENCE || !MTA_CORE_RENDER_CUSTOM_PRESENT_REFERENCE ||
            !MTA_CORE_RENDER_GPU_REFERENCE || !MTA_CORE_RENDER_REFERENCE || MTA_CORE_RENDER_ROUTE !== 'd3d12')))
        throw new Error('Native-dispatch observation requires explicit D3D12 Core GPU, custom-present and frame-boundary observation');
    if (typeof MTA_CORE_RENDER_FRAME_BOUNDARY_REFERENCE !== 'boolean' ||
        (MTA_CORE_RENDER_FRAME_BOUNDARY_REFERENCE && (!MTA_CORE_RENDER_CUSTOM_PRESENT_REFERENCE || !MTA_CORE_RENDER_GPU_REFERENCE ||
            !MTA_CORE_RENDER_REFERENCE || MTA_CORE_RENDER_ROUTE !== 'd3d12')))
        throw new Error('Frame-boundary observation requires explicit D3D12 Core GPU and custom-present observation');
    if (typeof MTA_CORE_RENDER_CUSTOM_PRESENT_REFERENCE !== 'boolean' ||
        (MTA_CORE_RENDER_CUSTOM_PRESENT_REFERENCE && (!MTA_CORE_RENDER_GPU_REFERENCE || !MTA_CORE_RENDER_REFERENCE || MTA_CORE_RENDER_ROUTE !== 'd3d12')))
        throw new Error('Custom-present recognition requires explicit D3D12 Core GPU observation');
    if (typeof MTA_CORE_MOVEMENT_REFERENCE !== 'boolean' ||
        (MTA_CORE_MOVEMENT_REFERENCE && (MODE !== 'smoke' || INPUT_REFERENCE || GAIT_REFERENCE || GAIT_RATE || GAIT_ROOT_LOCK ||
            MTA_NATIVE_PAD_REFERENCE || MTA_NATIVE_MOVEMENT_TRIAL || MTA_CORE_STARTUP_REFERENCE || MTA_CORE_INPUT_REFERENCE)))
        throw new Error('MTA Core configured movement reference requires separate opt-in Smoke mode');
    if (typeof MTA_CORE_INPUT_REFERENCE !== 'boolean' ||
        (MTA_CORE_INPUT_REFERENCE && (MODE !== 'smoke' || INPUT_REFERENCE || GAIT_REFERENCE || GAIT_RATE || GAIT_ROOT_LOCK ||
            MTA_NATIVE_PAD_REFERENCE || MTA_NATIVE_MOVEMENT_TRIAL || MTA_CORE_STARTUP_REFERENCE)))
        throw new Error('MTA Core configured input reference requires separate opt-in Smoke mode');
    if (typeof INPUT_REFERENCE !== 'boolean' || (INPUT_REFERENCE && (MODE !== 'smoke' || GAIT_REFERENCE)))
        throw new Error('Native input reference requires opt-in Smoke mode without gait reference');
    if (typeof MTA_NATIVE_PAD_REFERENCE !== 'boolean' ||
        (MTA_NATIVE_PAD_REFERENCE && (MODE !== 'smoke' || INPUT_REFERENCE || GAIT_REFERENCE)))
        throw new Error('MTA native pad reference requires separate opt-in Smoke mode');
    if (typeof MTA_NATIVE_MOVEMENT_TRIAL !== 'boolean' ||
        (MTA_NATIVE_MOVEMENT_TRIAL && (MODE !== 'smoke' || INPUT_REFERENCE || GAIT_REFERENCE || MTA_NATIVE_PAD_REFERENCE)))
        throw new Error('MTA native movement trial requires separate opt-in Smoke mode');
    if (typeof MTA_CORE_STARTUP_REFERENCE !== 'boolean' ||
        (MTA_CORE_STARTUP_REFERENCE && (MODE !== 'smoke' || INPUT_REFERENCE || GAIT_REFERENCE || MTA_NATIVE_PAD_REFERENCE || MTA_NATIVE_MOVEMENT_TRIAL)))
        throw new Error('MTA Core startup reference requires separate opt-in Smoke mode');
    if (MODE === 'diagnostic') {
        // Keep the installed loader/runtime available while separating its
        // startup from our DLL, FFI, player reads and transport. This finite
        // entry can provide evidence without repeating a gameplay probe.
        phase = 'diagnostic_complete';
        emit({ event: 'adapter_diagnostic_complete', phase, mode: MODE, bridge_loaded: false,
            game_adapter_active: false, network_active: false });
        return;
    }
    if (typeof GAIT_REFERENCE !== 'boolean' || (GAIT_REFERENCE && MODE !== 'smoke'))
        throw new Error('Native gait reference requires opt-in Smoke mode');
    if (typeof GAIT_RATE !== 'boolean' || (GAIT_RATE && MODE !== 'online'))
        throw new Error('Matched gait rate requires opt-in Online mode');
    if (typeof GAIT_ROOT_LOCK !== 'boolean' || (GAIT_ROOT_LOCK && MODE !== 'online'))
        throw new Error('Animation root lock requires opt-in Online mode');
    if (HOST !== 'sa_unreal') throw new Error('This adapter requires the San Andreas Definitive Edition host');
    const runtimeVersion = String(CLEO.version).split(/[.-]/).map(Number);
    if (runtimeVersion.length < 3 || !runtimeVersion.slice(0, 3).every(value => Number.isInteger(value) && value >= 0) || runtimeVersion[0] < 1 ||
        (runtimeVersion[0] === 1 && (runtimeVersion[1] < 5 || (runtimeVersion[1] === 5 && runtimeVersion[2] < 1))))
        throw new Error('CLEO Redux 1.5.1 or newer is required');
    // The runtime documents a version object, but a numeric binding can omit
    // trailing zeros (0.500 becomes 0.5). Compare the API revision numerically
    // rather than misreading that shortened value as revision five.
    const definitionVersion = Number(String(CLEO.apiVersion));
    if (!Number.isFinite(definitionVersion) || definitionVersion < 0.493 || definitionVersion >= 1)
        throw new Error('SA Unreal API definitions 0.493 or newer are required');
    // These untested input/velocity bindings come from the pinned 0.500 API.
    // Reject older definitions before activation reads, DLL access, or CJ reads.
    if ((INPUT_REFERENCE || MTA_NATIVE_PAD_REFERENCE || MTA_NATIVE_MOVEMENT_TRIAL || MTA_CORE_STARTUP_REFERENCE || MTA_CORE_INPUT_REFERENCE || MTA_CORE_MOVEMENT_REFERENCE || MTA_CORE_RENDER_REFERENCE || MTA_CORE_GUI_STARTUP_REFERENCE || MTA_CORE_GUI_SURFACE_REFERENCE || MTA_CORE_GUI_FRONTEND_REFERENCE) && definitionVersion < 0.500)
        throw new Error('Native input reference requires SA Unreal API definitions 0.500 or newer');

    const stopKey = 'Ctrl+Shift+End';
    // Home opens the Rockstar console on the tested build and interrupts this
    // read-only observation, so the input reference needs its own shortcut.
    const activateKey = (INPUT_REFERENCE || MTA_NATIVE_PAD_REFERENCE || MTA_NATIVE_MOVEMENT_TRIAL || MTA_CORE_STARTUP_REFERENCE || MTA_CORE_INPUT_REFERENCE || MTA_CORE_MOVEMENT_REFERENCE || MTA_CORE_RENDER_REFERENCE || MTA_CORE_GUI_STARTUP_REFERENCE || MTA_CORE_GUI_SURFACE_REFERENCE || MTA_CORE_GUI_FRONTEND_REFERENCE) ? 'Ctrl+Shift+F9' : 'Ctrl+Shift+Home';
    const chordHeld = keys => keys.every(key => native('IS_KEY_PRESSED', key));
    if (MODE === 'online' || GAIT_REFERENCE || INPUT_REFERENCE || MTA_NATIVE_PAD_REFERENCE || MTA_NATIVE_MOVEMENT_TRIAL || MTA_CORE_STARTUP_REFERENCE || MTA_CORE_INPUT_REFERENCE || MTA_CORE_MOVEMENT_REFERENCE || MTA_CORE_RENDER_REFERENCE || MTA_CORE_GUI_STARTUP_REFERENCE || MTA_CORE_GUI_SURFACE_REFERENCE || MTA_CORE_GUI_FRONTEND_REFERENCE) {
        if (MODE === 'online' && !['manual', 'automatic'].includes(ONLINE_ACTIVATION))
            throw new Error('Unknown online activation mode');
        // The reference changes one owned NPC, so even an automatic Online
        // preference cannot start this separate experiment during save loading.
        if (GAIT_REFERENCE || INPUT_REFERENCE || MTA_NATIVE_PAD_REFERENCE || MTA_NATIVE_MOVEMENT_TRIAL || MTA_CORE_STARTUP_REFERENCE || MTA_CORE_INPUT_REFERENCE || MTA_CORE_MOVEMENT_REFERENCE || MTA_CORE_RENDER_REFERENCE || MTA_CORE_GUI_STARTUP_REFERENCE || MTA_CORE_GUI_SURFACE_REFERENCE || MTA_CORE_GUI_FRONTEND_REFERENCE || ONLINE_ACTIVATION === 'manual') {
            // Startup freezes have also occurred before this script enters.
            // Waiting here cannot prevent those loader failures, but it gives
            // a usable game session a separate, observable boundary before
            // our DLL, player reads, transport and remote-ped work begin.
            enterPhase('awaiting_activation');
            emit({ event: 'adapter_armed', mode: MODE, activate_key: activateKey, stop_key: stopKey,
                bridge_loaded: false, game_adapter_active: false, network_active: false });
            while (true) {
                // Stop takes priority if both chords are held. Read held keys
                // together on every tick rather than accumulating presses.
                if (chordHeld([17, 16, 35])) {
                    emit({ event: 'adapter_activation_cancelled', reason: stopKey, mode: MODE,
                        bridge_loaded: false, game_adapter_active: false, network_active: false });
                    return;
                }
                if (chordHeld([17, 16, (INPUT_REFERENCE || MTA_NATIVE_PAD_REFERENCE || MTA_NATIVE_MOVEMENT_TRIAL || MTA_CORE_STARTUP_REFERENCE || MTA_CORE_INPUT_REFERENCE || MTA_CORE_MOVEMENT_REFERENCE || MTA_CORE_RENDER_REFERENCE || MTA_CORE_GUI_STARTUP_REFERENCE || MTA_CORE_GUI_SURFACE_REFERENCE || MTA_CORE_GUI_FRONTEND_REFERENCE) ? 120 : 36]) && native('IS_PLAYER_PLAYING', 0)) {
                    emit({ event: 'adapter_activation_requested', mode: MODE, activate_key: activateKey });
                    break;
                }
                wait(50);
            }
        }
    }
    if (MTA_NATIVE_PAD_REFERENCE || MTA_NATIVE_MOVEMENT_TRIAL || MTA_CORE_STARTUP_REFERENCE || MTA_CORE_INPUT_REFERENCE || MTA_CORE_MOVEMENT_REFERENCE || MTA_CORE_RENDER_REFERENCE || MTA_CORE_GUI_STARTUP_REFERENCE || MTA_CORE_GUI_SURFACE_REFERENCE || MTA_CORE_GUI_FRONTEND_REFERENCE) {
        // Holding the activation modifiers contaminated the prior source-state
        // window. Begin DLL access and the recorder only after their release,
        // while keeping cancellation and a finite pre-binding deadline.
        enterPhase('awaiting_activation_release');
        const releaseStartedAt = Date.now();
        if (!Number.isSafeInteger(releaseStartedAt) || releaseStartedAt < 0)
            throw new Error('Invalid native source activation release clock');
        let lastReleaseAt = releaseStartedAt;
        while (true) {
            if (chordHeld([17, 16, 35])) {
                emit({ event: 'adapter_activation_cancelled', reason: stopKey, mode: MODE,
                    bridge_loaded: false, game_adapter_active: false, network_active: false });
                return;
            }
            const now = Date.now();
            if (!Number.isSafeInteger(now) || now < lastReleaseAt)
                throw new Error('Invalid native source activation release clock');
            lastReleaseAt = now;
            if (now - releaseStartedAt >= 10000) {
                emit({ event: 'adapter_activation_release_timeout', mode: MODE, activate_key: activateKey,
                    at_ms: now, release_wait_ms: now - releaseStartedAt,
                    bridge_loaded: false, game_adapter_active: false, network_active: false });
                throw new Error('Native source activation release timed out');
            }
            if (!native('IS_PLAYER_PLAYING', 0))
                throw new Error('Player readiness lost while waiting for native source activation release');
            const activationHeld = [17, 16, 120].map(key => native('IS_KEY_PRESSED', key));
            if (activationHeld.every(held => !held)) {
                emit({ event: 'adapter_activation_released', mode: MODE, activate_key: activateKey,
                    at_ms: now, release_wait_ms: now - releaseStartedAt,
                    bridge_loaded: false, game_adapter_active: false, network_active: false });
                break;
            }
            wait(50);
        }
    }
    if (!Memory.Fn || typeof DataView !== 'function')
        throw new Error('CLEO x64 memory helpers are unavailable');

    // These startup-only markers locate the last boundary reached in a
    // recovered log. They do not add per-frame logging or claim that a GPU
    // timeout is caused by whichever phase happens to be last.
    enterPhase('bridge_load');
    const library = DynamicLibrary.Load('./de_client_bridge.dll');
    if (!library) throw new Error('The standalone x64 bridge could not be loaded');
    enterPhase('bridge_loaded');
    const bind = (name, method = 'X64I32') => {
        const address = library.getProcedure(name);
        if (!Number.isSafeInteger(address) || address <= 0) throw new Error('Missing bridge procedure: ' + name);
        return Memory.Fn[method](address, false);
    };
    const apiVersion = bind('DE_ApiVersion', 'X64U32');
    const checkBuild = bind('DE_CheckGameBuild');
    const bindRuntime = bind('DE_BindRuntime');
    enterPhase('bridge_abi_check');
    if (apiVersion() !== 2) throw new Error('Bridge ABI version 2 is required');
    enterPhase('game_build_check');
    if (checkBuild() !== 1) throw new Error('The game executable hash is not the verified local build');
    enterPhase('game_build_verified');
    enterPhase('runtime_bind');
    if (bindRuntime() !== 1) throw new Error('CLEO runtime lifecycle binding failed');
    enterPhase('runtime_bound');
    if (MTA_CORE_GUI_FRONTEND_REFERENCE) {
        const version = bind('DE_CoreGuiFrontendApiVersion', 'X64U32');
        const begin = bind('DE_CoreGuiFrontendBegin');
        const capture = bind('DE_CoreGuiFrontendCapture');
        const readGui = bind('DE_CoreGuiFrontendRead', 'X64U32');
        const poll = bind('DE_CoreStartupPoll'), cancel = bind('DE_CoreStartupCancel');
        const read = bind('DE_CoreStartupRead', 'X64U32');
        const controlBridge = { capture: bind('DE_CoreGuiFrontendControlCapture'),
            read: bind('DE_CoreGuiFrontendControlRead', 'X64U32'),
            ack: bind('DE_CoreGuiFrontendControlAck'), heartbeat: bind('DE_CoreGuiFrontendControlHeartbeat'),
            revoke: bind('DE_CoreGuiFrontendControlRevoke'), restored: bind('DE_CoreGuiFrontendControlRestored') };
        const focused = bind('DE_CoreGuiFrontendControlFocused');
        if (version() !== 1) throw new Error('Core GUI frontend API1 is required');
        const lease = new PlayerControlLease({
            stopRequested: () => chordHeld([17, 16, 35]),
            isFocused: () => focused() === 1,
            isReady: () => native('IS_PLAYER_PLAYING', 0), localPed: () => native('GET_PLAYER_CHAR', 0),
            doesPedExist: ped => native('DOES_CHAR_EXIST', ped), isOnFoot: ped => native('IS_CHAR_ON_FOOT', ped),
            isControlOn: () => native('IS_PLAYER_CONTROL_ON', 0),
            setControl: enabled => native('SET_PLAYER_CONTROL', 0, enabled)
        }, controlBridge, { log: emit });
        emit({ event: 'adapter_start', host: HOST, cleo: String(CLEO.version), host_version: String(CLEO.hostVersion),
            definitions: String(CLEO.apiVersion), mode: MODE, endpoint: null, stop_key: stopKey,
            core_gui_frontend_reference: true, provenance: 'mta_core_owned_gui_frontend', core_api: 13, frontend_trial_api: 1,
            adapter_abi: 6, network_active: false, input_write_trial: true, input_writes: false,
            native_player_control_lease_planned: true, native_player_control_writes: false, native_pad_writes: false,
            remote_peds_created: false, native_queue_commands_issued: false, native_backbuffer_touched: false,
            gameplay_ready: false, full_mta_ready: false, camera_capture_verified: false });
        enterPhase('core_gui_frontend_reference');
        const startedAt = Date.now();
        if (!Number.isSafeInteger(startedAt) || startedAt < 0) throw new Error('Invalid Core GUI frontend clock');
        emit({ event: 'core_gui_frontend_reference_started', at_ms: startedAt, core_api: 13, frontend_trial_api: 1,
            sample_interval_ms: 100, control_poll_interval_ms: 50, device_wait_budget_ms: 10000,
            control_acquire_ms: 5000, display_duration_ms: 30000, script_control_deadline_ms: 35000,
            heartbeat_expiry_ms: 1000, cleanup_deadline_ms: 2000 });
        if (begin() !== 1) throw new Error('MTA Core GUI frontend worker could not start');
        let terminal = false, cancelRequested = false, cancelAccepted = false, trialError = null;
        let lastAt = startedAt, nextSampleAt = startedAt, latest = null, samples = 0;
        const unsigned = state => state.every(value => Number.isInteger(value) && value >= 0 && value <= 0xffffffff);
        const requestCancel = reason => {
            cancelRequested = true; lease.stop(reason);
            if (!cancelAccepted) {
                if (cancel() !== 1) throw new Error('MTA Core GUI frontend cancellation was rejected');
                cancelAccepted = true;
                emit({ event: 'core_gui_frontend_reference_stop_requested', at_ms: lastAt, reason });
            }
        };
        const captureSample = at => {
            const captured = capture();
            if (captured !== 0 && captured !== 1) throw new Error('Core GUI frontend capture status unavailable');
            if (!captured) return;
            const state = Array.from({ length: 160 }, (_, field) => readGui(field));
            const booleanFields = [5, 6, 7, 8, 9, 10, 11, 19, 20, 21, 30, 31, 32, 37, 39, 43, 44, 45,
                49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64, 65, 66, 72, 73, 74,
                75, 76, 77, 78, 79, 80, 86, 100, 101, 102, 103, 104, 106, 107, 108, 109, 110,
                112, 113, 114, 115, 116, 117, 118, 119, 120, 121, 122, 123, 124,
                129, 130, 139, 140, 141, 143, 144, 145, 146, 147, 148, 149, 152, 154, 155, 156, 157];
            if (!unsigned(state) || state[0] !== 1 || ![1, 2, 3].includes(state[1]) || !state[2] || state[4] > 11 ||
                booleanFields.some(field => state[field] > 1) || ![0, 2].includes(state[18]) || state[23] > 6 ||
                state[41] > 4 || state[42] > 10 || state[47] > 9 || state[48] > 9 ||
                state.slice(88, 96).some(Boolean) || state.slice(125, 128).some(Boolean) || state.slice(158).some(Boolean) ||
                [44, 45, 53, 54, 110, 119, 120, 122, 123, 156, 157].some(field => state[field]) ||
                ![0, 1].includes(state[96]) || state[128] !== 1)
                throw new Error('Invalid Core GUI frontend scalar state');
            const frontend = state.slice(96, 128);
            const frontendClean = frontend.every(value => !value) ||
                (frontend[1] === 4 && frontend[17] === 1 &&
                    [6, 16, 19, 20, 21].every(field => frontend[field] === 0) && state[155] === 1);
            if ((state[5] && state[6]) || (state[9] && state[43]) ||
                (state[6] && (state[1] !== 2 || state[5] || !state[9] || !state[39] || state[43] ||
                    state.slice(49, 52).some(Boolean) || state[59] || state[41] || state.slice(75, 78).some(Boolean) || state[72] ||
                    ![78, 79, 80].every(field => state[field] === 1) || !frontendClean || state[137] || state[138] ||
                    (state[129] && !state[139]) ||
                    (state[143] && (!state[146] || !state[147] || !state[148] || state[145])))))
                throw new Error('Core GUI frontend claimed cleanup with retained ownership');
            const pairPresent = field => state[field] !== 0 || state[field + 1] !== 0;
            if ((state[1] === 2 && (!state[6] || !state[4])) || (state[1] === 3 && (!state[5] || state[6])) ||
                (!state[10] && (state.slice(12, 19).some(Boolean) || state[58])) ||
                (state[58] && (!state[10] || state[18] !== 2 || ![12, 14, 16].every(pairPresent))) ||
                (state[19] && (![8, 9, 10, 11, 58].every(field => state[field] === 1) || state[42])) ||
                // StartGpu requests controls before creating a GUI session; a
                // refused handoff may complete a failed workload with19=0.
                (state[21] && (!state[19] || !state[20] || !state[154])) ||
                (state[37] && (![19, 30, 31, 32, 55].every(field => state[field] === 1) || state[38] <= 10000)) ||
                (state[4] === 1 && (!state[20] || !state[21])) ||
                (state[35] === 0xffffffff && state[36] === 0xffffffff && !state[52]))
                throw new Error('Invalid Core GUI frontend rendering ownership state');
            if ((!state[143] && state.slice(142, 150).some(Boolean)) || (state[143] && !state[142]) ||
                (state[145] && (!state[144] || state[146] || state[148] || state[149])) ||
                (state[147] && (!state[143] || !state[146] || state[145])))
                throw new Error('Invalid Core GUI frontend control ownership state');
            // Enabled records the historical interaction opt-in. Detachment
            // retires live input without erasing that admission evidence.
            if ((state[129] && !state[139] && (![9, 143, 144, 145].every(field => state[field] === 1) || state[146])) ||
                (state[148] && (!state[146] || !state[147])) || (state[144] && !state[143]) ||
                (state[154] && (!state[96] || !state[9] || !state[144] || !state[129] || !state[19])) ||
                (state[139] && (state[130] || state[137] || state[138])) ||
                (state[113] && (state[112] || state[114] || state[115] || state[116] || state[117])))
                throw new Error('Invalid Core GUI frontend input ownership state');
            if ((frontend[1] === 0 && (frontend.some(Boolean) || state[154])) ||
                (frontend[1] !== 0 && (frontend[0] !== 1 || ![1, 2, 3, 4, 5].includes(frontend[1]) || !frontend[2] || !frontend[22])) ||
                (frontend[1] === 2 && ![10, 11, 12, 13, 21, 28].every(field => frontend[field] === 1)) ||
                (frontend[1] === 4 && (!frontend[17] || [6, 16, 19, 20, 21].some(field => frontend[field]))) ||
                (state[155] && frontend[1] !== 0 && (frontend[1] !== 4 || !frontend[17])))
                throw new Error('Invalid Core GUI frontend lifecycle state');
            if (latest && (state[2] < latest[2] || state[3] < latest[3]))
                throw new Error('Core GUI frontend publication order is invalid');
            if (latest && state[2] === latest[2]) {
                if (state.some((value, field) => value !== latest[field])) throw new Error('Core GUI frontend changed without a new sequence');
                return;
            }
            latest = state; ++samples;
            emit({ event: 'core_gui_frontend_reference_sample', at_ms: at, elapsed_ms: at - startedAt, state });
        };
        // A failure closes admission, but this loop keeps every live owner:
        // input detaches, the script restores its exact native state, then Core
        // drains its independently owned GPU/GUI resources before final release.
        while (true) {
            let at = lastAt;
            try {
                at = Date.now();
                if (!Number.isSafeInteger(at) || at < lastAt) throw new Error('Invalid Core GUI frontend clock');
                lastAt = at;
                const status = poll();
                if (status !== 0 && status !== 1) throw new Error('Core GUI frontend worker status unavailable');
                terminal = status === 1;
                lease.advance(at);
                if (lease.error && !trialError) trialError = lease.error;
                if (lease.stopReason && lease.stopReason !== 'worker_revoked') requestCancel(lease.stopReason);
                if (!terminal && at - startedAt >= 50000) requestCancel('worker_deadline');
                if (at >= nextSampleAt || terminal) { captureSample(at); nextSampleAt = at + 100; }
                if (terminal && (lease.completed || lease.finishWithoutRequest())) break;
            } catch (error) {
                trialError = trialError || String(error);
                lease.stop('error');
                try { requestCancel('error'); } catch (_) {}
                // Control cleanup is independent of snapshot validation/poll
                // failures; keep retrying it even if presentation is broken.
                lease.advance(Number.isSafeInteger(at) && at >= lastAt ? at : lastAt);
                if (terminal && (lease.completed || lease.finishWithoutRequest())) break;
            }
            try { wait(50); }
            catch (error) {
                trialError = trialError || String(error); lease.stop('wait_error');
                try { requestCancel('wait_error'); } catch (_) {}
                // A recoverable host wait failure cannot skip native restore.
                // Forced runtime/process termination remains outside script's
                // cleanup contract, so no success is claimed for that case.
            }
        }
        const state = Array.from({ length: 19 }, (_, field) => read(field));
        const control = lease.snapshot();
        // Worker error12 records accepted cancellation after its owned cleanup;
        // it is not an unload failure. Keep the workload failed, while requiring
        // the same independent GUI/native retirement proof as a normal finish.
        const workerRetired = (state[16] === 0 && state[17] === 1) ||
            (state[16] === 12 && state[17] === 0 && cancelAccepted);
        const cleanupPassed = terminal && unsigned(state) && latest !== null && latest[1] === 2 && latest[6] === 1 && !latest[5] &&
            !latest[59] && control.completed &&
            (!control.mutation_attempted || control.native_restored_verified) &&
            state[7] === 1 && state[13] === 1 && state[14] === 0 && state[15] === latest[57] &&
            !(state[12] & ~state[11]) && workerRetired && state[18] === 0;
        const expected = [1, 5, 6, 97, 0, 0, 0, 1, 3, 4, 5];
        const pairPresent = field => latest !== null && (latest[field] !== 0 || latest[field + 1] !== 0);
        const fenceComplete = latest !== null && !(latest[35] === 0xffffffff && latest[36] === 0xffffffff) &&
            (latest[36] > latest[34] || (latest[36] === latest[34] && latest[35] >= latest[33]));
        const passed = !trialError && !cancelRequested && cleanupPassed && expected.every((value, field) => state[field] === value) &&
            latest[4] === 1 && [8, 9, 10, 11, 19, 20, 21, 30, 31, 32, 37, 55, 56, 58, 60, 61, 62, 63, 64, 65, 66, 86,
                96, 106, 107, 108, 109, 113, 118, 124, 128, 129, 139, 143, 144, 146, 147, 148, 154, 155].every(field => latest[field] === 1) &&
            !latest[7] && !latest[22] && !latest[23] && !latest[42] && !latest[48] && !latest[52] && !latest[73] &&
            !latest[140] && !latest[149] && !latest[152] &&
            latest[18] === 2 && [12, 14, 16, 33].every(pairPresent) && fenceComplete && latest[38] > 10000 &&
            latest[24] > 0 && latest[27] === 87 && latest[28] > 0 && latest[28] <= 16384 && latest[29] > 0 && latest[29] <= 16384 &&
            latest[46] > 0 && latest[47] === 9 && latest[68] >= 2 && latest[67] === latest[68] + latest[87] && !latest[69] &&
            latest[70] >= latest[68] && latest[81] > 0 && latest[82] > 0 && latest[83] === 1 && latest[84] > 0 &&
            latest[85] === 30000 && latest[99] >= 2 && (latest[153] >= 30000 || latest[103] === 1 || latest[141] === 1) &&
            latest[151] === 1000 && control.acquired && control.native_initial === true &&
            control.native_disabled_verified && control.native_restored_verified && control.restored_acknowledged;
        emit({ event: 'core_gui_frontend_reference_finished', at_ms: Date.now(), elapsed_ms: lastAt - startedAt,
            state, control, passed, cleanup_passed: cleanupPassed, stop_requested: cancelRequested, error: trialError, samples,
            own_surface_workload_verified: passed, composition_present_verified: passed,
            actual_about_clicks: latest ? latest[105] : 0, actual_quit_requested: latest ? latest[103] === 1 : false,
            actual_credits_visible: latest ? latest[104] === 1 : false,
            full_core_ready: false, full_mta_ready: false, gameplay_ready: false, camera_capture_verified: false,
            network_active: false, input_writes: control.mutation_attempted,
            native_player_control_writes: control.mutation_attempted,
            native_player_control_write_attempts: control.disable_attempts + control.restore_attempts,
            native_pad_writes: false, remote_peds_created: false,
            native_queue_commands_issued: false, native_backbuffer_touched: false });
        if (!passed) throw new Error('MTA Core GUI frontend incomplete; inspect its saved state');
        return;
    }
    if (MTA_CORE_GUI_SURFACE_REFERENCE) {
        // A correlated device/window lease is consumed only after native End.
        // Core draws through MTA's own child/DComp swapchain and direct queue;
        // this path never receives native queue or backbuffer write permission.
        const version = bind('DE_CoreGuiSurfaceApiVersion', 'X64U32');
        const begin = bind('DE_CoreGuiSurfaceBegin');
        const capture = bind('DE_CoreGuiSurfaceCapture');
        const readGui = bind('DE_CoreGuiSurfaceRead', 'X64U32');
        const poll = bind('DE_CoreStartupPoll');
        const cancel = bind('DE_CoreStartupCancel');
        const read = bind('DE_CoreStartupRead', 'X64U32');
        if (version() !== 1) throw new Error('Core GUI surface API1 is required');
        emit({ event: 'adapter_start', host: HOST, cleo: String(CLEO.version), host_version: String(CLEO.hostVersion),
            definitions: String(CLEO.apiVersion), mode: MODE, endpoint: null, stop_key: stopKey,
            core_gui_surface_reference: true, provenance: 'mta_core_owned_gui_surface', core_api: 12, surface_api: 1, adapter_abi: 6,
            network_active: false, input_writes: false, remote_peds_created: false,
            native_queue_commands_issued: false, native_backbuffer_touched: false });
        enterPhase('core_gui_surface_reference');
        const startedAt = Date.now();
        if (!Number.isSafeInteger(startedAt) || startedAt < 0) throw new Error('Invalid Core GUI surface clock');
        emit({ event: 'core_gui_surface_reference_started', at_ms: startedAt, core_api: 12, surface_api: 1,
            sample_interval_ms: 100, device_wait_budget_ms: 10000, display_duration_ms: 8000, cleanup_deadline_ms: 2000 });
        if (begin() !== 1) throw new Error('MTA Core GUI surface worker could not start');
        let terminal = false, stopRequested = false, lastAt = startedAt, latest = null, samples = 0;
        const unsigned = values => values.every(value => Number.isInteger(value) && value >= 0 && value <= 0xffffffff);
        const captureSample = at => {
            const captured = capture();
            if (captured !== 0 && captured !== 1) throw new Error('Core GUI surface capture status unavailable');
            if (!captured) return;
            const state = Array.from({ length: 96 }, (_, field) => readGui(field));
            const booleanFields = [5, 6, 7, 8, 9, 10, 11, 19, 20, 21, 30, 31, 32, 37, 39, 43, 44, 45,
                49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64, 65, 66, 72, 73, 74,
                75, 76, 77, 78, 79, 80, 86];
            if (!unsigned(state) || state[0] !== 1 || ![1, 2, 3].includes(state[1]) || !state[2] || state[4] > 11 ||
                booleanFields.some(field => state[field] > 1) || ![0, 2].includes(state[18]) || state[23] > 6 ||
                state[41] > 4 || state[42] > 10 || state[47] > 9 || state[48] > 9 || state.slice(88).some(Boolean) || state[44] || state[45])
                throw new Error('Invalid Core GUI surface scalar state');
            if ((state[5] && state[6]) || (state[9] && state[43]) ||
                (state[6] && (state[1] !== 2 || state[5] || !state[9] || !state[39] || state[43] ||
                    state.slice(49, 52).some(Boolean) || state[59] || state[41] || state.slice(75, 78).some(Boolean) || state[72] || ![78, 79, 80].every(field => state[field] === 1))))
                throw new Error('Core GUI surface claimed cleanup with retained ownership');
            const pairPresent = field => state[field] !== 0 || state[field + 1] !== 0;
            // A surface can verify its first frame while the display interval is
            // still running. Readback needs owned rendering, not final success.
            if ((state[1] === 2 && (!state[6] || !state[4])) || (state[1] === 3 && (!state[5] || state[6])) ||
                (!state[10] && (state.slice(12, 19).some(Boolean) || state[58])) ||
                (state[58] && (!state[10] || state[18] !== 2 || ![12, 14, 16].every(pairPresent))) ||
                (state[19] && (![8, 9, 10, 11, 58].every(field => state[field] === 1) || state[42])) ||
                (state[20] && !state[19]) || (state[21] && (!state[19] || !state[20])) ||
                (state[37] && ![19, 30, 31, 32].every(field => state[field] === 1)) || (state[4] === 1 && (!state[20] || !state[21])) ||
                (state[35] === 0xffffffff && state[36] === 0xffffffff && !state[52]))
                throw new Error('Invalid Core GUI surface ownership state');
            if (latest && (state[2] < latest[2] || state[3] < latest[3]))
                throw new Error('Core GUI surface publication order is invalid');
            if (latest && state[2] === latest[2]) {
                if (state.some((value, field) => value !== latest[field]))
                    throw new Error('Core GUI surface changed without a new sequence');
                return;
            }
            latest = state;
            ++samples;
            emit({ event: 'core_gui_surface_reference_sample', at_ms: at, elapsed_ms: at - startedAt, state });
        };
        try {
            while (true) {
                const at = Date.now();
                if (!Number.isSafeInteger(at) || at < lastAt) throw new Error('Invalid Core GUI surface clock');
                lastAt = at;
                if (!stopRequested && (chordHeld([17, 16, 35]) || !native('IS_PLAYER_PLAYING', 0))) {
                    if (cancel() !== 1) throw new Error('MTA Core GUI surface cancellation was rejected');
                    stopRequested = true;
                    emit({ event: 'core_gui_surface_reference_stop_requested', at_ms: at });
                }
                const status = poll();
                if (status !== 0 && status !== 1) throw new Error('Core GUI surface worker status unavailable');
                captureSample(at);
                if (status === 1) { terminal = true; break; }
                wait(100);
            }
        } catch (error) {
            if (!terminal) { try { cancel(); } catch (_) {} }
            throw error;
        }
        const state = Array.from({ length: 19 }, (_, field) => read(field));
        if (!unsigned(state)) throw new Error('Invalid Core GUI surface final worker state');
        const expected = [1, 5, 6, 97, 0, 0, 0, 1, 3, 4, 5];
        const cleanupPassed = latest !== null && latest[1] === 2 && latest[6] === 1 && latest[5] === 0 && latest[7] === 0 &&
            latest[9] === 1 && latest[39] === 1 && latest[41] === 0 && latest[43] === 0 && latest.slice(49, 52).every(value => value === 0) &&
            latest[59] === 0 && latest.slice(75, 78).every(value => value === 0) && latest[72] === 0 && [78, 79, 80].every(field => latest[field] === 1) &&
            state[7] === 1 && state[13] === 1 && state[14] === 0 && state[15] === latest[57];
        const pairPresent = field => latest !== null && (latest[field] !== 0 || latest[field + 1] !== 0);
        const fenceComplete = latest !== null && !(latest[35] === 0xffffffff && latest[36] === 0xffffffff) &&
            (latest[36] > latest[34] || (latest[36] === latest[34] && latest[35] >= latest[33]));
        const passed = !stopRequested && cleanupPassed && expected.every((value, field) => state[field] === value) &&
            state[16] === 0 && state[17] === 1 && state[18] === 0 && state[11] <= 3 && state[12] <= 3 && (state[12] & ~state[11]) === 0 &&
            latest[4] === 1 && [8, 10, 11, 19, 20, 21, 30, 31, 32, 37, 53, 54, 55, 56, 58,
                60, 61, 62, 63, 64, 65, 66, 78, 79, 80, 86].every(field => latest[field] === 1) &&
            latest[18] === 2 && [12, 14, 16, 33].every(pairPresent) && latest[22] === 0 && latest[23] === 0 &&
            latest[24] > 0 && latest[27] === 87 && latest[28] > 0 && latest[28] <= 16384 && latest[29] > 0 && latest[29] <= 16384 &&
            fenceComplete && latest[38] > 0 && latest[42] === 0 && latest[44] === 0 && latest[45] === 0 && latest[46] > 0 &&
            latest[47] === 9 && latest[48] === 0 && latest[52] === 0 && latest[68] >= 2 && latest[67] === latest[68] + latest[87] &&
            latest[69] === 0 && latest[70] >= latest[68] && latest[73] === 0 && latest[81] > 0 && latest[82] > 0 &&
            latest[83] === 1 && latest[84] > 0 && latest[85] === 8000 && latest[3] >= 8000;
        emit({ event: 'core_gui_surface_reference_finished', at_ms: Date.now(), elapsed_ms: Date.now() - startedAt, state,
            passed, cleanup_passed: cleanupPassed, stop_requested: stopRequested, samples, own_surface_workload_verified: passed, composition_present_verified: passed,
            full_core_ready: false, visible_gta_drawing_verified: false, network_active: false, input_writes: false,
            remote_peds_created: false, native_queue_commands_issued: false, native_backbuffer_touched: false });
        if (!passed) throw new Error('MTA Core GUI surface incomplete; inspect its saved state');
        return;
    }
    if (MTA_CORE_GUI_STARTUP_REFERENCE) {
        // Core preloads cgui, takes the explicit strong device lease and fully
        // ends native observation before using MTA's own queue/offscreen target.
        // Only scalar evidence crosses this bridge; script receives no COM pointer.
        const version = bind('DE_CoreGuiStartupApiVersion', 'X64U32');
        const begin = bind('DE_CoreGuiStartupBegin');
        const capture = bind('DE_CoreGuiStartupCapture');
        const readGui = bind('DE_CoreGuiStartupRead', 'X64U32');
        const poll = bind('DE_CoreStartupPoll');
        const cancel = bind('DE_CoreStartupCancel');
        const read = bind('DE_CoreStartupRead', 'X64U32');
        if (version() !== 1) throw new Error('Core GUI startup API1 is required');
        emit({ event: 'adapter_start', host: HOST, cleo: String(CLEO.version), host_version: String(CLEO.hostVersion),
            definitions: String(CLEO.apiVersion), mode: MODE, endpoint: null, stop_key: stopKey,
            core_gui_startup_reference: true, provenance: 'mta_core_owned_d3d12_gui', core_api: 11, gui_api: 1, adapter_abi: 6,
            network_active: false, input_writes: false, remote_peds_created: false,
            native_queue_commands_issued: false, native_backbuffer_touched: false });
        enterPhase('core_gui_startup_reference');
        const startedAt = Date.now();
        if (!Number.isSafeInteger(startedAt) || startedAt < 0) throw new Error('Invalid Core GUI startup clock');
        emit({ event: 'core_gui_startup_reference_started', at_ms: startedAt, core_api: 11, gui_api: 1,
            sample_interval_ms: 100, device_wait_budget_ms: 10000, gpu_budget_ms: 10000, cleanup_deadline_ms: 2000 });
        if (begin() !== 1) throw new Error('MTA Core GUI startup worker could not start');
        let terminal = false, stopRequested = false, lastAt = startedAt, latest = null, samples = 0;
        const unsigned = values => values.every(value => Number.isInteger(value) && value >= 0 && value <= 0xffffffff);
        const captureSample = at => {
            const captured = capture();
            if (captured !== 0 && captured !== 1) throw new Error('Core GUI startup capture status unavailable');
            if (!captured) return;
            const state = Array.from({ length: 64 }, (_, field) => readGui(field));
            const booleanFields = [5, 6, 7, 8, 9, 10, 11, 19, 20, 21, 30, 31, 32, 37, 39, 43, 44, 45, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59];
            if (!unsigned(state) || state[0] !== 1 || ![1, 2, 3].includes(state[1]) || !state[2] || state[4] > 11 ||
                booleanFields.some(field => state[field] > 1) || ![0, 2].includes(state[18]) || state[46] > 2 ||
                state[47] > 8 || state[48] > 8 || state[60] || state[61] || state[62] || state[63] || state[44] || state[45])
                throw new Error('Invalid Core GUI startup scalar state');
            if (state[6] && (state[5] || !state[9] || !state[39] || state[43] || state[49] || state[50] || state[51] || state[59]))
                throw new Error('Core GUI startup claimed cleanup with retained ownership');
            if (latest && (state[2] < latest[2] || state[3] < latest[3]))
                throw new Error('Core GUI startup publication order is invalid');
            if (latest && state[2] === latest[2]) {
                if (state.some((value, field) => value !== latest[field]))
                    throw new Error('Core GUI startup changed without a new sequence');
                return;
            }
            latest = state;
            ++samples;
            emit({ event: 'core_gui_startup_reference_sample', at_ms: at, elapsed_ms: at - startedAt, state });
        };
        try {
            while (true) {
                const at = Date.now();
                if (!Number.isSafeInteger(at) || at < lastAt) throw new Error('Invalid Core GUI startup clock');
                lastAt = at;
                if (!stopRequested && (chordHeld([17, 16, 35]) || !native('IS_PLAYER_PLAYING', 0))) {
                    if (cancel() !== 1) throw new Error('MTA Core GUI startup cancellation was rejected');
                    stopRequested = true;
                    emit({ event: 'core_gui_startup_reference_stop_requested', at_ms: at });
                }
                const status = poll();
                if (status !== 0 && status !== 1) throw new Error('Core GUI startup worker status unavailable');
                captureSample(at);
                if (status === 1) { terminal = true; break; }
                wait(100);
            }
        } catch (error) {
            if (!terminal) { try { cancel(); } catch (_) {} }
            throw error;
        }
        const state = Array.from({ length: 19 }, (_, field) => read(field));
        if (!unsigned(state)) throw new Error('Invalid Core GUI startup final worker state');
        const expected = [1, 5, 6, 97, 0, 0, 0, 1, 3, 4, 5];
        const cleanupPassed = latest !== null && latest[1] === 2 && latest[6] === 1 && latest[5] === 0 && latest[7] === 0 &&
            latest[9] === 1 && latest[39] === 1 && latest[43] === 0 && latest.slice(49, 52).every(value => value === 0) && latest[59] === 0 &&
            state[7] === 1 && state[13] === 1 && state[14] === 0 && state[15] === latest[57];
        const pairPresent = field => latest !== null && (latest[field] !== 0 || latest[field + 1] !== 0);
        const fenceComplete = latest !== null && (latest[36] > latest[34] || (latest[36] === latest[34] && latest[35] >= latest[33]));
        const passed = !stopRequested && cleanupPassed && expected.every((value, field) => state[field] === value) &&
            state[16] === 0 && state[17] === 1 && state[18] === 0 && state[11] <= 3 && state[12] <= 3 && (state[12] & ~state[11]) === 0 &&
            latest[4] === 1 && latest[8] === 1 && latest[10] === 1 && latest[11] === 1 && latest[18] === 2 && latest[58] === 1 &&
            [12, 14, 16, 33].every(pairPresent) && latest[19] === 1 && latest[20] === 1 && latest[21] === 1 && latest[22] === 0 && latest[23] === 0 &&
            latest[24] === 1 && latest[27] === 87 && latest[28] === 640 && latest[29] === 480 && latest[30] === 1 && latest[31] === 1 && latest[32] === 1 &&
            fenceComplete && latest[37] === 1 && latest[38] > 10000 && latest[42] === 0 && latest[44] === 0 && latest[45] === 0 && latest[46] === 2 &&
            latest[52] === 0 && latest.slice(53, 57).every(value => value === 1);
        emit({ event: 'core_gui_startup_reference_finished', at_ms: Date.now(), elapsed_ms: Date.now() - startedAt, state,
            passed, cleanup_passed: cleanupPassed, stop_requested: stopRequested, samples, own_gpu_workload_verified: passed,
            full_core_ready: false, visible_gta_drawing_verified: false, network_active: false, input_writes: false,
            remote_peds_created: false, native_queue_commands_issued: false, native_backbuffer_touched: false });
        if (!passed) throw new Error('MTA Core GUI startup incomplete; inspect its saved state');
        return;
    }
    if (MTA_CORE_RENDER_REFERENCE) {
        // Core owns the optional backend observer and all teardown on its
        // worker. Script reads coherent scalar snapshots; it draws nothing
        // and never receives viewport, swapchain or device pointers.
        const observationApi = MTA_CORE_RENDER_NATIVE_DISPATCH_REFERENCE ? 6 : MTA_CORE_RENDER_FRAME_BOUNDARY_REFERENCE ? 5 : MTA_CORE_RENDER_CUSTOM_PRESENT_REFERENCE ? 4 : MTA_CORE_RENDER_GPU_REFERENCE ? 3 : 2;
        const coreApi = MTA_CORE_RENDER_NATIVE_DISPATCH_REFERENCE ? 10 : MTA_CORE_RENDER_FRAME_BOUNDARY_REFERENCE ? 9 : MTA_CORE_RENDER_CUSTOM_PRESENT_REFERENCE ? 8 : MTA_CORE_RENDER_GPU_REFERENCE ? 7 : 6;
        const renderApiVersion = bind('DE_CoreRenderApiVersionV' + observationApi, 'X64U32');
        const begin = bind('DE_CoreRenderBeginV' + observationApi);
        const capture = bind('DE_CoreRenderCaptureV' + observationApi);
        const readRender = bind('DE_CoreRenderReadV' + observationApi, 'X64U32');
        const poll = bind('DE_CoreStartupPoll');
        const cancel = bind('DE_CoreStartupCancel');
        const read = bind('DE_CoreStartupRead', 'X64U32');
        if (renderApiVersion() !== observationApi) throw new Error('Core renderer observation API ' + observationApi + ' is required');
        const requestedRoute = MTA_CORE_RENDER_ROUTE === 'd3d11' ? 1 : 2;
        const expectedPresence = requestedRoute === 1 ? 15 : 3;
        const expectedKnown = requestedRoute === 1 ? 15 : 19;
        // These are metadata eligibility checks, not permission to submit GPU
        // work. Engine queue ordering, target states and resize remain separate.
        const gpuEligible = state => state[54] === 0 && state[57] <= 1 && state[58] === 1 && state[61] === 7 && state[63] >= 2 && state[63] <= 16 && state[62] < state[63] && state[64] > 0 &&
            state[66] > 0 && state[66] <= 32768 && state[67] > 0 && state[67] <= 32768 &&
            state[68] === state[66] && state[69] === 0 && state[70] === state[67] && state[71] === state[64] &&
            state[72] === 1 && state[73] === 0 && state[74] === 0 && state[76] === 1 && state[77] === 1;
        const gpuSupported = state => state[50] === 19 && state[52] === 1 && state[53] === 1 && state[51] === 0 && state[75] === 1 && gpuEligible(state);
        const compare64 = (state, a, b) => state[a + 1] === state[b + 1] ? Math.sign(state[a] - state[b]) : Math.sign(state[a + 1] - state[b + 1]);
        const gpuConsistent = (state, previous) => {
            const sum = state[46] + state[48];
            if (state[44] !== sum % 0x100000000 || state[45] !== state[47] + state[49] + Math.floor(sum / 0x100000000) ||
                compare64(state, 44, 19) !== 0 || state[50] > 19 || state[52] > 1 || state[53] > 1 || state[61] > 7 || state[74] > 1 ||
                state[75] > 6 || state[78] !== 0 || state[79] !== 0)
                return false;
            if (previous && [44, 46, 48].some(field => state[field + 1] < previous[field + 1] ||
                    (state[field + 1] === previous[field + 1] && state[field] < previous[field])))
                return false;
            if (state[44] === 0 && state[45] === 0)
                return state.slice(46, 80).every(value => value === 0);
            if (state[50] === 0 || state[75] === 0 || (state[53] && !gpuSupported(state)))
                return false;
            if (state[75] === 1)
                return gpuSupported(state) && (state[46] !== 0 || state[47] !== 0);
            if (state[53] !== 0 || (state[48] === 0 && state[49] === 0))
                return false;
            if (state[75] === 2)
                return state[51] === 0 && (state[52] === 1 ? state[50] === 19 && !gpuEligible(state) :
                    state[50] === 2 ? state[74] === 1 : state[50] === 3 && state[74] === 0 && (state[76] !== 1 || state[77] !== 1));
            if (state[52] !== 0) return false;
            if (state[75] === 3)
                return (state[50] === 1 || state[50] === 3 || (state[50] >= 4 && state[50] <= 16)) && state[51] >= 0x80000000;
            if (state[75] === 4) return state[50] >= 4 && state[50] <= 16 && state[51] === 0x8000ffff;
            if (state[75] === 5) return state[50] === 17 && state[51] === 0;
            return state[75] === 6 && state[50] === 18 && state[51] === 0x8000ffff;
        };
        const customPresentConsistent = state => {
            const [status, implementation, signatures, policies] = state.slice(80, 84);
            if (status > 7 || signatures > 127) return false;
            if (state[44] === 0 && state[45] === 0)
                return status === 0 && implementation === 0 && signatures === 0 && policies === 0;
            if (status === 0) return false;
            if (status === 2)
                return state[74] === 1 && implementation === 1 && signatures === 127 && policies === 15;
            if (implementation !== 0 || policies !== 0 || ([1, 3, 7].includes(status) && signatures !== 0)) return false;
            return status === 1 ? state[74] === 0 : [5, 7].includes(status) || state[74] === 1;
        };
        const boundaryConsistent = (state, previous, offset = 84, expectedProfile = 1) => {
            // Validate each independent tail against its own profile/counters;
            // neither can supply missing evidence for the other hook.
            if (offset !== 84) {
                state = state.slice(0, 84).concat(state.slice(offset, offset + 26));
                if (previous) previous = previous.slice(0, 84).concat(previous.slice(offset, offset + 26));
            }
            const [status, profile, facts, presentHresult] = state.slice(84, 88);
            const [stage, queryHresult] = state.slice(108, 110);
            const pairs = [92, 94, 96, 98, 100, 102];
            const anyPair = field => state[field] !== 0 || state[field + 1] !== 0;
            if (status > 10 || (profile !== 0 && profile !== expectedProfile) || facts > 255 || state[90] > 0x7fffffff || state[91] > 1 ||
                (!state[91] && state[90]) || state[107] > 3 || stage > 9 || compare64(state, 94, 92) > 0)
                return false;
            if (previous && pairs.some(field => state[field + 1] < previous[field + 1] ||
                    (state[field + 1] === previous[field + 1] && state[field] < previous[field])))
                return false;
            if (!state[44] && !state[45]) return state.slice(84, 110).every(value => value === 0);
            // Fatal earlier COM cleanup can disarm before this separate query
            // begins. A zero tail then describes no Present association at all.
            if (!status) return state[30] === 10 && state[13] === 0 && state.slice(84, 110).every(value => value === 0);
            if (status === 1 || status === 10)
                return profile === (status === 10 ? expectedProfile : 0) && !facts && !presentHresult && !stage && !queryHresult &&
                    state.slice(88, 92).every(value => value === 0) && !state[104];
            if (status === 5) {
                const entered = profile === expectedProfile && anyPair(92) && anyPair(94) && state[104] !== 0;
                return stage >= 1 && stage <= 9 && queryHresult >= 0x80000000 &&
                    (stage !== 9 || queryHresult === 0x8000ffff) &&
                    (entered || (stage === 1 && facts === 0 && state[104] === 0 && presentHresult === 0));
            }
            // An owned-patch mismatch can refuse before any Present entry.
            if (status === 8 && !state[104])
                return profile === expectedProfile && !facts && !presentHresult && !stage && !queryHresult && state.slice(88, 92).every(value => value === 0);
            if (profile !== expectedProfile || !anyPair(92) || !state[104]) return false;
            if (status === 2 || status === 9)
                return facts === 255 && state[80] === 2 && state[88] === state[89] && state[89] < 16 &&
                    state[90] === 0 && state[91] === 1 && state[104] === state[25] && !state[107] && stage === 9 && queryHresult === 0 &&
                    (status === 2 ? presentHresult < 0x80000000 && compare64(state, 92, 94) > 0 : presentHresult >= 0x80000000 && anyPair(94));
            if (!anyPair(94)) return false;
            if (status === 4 || status === 8)
                return !facts && !stage && !queryHresult && state.slice(88, 92).every(value => value === 0);
            if (status === 3) return stage >= 1 && stage <= 8 && queryHresult === 1;
            if (status === 6) return stage === 8 && queryHresult === 1 && facts === 127;
            return status === 7 && ((stage === 0 && queryHresult === 0 && facts === 0) ||
                (stage === 8 && queryHresult === 1 && facts === 127) || (stage === 9 && queryHresult === 0 && facts === 127));
        };
        emit({ event: 'adapter_start', host: HOST, cleo: String(CLEO.version), host_version: String(CLEO.hostVersion),
            definitions: String(CLEO.apiVersion), mode: MODE, endpoint: null, stop_key: stopKey,
            core_render_reference: true, provenance: 'mta_core_native_render_observation', core_api: coreApi,
            observation_api: observationApi, requested_route: requestedRoute,
            network_active: false, input_writes: false, remote_peds_created: false });
        enterPhase('core_render_reference');
        const startedAt = Date.now();
        if (!Number.isSafeInteger(startedAt) || startedAt < 0) throw new Error('Invalid Core render clock');
        emit({ event: 'core_render_reference_started', at_ms: startedAt, core_api: coreApi, observation_api: observationApi,
            requested_route: requestedRoute, sample_interval_ms: 100, budget_ms: 10000 });
        if (begin(requestedRoute) !== 1) throw new Error('MTA Core renderer observation worker could not start');
        let terminal = false, stopRequested = false, lastAt = startedAt, lastSequence = 0, lastElapsed = -1;
        let latest = null, samples = 0;
        const captureSample = at => {
            const captured = capture();
            if (captured !== 0 && captured !== 1) throw new Error('Core render capture status unavailable');
            if (!captured) return;
            const state = Array.from({ length: observationApi === 6 ? 136 : observationApi === 5 ? 110 : observationApi === 4 ? 84 : observationApi === 3 ? 80 : 44 }, (_, field) => readRender(field));
            if (!state.every(value => Number.isInteger(value) && value >= 0 && value <= 0xffffffff))
                throw new Error('Core render returned invalid unsigned state');
            if (state[0] !== observationApi || ![1, 2, 3].includes(state[1]) || state[2] < lastSequence || state[3] < lastElapsed)
                throw new Error('Core render publication order is invalid');
            // Requested route is an admission choice, while observed route
            // requires a returned native callback. Never infer it from DLLs.
            if (state[40] !== requestedRoute || ![0, requestedRoute].includes(state[41]) ||
                state[42] !== (state[41] ? expectedKnown : 0) || state[43] !== (state[41] && requestedRoute === 2 ? 1 : 0) ||
                state[37] !== (state[41] ? expectedPresence : 0) || state[38] !== 0 || state[39] !== 0 ||
                (state[41] !== 0 && ((state[19] === 0 && state[20] === 0) || state[31] !== 1)) ||
                (state[41] === 0 && (state[19] !== 0 || state[20] !== 0 || state.slice(25, 30).some(value => value !== 0) || state[31] !== 0)))
                throw new Error('Core render route or presence evidence is contradictory');
            if (state[3] > at - startedAt + 250) throw new Error('Core render elapsed time exceeds its script clock');
            // Add split64 counters with exact integer arithmetic, without
            // relying on lossy Number conversion or a new FFI numeric type.
            const outcomes = [19, 21, 33, 35];
            const lowSum = outcomes.reduce((sum, field) => sum + state[field], 0);
            const highSum = outcomes.reduce((sum, field) => sum + state[field + 1], Math.floor(lowSum / 0x100000000));
            const low = lowSum % 0x100000000;
            if (highSum > state[18] || (highSum === state[18] && low > state[17]) ||
                state[24] > state[18] || (state[24] === state[18] && state[23] > state[17]))
                throw new Error('Core render outcomes exceed native callback entries');
            if (observationApi >= 3 && !gpuConsistent(state, latest))
                throw new Error('Core GPU ownership metadata or counters are contradictory');
            // Classification preserves API3's unsupported custom-present result.
            // The additive tail describes checked code/association, never a GPU frame boundary.
            if (observationApi >= 4 && !customPresentConsistent(state))
                throw new Error('Core custom-present classification is contradictory');
            if (observationApi >= 5 && !boundaryConsistent(state, latest))
                throw new Error('Core scoped Present association or lifecycle counters are contradictory');
            if (observationApi === 6 && !boundaryConsistent(state, latest, 110, 2))
                throw new Error('Core native dispatch association or lifecycle counters are contradictory');
            if (state[2] === lastSequence) {
                if (!latest || state.some((value, field) => value !== latest[field]))
                    throw new Error('Core render publication changed without a new sequence');
                return;
            }
            lastSequence = state[2]; lastElapsed = state[3]; latest = state;
            ++samples;
            emit({ event: 'core_render_reference_sample', at_ms: at, elapsed_ms: at - startedAt, state });
        };
        try {
            while (true) {
                const at = Date.now();
                if (!Number.isSafeInteger(at) || at < lastAt) throw new Error('Invalid Core render clock');
                lastAt = at;
                if (!stopRequested) {
                    const stop = chordHeld([17, 16, 35]);
                    if (stop || !native('IS_PLAYER_PLAYING', 0)) {
                        if (cancel() !== 1) throw new Error('MTA Core render cancellation was rejected');
                        stopRequested = true;
                        emit({ event: 'core_render_reference_stop_requested', at_ms: at, reason: stop ? stopKey : 'loading' });
                    }
                }
                const status = poll();
                if (status !== 0 && status !== 1) throw new Error('MTA Core render worker status unavailable');
                captureSample(at);
                if (status === 1) { terminal = true; break; }
                // Pending hook detachment retains Core and keeps yielding,
                // including after any diagnostic cleanup deadline has elapsed.
                wait(100);
            }
        } catch (error) {
            if (!terminal) { try { cancel(); } catch (_) {} }
            throw error;
        }
        const state = Array.from({ length: 19 }, (_, field) => read(field));
        if (!state.every(value => Number.isInteger(value) && value >= 0 && value <= 0xffffffff))
            throw new Error('Core render returned invalid unsigned final state');
        const now = Date.now();
        if (!Number.isSafeInteger(now) || now < lastAt) throw new Error('Invalid Core render clock');
        const pinned = latest ? latest[9] : 0;
        const cleanupPassed = state[7] === 1 && state[13] === 1 && state[14] === 0 && state[15] === pinned &&
            latest !== null && latest[1] === 2 && latest[6] === 1 && latest[10] === 0 && latest[13] === 0 && latest[32] === 0;
        const expected = [1, 5, 6, 97, 0, 0, 0, 1, 3, 4, 5];
        const runResult = state[17] === 1 && state[16] === 0 ? 1 : 0;
        const observed = latest !== null && latest[1] === 2 && latest[3] >= 10000 && latest[3] <= 12000 && latest[4] === 1 &&
            latest[5] > 0 && latest[7] > 0 && (latest[15] !== 0 || latest[16] !== 0) &&
            latest[8] === 0 && latest[9] === 1 && latest[11] === 1 && latest[12] === 1 && latest[14] === 1 &&
            (latest[17] !== 0 || latest[18] !== 0) && (latest[19] !== 0 || latest[20] !== 0) &&
            latest.slice(21, 25).every(value => value === 0) && latest[25] !== 0 &&
            latest[26] > 0 && latest[26] <= 32768 && latest[27] > 0 && latest[27] <= 32768 &&
            latest[29] <= 1 && latest[30] === 0 && latest[31] === 1 && latest[32] === 0 &&
            latest[33] === 0 && latest[34] === 0 && latest[37] === expectedPresence &&
            latest[38] === 0 && latest[39] === 0 && latest[40] === requestedRoute &&
            latest[41] === requestedRoute && latest[42] === expectedKnown && latest[43] === (requestedRoute === 2 ? 1 : 0);
        const gpuObserved = observationApi < 3 || (latest !== null && gpuSupported(latest) &&
            (latest[46] !== 0 || latest[47] !== 0) && latest[48] === 0 && latest[49] === 0);
        const passed = observationApi < 5 && !stopRequested && runResult === 1 && expected.every((value, field) => state[field] === value) &&
            state[11] <= 3 && state[12] <= 3 && (state[12] & ~state[11]) === 0 && cleanupPassed && state[18] === 0 && observed && gpuObserved;
        emit({ event: 'core_render_reference_finished', at_ms: now, elapsed_ms: now - startedAt, state, run_result: runResult,
            passed, cleanup_passed: cleanupPassed, stop_requested: stopRequested, samples,
            full_core_ready: observationApi >= 5 ? false : state[6] === 1, rendering_started: false, network_active: false,
            input_writes: false, remote_peds_created: false });
        if (!passed) throw new Error('MTA Core renderer observation incomplete; inspect its saved state');
        return;
    }
    if (MTA_CORE_MOVEMENT_REFERENCE) {
        // Core owns XML, configured bindings and the eligible local write scope.
        // JS only observes coherent value snapshots and requests cancellation;
        // pending native restoration retains the worker instead of faking unload.
        const begin = bind('DE_CoreMovementBegin');
        const capture = bind('DE_CoreMovementCapture');
        const readMovement = bind('DE_CoreMovementRead', 'X64U32');
        const poll = bind('DE_CoreStartupPoll');
        const cancel = bind('DE_CoreStartupCancel');
        const read = bind('DE_CoreStartupRead', 'X64U32');
        emit({ event: 'adapter_start', host: HOST, cleo: String(CLEO.version), host_version: String(CLEO.hostVersion),
            definitions: String(CLEO.apiVersion), mode: MODE, endpoint: null, stop_key: stopKey,
            core_movement_reference: true, provenance: 'mta_core_configured_movement', core_api: 4, adapter_abi: 6,
            network_active: false, input_writes: true, remote_peds_created: false });
        enterPhase('core_movement_reference');
        const startedAt = Date.now();
        if (!Number.isSafeInteger(startedAt) || startedAt < 0) throw new Error('Invalid Core movement clock');
        emit({ event: 'core_movement_reference_started', at_ms: startedAt, core_api: 4, adapter_abi: 6, sample_interval_ms: 100,
            budget_ms: 30000, readiness_budget_ms: 10000, cleanup_deadline_ms: 2000, context_source: 'engine_eligibility' });
        if (begin() !== 1) throw new Error('MTA Core configured movement worker could not start');
        let terminal = false, stopRequested = false, lastAt = startedAt, lastSequence = 0, lastElapsed = -1;
        let latest = null, samples = 0;
        const captureSample = at => {
            const captured = capture();
            if (captured !== 0 && captured !== 1) throw new Error('Core movement capture status unavailable');
            if (!captured) return false;
            const state = Array.from({ length: 96 }, (_, field) => readMovement(field));
            if (!state.every(value => Number.isInteger(value) && value >= 0 && value <= 0xffffffff))
                throw new Error('Core movement returned invalid unsigned state');
            if (state[0] !== 1 || ![1, 2, 3].includes(state[1]) || state[2] < lastSequence || state[3] < lastElapsed)
                throw new Error('Core movement publication order is invalid');
            // CLEO may sample the same protected frame twice or skip native
            // pulses. Preserve each distinct sequence once, including terminal.
            if (state[2] === lastSequence) {
                if (!latest || state.some((value, field) => value !== latest[field]))
                    throw new Error('Core movement publication changed without a new sequence');
                return true;
            }
            lastSequence = state[2]; lastElapsed = state[3]; latest = state;
            ++samples;
            emit({ event: 'core_movement_reference_sample', at_ms: at, elapsed_ms: at - startedAt, state });
            return true;
        };
        try {
            while (true) {
                const at = Date.now();
                if (!Number.isSafeInteger(at) || at < lastAt) throw new Error('Invalid Core movement clock');
                lastAt = at;
                if (!stopRequested) {
                    const stop = chordHeld([17, 16, 35]);
                    if (stop || !native('IS_PLAYER_PLAYING', 0)) {
                        if (cancel() !== 1) throw new Error('MTA Core movement cancellation was rejected');
                        stopRequested = true;
                        emit({ event: 'core_movement_reference_stop_requested', at_ms: at, reason: stop ? stopKey : 'loading' });
                    }
                }
                const status = poll();
                if (status !== 0 && status !== 1) throw new Error('MTA Core movement worker status unavailable');
                captureSample(at);
                if (status === 1) { terminal = true; break; }
                // DLL loading and owner cleanup may exceed CLEO's watchdog;
                // pending work always yields instead of blocking script ticks.
                wait(100);
            }
        } catch (error) {
            if (!terminal) { try { cancel(); } catch (_) {} }
            throw error;
        }
        const state = Array.from({ length: 19 }, (_, field) => read(field));
        if (!state.every(value => Number.isInteger(value) && value >= 0 && value <= 0xffffffff))
            throw new Error('Core movement returned invalid unsigned final state');
        const now = Date.now();
        if (!Number.isSafeInteger(now) || now < lastAt) throw new Error('Invalid Core movement clock');
        const cleanupPassed = state[7] === 1 && state[13] === 1 && state[14] === 0 && state[15] === (latest === null ? 0 : latest[94]);
        const expected = [1, 5, 6, 97, 0, 0, 0, 1, 3, 4, 5];
        const runResult = state[17] === 1 && state[16] === 0 ? 1 : 0;
        const pairZero = index => latest !== null && latest[64 + index * 2] === 0 && latest[65 + index * 2] === 0;
        const pairEqual = (left, right) => latest !== null && latest[64 + left * 2] === latest[64 + right * 2] &&
            latest[65 + left * 2] === latest[65 + right * 2];
        const passed = !stopRequested && runResult === 1 && expected.every((value, field) => state[field] === value) &&
            state[11] <= 3 && state[12] <= 3 && (state[12] & ~state[11]) === 0 && cleanupPassed && state[18] === 0 &&
            latest !== null && latest[1] === 2 && latest[14] === 1 && latest[15] === 1 &&
            latest[4] === 0 && latest[88] === 15 && latest[89] === 15 && latest[90] === 1 &&
            latest[91] === 1 && latest[92] >= 1 && latest[93] === 0 && latest[94] === 1 && latest[95] === 0 &&
            latest.slice(16, 64).every(value => value === 0) && latest[12] === 0 && latest[13] === 0 &&
            pairZero(0) && pairZero(1) && !pairZero(3) && !pairZero(4) && pairEqual(3, 6) && latest[87] === 0 && latest[86] !== 0xffffffff &&
            [7, 8, 9, 10].every(pairZero);
        emit({ event: 'core_movement_reference_finished', at_ms: now, elapsed_ms: now - startedAt, state, run_result: runResult,
            passed, cleanup_passed: cleanupPassed, stop_requested: stopRequested, samples,
            full_core_ready: state[6] === 1, rendering_started: false, network_active: false,
            input_writes: true, remote_peds_created: false });
        if (!passed) throw new Error('MTA Core configured movement reference failed; inspect its saved state');
        return;
    }
    if (MTA_CORE_INPUT_REFERENCE) {
        // The worker loads real XML records and pulses Core's existing bind
        // owner. Capture copies one publication before field reads; it never
        // exposes Core pointers, polls keyboard state in JS, or writes the pad.
        const begin = bind('DE_CoreInputBegin');
        const capture = bind('DE_CoreInputCapture');
        const readInput = bind('DE_CoreInputRead', 'X64U32');
        const poll = bind('DE_CoreStartupPoll');
        const cancel = bind('DE_CoreStartupCancel');
        const read = bind('DE_CoreStartupRead', 'X64U32');
        emit({ event: 'adapter_start', host: HOST, cleo: String(CLEO.version), host_version: String(CLEO.hostVersion),
            definitions: String(CLEO.apiVersion), mode: MODE, endpoint: null, stop_key: stopKey,
            core_input_reference: true, provenance: 'mta_core_configured_input', core_api: 3,
            network_active: false, input_writes: false, remote_peds_created: false });
        enterPhase('core_input_reference');
        const startedAt = Date.now();
        if (!Number.isSafeInteger(startedAt) || startedAt < 0) throw new Error('Invalid Core input clock');
        emit({ event: 'core_input_reference_started', at_ms: startedAt, core_api: 3, sample_interval_ms: 100,
            budget_ms: 60000, context_source: 'assumed_on_foot', context: { in_vehicle: false, has_detonator: false,
                is_dead: false, aiming_weapon: false, entering_vehicle: false } });
        if (begin() !== 1) throw new Error('MTA Core configured input worker could not start');
        let terminal = false, stopRequested = false, lastAt = startedAt, lastSequence = 0, lastElapsed = -1;
        let latest = null, samples = 0;
        const captureSample = at => {
            const captured = capture();
            if (captured !== 0 && captured !== 1) throw new Error('Core input capture status unavailable');
            if (!captured) return false;
            const state = Array.from({ length: 64 }, (_, field) => readInput(field));
            if (!state.every(value => Number.isInteger(value) && value >= 0 && value <= 0xffffffff))
                throw new Error('Core input returned invalid unsigned state');
            if (state[0] !== 1 || ![1, 2].includes(state[1]) || state[2] < lastSequence || state[3] < lastElapsed)
                throw new Error('Core input publication order is invalid');
            // CLEO may sample the same protected frame twice or skip native
            // pulses. Preserve each distinct sequence once, including terminal.
            if (state[2] === lastSequence) {
                if (!latest || state.some((value, field) => value !== latest[field]))
                    throw new Error('Core input publication changed without a new sequence');
                return true;
            }
            lastSequence = state[2]; lastElapsed = state[3]; latest = state;
            ++samples;
            emit({ event: 'core_input_reference_sample', at_ms: at, elapsed_ms: at - startedAt, state });
            return true;
        };
        try {
            while (true) {
                const at = Date.now();
                if (!Number.isSafeInteger(at) || at < lastAt) throw new Error('Invalid Core input clock');
                lastAt = at;
                if (!stopRequested) {
                    const stop = chordHeld([17, 16, 35]);
                    if (stop || !native('IS_PLAYER_PLAYING', 0)) {
                        if (cancel() !== 1) throw new Error('MTA Core input cancellation was rejected');
                        stopRequested = true;
                        emit({ event: 'core_input_reference_stop_requested', at_ms: at, reason: stop ? stopKey : 'loading' });
                    }
                }
                const status = poll();
                if (status !== 0 && status !== 1) throw new Error('MTA Core input worker status unavailable');
                captureSample(at);
                if (status === 1) { terminal = true; break; }
                // DLL loading and owner cleanup may exceed CLEO's watchdog;
                // pending work always yields instead of blocking script ticks.
                wait(100);
            }
        } catch (error) {
            if (!terminal) { try { cancel(); } catch (_) {} }
            throw error;
        }
        const state = Array.from({ length: 19 }, (_, field) => read(field));
        if (!state.every(value => Number.isInteger(value) && value >= 0 && value <= 0xffffffff))
            throw new Error('Core input returned invalid unsigned final state');
        const now = Date.now();
        if (!Number.isSafeInteger(now) || now < lastAt) throw new Error('Invalid Core input clock');
        const cleanupPassed = state[7] === 1 && state[13] === 1 && state[14] === 0 && state[15] === 0;
        const expected = [1, 5, 6, 97, 0, 0, 0, 1, 3, 4, 5];
        const runResult = state[17] === 1 && state[16] === 0 ? 1 : 0;
        const passed = !stopRequested && runResult === 1 && expected.every((value, field) => state[field] === value) &&
            state[11] <= 3 && state[12] <= 3 && (state[12] & ~state[11]) === 0 && cleanupPassed && state[18] === 0 &&
            latest !== null && latest[1] === 2 && latest[14] === 1 && latest[15] === 1;
        emit({ event: 'core_input_reference_finished', at_ms: now, elapsed_ms: now - startedAt, state, run_result: runResult,
            passed, cleanup_passed: cleanupPassed, stop_requested: stopRequested, samples,
            full_core_ready: state[6] === 1, rendering_started: false, network_active: false,
            input_writes: false, remote_peds_created: false });
        if (!passed) throw new Error('MTA Core configured input reference failed; inspect its saved state');
        return;
    }
    if (MTA_CORE_STARTUP_REFERENCE) {
        // Exercise Core's real entry and adapter ownership inside the verified
        // game. Result five is expected at its complete-gameplay gate; treating
        // that as a playable client would hide the remaining CGame/render work.
        const begin = bind('DE_CoreStartupBegin');
        const poll = bind('DE_CoreStartupPoll');
        const cancel = bind('DE_CoreStartupCancel');
        const read = bind('DE_CoreStartupRead', 'X64U32');
        emit({ event: 'adapter_start', host: HOST, cleo: String(CLEO.version), host_version: String(CLEO.hostVersion),
            definitions: String(CLEO.apiVersion), mode: MODE, endpoint: null, stop_key: stopKey,
            core_startup_reference: true, provenance: 'mta_core_de_startup', core_api: 2,
            network_active: false, input_writes: false, remote_peds_created: false });
        enterPhase('core_startup_reference');
        const startedAt = Date.now();
        if (!Number.isSafeInteger(startedAt) || startedAt < 0) throw new Error('Invalid Core startup clock');
        emit({ event: 'core_startup_reference_started', at_ms: startedAt, core_api: 2 });
        if (begin() !== 1) throw new Error('MTA Core startup worker could not start');
        let terminal = false, stopRequested = false, lastAt = startedAt;
        try {
            while (true) {
                const at = Date.now();
                if (!Number.isSafeInteger(at) || at < lastAt) throw new Error('Invalid Core startup clock');
                lastAt = at;
                if (!stopRequested) {
                    const stop = chordHeld([17, 16, 35]);
                    if (stop || !native('IS_PLAYER_PLAYING', 0)) {
                        if (cancel() !== 1) throw new Error('MTA Core startup cancellation was rejected');
                        stopRequested = true;
                        emit({ event: 'core_startup_reference_stop_requested', at_ms: at,
                            reason: stop ? stopKey : 'loading' });
                    }
                }
                const status = poll();
                if (status === 1) { terminal = true; break; }
                if (status !== 0) throw new Error('MTA Core startup worker status unavailable');
                // Native loading can exceed CLEO's two-second watchdog. The
                // bridge owns the entire Core lifecycle on one worker; this
                // loop yields and observes only completed value snapshots.
                // Cancellation still polls until owner-thread cleanup ends.
                wait(50);
            }
        } catch (error) {
            // An invalid FFI/clock result cannot authorize cached reads. Ask
            // the pinned worker to unwind without blocking script execution.
            if (!terminal) { try { cancel(); } catch (_) {} }
            throw error;
        }
        const state = Array.from({ length: 19 }, (_, field) => read(field));
        if (!state.every(value => Number.isInteger(value) && value >= 0 && value <= 0xffffffff))
            throw new Error('Core startup returned invalid unsigned state');
        const now = Date.now();
        if (!Number.isSafeInteger(now) || now < lastAt) throw new Error('Invalid Core startup clock');
        const cleanupPassed = state[7] === 1 && state[13] === 1 && state[14] === 0 && state[15] === 0;
        const expected = [1, 5, 6, 97, 0, 0, 0, 1, 3, 4, 5];
        const runResult = state[17] === 1 && state[16] === 0 ? 1 : 0;
        const passed = !stopRequested && runResult === 1 && expected.every((value, field) => state[field] === value) &&
            state[11] <= 3 && state[12] <= 3 && (state[12] & ~state[11]) === 0 && cleanupPassed && state[18] === 0;
        emit({ event: 'core_startup_reference_finished', at_ms: now, elapsed_ms: now - startedAt, run_result: runResult,
            state, passed, cleanup_passed: cleanupPassed, stop_requested: stopRequested,
            full_core_ready: state[6] === 1, rendering_started: false,
            network_active: false, input_writes: false, remote_peds_created: false });
        if (!passed) throw new Error('MTA Core startup reference failed; inspect its saved state');
        return;
    }
    if (MTA_NATIVE_MOVEMENT_TRIAL) {
        const reader = { begin: bind('DE_MtaMovementBegin'), submit: bind('DE_MtaMovementSubmit'),
            capture: bind('DE_MtaMovementCapture'), read: bind('DE_MtaMovementRead', 'X64U32'), end: bind('DE_MtaMovementEnd') };
        const game = { stopRequested: () => chordHeld([17, 16, 35]), isReady: () => native('IS_PLAYER_PLAYING', 0),
            localPed: () => native('GET_PLAYER_CHAR', 0), doesPedExist: handle => native('DOES_CHAR_EXIST', handle),
            readVelocity: handle => native('GET_CHAR_VELOCITY', handle),
            readTransform: handle => ({ ...native('GET_CHAR_COORDINATES', handle), heading: native('GET_CHAR_HEADING', handle) }) };
        emit({ event: 'adapter_start', host: HOST, cleo: String(CLEO.version), host_version: String(CLEO.hostVersion),
            definitions: String(CLEO.apiVersion), mode: MODE, endpoint: null, stop_key: stopKey,
            native_movement_trial: true, provenance: 'mta_game_de_scoped_local_movement', adapter_abi: 6,
            network_active: false, input_writes: true, remote_peds_created: false });
        controller = new MtaNativeMovementTrialController(game, reader, { log: emit, stopReason: stopKey });
        enterPhase('controller_run');
        while (controller.running) { controller.tick(Date.now()); if (controller.running) wait(50); }
        return;
    }
    if (MTA_NATIVE_PAD_REFERENCE) {
        const nativePadRead = bind('DE_NativePadRead', 'X64U32');
        const nativeDeviceRead = bind('DE_NativeDeviceRead', 'X64U32');
        const resetAddress = library.getProcedure('DE_NativePadReset');
        if (!Number.isSafeInteger(resetAddress) || resetAddress <= 0) throw new Error('Missing bridge procedure: DE_NativePadReset');
        const reader = {
            bind: bind('DE_NativePadBind'), capture: bind('DE_NativePadCapture'),
            read: (pad, which, index) => nativePadRead(pad, which, index),
            readDevice: index => nativeDeviceRead(index),
            reset: () => Memory.CallFunction(resetAddress, false, 0)
        };
        const game = {
            stopRequested: () => chordHeld([17, 16, 35]),
            isReady: () => native('IS_PLAYER_PLAYING', 0),
            localPed: () => native('GET_PLAYER_CHAR', 0),
            doesPedExist: handle => native('DOES_CHAR_EXIST', handle),
            readVelocity: handle => native('GET_CHAR_VELOCITY', handle),
            readTransform: handle => ({ ...native('GET_CHAR_COORDINATES', handle), heading: native('GET_CHAR_HEADING', handle) })
        };
        emit({ event: 'adapter_start', host: HOST, cleo: String(CLEO.version), host_version: String(CLEO.hostVersion),
            definitions: String(CLEO.apiVersion), mode: MODE, endpoint: null, stop_key: stopKey,
            native_pad_reference: true, provenance: 'mta_game_de_native_pad_reader',
            network_active: false, input_writes: false, remote_peds_created: false });
        controller = new MtaNativePadReferenceController(game, reader, { log: emit, stopReason: stopKey });
        enterPhase('controller_run');
        while (controller.running) {
            controller.tick(Date.now());
            if (controller.running) wait(50);
        }
        return;
    }
    if (INPUT_REFERENCE) {
        // Keep this probe before transport bindings and provide only native
        // reads. Canonical state publication changes our C++ snapshots, not
        // GTA's pad, player tasks, physical velocity, or local-player ownership.
        const begin = bind('DE_ControllerBegin'), set = bind('DE_ControllerSet'), commit = bind('DE_ControllerCommit');
        const read = bind('DE_ControllerRead', 'X64U32');
        const currentMask = bind('DE_ControllerCurrentMask', 'X64U32');
        const previousMask = bind('DE_ControllerPreviousMask', 'X64U32');
        const resetAddress = library.getProcedure('DE_ControllerReset');
        if (!Number.isSafeInteger(resetAddress) || resetAddress <= 0) throw new Error('Missing bridge procedure: DE_ControllerReset');
        const canonical = { begin, set, commit, currentMask, previousMask,
            read: (which, index) => {
                const bits = read(which, index);
                if (!Number.isInteger(bits) || bits < 0 || bits > 0xffffffff)
                    throw new Error('Canonical controller returned invalid unsigned readback');
                return bits | 0;
            },
            reset: () => Memory.CallFunction(resetAddress, false, 0) };
        const localReads = {
            stopRequested: () => chordHeld([17, 16, 35]),
            isReady: () => native('IS_PLAYER_PLAYING', 0),
            localPed: () => native('GET_PLAYER_CHAR', 0),
            doesPedExist: handle => native('DOES_CHAR_EXIST', handle),
            readPadState: index => native('GET_PAD_STATE', 0, index),
            readVelocity: handle => native('GET_CHAR_VELOCITY', handle),
            readTransform: handle => ({ ...native('GET_CHAR_COORDINATES', handle), heading: native('GET_CHAR_HEADING', handle) })
        };
        emit({ event: 'adapter_start', host: HOST, cleo: String(CLEO.version), host_version: String(CLEO.hostVersion),
            definitions: String(CLEO.apiVersion), mode: MODE, endpoint: null, stop_key: stopKey,
            native_input_reference: true, network_active: false, input_writes: false, remote_peds_created: false });
        controller = new NativeInputReferenceController(localReads, canonical, { log: emit, stopReason: stopKey });
        enterPhase('controller_run');
        while (controller.running) {
            controller.tick(Date.now());
            if (controller.running) wait(50);
        }
        return;
    }
    // BindRuntime pins the library because CLEO holds a callback into it.
    // Do not free it while those callbacks can still execute. No JS buffer
    // allocations or classic game pointers are needed by this integer ABI.
    const connect = bind('DE_ConnectLocal');
    const pump = bind('DE_Pump');
    const localId = bind('DE_LocalPlayerId', 'X64U32');
    const submit = bind('DE_SubmitTransformBits');
    const playerCount = bind('DE_PlayerCount', 'X64U32');
    const component = bind('DE_ReadPlayerComponent', 'X64U32');
    const disconnectAddress = library.getProcedure('DE_Disconnect');
    if (!Number.isSafeInteger(disconnectAddress) || disconnectAddress <= 0) throw new Error('Missing DE_Disconnect');
    // QuickJS distinguishes signed integer values from floating-point values.
    // Negative coordinates have unsigned IEEE bits above INT32_MAX. Use a
    // standard JS DataView and signed bits so CLEO's integer FFI accepts them;
    // the native uint32_t argument preserves the identical low 32 bits.
    const floatView = new DataView(new ArrayBuffer(4));
    const floatBits = value => { floatView.setFloat32(0, value, true); return floatView.getInt32(0, true); };
    const fromBits = bits => { floatView.setUint32(0, bits, true); return floatView.getFloat32(0, true); };
    emit({ event: 'adapter_start', host: HOST, cleo: String(CLEO.version), host_version: String(CLEO.hostVersion),
        definitions: String(CLEO.apiVersion), mode: MODE, endpoint: GAIT_REFERENCE ? null : '127.0.0.1:22040', stop_key: stopKey,
        ...(GAIT_REFERENCE ? { native_gait_reference: true, network_active: false } : {}),
        ...(GAIT_RATE ? { matched_gait_rate: true } : {}),
        ...(GAIT_ROOT_LOCK ? { horizontal_walk_root_lock: true } : {}) });
    const transport = {
        connect: nonce => connect(nonce) === 1,
        pump: () => pump() === 1,
        localId,
        submit: value => submit(floatBits(value.x), floatBits(value.y), floatBits(value.z), floatBits(value.heading)) === 1,
        players: () => {
            const count = playerCount();
            if (!Number.isInteger(count) || count < 0 || count > 16) throw new Error('Bridge returned an invalid player count');
            const players = [];
            for (let index = 0; index < count; ++index) players.push({ id: component(index, 0),
                x: fromBits(component(index, 1)), y: fromBits(component(index, 2)),
                z: fromBits(component(index, 3)), heading: fromBits(component(index, 4)) });
            return players;
        },
        disconnect: () => Memory.CallFunction(disconnectAddress, false, 0)
    };
    const game = {
        // F11 already toggles the game's fullscreen state. A separate chord
        // prevents that normal action from stopping the adapter. CLEO's
        // IS_KEY_PRESSED reports held keys; its edge-only IS_KEY_DOWN could
        // miss a press between ticks. These reads consume no input.
        stopRequested: () => chordHeld([17, 16, 35]),
        isReady: () => native('IS_PLAYER_PLAYING', 0),
        localPed: () => native('GET_PLAYER_CHAR', 0),
        doesPedExist: handle => native('DOES_CHAR_EXIST', handle),
        readTransform: handle => ({ ...native('GET_CHAR_COORDINATES', handle), heading: native('GET_CHAR_HEADING', handle) }),
        requestModel: model => native('REQUEST_MODEL', model),
        hasModelLoaded: model => native('HAS_MODEL_LOADED', model),
        createPed: (model, value) => native('CREATE_CHAR', 4, model, value.x, value.y, value.z),
        makeRemoteInert: handle => {
            native('CLEAR_CHAR_TASKS', handle);
            native('TASK_TOGGLE_PED_THREAT_SCANNER', handle, false, false, false);
            native('FREEZE_CHAR_POSITION', handle, true);
            native('SET_CHAR_COLLISION', handle, false);
            native('SET_CHAR_PROOFS', handle, true, true, true, true, true);
        },
        movePed: (handle, value) => {
            // Received positions come from GET_CHAR_COORDINATES. The no-offset
            // setter places the body center rather than adding a feet offset;
            // actual DE placement still needs readback and visual validation.
            native('SET_CHAR_COORDINATES_NO_OFFSET', handle, value.x, value.y, value.z);
            native('SET_CHAR_HEADING', handle, value.heading);
        },
        playRemoteAnimation: (handle, clip) => {
            // PED is built in. Its documented lifecycle forbids removal;
            // releasing it can crash the game. Only the owned NPC gets tasks.
            // Replication owns the body position. Classic animation processing
            // can leave unextracted root translation in the rendered pose;
            // these supported DE flags are an opt-in test of that explanation,
            // not a claim that coordinate readback proves visual continuity.
            const lockHorizontal = GAIT_ROOT_LOCK && clip === 'Walk_civi';
            native('TASK_PLAY_ANIM', handle, clip, 'PED', 4.0, true, lockHorizontal, lockHorizontal, false, -1);
            if (GAIT_ROOT_LOCK) emit({ event: 'remote_animation_root_policy', ped: handle, clip,
                lock_x: lockHorizontal, lock_y: lockHorizontal, sample_at_ms: Date.now() });
        },
        readRemoteAnimation: (handle, clip) => {
            // The phase getter requires an existing association. Both native
            // queries accept only the handle and clip, without an IFP argument.
            const playing = native('IS_CHAR_PLAYING_ANIM', handle, clip);
            if (typeof playing !== 'boolean') throw new Error('Invalid native animation condition');
            return { playing, time: playing ? native('GET_CHAR_ANIM_CURRENT_TIME', handle, clip) : null };
        },
        prepareReferencePed: (handle, value) => {
            // This initial body-center placement is the last external pose
            // write. Collision and native locomotion must drive every measured
            // step; the frozen multiplayer proxy would give a false reference.
            native('SET_CHAR_COORDINATES_NO_OFFSET', handle, value.x, value.y, value.z);
            native('SET_CHAR_HEADING', handle, value.heading);
            native('CLEAR_CHAR_TASKS', handle);
            native('TASK_TOGGLE_PED_THREAT_SCANNER', handle, false, false, false);
            native('SET_CHAR_PROOFS', handle, true, true, true, true, true);
            native('SET_CHAR_COLLISION', handle, true);
            native('FREEZE_CHAR_POSITION', handle, false);
        },
        walkReferencePed: (handle, value) => {
            // MoveState.Walk is 4 in the pinned DE enum. A single finite task
            // leaves both movement and gait selection to the game engine.
            native('TASK_GO_STRAIGHT_TO_COORD', handle, value.x, value.y, value.z, 4, 10000);
        },
        setRemoteAnimationRate: (handle, clip, rate) => {
            // The measured native travel speed gives this multiplier a local
            // reference. An association must be observed playing before the
            // controller calls this setter; later phase reads verify its effect.
            native('SET_CHAR_ANIM_SPEED', handle, clip, rate);
        },
        deletePed: handle => native('DELETE_CHAR', handle),
        releaseModel: model => native('MARK_MODEL_AS_NO_LONGER_NEEDED', model)
    };
    // Keep input arguments inside QuickJS's signed integer range as well.
    const nonce = Math.floor(Math.random() * 0x7ffffffe) + 1;
    controller = GAIT_REFERENCE ? new NativeGaitReferenceController(game, { log: emit, stopReason: stopKey })
        : new AdapterController(game, transport, { mode: MODE, nonce, log: emit, stopReason: stopKey,
            renderSmoothing: MODE === 'online', gaitProbe: MODE === 'online', gaitRate: GAIT_RATE });
    enterPhase('controller_run');
    // Hot reloading this file is unsupported: CLEO does not document a forced
    // script-unload cleanup callback. Stop with Ctrl+Shift+End before editing
    // or removal.
    while (controller.running) {
        controller.tick(Date.now());
        // The controller gates bridge work at 50 ms while online presentation
        // advances more often. Manual activation and smoke retain their 50 ms
        // yield; only an active online session takes the shorter display tick.
        if (controller.running) wait(MODE === 'online' ? 16 : 50);
    }
}

try {
    runAdapter();
} catch (error) {
    emit({ event: 'adapter_failed', phase, error: String(error), game_adapter_active: false });
} finally {
    // This covers controlled stop-key exit and script exceptions only. It is not a
    // guarantee that CLEO executes finally on forced reload or engine shutdown.
    if (controller && controller.running) controller.stop('script_exit');
}
