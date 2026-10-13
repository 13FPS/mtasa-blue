// SPDX-License-Identifier: GPL-3.0-or-later
import { MODE, ONLINE_ACTIVATION, GAIT_REFERENCE, GAIT_RATE, GAIT_ROOT_LOCK, INPUT_REFERENCE, MTA_NATIVE_PAD_REFERENCE } from './config.mjs';
import { AdapterController, NativeGaitReferenceController, NativeInputReferenceController } from './controller.mjs';
import { MtaNativePadReferenceController } from './native-pad-reference.mjs';

let controller = null;
let phase = 'entry';
const emit = event => log(JSON.stringify(event));
const enterPhase = value => { phase = value; emit({ event: 'adapter_phase', phase, mode: MODE }); };

function runAdapter() {
    emit({ event: 'adapter_phase', phase, mode: MODE, host: HOST, cleo: String(CLEO.version),
        host_version: String(CLEO.hostVersion), definitions: String(CLEO.apiVersion) });
    if (!['diagnostic', 'smoke', 'online'].includes(MODE)) throw new Error('Unknown adapter mode');
    if (typeof INPUT_REFERENCE !== 'boolean' || (INPUT_REFERENCE && (MODE !== 'smoke' || GAIT_REFERENCE)))
        throw new Error('Native input reference requires opt-in Smoke mode without gait reference');
    if (typeof MTA_NATIVE_PAD_REFERENCE !== 'boolean' ||
        (MTA_NATIVE_PAD_REFERENCE && (MODE !== 'smoke' || INPUT_REFERENCE || GAIT_REFERENCE)))
        throw new Error('MTA native pad reference requires separate opt-in Smoke mode');
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
    if ((INPUT_REFERENCE || MTA_NATIVE_PAD_REFERENCE) && definitionVersion < 0.500)
        throw new Error('Native input reference requires SA Unreal API definitions 0.500 or newer');

    const stopKey = 'Ctrl+Shift+End';
    // Home opens the Rockstar console on the tested build and interrupts this
    // read-only observation, so the input reference needs its own shortcut.
    const activateKey = (INPUT_REFERENCE || MTA_NATIVE_PAD_REFERENCE) ? 'Ctrl+Shift+F9' : 'Ctrl+Shift+Home';
    const chordHeld = keys => keys.every(key => native('IS_KEY_PRESSED', key));
    if (MODE === 'online' || GAIT_REFERENCE || INPUT_REFERENCE || MTA_NATIVE_PAD_REFERENCE) {
        if (MODE === 'online' && !['manual', 'automatic'].includes(ONLINE_ACTIVATION))
            throw new Error('Unknown online activation mode');
        // The reference changes one owned NPC, so even an automatic Online
        // preference cannot start this separate experiment during save loading.
        if (GAIT_REFERENCE || INPUT_REFERENCE || MTA_NATIVE_PAD_REFERENCE || ONLINE_ACTIVATION === 'manual') {
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
                if (chordHeld([17, 16, (INPUT_REFERENCE || MTA_NATIVE_PAD_REFERENCE) ? 120 : 36]) && native('IS_PLAYER_PLAYING', 0)) {
                    emit({ event: 'adapter_activation_requested', mode: MODE, activate_key: activateKey });
                    break;
                }
                wait(50);
            }
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
    if (MTA_NATIVE_PAD_REFERENCE) {
        const nativePadRead = bind('DE_NativePadRead', 'X64U32');
        const resetAddress = library.getProcedure('DE_NativePadReset');
        if (!Number.isSafeInteger(resetAddress) || resetAddress <= 0) throw new Error('Missing bridge procedure: DE_NativePadReset');
        const reader = {
            bind: bind('DE_NativePadBind'), capture: bind('DE_NativePadCapture'),
            read: (pad, which, index) => nativePadRead(pad, which, index),
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
