# Definitive Edition compatibility harness for the MTA port

**The target is the existing MTA client on Definitive Edition.** On October 13
the user clarified that MTA's existing systems should be reused. This prototype
is a compatibility test harness; its custom transport is not the target MTA
network implementation. Further work starts from the existing client/game
interfaces and synchronization code.

The experimental x64 MTA launcher now offers explicit `--de-launch` startup
through Steam after checking the native package, installation ownership and
registered app/library. Its existing `--de-preflight` remains read-only; see
the [launcher contract](../launch/DefinitivePreflight/README.md). A launch
request is not proof that the full MTA client has started.

The retained Core also checks native menu eligibility before creating a GUI
or borrowing input. Known frontend/loading states and fully identified but
ineligible local players defer that view request, leaving Core hidden for a
later explicit F9. Unknown evidence remains an error. The first deferred view
cannot arm MTA movement. The October 20 GTA check on `8697e3f13` confirmed
Steam launch through MTA, hidden Core readiness in 47 ms, early-view deferral,
and a later explicit on-foot menu with 65 frames/presents. All 205 input scopes
restored, the cursor hook detached, Core/GUI/XML/pthread unloaded, and GTA
exited normally. The user reported every combined check worked; the logs
contain one actual menu view, so reopening remains unconfirmed for this run.
Full engine and network startup remain incomplete.

**The real Deathmatch client now compiles and links as Release/x64.** Generate
with `--with-definitive-adapter --with-definitive-client` and build the existing
`Client Deathmatch` target. Its output is `Bin/mods/deathmatch/x64/client.dll`;
the DE mod loader selects this directory, keeping the classic module separate.
Default and adapter-only solutions still exclude this unfinished client target.

The port reuses MTA's Lua, microphone and SVG projects. Its four BASS import
libraries must be AMD64 files in `vendor/bass/lib/x64`, with their matching DLLs
beside the client output. The checked vendor packages are linked from the
[official BASS page](https://www.un4seen.com/bass.html): BASS, BASS FX, BASSmix
and TAGS. Their Windows archives contain `c/x64/*.lib` and `x64/*.dll` (the FX
package uses `C/x64`). Existing x86 imports remain unchanged.

Shared client code now asks the game SDK about pending vehicle audio settings;
the classic backend owns its native comparison. The appended query requires
matching client/backend builds. Marker IDs reuse MTA's existing 32-bit ID
allocator on x64, and Lua handles retain their existing numeric ID format while
rejecting wider pointer payloads. Native counts use the SDK's `size_t` type.

The normal x64 client build, its `InitClient` export, the classic x86 audio
translation unit and 29 x86/40 x64 Lua ID regression checks passed. This has not
loaded the new client in GTA. The full DE game interface and x64 networking
remain incomplete, so gameplay/full-startup gates and **EXPERIMENTAL** remain.

**Startup module loading now reuses one architecture-aware MTA owner.**
`CModuleLoader` and the existing deathmatch mod manager share exact-path module
ownership. Incompatible PE architecture is rejected before module execution;
replacement and unload refusals retain the existing owner. Core checks the
requested load's result rather than mistaking a retained earlier module for it.
Network compatibility is checked before `InitNetInterface`; a missing export
cannot become a null function call. The saved official x86 module's version
comparison and version-query paths were checked statically without loading it.

The verified current/previous pad span also has a reusable checked native
access class, immediately used by the existing movement path. Its transaction
retains the original snapshot after a failed restore and requires matching
readback before retirement. This does not expose an incomplete `CPad`.

The checked [official installer](https://github.com/multitheftauto/mtasa-blue/blob/master/utils/buildactions/install_data.lua)
provides an x64 server `net.dll`, while its client `netc.dll` is x86. The
[nightly index](https://nightly.multitheftauto.com/) likewise labels its Windows
x64 downloads as server packages. No public x64 client network build was found
in those channels. Full DE multiplayer needs an AMD64 client `netc.dll` matching
this SDK; the server library cannot supply its client interface. Engine work
can continue independently. The existing gameplay/full-startup gates remain.
MTA's [official fork guidance](https://wiki.multitheftauto.com/wiki/Forks)
confirms that `netc.dll`, `net.dll` and the anti-cheat components are closed-source.

Offline checks passed for both x86/x64 loader owners and compatibility gates,
2,937 native movement/pad checks, the normal Core build and both classic
compile profiles, and all seven Core/GUI host cases. No GTA installation changed.

**The next startup foundation adds read-only native game time through MTA's
existing adapter owner.** The optional `ReadGameTimeStateV1` service supplies
the native timer, hour/minute/raw seconds, last minute change and minute duration
from the already identified exact-image clock fields. It installs no hook and
does not acquire or renew an input, rendering or clock-write lease. The shared
raw clock reader also backs the existing `CClockDE` service.

The retained Core exposes fresh explicit reads with
`MTA_ReadDEStartupGameTimeV1`; its separate 16-word result leaves existing
startup, GUI and input protocols unchanged. A refused sample is marked
unavailable with zero native fields. Core neither substitutes wall time nor
reuses an earlier successful sample. Two matching complete native samples
provide sampled consistency, not an atomic engine-thread snapshot. Changed
clock calculation bytes, including the freeze patch, refuse a read.

This is a prerequisite for native `CGame` time consumers. It does not expose
a partial `CGame`, promote gameplay capabilities or bypass full startup's
remaining engine/network requirements. The installed combined input/menu
runtime and its October 19 GTA result below remain unchanged; this new service
has separate offline verification and has not been tested in GTA.

This batch passed 807 native clock/time checks, 1,780 real adapter checks,
1,812 x64 consumer checks plus the x86 refusal checks, and 2,686 assertions
across seven real Core/GUI host cases. The normal Core build passed its 69
guards and both classic compile profiles; the rebuilt Bridge passed 2,463
checks. No game installation was changed.

**The timing backend implements eight actual SDK `CGame` getters.**
`CGameTimingDE` supplies `GetSystemTime`, `GetSystemFrameCounter`, `GetTimeStep`,
`GetOldTimeStep`, `GetTimeScale`, `GetGameSpeed`, `GetMinuteDuration` and `GetFPS` through checked native
timing reads. The class remains abstract and cannot be instantiated or exposed
as a game interface. The adapter's readiness and advertised capabilities remain
unchanged; full `CGame`, startup and multiplayer are still incomplete.

Offline validation passed 849 clock/timing fixture checks, the normal
Release/x64 backend build and 1,780 rebuilt-DLL factory-host checks. The shared
enum relocation preserved its types and values under x86/x64 `/W4 /WX` checks.
The passive 12-second capture then passed in GTA: 188 matching samples,
717 frames and 11,485 native timer units of progress, with all eight code pins
matching and no nonfinite, out-of-range clock or backward samples. An earlier
paused/background attempt is preserved separately. This validates the observed
native fields; it does not construct a `CGame` or prove full startup. The
completed input/menu result below and **EXPERIMENTAL** status remain unchanged.

`CKeyGenDE` implements the existing four-method MTA hashing interface without
calling game routines. The DE image independently supplies the raw CRC table,
seed, string loop and CRT uppercase behavior. Sized-input and append contracts
are corroborated by the address-matched
[classic implementation reference](https://github.com/gta-reversed/gta-reversed/blob/a235da5fa39161d997a7099ed7275a22545cada1/source/game_sa/Core/KeyGen.h).
The implementation generates its own polynomial table; it adds no vendor library.
The still-abstract `CGameDE` assembles this service and the existing owner's
`CClockDE` behind the real `GetKeyGen` and `GetClock` SDK methods. It does not
create a second clock owner or return an incomplete object from the factory.

`SetTimeScale`, its `SetGameSpeed` alias and `SetMinuteDuration` delegate to the
same native clock owner's internal timing access. Only the qualified engine
callback can capture an original scalar or write it; startup reader binding
grants no write access. End restores owned scale/duration independently of
displayed time, preserving natural clock progression for timing-only changes.
The public clock protocol and adapter ABI are unchanged. `Pause` still needs
an engine owner that continues to run while the game is paused.

This SDK batch passed 67 key-generation checks on each x86/x64 architecture,
1,094 production clock/timing checks, the normal Release/x64 backend build and
1,780 fresh factory-host checks. No GTA installation or running game changed.

`GetFPS` reads DE's stored averaged FPS float, independently identified through
the native wall-clock calculation and its named display consumer. Its separate
reader checks pins, pages, thread ownership and two matching finite samples;
it preserves output on refusal. Existing 64-byte timing values and public wire
formats stay unchanged. A passive capture of the already-running GTA process
accepted 187 samples and 186 platform-clock advances, with stored FPS between
59.9874 and 60.0103 and no native FPS/average-time mismatches. Gameplay time was
frozen while these native wall-clock values advanced. This validates the field
observation, not construction or execution of a full `CGame` in GTA.
The follow-up passed 1,212 clock/timing/FPS fixture checks, a rebuilt normal
Release/x64 backend and all 1,780 fresh factory-host checks. The installed
runtime remains unchanged; full MTA startup and gameplay are still incomplete.

**The current source connects the retained MTA bindings to local on-foot input.**
The existing runtime selection now starts a separate renewable input service
after the menu closes. It uses the same `CKeyBinds` manager and controller
synthesis as Settings and the earlier movement work. A fresh menu must wait
for that writer's actual retirement; final shutdown retires it before releasing
bindings or the backend. There is no additional trial flag or walking timer.

The optional local-input API keeps Adapter ABI6 and the existing frontend
reports unchanged. Its 500 ms owner/generation/focus lease permits only forward,
backward, left, right, walk and sprint. Lost eligibility or focus suspends it;
a later menu closure deliberately rearms through released keys and fresh
eligibility. The original 35-second movement trial and neutral frontend lease
retain their limits. The combined input/menu milestone passed in GTA on
October 19; gameplay, networking and full client startup remain incomplete.

**The combined MTA input/menu path passed on runtime `4ed8f898a`, with capture
tooling `2602e1375`.** Two actual menu views were acknowledged and retired
within session generation 1, using the same Core owner thread 29920. Between
views, the retained MTA bindings produced held actions and renewed local input.
That input writer retired before the next visible view. Final runtime phase 7
recorded native generation 4 retired with neutral input, and the live module
snapshot contained no Core, GUI, XML or pthread. The expected backend module
PIN and the Bridge's script lifetime are separate from active service ownership.

The fourteen raw logs yielded 174 events, including 67 Runtime416 records and
75 local-input32 records. The user's verbatim report, **"All checks worked"**,
supports the requested Settings Apply-to-I, I movement/sprint, second menu,
retained I binding, Cancel-to-O rejection and restored W/control checks.
Machine records establish the service transitions and held input; they cannot
identify physical keys, prove a Quit click or supply visual confirmation.

Shutdown preserved all fourteen live log prefixes. Its fresh CLEO delta contains
the actual `host shut down` event followed by `Exit`; the earlier parser falsely
rejected that spelling. The original failed capture remains unchanged, and a
separate saved-byte reanalysis records the corrected logged shutdown evidence.
The OS exit code remains unknown. The actual protocol pass, human report and
shutdown classification stay separate behind the workspace pointer
`Build/definitive-recovery/current-actual-local-input-checkpoint.json`.
Earlier failed trials remain unchanged, with their unresolved causes unproven.
Adapter ABI6 and advertised capabilities are unchanged; no full `CGame` or
MTA networking is enabled, and the root **EXPERIMENTAL** notice remains.

Development now batches related wiring and checks it offline before a combined
GTA test at an integration milestone. Individual source edits do not require
relaunching the game.

Show/Hide admission also tolerates a brief concurrent cache publication. The
script retains the unused command sequence and requires a fresh coherent read
before retrying, with three attempts at most. Stop takes priority over a deferred
view command. Invalid replies or persistent refusal still retire the runtime;
this does not establish the cause of the earlier missing GTA reopening command.

This batch passed the normal Core build and both classic compile profiles,
seven actual Core/GUI host cases, 180 worker cases, 1,454 script checks and
2,463 Bridge checks. Native input tests passed 2,930 checks in an owned test
process; the real module and x64 MTA consumer passed 1,763 and 1,782 checks.
These are offline results. The combined menu, changed bindings and native
movement milestone has the separate October 19 GTA evidence above.

**The retained frontend runtime keeps real MTA services across menu reopenings.**
An additional `-MtaCoreFrontendNativeHost` selection starts that same Core owner
through the pinned ASI loader's `InitializeASI` entry, independently of CLEO
script resumption. It requires `-MtaCoreFrontendRuntime` and adds only the owned
`MtaDefinitive.asi` / `MtaDefinitive.ini` pair (64 files total with the four MTA dialog icons). Core starts hidden;
there is no automatic menu, movement or network connection. With GTA focused,
hold Ctrl+Shift+F9 for three seconds and release every key to show/hide a view;
Ctrl+Shift+End stops the runtime. Existing native view eligibility still applies,
so hidden Core startup does not establish a playerless main-menu renderer or
complete `CGame` startup. Native lifecycle transitions go to
`CLEO/de-online/mta/logs/nativefrontend.log`. The script yields controller
ownership in this selection. Ordinary mode changes disarm the owned INI, and
updates can remove the complete pair while preserving user binding data.
The October 20 combined GTA check passed on `f556dd92f`. Hidden Core was ready
32 ms after native controller startup; the first Show command arrived 111.688 s
after startup. Core initialized before the CLEO script resumed. Two genuine
menu views produced 179 and 298 presents, then closed with input scopes fully
restored and cursor hooks detached. The user confirmed Apply to I, movement and
sprinting with I, retained I on reopening, and ordinary W/camera control after
End. All 15 run logs and normal shutdown are saved against the 14 existing-log
baseline. GTA's native-main-menu location is the user's report; these records
do not establish a visible playerless GUI, full client startup or networking.
The baseline's missing `cgui/images/question.png` error reproduced. The
backend `game_de.dll` may remain mapped through its expected process pin after
its callbacks end. Full MTA remains experimental.

The following offline packaging update ships MTA's existing `question`, `info`,
`warning` and `error` PNGs as one complete optional group. It addresses the
missing-icon path without changing GUI code. New frontend/native packages own
62/64 files; historical 23/36/58/60 packages remain accepted. The existing MTA
launcher preflight and loader switch validate these complete groups and native
INI/configuration agreement. This adds package admission, not a full-client
launch path or a new GTA visual result.

`package.ps1 -Mode Smoke -MtaCoreFrontendRuntime` selects a separate 62-file
package; the flag defaults to false and excludes every other trial. After the
initial three-second Ctrl+Shift+F9 activation and key release, the existing
MTA menu opens without a display timer or a clock-test prelude. Later presses
of Ctrl+Shift+F9 hide/show the menu. Menu Quit, Escape or focus loss hides the
view; Ctrl+Shift+End stops the retained runtime.

One completed Core/backend owns the actual XML, CVars and configured `CKeyBinds`
manager, together with MTA's real localization, command manager and console
logger. These services are initialized while hidden and retained between views;
configured locales use MTA's existing English fallback when their assets are not
shipped. Normal classic startup shares their construction and rollback. Each
view borrows the retained services and keeps its own console counters and routes.
Settings Apply changes these session bindings, so a fresh GUI can
borrow them again after the old GUI is released. Cancel discards staged edits;
configuration files are not saved by default. Adding
`-MtaCoreFrontendBindingPersistence` to `package.ps1` or `set-mode.ps1` explicitly
enables saved movement bindings for this runtime. The retained Core loads them
before its first GUI or gameplay-input borrower; Settings Apply atomically saves
the six supported controls in MTA's user-data `definitive/keybinds.xml`. A later
launch with the same option reloads that file. Cancel and a failed save preserve
the previous bindings. Ordinary mode changes clear this option, and older
packages need updating before they can select it. The installed
`mta/config/coreconfig.xml` remains unchanged. This integration has offline
coverage; cross-launch behavior in GTA still needs a combined live check.

Each visible view still owns its native
input, cursor and rendering leases. Hidden acknowledgement requires their
retirement and neutral key history; final Stop releases the session services
before the backend/Core. Cleanup debt retains its owners rather than unloading
their code. The independent host lease remains one second; native and cursor
leases remain 500 ms.

The additive API1 Core64 and Bridge416 reports preserve Startup32, Input256 and
Persistent288 meanings. The Core sample identifies an actual owner read;
live GUI state comes from the view publication while that owner is busy.
The first GTA reopening trial on `9cce69bf9` failed: the second F9 command was
received, but the new view could not reacquire native input. The first view
also exposed an acknowledgement check that confused active cleanup ownership
with a cleanup fault. The correction keeps that ownership flag while visible
and permits fresh input eligibility only after the same adapter has fully
retired its previous native generation. Pending cleanup and replacement owners
still refuse reacquisition.

The follow-up GTA trial on `dcf4ee357` acknowledged and retired its first menu
correctly, then stopped cleanly. No second Show command was recorded, so native
re-entry was untested in that recording. The user was unsure which reopening chord was used;
that recording contains no key-state evidence to resolve the cause. The script
now rearms when F9 is released with modifiers held and preserves a newer reopen
request while GUI closing finishes. Focus loss still hides the menu. A change
log for only Ctrl, Shift and F9 distinguishes observed keys, debounce and queued
intent without recording other keystrokes. The later combined two-menu milestone
passed as recorded above; it does not establish that earlier trial's cause.
Earlier cursor, Settings and clock
results below remain separate evidence. Gameplay, networking and full client
startup remain unavailable, and the root **EXPERIMENTAL** notice stays.

The preceding menu-only candidate's offline validation passed 69 normal Core build guards and both classic compile
profiles; seven actual MTA GUI host cases; 172 worker cases;
1,361 script checks; 1,329 native callback and input-retirement checks in an
owned test process; and 2,386 Bridge checks in a rejected non-game host.
The new package matrix passed 3,746 checks using one disposable game copy, and
the preceding frontend ownership matrix passed all 1,175 checks again. These
results verify service ownership and failure handling; live hide/show and
Settings retention have the separate October 19 combined result above.

**Combined clock-to-menu cursor interaction passed in GTA on runtime `f731beec0`.**
The user reported a free pointer, a successful Quit click and restored movement
and camera controls. The same completed MTA Core owner performed the read-only
clock stage and then the existing menu. Forty Read commands were acknowledged
over a 2,016 ms native workload; all 273 borrowed input scopes were restored.
MTA's existing cursor lease renewed 87 times and suppressed 267 engine cursor
warps. Its hook detached with no active callbacks, and Core/GUI/XML/pthread
unloaded. Quit is a human-reported click; the close flag also covers Escape and
does not provide a separate machine-verified Quit callback count.

The bounded path now shares the existing cursor lease with persistent menus:
begin after neutral input proof, renew before drawing, disarm before GUI input
detachment, and retire after native input restoration. Failed cleanup retains
the same resource for retry. Cursor policy, persistent timing and public input
layouts remain unchanged. The bounded diagnostic is
`mta_core_bounded_frontend_cursor_v1`.

All fourteen logs, the fresh pre-F9 baseline, source snapshots, runtime package
and normal shutdown evidence are sealed behind
`Build/definitive-recovery/current-actual-bounded-cursor-checkpoint.json`.
The earlier `0dedf0b06` trial remains recorded as a pointer interaction failure;
its clock/cleanup results do not turn Escape into a Quit click. Full gameplay,
network and complete client startup remain gated; this result verifies the
bounded frontend sequence. The root **EXPERIMENTAL** notice remains in place.

**Read-only Core clock integration passed in GTA on runtime `d9de393a1`.**
`package.ps1 -Mode Smoke -MtaCoreClockReference` selects an optional 23-file
package using MTA's existing Core startup worker and `CClock` adapter. It is
disabled by default and exclusive with every input, rendering, GUI and other
trial selection. No GUI, NPC, network connection, player-input write or
game-time write is part of this trial. It does not enable full MTA startup.

The trial waits indefinitely for Ctrl+Shift+F9 held for three seconds and
starts after the keys are released. It allows ten seconds for a qualified
engine callback, then observes natural clock progression for up to ten seconds
using generation-bound Read commands and producer heartbeats. Ctrl+Shift+End,
focus loss or a failed heartbeat requests cleanup. A cleanup deadline changes
diagnostics and retry backoff; it never grants permission to unload a retained
native owner. The separate 96-field clock report requires matched Read
acknowledgements and advancing native timer values before calling the workload
complete. A read-only session retires without restoring old time or writing
clock fields.

With GTA closed, `set-mode.ps1 -Mode Smoke -MtaCoreClockReference` explicitly
enables an intact clock-capable base installation. Ordinary
`set-mode.ps1 -Mode Smoke` clears the opt-in. Install/update reject clock-enabled
GUI ownership, mixed trials and malformed or duplicate clock declarations.
Intact older configurations without the marker retain ordinary mode switching;
enabling this trial requires an update.

The fresh PID 13288 / October 18 runtime
`d9de393a1078deb12432bcdd984357eefb60b261` completed **198 Read commands with
198 matching acknowledgements**, **608 qualified native callbacks** and
**200 advancing-timer observations** over **10,000 ms**. The user reported
that GTA continued normally. All 88 recorded 96-field samples retained one
generation and consistent owners. Native hooks, pending commands, peer holds
and recovery debt ended cleanly, and Core/GUI/XML/pthread unloaded. The backend
module's expected PIN does not represent a retained clock service.

The saved raw recording passed **5,033 independent protocol checks** and a
separate **51-check metadata/layout review**. Normal exit preserved all fourteen
live log prefixes and all 23 installed hashes, adding only CLEO's 74 shutdown
bytes. The machine exit code is unknown; process absence, the new shutdown/Exit
markers and the user's normal-exit report are recorded separately. Frozen
runtime source copies, the fresh pre-F9 baseline, raw logs and both reviews are
archived behind the workspace pointer
`Build/definitive-recovery/current-actual-clock-read-checkpoint.json`.

**Core-owned Set/readback/restoration passed in GTA on runtime `f749e634f`.**
`package.ps1 -Mode Smoke -MtaCoreClockSetReference` selects the optional base
23-file candidate. `MTA_CORE_CLOCK_SET_REFERENCE` defaults false. Set and the
existing `MTA_CORE_CLOCK_REFERENCE` Read trial are mutually exclusive, along
with every input, rendering, GUI and other trial. Set reuses MTA's `CClock`
interface and engine owner for controlled time changes, readback and restoration;
it creates no GUI, NPC or network connection and applies no player input.
Its distinct mode18 / API1 / 128-field Bridge/Core contract preserves the
completed Read mode17 / 96-field route.

After a qualified callback, the five-second workload first reads the clock and
requires the normal 1,000 ms per game minute rate. It issues exactly one Set to
12:15 (06:15 when the initial hour is 12), verifies coherent acknowledgement and
subsequent native progression, then ends the provider to restore its captured
clock values and phase. Freeze, Unfreeze and ResetTimeFrozen remain unused.
The provider capture predates the producer's first Read; restoration proof does
not claim those two observations are identical.

Ordinary Smoke selection clears both clock opt-ins; explicit Set selection
requires its reviewed marker and intact 23-file ownership. Historical intact
configurations without the new Set marker retain ordinary mode switching and
require an update before enabling it. The new package matrix covers both
23/58-file transitions, dual Read+Set and malformed-marker refusal, backups,
loader preservation and historical compatibility without running GTA.
Offline checks passed: 710 native clock checks; 670 Set lifecycle and 648
preserved Read lifecycle checks per x86/x64 architecture; 125 worker cases;
1,202 script checks; 1,415 actual MTA frontend host checks; and package ownership
matrices with 980 Set, 716 Read and 687 legacy checks. The normal Core/Game DE
build passed 66 guards, with classic compatibility and both owned Core entry
hosts retained. A separate saved-evidence review passed 8,310 checks.

The October 18 runtime `f749e634f1b8227123bb035a0868d4e614d620fa`
completed **one Set to 12:15**, acknowledged after **62 ms**, from an initial
producer Read of **19:11:53**. It then completed **97 subsequent Reads / 98
total Read acknowledgements** and observed **12:19:43** before ending the
provider. The workload lasted **5,000 ms**, with **305 qualified callbacks** and
**101 timer-progress observations**. The user reported normal GTA continuation
and a visible time change.

All **45 saved 128-field samples** passed **3,348 independent checks**. End
verified restoration to the provider's captured **19:11:48** and fractional
phase. That capture predates the producer Read of 19:11:53; these observations
are not claimed to be equal. Native hooks, pending commands, peer holds and
recovery debt retired, and Core/GUI/XML/pthread were absent from the final
module snapshot. Normal-exit capture passed **159 checks**, preserving all
fourteen live log prefixes and 23 installed hashes; only CLEO appended its
74 shutdown bytes. The OS exit code remains unknown and is recorded separately
from process absence, shutdown/Exit markers and the human normal-exit report.
The saved evidence has its own workspace pointer:
`Build/definitive-recovery/current-actual-clock-set-checkpoint.json`.

This establishes the bounded single-Set recipe, without exercising live Freeze,
Unfreeze or ResetTimeFrozen. Full game/network readiness remains unverified.
The earlier Read and Settings/menu results retain their separate runtime
evidence. The port remains EXPERIMENTAL.

**Frontend startup now has a separate completed Core owner.**
`DefinitiveFrontendStartup` creates a successfully constructed `CCore`, then
starts the adjacent Game DE adapter outside its constructor. Native image
binding is explicit. Existing frontend sessions borrow this Core; closing or
releasing a menu leaves the Core and backend alive until explicit shutdown.
Shutdown refuses while a frontend handle still owns CPU services, including a
stopped handle awaiting release. Backend cleanup failure retains the owner for
retry. Its separately versioned 32-field snapshot never grants full readiness.

The bounded view operation reuses the tested MTA menu, console, configured
neutral input, rendering and cleanup path. It no longer needs the ordinary
`CCore` constructor to deliberately fail after rendering a menu. The existing
`InitializeCore` result and eight full-startup fields retain their meanings.
The historical Core15 worker uses this completed lifecycle instead of
`InitializeCore`. Its 96-field report preserves Begin, after-view and stopped snapshots. The
after-view snapshot must prove Core/backend survival before explicit shutdown;
the stopped snapshot is copied before final release. Busy cleanup keeps the
same worker alive and retries, including after cancellation or a deadline.
The existing Core14 input256 route and all older contracts remain separate.
The historical view keeps its native 35-second trial limit.

**The preceding completed-Core trial composes clock Read and the existing menu.**
The same Smoke option `-MtaCoreGuiFrontendStartupReference` now requires distinct
V2 bridge exports and selects Core19. After successful Core/backend creation,
it runs the existing read-only clock lifecycle for two seconds, with a separate
ten-second readiness budget. Only independently qualified clock retirement
allows the same Core owner to enter the existing twenty-second MTA menu trial.
The separate Read96 and owner32 snapshots must agree with the completed owner;
the input256 and lifecycle96 reports keep their existing field meanings.

Direct GUI borrowing is synchronized with Core startup operations and refused
before allocation while clock callbacks may hold native peers. Cancellation or
a deadline cannot release the Core, unload its backend or start the GUI while
clock retirement remains pending. The backend stays alive between the clock
and menu stages, then follows the existing explicit shutdown and release path.
This composition adds no clock writes or new native offsets. The historical
Core15 exports, persistent Core16 route, standalone Read and Set trials, classic
startup and full-game readiness gates remain separate. The combined sequence
still requires its own GTA test; the successful standalone results above do not
establish live clock-to-menu startup.

Offline validation passes 473 policy checks on each of x86 and x64, all 151
worker cases (including the 125 prior cases), 1,304 script tests and 293 bridge
checks. Actual Core/GUI hosts pass 1,559 checks across four profiles. The normal
Core/Game DE build passes 67 guards, both classic compile profiles and both
294-check entry classifications. Controlled failures cover cancelled startup,
missing or malformed clock evidence, capability changes and retained cleanup;
they never authorize a GUI view or unload while native retirement is pending.
These checks preserve the historical live results rather than replace the
pending GTA validation of this combined sequence.

**A separate persistent frontend candidate reuses this same MTA owner.**
`-MtaCorePersistentFrontend` selects a Smoke package that runs the existing
menu, console, bindings and rendering until close, cancellation, focus loss or
a fault. Its script owner renews a generation-bound one-second heartbeat; Core
independently renews a neutral-only 500 ms native input lease. Either stalled
owner therefore loses input admission. Expired, revoked or stale generations
cannot resume. Native window-family focus is checked before new scopes, while
every admitted scope retains its original physical-input restoration obligation.

This optional native export contract preserves Adapter ABI6 and does not grant
gameplay or remote-player capability. Its distinct 288-field report and bridge
exports leave the old input256/lifecycle96 trials unchanged. Device acquisition
stays bounded; cleanup deadlines still retain owners and retry. The existing
20-second menu and 35-second native trial limits are separately regression tested.
The renewable runner passes simulated execution beyond two minutes, and actual
Core hosts reject wrong-image persistent startup without starting a view.

**The frontend now reuses MTA's existing Settings / Binds tab.** Once the real
configured-bindings manager is admitted, Settings exposes its six supported
movement controls. Keyboard capture stages edits in the original grid; Apply
updates the session's actual MTA bindings, and Cancel discards staged changes.
Changes last for that frontend session. The other settings tabs and GTA-defaults
action remain unavailable because they require later game services; configuration
files stay unchanged. Classic settings retain their ordinary mode and save path.

Capture owns Escape and reserved shortcuts before console or menu handling.
Reopening, focus loss and teardown cancel its prompt before retiring the grid or
bindings. A consumed modifier release also discharges any earlier GUI key hold.
Actual MTA GUI hosts pass **1,416 checks across four profiles**, with Apply,
Cancel, real binding-record reload, alternate keys and retained cleanup. A real
owned-window surface passes **512 checks**, including the modifier-release path;
the shared translator passes **130 checks each on x86 and x64**. The normal
Core build and both classic compile profiles pass. These checks run offline;
the separate GTA result below covers visible Settings interaction.

**Settings and persistent menu interaction passed in GTA on runtime `af4f2dac1`.**
On October 18 the user confirmed that applying primary Forwards as I survived
reopening Settings, while staging O and pressing Cancel preserved I. The pointer
moved freely, menu Quit worked, and walking, sprinting and camera controls
returned. These Settings edits affect the current MTA session; they do not
change GTA's normal bindings or write configuration files. Apply/Cancel and
visible pointer movement are human observations, not fields added to the
existing diagnostic protocols.

The fresh PID 11412 / run 483836 records agree across Core, script and lifecycle
reports. They record **61,090 ms** of native input admission, exceeding the
unchanged greater-than-35-second persistence criterion, **1,193 presents** and
**3,660 native scopes with matching restores** and no restoration failures.
The cursor hook recorded **1,192 renewals**, **3,654 suppressed recentres** and
five forwarded calls before orderly retirement with no hook or callback left
in flight. The actual Quit callback and completed Core/backend cleanup passed;
the saved process snapshot confirms Core, GUI, XML and pthread unloaded.

Fresh pre-F9 raw prefixes distinguish this run from retained older records.
Normal exit preserved all fourteen saved live prefixes, appended the usual
74 CLEO shutdown bytes and left the other logs and all 58 installed files
unchanged. The saved evidence and independent assessments are archived in the
workspace. Console commands were not exercised during this Settings test;
earlier console evidence remains separate. Camera exclusion remains unverified
by machine observation, and full game/network readiness stays false. The port
remains EXPERIMENTAL.

**The cursor correction worked in GTA on runtime `7e7018b66`.** MTA's existing
`CSetCursorPosHook` recorded **383 lease renewals**, **1,177 suppressed native
recentres** and four forwarded calls. The user confirmed free pointer movement,
a working menu Quit and restored walking, sprinting and camera controls. The
fresh Core/script records agree on the real Quit callback, and **1,182 native
input scopes have matching restores**. Cursor retirement ended with no hook,
active lease or callback in flight; Core, GUI, XML and pthread unloaded.

This earlier attempt supplied **19,794 ms** of native input admission. The unchanged
greater-than-35-second persistence criterion remained false for that run; it is
a positive cursor-interaction result, not a completed long-duration workload.
Independent post-launch/pre-F9 prefixes bind all three protocol logs to the
fresh PID/run. Normal exit preserved all fourteen saved live prefixes, appended
74 CLEO shutdown bytes and left the other logs and all 58 installed files
unchanged. Camera exclusion remains unverified by machine observation. Full
game/network readiness stays false and the port remains EXPERIMENTAL.

**The preceding persistent attempt exceeded 35 seconds but failed cursor interaction.**
Runtime `8b2226a04` recorded **39,781 ms** of native input admission, **775 lease
renewals**, **2,379 scopes with matching restores** and **776 successful presents**.
The user saw the menu remain visible, but the cursor stayed centred and could not
reach Quit. Escape closed the menu and normal gameplay controls returned. CJ was
obscured by the panel; this attempt supplies no human confirmation of player or
camera exclusion. Its overall frontend workload remains unverified.

Cleanup and module removal passed, and normal exit appended 74 CLEO shutdown
bytes. A fresh post-launch, pre-F9 baseline was captured for this attempt; the
earlier closed-game recovery snapshot is not its trial prefix. Its cursor failure
is retained separately from the corrected runtime's result above. Older
successful bounded trials below remain separate evidence.

The preceding persistent 58-file package passed 1,117 ownership/package checks,
including 29 real launcher preflight cases and missing-export/architecture
rejection. Its worker passed 69 controlled cases and 91 regression cases;
1,126 script mocks and 100 saved-log reader tests passed separately. These are
offline checks, separate from the corrected runtime's live cursor result.

The earlier completed-frontend package passed 733 ownership/package checks,
including 22 launcher preflight cases. Its worker passed 56 controlled cases and
90 regression cases, including the real Core's completed startup followed by
deliberate wrong-image refusal. Its script passed 1,084 mocks, and 69 saved-log
reader tests covered the lifetime report and exact script/Core input matching.
These remain historical offline checks.

**The completed frontend lifecycle was exercised in GTA on runtime `91819c896`.**
The fresh PID 19148 / run 348502 Core record shows 370 successful presents,
1,138 native scopes with matching restores, and four executed console commands
with no rejections. The 96 lifecycle fields preserve the same Core/backend
through menu release, then record explicit backend shutdown and cleanup. All
145 ordered script samples end with the exact fresh Core input256 values.
Process inspection independently found Core, GUI, XML and pthread unloaded.

CLEO replaced its log at launch, so the strict original script-baseline check
correctly refused the changed prefix. That refusal is preserved. A supplemental
assessment passes the unchanged reader using a 5,367-byte current-launch prefix
derived after the test, with the same owner and exact fresh Core record. This
proves the matched lifecycle content; it does not turn the derived prefix into
an independently captured pretrial baseline. Normal exit preserved all live log
prefixes, appended 74 CLEO shutdown bytes and left the other eleven logs unchanged.

The user confirmed menu/console interaction, CJ/camera blocking and restored
controls. These remain separate human observations; machine camera and full
game/network readiness stay false. The generic GUI close was recorded, with
no menu Quit callback. Raw logs, both reader assessments and the log-reset
limitation are retained in the sealed trial evidence.

The actual normal Core and GUI DLLs pass 659 checks in four owned WARP hosts:
menu/credits/console/bindings, GPU-retained work, missing XML and missing Game DE.
They verify Core survival and reacquisition after frontend release, refusal to
borrow a stopped or failed Core, and unchanged legacy startup diagnostics.
Normal x64 Core/GUI builds and classic Core compilation pass separately.

**MTA's existing client Lua dependency now builds for x64.** Only the explicit
DE opt-in enables `Lua_Client Release|x64`, with isolated output under
`Bin/mods/deathmatch/x64`; classic builds keep their original output and policy.
The actual modified Lua DLL passes 48 checks across seven cases for 64-bit
owner/coroutine pointers, MTA pre/post-call and undump hooks, garbage collection,
allocator retirement and loading a genuine four-byte-size_t Lua chunk. Classic
Lua also builds, and twelve project-policy guards pass. This does not enable
the x64 deathmatch client or supply the missing compatible network/BASS modules.

**The existing Speex voice dependency also builds for x64 under the DE opt-in.**
Its output is isolated beside the x64 Lua dependency, and default/classic build
selection is preserved. Actual x86 and x64 hosts exchange compressed packets in
both directions using MTA's codec/preprocessor settings: 18 cases and 864 frames
per direction, including all three voice sample rates and repeated ownership.
Decoded audio is checked against reconstruction and opposite-architecture
reference tolerances. This tests codec interoperability, without audio hardware
or a running multiplayer client; deathmatch, BASS and network admission remain
unchanged.

**Core-owned frontend input uses MTA's existing systems.** The separate
Smoke option `-MtaCoreGuiFrontendInputReference` selects Core14 / FrontendInput1
with 256 copied fields. It reuses the actual menu, console, parsed XML and
`CKeyBinds` configured-control synthesis. Core refreshes neutral current/previous
controller frames while the frontend owns input; the existing DE ProcessControl
callback borrows the pad/provider only for its local scope and restores physical
state before returning. This mode makes no script `SET_PLAYER_CONTROL` write.

The frontend starts hidden. Eligible native identity, routed provider queries
and a completed/restored scope must be observed before its child becomes visible
and accepts input. Closing, cancellation or focus loss detaches GUI input first,
then retries native End before retiring bindings, XML, GUI and GPU owners. Core
keeps those owners alive while native restoration is pending, including after a
cleanup deadline. Script pauses therefore cannot hold a persistent player-control
disable flag in this new mode. Historical Frontend160 trials keep their separate
script lease and unchanged layouts.

**Core-owned frontend input passed in GTA on runtime `e93524ae3`.** The matched
PID 10412 / run 389063 records show 138 successful presents at 1920x1001, 34,647
nonzero readback pixels and all 426 native input scopes restored without failure.
The real configured manager produced 145 neutral frames. Help and Clear each
executed once, with two accepted commands and no rejections. All 56 ordered
script snapshots agree with Core's final 256 fields; the embedded console fields
match the separate fresh 32-field console record.

The script reports successful workload and cleanup without player-control
disable/restore writes; no independent opcode-level trace is claimed. Core,
GUI, XML and pthread unloaded, while the reusable bridge and pinned game adapter
remained. Normal exit preserved every live log prefix, appended exactly 74 CLEO
shutdown bytes and left the other eleven logs unchanged. The early GUI close
completed the test after about 7.6 seconds; no Quit callback was recorded.

The user confirmed the menu worked, CJ/camera stayed blocked and controls
returned. This is separate human evidence: native queries outside local
ProcessControl remain physical, and the machine camera/full-input flags still
stay false. Full game/network readiness also remains false. The menu runs
for up to 20 seconds after native admission, and activation still waits
indefinitely for Ctrl+Shift+F9 and release. Use F8 for the actual console, About/OK
for credits and Quit to end this trial; Quit does not close GTA.

Validation includes 1,507 checks in thirteen actual MTA menu/console/bindings
hosts, 1,294 native callback checks, 26 worker/mailbox cases, 970 script mocks
and 51 saved-log reader tests. The normal x64 Core build and classic compile
passed, with 294 entry checks for each classic and nonclassic diagnostic layout.
Deferred admission, discarded early input, one-time focus, retained cleanup and
neutral current/previous history are covered separately from live GTA behavior.

**The following offline step reuses MTA's per-player data storage.** The existing
`CRemoteDataStorage` SDK interface and classic accessors now share an implementation
parameterized by the backend pad. Classic MTA retains its full `CPadSAInterface`,
public member order and borrowed state pointers; DE uses only the canonical
96-byte current/previous controller pair. The adapter commits this history only
after its existing native update succeeds. Shot, stats, camera and gravity values
remain CPU data; this grants no remote engine-context or gameplay capability.

Both storage architectures pass their focused checks, including the original
classic layout and complete pad copying. Actual module tests pass 1,733 checks,
and the x64 Core adapter consumer passes 1,684. Normal x64 Core, classic Core and
classic multiplayer consumer compilation also pass. This storage implementation
is included in the installed `91819c896` frontend candidate. Its live frontend
regression does not grant remote engine-context or gameplay capability.

**The real MTA menu and player-control handoff passed in GTA.** Runtime commit
`c949c4a90` recorded 323 GUI frames and successful presents, a completed 323/323
fence, and 34,647 nonzero pixels at 1920x1001. About opened the existing credits
and Quit ended the trial after about 17 seconds. All 128 copied script snapshots
were ordered; the final 160 fields exactly match Core's terminal record.

The script verified initially enabled control, disabled it with native readback,
then restored it after GUI input detached and acknowledged the same generation.
Core/cgui/XML/pthread unloaded; the reusable bridge and game adapter remained.
The user confirmed menu interaction, player/camera blocking and restored controls.
Those visual observations are recorded separately from the native readbacks;
there is no independent screenshot or instrumentation of every camera consumer.

**The frontend now reuses MTA's actual console and command parser.** F8 toggles
the existing console widgets. Help and Clear invoke the ordinary MTA handlers;
the frontend command policy refuses networking, gameplay, nickname, scripted-bind
and generic CVar paths until their real dependencies exist. History navigation
and completion use `CEntryHistory` in memory without loading or saving disk history.
Menu and console input claims are combined, so closing either cannot release the
other's ownership. Classic console behavior retains its existing policy.
Thirteen owned WARP cases pass 1,314 checks of the real parser, Unicode, history,
command refusals, ownership and partial/retained cleanup. They exposed a Default
skin close button clipped out of its titlebar; using the existing system-button
type fixes its hitbox while preserving the ordinary button look.

The first live console trial displayed the menu and console, but did not submit
`help`: the terminal input contained four bytes while accepted/executed command
counts stayed zero. Near the end of the 30-second window, native player control
became enabled unexpectedly and the script cancelled the trial. Its writer is
not established by the logs. Native readback and the same-generation restoration
acknowledgement completed without another engine write; Core/cgui/XML/pthread
unloaded. That attempt did not verify command execution. Clean cancellation is
reported separately from workload success, so it cannot turn this failed trial
into a console interaction pass. F8 is available only while the trial is active.

Startup now attempts keyboard focus once after the admitted frontend has created
its widgets, while GTA's parent window is already foreground and active. This
removes the initial mouse-click requirement for F8 and typing. Background games
are not activated, denied focus keeps the existing click path, and later focus
loss is respected.
The owned host queues F8, `help` and Enter through the actual startup focus,
verifies the existing Help handler, then checks that focus is not reclaimed.
Another case preserves a different disposable window's foreground and focus.

**Live console command dispatch passed on runtime commit `db2d4f0f1`.** The
matched PID/run records show three accepted and executed commands, no refusals,
and the final Help handler. The raw console log records `help`, `clear`, `help`
in order with two Help output sections; the user confirmed Help/Clear worked.
The first copied focused snapshot arrived 518 ms after trial start.

This is a console-command milestone, not a complete frontend trial pass. The
script paused before its restoration acknowledgement, and Core exceeded its
cleanup deadline. Refocusing GTA let the script restore the saved native control
state with one verified write and acknowledge the same generation. Core, GUI,
XML and pthread then unloaded; normal exit appended exactly 74 shutdown bytes.
The final script reports cleanup complete and overall workload failed. No Quit
callback was recorded in this run, and full client/game/network readiness stays
false. After future menu trials, keep GTA focused for five seconds after the
menu disappears before returning to the test prompt, so script cleanup can run.

Console1 has a separate 32-field owner-thread API and bounded copied UTF8 text.
The existing Frontend1/32 and Frontend160 layouts stay unchanged. The live runner
enables console input only after the script acknowledges native control capture;
shortcuts require a fresh lease. Its separate `gui-console.log` terminal record
uses the same PID/run identity as `gui-frontend.log`. The saved reader separates
command execution and CPU cleanup from rendering, native control and human visual
proof. Full game, multiplayer and ordinary startup capability gates remain closed.

**The live workload reuses MTA's real main menu and a reversible player-control
lease.** The separate Smoke option `-MtaCoreGuiFrontendReference` selects Core13 /
Frontend1 with 160 copied scalars. After native device/window observation ends,
the script saves initially enabled player control, calls GTA's existing
`SET_PLAYER_CONTROL(false)`, checks `IS_PLAYER_CONTROL_ON`, and acknowledges its
generation. Only a fresh acknowledgement admits the interactive MTA surface.
The existing Win32 translator routes clicks and keys to the actual menu,
About/credits and Quit callbacks; no replacement menu or game facade is used.

Cancellation, focus loss, identity loss or an expired heartbeat revokes GUI
input. Input holds/capture detach before the script restores and checks the
saved control state, even if GPU cleanup remains pending. Core, GUI and native
owners remain retained until their actual retirement is proved. A rejected
restoration acknowledgement requires fresh identity and native readback before
retrying; a past timeout or input fault cannot conceal completed cleanup.
Forced CLEO termination still has no independently validated engine-command
executor for restoration.

Owned menu and console hosts cover Win32 interaction, focus, resize, partial
startup and retained GPU/CPU cleanup. The script suite passes 895 mock checks;
separate controlled worker tests exercise lease expiry,
generation/owner rejection, restoration and final-read contention. These are
owned-host results. The live result above adds native restoration proof and the
user's visual report; control-command readback alone is not camera proof. Full
game, networking and multiplayer startup remain gated and the port stays
EXPERIMENTAL.

The frontend package owns 58 files: the existing 36-file GUI group plus 22
upstream MTA menu/localization assets. Ordinary 23-file and passive 36-file
packages retain their separate modes. `package.ps1 -OutputDirectory` and
`update.ps1 -PackageDirectory` allow a fresh candidate directory without
overwriting packages bound by earlier live evidence. Activation waits
indefinitely for Ctrl+Shift+F9 and key release; the menu runs for up to 30 seconds.
Quit/Escape or Ctrl+Shift+End requests trial cleanup. No server connection or
automatic movement is part of this trial.

**The existing MTA main menu now has a separate frontend lifecycle.** The real
Core starts its XML/configuration, CVars, localization, logging, commands and
`CLocalGUI`/`CMainMenu` on a borrowed MTA D3D12 GUI session. About opens the
existing credits window, its OK button closes it, and Quit requests host cleanup.
Game settings, server browsing and networking remain unavailable until their
actual DE backends are ready. Classic startup retains its eager services.

The optional `MTA_*DEFrontendSession` exports enforce one owner thread and keep
failed startup inspectable until explicitly released. Configuration is read-only;
the frontend does not change application-wide locale preferences. It uses the
real updater's cached metadata with background requests refused and no worker
started. Teardown retires menu callbacks and services before XML or GUI owners.
Owned WARP tests exercise menu/credits rendering, actual callbacks, wrong-thread
rejection, startup failure, reacquisition and CPU teardown with GPU work retained.
They do not establish full game or multiplayer startup.

The Core-owned surface has a separate internal interaction opt-in, using the
same Win32 translator as classic MTA and the actual CGUI button/edit callbacks.
The D3D12 session draws MTA's cursor. Focus loss, capture loss and shutdown
cancel mouse presses without completing clicks, while mouse capture changes
preserve keyboard modifiers. Unicode input and owner-thread checks are shared.
Owned WARP tests exercise these paths with real MTA widgets and GPU readback.
The new live workload uses a distinct opt-in and handshake. The earlier passive
GUI trial does not grant native input ownership or validate the new interaction.

**The Windowed MTA GUI trial passed, with user-confirmed eight-second visibility.**
Runtime commit `27c54ea9d` acquired GTA's device/window lease and ran the existing
CGUI/CEGUI through an MTA-owned child composition surface. Core recorded 158 GUI
frames and 158 successful presents, with no compositor-busy retries. The final
fence signaled/completed at 158/158. Readback from the 1920x1001 BGRA8 target
verified 115,860 nonzero pixels, transparent background, the red texture and
premultiplied alpha. Core completed in 8,411 ms; the script observed 8,580 ms.

The script captured 72 valid snapshots; its terminal snapshot matches the final
96-scalar Core record. Core/cgui/XML/pthread unloaded, child/compositor cleanup
completed, and the backend remained
pinned as expected. All 36 installed hashes matched. Normal exit preserved the
32,604-byte live log plus exactly 74 shutdown bytes; the 1,447-byte GUI log was
unchanged. Independent reviews and the completed checkpoint preserve the source,
package, whole logs and explicit user visibility report together. No independent
screen image was captured, so screenshot-based visibility verification remains
false. Full client startup, GTA GUI input capture and multiplayer remain unfinished.

An earlier attempt canceled after 392 ms because validation incorrectly required
full display completion before accepting early readback. The frontend and reader
now accept readback from started, submitted MTA-owned rendering while preserving
the full-duration success and cleanup requirements. Fullscreen attempts did not
acquire the required window/device lease. Use Windowed mode inside GTA for this
trial. The port remains EXPERIMENTAL.

**Earlier offscreen milestone: rendering, cleanup and normal exit passed.**
Actual MTA Core started the existing CGUI/CEGUI session on GTA's retained
D3D12 device. Its real widgets, text and texture rendered into an MTA-owned
640x480 BGRA8 target. Pixel readback verified 133,058 changed pixels, the
background, texture and opaque alpha. The helper submitted its two clear/copy
lists and the GUI submitted one frame; the final fence signaled/completed at
1/1. Core's bounded session completed in 401 ms; the script observed 523 ms.

The explicit device lease came from generation 1, with one accepted snapshot,
one native dispatch and coherent source profile 2. The native observer ended
before GUI/GPU work. The final of two logged Core11 / GUI1 snapshots matches the
terminal 64-scalar Core record; the separate 19-scalar worker report confirms cleanup.
Core/cgui/XML/pthread unloaded, the backend remained pinned as expected, and
all 36 installed hashes matched. The user reported normal GTA continuation.
Normal exit preserves the complete 6,580-byte capture plus 74 shutdown bytes;
the 219-byte GUI log is unchanged. Whole logs, the source/package snapshots and
independent actual/normal-exit reviews are sealed together.

That earlier run validates MTA's real GUI on the actual device using its own
queue and offscreen resources. Native queue/backbuffer writes remain unverified;
full startup and multiplayer remain gated. Direct native-frame drawing still
needs resize and owner-thread drain handling.

The visible trial reuses the same MTA GUI through a Core-owned transparent
child surface inside GTA's window. `package.ps1 -Mode Smoke
-MtaCoreGuiSurfaceReference` selects this separate, disabled-by-default trial.
It takes a one-shot device/window lease, ends native observation, then creates
its own DirectComposition swapchain and queue. The existing CGUI/CEGUI widgets
are displayed for eight seconds; Core handles resize, parent closure and GPU
drain before removing the child surface. No game queue or backbuffer is written.
This path requires windowed or borderless GTA. The user confirmed the panel
remained visible for eight seconds; an independently attributed screen capture
is still absent. This bounded trial displays widgets; mouse forwarding and GUI
interaction still need validation.

Its independent Core12 / Surface1 protocol carries 96 scalars and writes
`mta/logs/gui-surface.log`. Successful presents, compositor-busy retries,
parent-window validity and complete child/compositor cleanup are separate
facts. Pixel readback proves the owned target; compositor Present alone does
not prove the pixels were visible on the screen. The ordinary 23-file package
and the older offscreen GUI trial retain their separate modes and protocols.

The visible candidate's local tests pass 238 checks across six disposable
surface cases, including a composed screen capture, resize allocation failure,
parent closure during GPU drain and guarded release failure. Its shared
lifecycle passes 865 checks on each architecture, while the existing offscreen
477 checks remain green. All 89 worker scenarios pass against the selected
normal Core bundle. Package ownership passes 1,144 checks across 113 cases,
including the ordinary, offscreen and visible-surface variants. The corrected
frontend passes 766 mock tests and the saved-log reader passes 27 tests. These
results establish the owned implementation and its cleanup behavior; the live
Windowed run above validates the complete bounded workload and records the
user's visibility confirmation.

`package.ps1 -Mode Smoke -MtaCoreGuiStartupReference` selects a separate,
disabled-by-default trial. Its 36-file bundle adds the real `cgui.dll` and
MTA's skin/font assets to the ordinary 23-file bundle. Install, update and
uninstall require the complete owned group; ordinary packages remain unchanged.
The launcher's existing 23-file headless preflight does not certify this trial.

An explicit, one-shot device lease is separate from read-only API6. Core waits
for a coherent native callback, takes one owned device reference, and ends the
observer before constructing an MTA-owned queue and a 640x480 BGRA8 offscreen
target. The existing CGUI/CEGUI session renders real widgets, text and a texture;
Core verifies GPU completion and pixel readback before releasing its owners.
Neither the native queue nor GTA's backbuffer is written. No visible overlay,
NPC or automatic movement is expected, and this does not unlock full startup.

The separate Core11 / GUI API1 protocol publishes 64 unsigned scalars. It keeps
native observation, lease provenance, owned GPU work and cleanup distinct.
Pending fences retain their owners; a cleanup timeout or release fault cannot
report a successful unload. Core also writes `mta/logs/gui-startup.log`; the
saved-file reader requires a fresh PID/run and the exact pre-trial baseline.

Offline validation covers real three-buffer WARP swapchains in formats 24/87/28
(777 checks), the Core GUI helper including delayed signal and release faults
(103 checks), and all 65 startup-worker scenarios. The normal Core and GUI
builds pass with classic Win32 startup preserved; legacy D3DX9 loading is
delayed on the opt-in x64 GUI path. Package ownership passes 1,008 checks
across 105 cases, including complete 23/36-file upgrades and rollback.
These owned fixtures cover the lifecycle around the actual-device result above;
they do not establish visible GTA drawing or multiplayer.

**Latest live API6 result: native backbuffer association passed; cleanup and normal exit passed.**
The primary-backbuffer trial recorded 603 native callbacks, 601 accepted
snapshots and 601 native dispatches, with no association rejection. All 87
logged API6 samples report coherent profile 2 association, complete queries,
`S_OK` probe and forwarded results, and matching native/DXGI indices across
buffers 0/1/2. All ten fresh roots reports completed their after-Close
readback without a refusal or exception.

GTA continued normally. Hooks were removed, all held threads released and
Core/XML/pthread unloaded. All 23 installed hashes matched. Normal exit
preserves the complete 46,055-byte capture plus 74 shutdown bytes; the
2,678-byte endpoint, 1,583-byte setup and 2,369-byte roots logs are unchanged.
This validates native CPU/COM buffer association. Native frame GPU ownership remains
unsupported, with zero accepted GPU probes; no GPU commands or drawing were
included. The separate system DXGI API5 endpoint remains unadmitted. Full
MTA startup and multiplayer remain gated, and the port stays EXPERIMENTAL.

The next candidate above reuses MTA's existing D3D12 CGUI session. Its explicit
device handoff leaves read-only API6 unchanged and grants no native queue or
backbuffer write permission.

**Preceding live API6 result: logged texture-null refusal; cleanup and normal exit passed.**
The roots-diagnostic ten-second trial installed its hooks with 187 initial/peak held threads and
three stable-discovery passes. It recorded 606 native wrapper entries, 601
accepted scalar snapshots and 87 complete metadata/known-presenter reports.
The native dispatch hook ran 601 times; all 601 association queries refused.
Every sampled dispatch report has profile 2, `QueryFailed`, query stage
`NativeRoots` and probe HRESULT `E_POINTER`. The forwarded target returned
`S_OK`; its result is separate from the failed probe. All 11 fresh roots
reports identify `Texture` (checkpoint 22), `RequiredNull`, no guarded
exception, known node mask 1 and buffer indices 0/1/2. The selected texture
slot read succeeded, then its required-nonnull guard failed. These sampled
reports do not identify every unsampled query's cause, the selector branch,
why the slot was null or the cause of an earlier trial.

GTA continued normally. Hooks were removed, all holds released and
Core/XML/pthread unloaded. All 23 installed hashes matched. Normal exit retains
the complete 46,824-byte capture plus 74 shutdown bytes; the 2,291-byte endpoint,
1,339-byte setup and 1,235-byte roots logs are unchanged. Buffer association, GPU ownership,
drawing and full MTA readiness remain unverified.

Pinned native instructions distinguish the primary backbuffer at
`viewport+0xA8` from the format-dependent helper's nullable alternate at
`+0x110`. The observer incorrectly required that helper-selected texture
when associating the DXGI backbuffer. The correction follows the primary
texture, retaining the existing selector/coherence reads, nonnull guards,
canonical `GetBuffer` identity, resource state, queue and cleanup checks.
That earlier log did not establish that the primary slot was nonnull; the
subsequent trial above validates the corrected association. Earlier command-data extraction
was separately corrected to read `context+0x1D0` then data `+0x18`; neither
source correction retrospectively identifies an earlier trial's exact cause.

An optional, separately versioned 14-scalar roots diagnostic now records the
existing read checkpoint, refusal category, root-query phase, actual guarded
exception code and safely obtained node/index scalars. It performs no new
native reads and changes no association guard. Reports publish only after an
accepted callback returns and carry that generation's snapshot/dispatch
counters. Core caches the export before Begin and writes bounded JSONL to
`mta/logs/render-roots.log` on its owning worker. Missing, busy or held reads
preserve caller output; a failed final read cannot replay a pre-End report.
The strict saved-file reader attributes only fresh PID/run/generation records.
This diagnostic does not grant buffer admission, GPU work or readiness. Its
live result is the logged predicate above; no walking test is needed.

The owned primary-texture regression fails on the old reader with a null
alternate, then passes with the correction, including a distinct decoy and an
unreadable alternate. A null primary still refuses even with a valid alternate.
Offline validation passes 3,697 native checks plus four unavailable-build
checks, 1,703 adapter checks, 1,330 x64 consumer checks, all 51 worker scenarios
and 49 scalar-logger checks on each architecture plus 16 saved-reader tests.
The normal Core build passes 39 guards and 248 startup checks in each layout.
Owned endpoint/callsite checks remain separate from live frame association.

**Preceding live API6 result: stable discovery limit refused; cleanup and normal exit passed.**
The trial stopped after 272 ms with `StableLimit` (stage 21), no OS error, 189
initial/peak held threads and four discovery passes entered. No transaction or
hook write began. All holds were released and Core/XML/pthread unloaded. GTA
continued normally; all 23 installed hashes matched. The final 6,511-byte log
preserves the 6,437-byte capture plus 74 shutdown bytes, and the 1,133-byte
endpoint and 488-byte setup logs are unchanged through exit. Callback and
dispatch/buffer observations remain zero.

The diagnostic does not distinguish the per-pass enumeration bound from
four-pass exhaustion, or identify a thread, its liveness, an exited-thread skip,
or the cause. The next correction passes 3,617 native checks plus four
unavailable-build checks. It continues with a valid iterator and a separate
bounded cache of exact exited objects. First sightings reset stability; every
repeat still needs fresh exit proof before it can leave stability unchanged.
The four-pass budget and two consecutive stable passes remain. Separate owned
Windows programs pass 27 continuation and 34 native identity checks; these do
not identify the actual game's limit cause or validate rendering.

**Preceding live API6 result: stable suspension refused; cleanup and normal exit passed.**
That trial stopped after 288 ms. Its setup report identifies `StableSuspend`
(stage 18), Win32 access denied (5), on a newly discovered thread after 190
initial holds. No transaction or hook write began; all holds were released and
Core/XML/pthread unloaded. GTA continued normally and all 23 installed hashes
matched. The final 6,511-byte log retains the full 6,437-byte capture plus 74
shutdown bytes; the 885-byte endpoint and 244-byte setup logs are unchanged
through exit. Callbacks and dispatch/buffer observations remain zero.

The failed thread's identity and liveness are unknown. An owned Windows test
passes 350 checks across 20 cases and reproduces error 5 for both exited threads
and a live thread without suspension rights. The correction now passes 3,483
native checks plus four unavailable-build checks. It discards only a proven
exited handle, then restarts bounded discovery; live and unconfirmed threads
still refuse. Retained exited objects can still exhaust the unchanged four-pass
limit safely. The earlier API6 trial's unlogged failure remains unknown.
The separate strict DXGI refusal is tolerated during preparation. No further
walking test is needed.

**Latest live API5 result: pre-existing Present patch identified; cleanup and normal exit passed.**
The ten-second trial ran for 10,001 ms with 603 native wrapper entries,
600 accepted scalar snapshots and 87 logged complete metadata/known-presenter
reports. All 87 API5 tails contain EndpointUnadmitted 1, profile 0 and no
Present or buffer-association observations. The human reported normal GTA
continuation. Core/XML/pthread unloaded, all 23 hashes matched and the complete
40,922-byte shutdown log passed independent review.

The separate 20-scalar endpoint diagnostic identifies a loaded-code hash
mismatch before MTA's Present hook installation. The native swapchain slot
still points to DXGI RVA `0x19000`, whose first E9 branch targets private
executable memory. This proves an existing patch, without identifying the DLL
that installed it. Actual Core's bounded JSONL and the complete CLEO log are
sealed together; the existing 110-field observation contract remains unchanged.

API6 source work now adds a separate native dispatch record at the game's
source-pinned Present callsite. It forwards the existing target and preserves
the original return address and argument values. The 110-field API5 prefix
remains separate from the new 26-field profile2 record. The native, Core,
worker, bridge and normal build checks pass offline. The latest trial above
validates native dispatch and buffer association; drawing remains unverified.
No GUI or automatic movement is included in the read-only trial.

`-MtaCoreRenderNativeDispatchReference` is a separate Smoke opt-in requiring
all four earlier renderer flags and the explicit D3D12 route. Its Core10/API6
wire contains 136 scalar fields. The source flag defaults to false, and the
transaction requires the verified local Windows syscall profile before holding
threads. No association report grants permission to submit GPU work.

The diagnostic candidate adds an optional API6 setup diagnostic alongside those
contracts. Its 18 unsigned scalars identify the first failed setup stage and
explicit error code, with installation and cleanup facts kept separate. MTA
Core resolves its export before Begin and writes bounded
`mta/logs/render-setup.log` records only when no threads remain held. Neither
that report nor a successful offline fixture establishes live dispatch or
permission to draw.

The serial package ownership suite passes 687 checks across 59 disposable
install/update/uninstall and configuration cases. The earlier refusal while
owned worker hosts were running remains saved as a failed test result.

**Earlier live renderer result: native D3D12 presenter identified; drawing remains gated.**
Actual Core's API4 trial completed in **10,049 ms**, with **199 Core samples,
87 script snapshots, 606 native entries and 603 accepted scalar snapshots**.
All 87 logged reports contain complete metadata and identify the known native
custom presenter: status 2, implementation 1, slot mask 127 and policy flags
15. Every published GPU probe remains rejected, with zero accepted probes
and no GPU commands. These counters establish observation and implementation
identity, not safe target state or permission to draw.

The human reported GTA continued normally. Observer/Core/XML/pthread cleanup,
the expected backend PIN, all 23 installed hashes and normal exit passed.
Independent live and shutdown reviews are preserved with the complete
36,311-byte log and immutable candidate archives. The next integration must
account for the presenter's Social Club callback and release GUI-held
backbuffers after the native queue drain and before resize. No MTA GUI is
installed into GTA yet; that completed runtime is preserved as historical evidence.

**API5 observer: actual standard Present and backbuffer association.**
The API5 candidate checks the standard DXGI Present entry after the native
Social Club callback, using the existing MTA Core owner and startup worker.
The completed trial refused endpoint admission. Its object and CPU tracker observations cannot
establish GPU completion, resource state or permission to draw. No GUI, input
writes or GPU commands are included; the port remains EXPERIMENTAL.

The current diagnostic suites pass 2,623 native checks, 1,642 module checks,
940 x64/3 x86 normal-module consumer checks, 10,292 lifecycle checks on each
architecture, 41 worker scenarios, 293 bridge protocol/1,482 ABI checks,
542 script checks and 64 reader methods. Package ownership passes 663 checks.
The normal Core/Game DE build passes 219 actual Core startup checks in each
folder layout, with classic Win32 compilation and default x64 exclusion intact.
The separately sealed GUI asset-root proof remains historical; this trial
ships the 23-file diagnostic bundle without cgui or GUI assets.

**Historical live renderer diagnostic: clean lifecycle, no D3D11 callbacks.**
The ten-second Core-owned trial ran for **10,004 ms**, with **198 Core samples
and 87 script snapshots**. All native callback counters stayed zero. The user
reported that GTA continued normally. Core/XML unloaded, the observer detached,
the backend PIN matched, all 23 installed hashes matched, and normal shutdown
was saved in the complete 24,886-byte log. The reader reports
`native_observation_missing` with `diagnostic_lifecycle_complete=true`.
This validates diagnostic cleanup; the active renderer path remains unconfirmed.
Loaded D3D11/D3D12 modules and missing D3D11 callbacks do not identify that path.

**Completed live check: D3D12 callback observation through actual MTA Core.**
The explicit D3D12 session completed in **10,046 ms**, with **199 Core samples,
88 script snapshots, 605 native callback entries and 603 accepted snapshots**.
Rejected, nested, foreign-thread and missed publications stayed zero, with no
native failure. The callback reported 1920×1080, window/swapchain presence `3`,
known presence `19` and a separate command queue. Device and immediate context
remain unknown. The first snapshot correctly reported an unobserved route;
later accepted snapshots identified route `2` on one callback thread.

The user reported normal game continuation. The native observer detached,
Core unloaded, the backend remained pinned as expected, and all 23 installed
hashes matched. Normal exit passed with the complete **26,948-byte** shutdown
log. This validates the bounded D3D12 callback and cleanup through MTA ownership.
Callback entries, normal returns and queue presence do not establish COM Present
execution, visible MTA output or a working GUI.

**Completed live GPU diagnostic: custom-present refusal and clean shutdown.**
The first API3 trial ran for **10,037 ms**, with **199 Core samples, 87 script
snapshots, 605 callback entries and 602 accepted native snapshots**. Native
rejections, nesting, foreign callbacks, publication misses and failure stayed
zero. All 602 published GPU probes were rejected; every logged sample
reported Unsupported at the custom-present safeguard before any COM query. Native
GPU/AFR counts were both one, while COM metadata stayed unobserved.

The user reported normal game continuation. Cleanup detached the observer and
unloaded Core/XML/pthread, retaining the expected backend PIN. All 23 installed
files matched, and normal exit was verified in the **33,440-byte** whole log,
including the exact captured prefix and 74 appended shutdown bytes. The reader
reports a clean diagnostic lifecycle with GPU ownership still unverified.

**Completed live native metadata check: custom-present remains unsupported.**
The next API3 trial ran for **10,011 ms**, with **198 Core samples, 88 script
snapshots, 604 callback entries and 600 accepted native snapshots**. Native
rejections, nesting, foreign callbacks, publication misses and failure stayed
zero. **87 logged snapshots** contained complete native swapchain, queue and
backbuffer metadata. All 600 aggregate published GPU probes were rejected;
each complete logged query reported Unsupported with custom-present set and
the drawing support predicate false.

The sampled native objects share the same canonical device, report one device
node and a direct queue, and describe three 1920×1080 buffers in DXGI format
24 (`R10G10B10A2_UNORM`). These facts do not identify the custom presenter's
actual frame sink, color space or synchronization boundary. The user reported
normal game continuation. Cleanup detached the observer and unloaded
Core/XML/pthread; the expected backend PIN remained. All 23 installed files
matched. Normal exit passed in the **35,646-byte** whole log, with the exact
captured prefix and 74 appended shutdown bytes. Native metadata is observed;
GPU drawing ownership remains unverified.

**Offline prerequisite: MTA's existing GUI layer on x64.**
The CEGUI, Falagard, DirectX9GUIRenderer and `cgui` projects now admit only
opt-in `--with-definitive-adapter` Release/x64 builds. Default x64 exclusion and
classic Win32 behavior remain intact. The real `cgui.dll` keeps its existing
`InitGUIInterface(IDirect3DDevice9*)` entry and uses MTA's existing widgets,
fonts and skins. This D3D9 subsystem is a prerequisite for reusing the widget
layer; it is not a DE rendering backend.

The port preserves the memo interface's 32-bit indices, uses a native-width
font registry handle and gives x64 callbacks aligned storage for the compiler's
widest member pointer. Win32 keeps its exact 28-byte callback layout. A real
offscreen GUI test also exposed a renderer leak: CEGUI borrows the renderer,
so `CGUI_Impl` must delete it after System finishes its texture/font cleanup.

`Tests/definitive/test_gui_x64_host.ps1` loads the real normal GUI/Core modules
in an owned host, without initializing Core or a CGame. It stages the existing
MTA skin/font assets and the repository's pinned Unifont release, creates a
hidden private D3D9 device, and checks widget callbacks, allocated text,
offscreen pixel changes, device reset and complete teardown. Its host filename
selects Core's existing DE classification; its executable contains only test
code and never launches the game binary. Device references must reach zero;
the fixture does not drain leaked references to manufacture success.

The final normal GUI build passed 66 policy/artifact guards and Win32 compile
checks. The fresh Core build passed 132 actual entry checks in each folder
layout. The real GUI host passed 91 checks, rasterized 128,964 non-background
pixels, handled widget/text events and reproduced the same image after device
reset. GUI/Core unloaded and the owned device's final release reached zero.
The saved old callback header fails the adjacent-storage canary; the saved
old GUI DLL fails the device-release check. These results validate the reused
x64 D3D9 GUI subsystem independently of the DE runtime.

The classic D3D9 Present path also requires a working CGame and cannot be called
from the DE callback. The next rendering work belongs behind CEGUI's existing
renderer/texture abstractions. D3D12 GPU ownership, synchronization and resize
lifetime, visible output and full MTA startup remain unverified and gated.

**Optional D3D12 backend for the existing MTA GUI.**
`D3D12GuiRenderer` implements CEGUI's existing renderer and texture interfaces;
the optional `CreateGUID3D12Session` factory constructs the real `CGUI_Impl`,
fonts, widgets and skins. Its versioned session supplies an actual target and
caller-declared resource states. The original D3D9 factory and public GUI
vtables keep their existing signatures. Browser textures and external D3D9
render targets remain unavailable through this D3D12 session.

Three fenced slots retain submitted allocators, descriptors, targets and
immutable texture versions. A busy queue returns `Busy` without waiting or
reusing live resources. Resize, shutdown and release retain ownership until
submitted work retires. Creation-thread and reentry guards protect CEGUI's
singletons; failed initialization unwinds the actual font/System ownership.
`Tests/definitive/test_gui_d3d12_host.ps1` exercises this production factory
against a private offscreen WARP device with MTA's actual assets, GPU pixel
readback, widget input, blocked-queue cleanup and resize. It does not run GTA
or use an engine-owned target. The GUI DLL is not included in the live probe.

The additive `ConfigureGUID3D12AssetRoot` export configures cgui's own existing
asset resolver before GUI construction. Core's root override is separate
because each DLL compiles its own SharedUtil state. The counted UTF-16 path
must be absolute, at most 32,767 characters, with `MTA/cgui`, `MTA/logs` and
`skins/Default` directories already present. Rejected configuration preserves
the prior root; a live legacy GUI or explicit session excludes changes. The
original session v1 layout and factory are unchanged.

Fresh tests now place the owned executable at `Win64/SanAndreas.exe` and the
actual Core/cgui DLLs and assets under `Win64/CLEO/de-online`, matching the
diagnostic adapter layout. They configure Core and GUI independently, with no
classic `MTA/core.dll` fallback. The normal build passes 70 policy/artifact
guards and both classic Win32 compile checks. Actual D3D12 hosts pass **510
checks, 170 per format**, including nonterminated input against a guard page,
worker-to-render-thread root publication, rejection/preservation and widget
rendering. The existing D3D9 host passes **93 checks**. These owned-device
results do not change the installed game runtime or enable DE drawing.

The preceding normal opt-in GUI build passed 69 policy/artifact guards, including
classic Win32 compile checks. The actual D3D12 WARP host passed **459 checks:
153 each for BGRA8 (87), RGBA8 (28) and R10G10B10A2_UNORM (24)**. All three
fresh sessions render MTA's existing widgets, fonts and textures. Native
readbacks verify channel order, straight-alpha RGB blending and opaque alpha
preservation, including raw 10-bit RGB 547/58/86 and two-bit alpha 3. Numerical
checks use untouched target pixels; nine separate BMP previews normalize only
artifact copies to BGRA8. Unsupported neighboring formats and a supported
resource with the wrong target format are rejected before submission.
Blocked-queue tests covered all three slots, texture destruction, resize and
busy shutdown/release. The final owned device release reached zero. The D3D12
debug layer was unavailable; these results come from actual GPU readback,
fences and ownership checks. Color space, HDR behavior and actual DE drawing
remain unverified. The prior D3D9 host passed 91 checks; its renderer is unchanged.
An earlier candidate aborts on an invalid-format constructor before CEGUI has
created its logger; the corrected backend throws without that singleton and
then successfully constructs the real GUI after the rejected attempt.

The pinned native caller emits transitions toward state zero before the
observed wrapper, but has custom-present and multi-GPU branches and rotates
buffer pointers after returning. That trace does not establish the ownership
or synchronization needed for MTA to submit drawing commands. Live drawing
therefore remains disabled while callback-local GPU metadata is validated.

**Separate read-only GPU ownership diagnostic.**
`package.ps1 -Mode Smoke -MtaCoreRenderReference -MtaCoreRenderRoute D3D12 -MtaCoreRenderGpuReference`
explicitly admits API3. Its separate factory, Core exports and 80-field wire
preserve API1/API2. The original first 44 fields retain their meaning; the
additional fields describe published query counts, stage/HRESULT, queue
properties, device identity, current backbuffer metadata and custom-present
or multi-GPU exclusions. Older observers execute no COM queries.

Queries run only inside the naturally invoked callback and release acquired
interfaces before the engine original can rotate its buffers. They submit no
commands and retain no swapchain or backbuffer. Canonical device identity,
single-node eligibility and matching descriptors are metadata checks; they do
not authorize drawing, prove resource state, or establish resize safety.
Failed or unsupported queries keep their exact stage and raw HRESULT in the
saved evidence. The whole-log reader separates clean diagnostic teardown from
GPU ownership acceptance and rejects nested pointer/readiness claims.
The current diagnostic permits read-only queries of the existing native
swapchain and creation queue when custom-present GPU/AFR counts are both one.
Those objects need not be the custom presenter's actual frame sink. Other
custom/count combinations retain the early refusal. The support predicate
still excludes every custom-present route, so complete metadata cannot enable
drawing. ABI3 and its 80 fields remain unchanged. The reader's
`native_gpu_metadata_observed` reports complete sampled queries separately
from GPU ownership; its sample count does not describe unlogged probes.

**Additive API4: identify the native custom presenter without calling it.**
The separate factory, Core exports and 84-field wire compose the existing
API3 state and append status, implementation ID, seven-slot signature mask and
static policy flags. API1/2/3 factories and layouts remain intact; fields 78/79
stay reserved and zero. The explicit package opt-in is
`-MtaCoreRenderCustomPresentReference`, together with the existing Core render,
GPU-reference and D3D12 selections. The source flag defaults to false. Package
and mode changes reject invalid combinations and clear classification when
returning to earlier diagnostics or ordinary modes.

The classifier checks the exact image-owned table, every slot and six unique
complete function signatures through guarded reads. Its device association
must match the native roots and survive coherent readback after the COM
attempt. It never calls a custom-present method or exports its address. Known
status 2 has implementation 1, signature mask 127 and static policy flags 15;
unknown, signature, read, device and root failures cannot become Known. These
policies describe the pinned implementation's native-present request, disabled
fallback advance, no-op post-present callback and resize notification. They do
not prove that a branch ran, identify current resource states or authorize
drawing. API3's custom-present support exclusion remains unchanged.

MTA Core retains the classification lease through the existing worker and
cleanup lifecycle. The reader's `custom_presenter_recognized` and sample count
describe logged recognized classifications independently of COM query success.
They do not promote GPU ownership or full startup. The completed live result
is qualified above: all 87 logged classifications are Known, while all 603
published GPU probes remain unsupported for drawing.

The owned COM-query fixture covers a real WARP swapchain and balanced
references, including custom-present metadata that remains unsupported for
drawing. The native callback fixture covers both the single-node query path
and the retained custom/multi-GPU refusal.
The focused checks pass 265 ownership and 2,466 native callback cases,
including repeated real custom-present metadata queries and balanced COM
references. These observer checks issue no rendering work.
Cleanup-release faults disable further queries before calling the
engine original; that failure survives a missed publication, a changed root
and an original exception. No GPU commands are submitted by this observer.

`package.ps1 -Mode Smoke -MtaCoreRenderReference -MtaCoreRenderRoute D3D12`
selects the separate, disabled by default 23-file trial. The real Core owns a
versioned observation lease from Game DE and runs it on its existing worker for
ten seconds. The ordinary gameplay gate remains intact. This trial draws
nothing and applies no input, networking or NPC changes.

The backend checks the exact executable and the complete selected function:
the 421-byte D3D11 callback or 429-byte D3D12 callback. The D3D12 viewport
constructor, destructor, native callers and swapchain creation establish its
owned callback and command-queue chain. For D3D12, `CreateSwapChainForHwnd`
receives a direct command queue, as specified by
[Microsoft's API contract](https://learn.microsoft.com/en-us/windows/win32/api/dxgi1_2/nf-dxgi1_2-idxgifactory2-createswapchainforhwnd).
The API1/API2 observer copies only dimensions, counters and presence flags from a natural
callback; no viewport, swapchain, queue, device or context address reaches Core
or JavaScript. D3D12 window/swapchain/queue knowledge is `19`; COM device and
immediate context remain unknown. D3D11 keeps its existing knowledge mask `15`.

API1 retains its original D3D11 factory, vtable and 40-field wire layout. The
additive API2 factory and exports use 44 fields, including requested route,
observed route, known presence mask and queue presence. Requested route is an
explicit choice; observed route stays zero until an accepted natural callback
returns and publishes. No automatic route guessing or API1 fallback occurs.
A false native return is valid when the original returned normally. Neither
callback entry nor return proves that COM Present ran or pixels were drawn.

After manual Ctrl+Shift+F9 activation and release, the script yields while
reading coherent scalar snapshots. Arming waits indefinitely. Ctrl+Shift+End
requests cleanup. Failed detachment or final readback retains the lease, actual
Core and worker for retries. A two-second cleanup deadline records failure
without permitting unload. Ended state is copied before lease release, and the
terminal Core diagnostics preserve the actual backend PIN.

The preceding API4 offline checks passed: 2,466 native callback checks, 1,579 module checks,
718 x64/3 x86 Core consumer checks against the exact normal Game DE DLL,
8,448 lifecycle checks on each architecture, all 32 worker scenarios,
293 bridge protocol/1,247 ABI checks, 503 script checks and 46 renderer log-reader tests.
The normal Core/Game DE build passes 190 actual Core entry checks in each
folder layout and preserves classic Win32 compilation and default x64 exclusion.
Fresh GUI builds and actual D3D12/D3D9 hosts bind the new Core binary; the
three D3D12 formats pass 459 checks and the D3D9 host passes 91. The ownership
suite passes 642 checks against updated package/configuration scripts with
previously built native fixture binaries; final package hashes separately bind
the newly built API4 binaries. Those ownership tests do not claim API4 runtime
execution in GTA.
The packaged launcher preflight verifies all 23 files while keeping full startup
unavailable. These checks use owned fixtures; they do not execute GTA.

Assess each complete saved trial with
`python utils/definitive_edition/core_render_log.py --log <saved-log> --output <assessment> --require-exit`
after normal shutdown. The reader keeps a clean diagnostic lifecycle separate
from native callback acceptance and rejects contradictory route or unknown COM
claims. Offline fixtures do not establish a live renderer, visible output, a
working MTA GUI or full startup. Full MTA remains EXPERIMENTAL and unplayable.

**Completed live check: actual Core configured bindings drive scoped native movement.**
`package.ps1 -Mode Smoke -MtaCoreMovementReference` selects this separate,
explicitly enabled 23-file experimental trial. The corrected trial passed the
configured local movement check described below.
The real Core owns the XML module, saved bind configuration, `CKeyBinds`,
adapter pulse and action synthesis. The bridge schedules the worker and copies
value-only log snapshots; JavaScript supplies no replacement key map or fixed
movement phases. After Ctrl+Shift+F9 and release, configured keys must reach a
neutral, focused baseline before the 30-second write window starts.
Ctrl+Shift+End requests cleanup. Source defaults and ordinary mode switches
disable this mutually exclusive Smoke trial.

The first live configured-write candidate stopped after **1,049 ms** when a
producer eligibility observation became unavailable. It recorded 20 native
samples, 10 script snapshots and 63 applied/restored scopes without native
failures. Right Shift reached MTA's walk action, but forward walking and sprint
were not captured. The human reported normal movement; this does not establish
the combined Core path because the trial had already stopped. Cleanup, normal
exit and all 23 installed hashes passed.

The correction publishes verified eligibility from the native callback for
the active Core producer. Each publication is tied to the trial generation,
local identity and a 250 ms freshness deadline. This avoids reading engine globals
while they are temporarily borrowed for another character. Before Begin,
unavailable observations wait within the existing readiness budget with no
writes. Every native write still requires the complete live eligibility guard.

The corrected Core-owned session completed in **30,009 ms**, with **595 native
samples and 259 script snapshots**. Forward input, isolated forward walk,
isolated forward sprint and neutral were captured. All **1,801 applied scopes
were restored**, with **61,299 routed queries** and no native failures. The
human reported normal walking and actual sprinting. Both canonical frames ended
neutral, Core/XML unloaded, the backend remained pinned as expected, and all
23 installed hashes matched. Normal GTA exit passed with the complete
99,637-byte shutdown log. This validates the bounded local forward movement
path through actual MTA bindings; vehicles, weapons, remote input and full MTA
startup/rendering remain unfinished.

ABI6 admission requires the exact local player/provider binding, native
InVehicle flag clear, `PED_IDLE` and the native controls-disable word zero.
The known mask and positive flags must both be `0xF`; the six allowed actions
are forwards, backwards, left, right, sprint and walk. Weapon/task context,
health-based death detection and other actions remain unknown or excluded.
The native callback repeats the same eligibility check before every scoped
write. This reuses the existing producer/engine mailbox and 96-byte pad
save/apply/restore path, with its 35-second native limit and two-second input
freshness guard.

An unfinished End keeps the actual Core and worker alive while restoration and
detachment are retried. Two neutral action frames clear canonical current and
previous state before bind/XML cleanup and terminal publication. The 96-field
snapshot includes native metrics and actual module-PIN status at field 94.
Core unloads after cleanup; Game DE is expected to remain loaded when that PIN
succeeded. A loaded pinned DLL alone does not mean hooks or adapter ownership
remain active. Use
`python utils/definitive_edition/core_movement_log.py --log <saved-log> --output <assessment> --require-exit`
after normal shutdown. Its complete result requires forward, isolated forward
walk, isolated forward sprint and neutral coverage, matching applied/restored
scopes, cleanup and the reported backend PIN. Visual movement still requires
the human report. Focused offline suites pass 1,259 native callback checks and
1,520 adapter-module checks. Normal Core/Game DE builds and 114 actual Core
entry checks in each classic/DE layout pass. Package and live results remain
separate evidence. Full MTA startup, CGame, DE rendering, resources and networking
remain gated.

Core's existing module-loading order now lives in `CreateStartupModules`, used
by both classic renderer startup paths. It preserves the early reentrancy flag
and Network/Game/Multiplayer/XML/GUI order. DE admission precedes the flag and
all classic module loads. DE `CreateGame` requests an owner-thread checked,
borrowed adapter facade, with teardown ownership distinct from the classic
self-deleting factory. The production backend still exposes no CGame; the
full startup rejection remains in place.

The opt-in Release/x64 MTA launcher has a headless file preflight through its
existing loader dispatch. It checks the supported executable and an owned
23-file package without starting GTA or loading Core. A valid file result
still reports launch/full-MTA readiness false. See the
[preflight contract](../launch/DefinitivePreflight/README.md).

**Completed live check: actual MTA Core startup.**
The validated `-MtaCoreStartupReference` package contained 21 owned files,
including the normal opt-in x64 `core.dll` and its `pthread.dll` dependency.
After manual Ctrl+Shift+F9 activation and release, one bridge worker loads that
actual Core, sets its MTA/game directories, calls `InitializeCore`, reads its
startup diagnostics and releases it. The script yields between completion polls
so a slow loader cannot consume CLEO's two-second watchdog. Ctrl+Shift+End
requests cancellation and keeps polling until worker cleanup finishes.
No walking, NPC, network session or input write is
part of this check. The expected result is the unchanged incomplete-gameplay
gate after exact-image binding and a backend pulse, followed by clean unloading.
It does not start the full MTA client or a DE renderer. Arming waits indefinitely.

The x64 Core now delay-loads its legacy D3DX9 dependency so this early startup
path does not load a D3D9 renderer. Real Core entry checks pass 93 cases in each
of the classic and `CLEO/de-online` layouts; the
classic Win32 option/default builds retain their existing exports and behavior.
The first in-game attempt failed during DLL initialization: a global timing
logger resolved the classic install root before localization existed. Its null
read at Core RVA `0x41FAB` matches an independently reproduced loader fault.
The old classic-layout fixture hid that assumption. Timing-path discovery now
waits until diagnostics are enabled, and the x64 owner supplies Core's existing
install-root override before startup logging. The corrected in-game check
passed in 70 ms: actual Core reached exact-image binding, backend pulse and
the unchanged gameplay gate, with clean adapter/Core unloading and no loader
error. All 21 installed hashes match, and a separate process-module snapshot
confirms Core and Game DE are absent after cleanup. The human reported normal
gameplay continuation. Normal shutdown and all 21 hashes are verified; the
5,730-byte final log retains the complete startup record and normal exit.
Probe API2 publishes 19 fields, including the original Win32 load error,
only after worker cleanup. Use `utils/definitive_edition/core_startup_log.py`
to assess its saved log. Source defaults and ordinary mode switches disable it;
it is mutually exclusive with the other Smoke trials. Intact 16/17/19-file
installs upgrade with ownership/collision checks, and the Core dependency pair
is always installed or removed together.
The 21-file ownership suite passes 335 checks across 30 cases, including all
four historical install generations and loader disable/enable round trips.
`Tests/definitive/test_core_startup_worker.ps1` compiles the production worker
and passes 1,901 controlled-loader checks plus 46 checks with the real Core in
an owned non-game host. A deliberate 2.2-second DLL initializer proves that
polling/cancellation do not wait on the loader lock. The actual Core run keeps
its expected wrong-image rejection; it does not validate GTA startup.

Core now also exposes configured-control dispatch and context-fed frame
processing in its actual `CKeyBinds` class. Classic input calls the same frame
builder and post-publication wheel cleanup. The actual-class fixture relinks
normal Core objects with test-only exports and passes 185 binding/adapter
checks plus 33 host checks through the real Game DE module. The old Core object
fails on exactly the three new methods. Normal Core builds and both Win32
configurations pass.

**Completed read-only live check: Core-owned configured keyboard input.**
`package.ps1 -Mode Smoke -MtaCoreInputReference` stages 23 owned files,
including the actual x64 XML module and an explicit `mta/config/coreconfig.xml`.
After F9 activation, the real Core remains on its startup worker for 60 seconds.
Its actual `CKeyBinds` loads the existing XML schema and polls configured keys
through MTA's key table, then publishes movement actions through the real DE
module. No commands, engine writes or networking are started. Focus loss
releases controls; refocus waits for all eligible keys to be released.
The session ends with both canonical controller frames neutral, destroys its
bind/XML owners and retains Core's existing gameplay rejection and teardown.
The script reads one frozen 64-field sample at a time; it never calls into Core
while the worker unloads it.

This candidate assumes an alive on-foot player without aiming, a detonator or
an enter-vehicle task. It does not detect those engine contexts. The keyboard
poller reports unsupported mouse, wheel, joystick, numpad and ambiguous modifier
routes rather than guessing. The explicit diagnostic XML binds W/S/A/D,
left Shift for sprint and right Shift for walk. Classic missing/empty
configuration still imports GTA defaults;
the DE reference requires explicit saved bindings. Use
`utils/definitive_edition/core_input_log.py` to assess the entire saved log,
adding `--require-exit` after normal shutdown. A clean lifecycle alone cannot
pass the movement coverage check: all four directions, forward sprint, forward
walk and neutral input must be recorded.

The actual Core session completed in **60,003 ms**, with **1,188 native
samples and 515 script snapshots**. Four directions, sprint and neutral are
recorded; Walk was never observed. The human was unsure which Shift key was
used for that step, so Walk remains untested. The strict reader reports
`movement_coverage_missing` with a clean session lifecycle, not six-action
success. Canonical current/previous state ended neutral; bind/XML/adapter
cleanup, Core/backend unloading, normal GTA exit and all 23 installed hashes
passed. This read-only run retained its explicitly assumed on-foot context.
Its final 150,553-byte log is preserved under
`Build/definitive-manual-test/20261013-203218-mta-core-input-ready-7054a113/actual-20261013-210222-999b0c6d/post-exit-20261013-211208-4050840d/`.
It establishes the earlier configured-input boundary through actual Core. The
corrected ABI6 forward movement result is qualified above; full game context,
gameplay and rendering remain unverified live.

The extended actual XML/bind-manager/adapter fixture passes 318 checks plus
34 host checks with unchanged saved configuration. Normal Core builds and both
classic Win32 configurations pass; real Core entry passes 106 checks in each
layout. The asynchronous worker passes 2,162 controlled callback/loader checks
and 46 checks against the real Core's wrong-image rejection. These offline
checks validate the producer and ownership boundaries. The completed live
session above supplies the exact-image evidence with partial action coverage.

**Historical ABI5 fixed-phase MTA-owned engine movement trial.** Build the normal
Game DE Release/x64 module and this bridge, then package with
`package.ps1 -Mode Smoke -MtaNativeMovementTrial`. The historical package included
the real `game_de.dll`, loaded through Core's actual module consumer only after
activation. Intact 16/17-file installs can be upgraded; occupied new paths
reject the update before any writes. An ordinary mode switch clears the trial.
The source configuration defaults to disabled.

Launch the game through Steam on a clear flat street, hold Ctrl+Shift+F9 for
three seconds, then release all keys and leave the controls alone for 22 seconds.
The local player receives MTA walk, forwards and sprint inputs separated by
neutral pauses. Ctrl+Shift+End cancels. Arming has no timeout before F9.
JavaScript supplies logical actions and observations; the normal MTA backend
owns native callback routing, scoped pad writes and restoration. This trial
does not create an NPC or network session. Logs establish callback/query and
cleanup evidence; the human checks how movement looks.

The historical thread-corrected trial completed successfully in the game. The human
reported movement phases and sprinting working; its 165 samples record 1,317
applied/restored callback scopes, 44,655 routed queries and no recorded failures.
Normal exit and all 19 installed hashes pass. This proves the scoped local
movement slice, with full MTA startup/networking/rendering still required.

**Current development integrates the adapter into MTA.** The normal build now
includes `Client/game_de` and a Core-owned module consumer. MTA controls module
lifecycle through a versioned SDK interface; controller storage moved into that
module, with this harness retaining a compatibility include. The opt-in Core
branch rejects missing gameplay capabilities before its classic startup code.
The module supports caller observations and now reuses Core's existing digital
action synthesis through ABI2. Logical walk/sprint state stays separate from
native observations. ABI3 also provides an explicit build-pinned native pad
reader in `game_de`; the first live trial rejected the proposed controller layout.
ABI4 added raw provider-source readback through the same Core-owned interface.
Both reads expose only diagnostic candidate data. ABI5 introduced the scoped
local writer; ABI6 adds build-pinned eligibility for actual Core configured
input. Full engine input and remote player capabilities remain unavailable. Classic Core
calls the same helper while retaining its bind callbacks and joystick/native
write policy. Real opt-in Release/x64 Core now builds and links; full
game/multiplayer, launcher and rendering services remain required.
Current build and regression results are recorded in the
[module documentation](../game_de/README.md) and [integration plan](MTA_REUSE_PLAN.md).

**Historical completed live test of MTA's native reader.**
`-MtaNativePadReference` selects a separate read-only Smoke trial that compiles
the actual `Client/game_de/NativePadReader.cpp` into this diagnostic bridge.
It records both candidates' proposed 24-field current/previous states, position and raw
velocity for 30 seconds. It is separate from the earlier zero-valued
`GET_PAD_STATE` experiment and MTA's synthesized logical actions. The existing
loader schedules observations; it does not host a complete MTA gameplay client.
Ctrl+Shift+F9 starts the trial and Ctrl+Shift+End cancels it. No NPC, transport or
game-input writes are used. Packages now contain 17 owned files; intact older
16-file installs remain valid, with a narrowly checked additive script upgrade.
Use `utils/definitive_edition/native_pad_log.py` for this trial's evidence.
The human reported normal walking and sprinting, and all 225 captures and cleanup
completed. Both candidates' axes, sprint and previous-state fields stayed zero;
the adapter therefore keeps `EngineInputRead` unavailable. The saved result is
linked from `Build/definitive-manual-test/current-native-pad-reference-test.json`.

The next candidate also captures all 127 signed source states used by the active
provider's native axis implementation. Capture version 2 keeps these separate
from the rejected pad layout. Use the log reader's `--require-device-states`
option for this candidate; older pad-only recordings remain readable but cannot
validate the new capture. Physical key/action mapping is still unverified.

The first source capture now confirms the native W/S/A sources. D and sprint
were beyond its 30-second window, so the recorder now starts after all activation
keys are released and allows 60 seconds. It waits indefinitely before F9;
the human can run the test whenever available. Complete controller mapping and
engine writes remain unverified.

The release-started minute trial completed 449 captures, clean disposal and
normal exit. D and W+LeftShift are observed, along with 76 triple-held captures
in the uncertain W/D/LeftShift segment. Key capture is sufficient to continue;
that segment's movement outcome and complete controller mapping remain open.
Core's original control-bind state policy is now shared in
`Shared/sdk/CControlBindState.h`, with its existing callback loop retained.
The extraction passes 12,650 oracle checks on each architecture and classic
Core compile-only checks with the adapter option both enabled and disabled.

The first shared slice extracts MTA's existing incremental ped position
correction into `Shared/sdk/CPedPositionInterpolation.h`. The classic client
calls it, and the same code passes 117 independent checks on both x86 and x64.
Run `Tests/definitive/test_mta_ped_interpolation.ps1`; see the
[reuse plan](MTA_REUSE_PLAN.md). This is source-level reuse verification; a
complete MTA client build and DE player/input backend are still required.

**Completed compatibility test: read-only local observations.**
The unchanged 48-byte `CControllerState` is shared by the classic client and
x64 bridge. Controller-layout checks (219) and snapshot transaction checks
(930) pass on both x86 and x64. The existing Win32 Release deathmatch gameplay
DLL compiles and links; this does not build the full client stack.

`INPUT_REFERENCE` defaults false. `-NativeInputReference` selects Smoke only
and always requires manual Ctrl+Shift+F9 activation. For 30 seconds the human stands,
walks and sprints normally; the harness reads 20 pad fields, raw engine velocity
and position. It publishes and reads back MTA's canonical state, with explicitly
unknown logical extras and velocity units. No network, NPCs or game-input writes
are used. The previous snapshot is the previous probe sample, not an engine
frame. `native_input_log.py` audits the saved evidence. That candidate was installed
in Smoke with all 16 owned hashes verified; the loader and definitions are
unchanged. The final source-bound run passes 47 input, 261 adapter, 36 gait,
250 utility, 33 native integration and 293 protocol/math checks. See
`Build/definitive-manual-test/current-input-reference-test.json`.
End cancels or stops the probe.

The first attempt stopped after 22 idle samples; the human identified Home opening the
Rockstar console. Cleanup passed, and the human reported normal movement; that
interrupted recording did not verify movement input or velocity. F9 avoided the conflict;
the full F9 retry ran for 30 seconds and reset cleanly. It captured 227 valid
samples and changing raw velocity, but every pad field remained zero while the
human used keyboard/mouse. Normal exit is verified. Pad mapping remains
unverified; further backend work follows MTA's own module and input paths.

**The horizontal animation-lock experiment completed its human test.** The
user reported normal walking with no repeated loop/rewind, while some choppiness
remained. The observed stages were very slow, slow and normal walking; no running
animation was tested. All seven tasks and requested root policies, owned cleanup,
End and normal exit were logged. The 264-sample cadence audit remains incomplete
because rate jitter left no stable two-wrap fits. This is a scoped visual
improvement, not a complete movement or MTA compatibility result.

Historical instructions for this completed comparison:
Launch GTA through Steam, stand on the same flat outdoor street, hold
**Ctrl+Shift+Home for three seconds**, then report ready and keep GTA focused.
The agent will start the 30-second NPC sweep at 0.5, 1.0 and 1.5 units/second.
Stay still with a side view, watch for repeated backward jumps and foot sliding, and
wait for the NPC to disappear. **Ctrl+Shift+End** stops the Online adapter;
the agent will inspect cleanup before normal game exit.

The previous rate-enabled trial produced visible backward jumps/chop. Its
100 ms samples showed continuous body coordinates, so this experiment tests
whether the walking animation's rendered horizontal root motion caused those
jumps. Only the `Walk_civi` task's X/Y lock flags change; idle tasks, the rate
estimator, bridge and loader keep their tested behavior. The flags are a
hypothesis to check visually, not proof of rendered root position.

The measured native speed sets an experimental walking multiplier, using
received XY path over 250–500 ms and a 0.25–2 rate clamp. The controller observes
playback before setting the rate, limits setters to one per 250 ms and preserves
smoothing/task transitions/cleanup. The reader must verify actual phase cadence
across all three speeds; requested setter values alone cannot pass. Source
defaults keep `GAIT_RATE=false` and `GAIT_ROOT_LOCK=false`; package/mode selection
explicitly opts in with `-MatchedGaitRate -LockWalkRoot` in Online mode.
At preparation, all 233 adapter, 36 native-reference and 215
utility checks passed, including 30 cadence-reader fixtures; 13 source hashes
and all 16 installed hashes verified. Historical plan:
`Build/definitive-manual-test/current-root-lock-test.json`.
The failed rate trial remains separately discoverable through
`current-gait-rate-test.json`. Its 46 rate changes prevented a stable two-wrap
cadence fit. The root-lock comparison improved the reported repeated rewind,
while rate jitter, accepted cadence, physical stride and two real clients remain
unresolved. Existing MTA synchronization and input-driven locomotion now take
priority over another custom estimator.

## Completed native walking reference

**The offline native walking reference completed its human-run GTA test.**
The measured natural `Walk_civi` speed is **1.5216 units/second** at a **1.13335-second
cycle**. The user reported natural walking with no obvious sliding. All 42
samples, owned cleanup and normal game exit were audited; the steady fit uses
31 samples and four observed wraps. This reference supplies the baseline for
the pending speed-matched remote animation trial.

For this reference, launch through Steam, load the same flat outdoor street and
hold **Ctrl+Shift+Home for three seconds** while GTA is focused. One nearby NPC
settles briefly, walks across a short eight-unit lane under game control, then
disappears on arrival or within ten seconds of starting its walk. Watch its feet
from the side and report natural contact, sliding or failure to appear. Keep
GTA open afterward for log inspection; **Ctrl+Shift+End** can cancel or stop the
reference. The user handles visual observation, and the agent handles the
measurements and audit.

The explicitly selected Smoke reference leaves natural movement and gait to
the engine after one initial pose setup. It takes at most 100 actual pose/phase
samples at 100 ms, owns one NPC and no transport, and performs bounded cleanup.
Source defaults keep the reference disabled; ordinary Smoke remains read-only.
Select it with `set-mode.ps1 -Mode Smoke -NativeGaitReference` while GTA is closed;
an ordinary mode selection clears the flag. The strict `native_gait_log.py`
reader excludes warmup/stopping samples and requires matching ownership, task,
contiguous phase evidence and completed cleanup before reporting an engine
reference. That result does not independently establish visible foot contact.

Checks passed **186 adapter mocks**, **36 reference-controller tests** and
**185 utility tests** including **32 reader fixtures**, with all nine current
source hashes bound. The installed profile's **16 owned hashes** verify.
Current plan is discoverable through
`Build/definitive-manual-test/current-native-reference-test.json`; source/test
certificate is in `Build/definitive-native-reference-check/current-candidate.json`.
The findings tracker retains preparation, rollback and historical evidence.
The completed reference is saved separately from the pending sliding repair
and two-real-client milestone.

## Completed October 12 gait timing

**The installed read-only gait probe completed its actual seven-stage GTA trial.**
The **12:26:01–12:26:32** helper submitted **570 updates** and **405 positions**
in **30.003 seconds**, with all seven stages, exit 0 and no run/cleanup error or
game departure. The unchanged `PED/Walk_civi` clip measured approximately
**1.1333 seconds/cycle** at commanded translation speeds **0.5/1.0/1.5 units/s**.
This establishes fixed playback cadence, while foot stride and sliding remain
unverified. The first owned remote
ped in a connection is sampled at most every **100 ms**, for at most **300
samples/30 seconds**. Existing one-second transform/playback queries are shared.
Samples record actual timestamps, task age, clip, playing flag, phase, received
target, requested displayed pose and actual readback; a task does not assume
phase zero. Cleanup, budgets and clock discontinuities terminate the probe.

Only the two installed adapter scripts changed. Existing tasks, playback rates,
native commands, DLL, loader, protocol, C++ and smoothing remain unchanged.
Current source-bound checks pass **180 adapter mocks**, **153 utility tests**
(including **69 peer mocks**); the stride audit passes **66 fixtures**.
All **16 installed hashes** verify. Certificates:
`Build/definitive-stride-check/20261012-121336-verified-8eee296d920b490bbbcbb879f6a57451/`.
Prior installed bytes:
`Build/definitive-update-backups/20261012-121339-movement-stride/`.

The opt-in `stride-speed-sweep` uses seven stages: idle → slow +X → idle/turn →
medium −X → idle/turn → fast +X → idle. Defaults are **30 seconds/radius 5**,
translation speeds **0.5/1.0/1.5 units per second** and nominal stage durations
**2/12/2/6/2/4/2 seconds**. The fixed six-unit lane copies the outdoor anchor's Z;
it does not query terrain. Older profiles remain available. Current plan:
`Build/definitive-manual-test/20261012-121416-stride-host-ready/`, through
`current-stride-test.json`. Steam launch was **12:14:18**; game **14068** was born
at **12:14:29**; Home registered at **12:25:04**. Game player **5** displayed
synthetic player **6**, ped **6153**. The probe recorded **263 valid samples** over
**29,901 ms**, all seven tasks playing with phase progression, then finished
before owned deletion. End at **12:27:44**, normal shutdown/Exit at **12:30:30**,
game closure, all 16 installed hashes and all five tested source hashes verify.
Both full/scoped strict readers pass with one completed session and zero active
sessions/issues/failures. The completed pointer links `assessment-final-reviewed.json`.
Post-exit diagnostics show unchanged boot, GPU/PnP OK/0 and no new grouped dumps
or collector issues/blockers; no crash was observed in this trial. Slow/fast still
frames show walking poses, while the medium character is occluded; continuous
gait, foot contact and no sliding remain unverified. No animation-speed setter
was applied. Next work needs a calibrated foot-contact reference before bounded
rate control; two real clients remain pending. The tracker preserves command
research, audit correction, controls and prior results.

## Completed October 12 movement smoothing

**Bounded movement smoothing now has actual GTA runtime evidence.** The
**11:52:02–11:52:32** five-stage straight-lane trial completed playback, owned
deletion, End at **11:53:18** and normal shutdown/Exit at **11:54:40**. Both full
and scoped strict readers pass with one completed session and zero active
sessions/issues/failures. All 16 installed files, five tested source hashes and
the known game executable verify after closure. The helper submitted 575 updates
and 232 positions, with exit 0 and no run/cleanup error.

Active Online yields at **16 ms** for presentation
while a separate **50 ms** deadline gates network/bridge work. Each remote keeps
received target and displayed pose separate, blends XY/shortest-arc heading over
50 ms, snaps received Z and resets on discontinuities. It does not extrapolate.
Stop/readiness/ownership checks run before the network gate; animation inference
uses received targets and retains the existing clips/task policy.

Current verification passes **149 adapter mocks**, **135 utility tests**
(including **51 peer mocks**) and **47 smoothing-audit fixtures**. Only the two
owned adapter scripts changed; native DLL, loader, protocol and C++ are unchanged.
All **16 installed hashes** verify. Source/install certificates:
`Build/definitive-smoothing-check/20261012-080403-verified-fa4b14e38fbb404ebb8d0bf6772c7535/`.
Completed plan: `Build/definitive-manual-test/20261012-114540-smoothing-host-ready/`;
pointer: `current-smoothing-test.json`. The agent launched through Steam under
the existing authorization at **11:45:51**; game **5816** was born at
**11:46:02**, and Home registered at **11:51:46**. Server **6740**, session
**45ad570fff9c44d2ad377e50a04aa6c0**, expires **12:44:16 local time**. The current
pointer is completed and links `assessment-final-reviewed.json`. Server identity
and listening endpoint were reverified at **11:57:52** after game exit. Game
player 2 displayed synthetic player 3/ped 6406 through all five stages. The earlier
08:10 attempt exited normally
at **08:19:10** while armed/unactivated, without bridge/network/peer activity;
its interruption record remains in `20261012-081051-smoothing-host-ready/`.

Motion evidence distinguishes network target, requested displayed pose and actual
readback, plus bounded tick/write/poll counters. The 30 logged motion windows
record 1,745 ticks, 574 writes, 582 polls and 190 changed targets, with 14 faster
write windows and 7 intermediate poses; an unsummarized trailing fraction is
excluded. Full walking windows have 42–51 writes against 13–17 target updates
per approximately one second. This verifies real-time blending; GPU frame cadence
and visible smoothness were not measured. Separate still frames show outbound
right-facing and return left-facing walking poses with apparently street-level
shoes, then absence. They do not establish continuous gait or no sliding.
Post-exit diagnostics find unchanged boot, GPU/PnP OK/0 and no new grouped dumps
or collector issues/blockers; no PC crash was observed in this run. Stride
matching/sliding, earlier PC-freeze
causes and two real game clients remain unverified. Next work is independent
visual movement/stride assessment, then two-real-client testing. The root findings
tracker and
`Build/definitive-recovery/current-checkpoint.json` retain history and recovery.

## Completed October 12 calibration

**October 12 straight-walk calibration is complete.** One real GTA client displayed
a synthetic peer through idle → walk out → idle/turn → walk back → idle, then
deleted it. The helper ran **06:48:55–06:49:25**, with **575 updates**, **232 distinct
positions**, exit **0** and no run/cleanup error. All **30 animation readbacks**
report playing=true with phase progression and exact requested/observed XYZ.
End at **06:51:18**, normal shutdown/Exit at **06:52:49**, game closure and all
**16 installed hashes** are verified. Both unfiltered full-session and scoped
strict readers pass with one completed session and zero active sessions,
issues or failures. The server records requested LEAVE for both game player 3
and helper 4. A targeted return-leg screenshot shows
left-facing movement at heading 90 and apparently grounded feet. It proves one
pose/direction frame; smoothness, stride alignment, both directions and every
idle stage are not visually established. October 11 lag/sliding remains open.

The new `straight-walk-calibration` helper profile uses five nominal six-second
stages at the default 30 seconds/radius 5, a fixed first anchor and parallel lane,
six-unit legs at **1 unit/second**, and candidate headings 270 out/90 back. The
installed game adapter, bridge, loader and protocol are unchanged. Current
verification passes **118 adapter mocks** and **135 utility tests**, including
**51 peer mocks**. Certificate:
`Build/definitive-calibration-check/20261012-062559-verified-e017e7fc881c4751916e63437244d86f/test-result.json`.
Default orbit and `idle-walk-idle` behavior remain available. The profile validates
its parameters/speed before loading the DLL, and only reports completion when
all five stages submit updates; late anchoring still requires runtime audit.

Completed plan: `Build/definitive-manual-test/20261012-062924-calibration-host-ready/`;
pointer: `current-calibration-test.json`; reviewed result:
`assessment-final-reviewed.json` in that plan. Targeted game-control images/actions
are under `Build/definitive-game-control/`. The user authorized automated launch
and game input for this test; the agent launched via Steam, resumed the observed
save, moved outside, activated Home and performed controlled shutdown. No save
overwrite or device/drive/security changes occurred. Home was requested at
**06:48:22** and `adapter_start` logged at **06:48:23**. Post-exit collection has
unchanged boot, GPU/PnP OK/0, no new grouped dump identities and no issues/blockers.
Server PID **8884**, session
**ac589aeabda047a1ac1a314450ee06db**, started **06:28:45** with scheduled expiry
**07:28:45 local time** on October 12. Its predecessor expired normally with zero
players. The root findings tracker and `Build/definitive-recovery/current-checkpoint.json`
retain the audit and recovery records.

Next work is bounded render interpolation with a separate network cadence and
further visual facing/stride checks. The existing 50 ms polling loop still writes
target positions directly; interpolation has not been installed. Two real game
clients and playable multiplayer remain unverified. Earlier PC-freeze causes
remain unresolved despite this completed test.

## Preserved October 11 results

The **walking/idle animation candidate is installed**, with **118 adapter mocks**
and **34 peer-helper mocks** passing and all **16 installed hashes** verified.
An actual synthetic trial completed idle → walk → idle at **20:38:45–20:39:15**,
with observed advancing animation phases, zero Z error and owned deletion.
The user confirmed visible moving feet and disappearance, with lag/sliding.
End at 20:40:57 and normal shutdown/Exit at 20:41:21 are verified. All 16 hashes
still match after exit. It keeps the proven no-offset setter and owned
cleanup, adds state-transition-only built-in PED tasks, and observes playback
and position at most once per second, including stationary idle. No PED request
or removal is used. That loopback server was PID **20868**, expiring at
**October 11 21:27:52 local time**. Its live identity/endpoint and welcome/requested-leave
probe passed without any position UPDATE.
Current manual plan/baseline:
`Build/definitive-manual-test/20261011-202954-animation-host-ready/`, discoverable
through `current-animation-test.json`. The completed retry used game player 8,
synthetic player 9/ped 5898, **574 updates**, **385 distinct positions** and exit 0
without run/cleanup errors. An earlier 1.897-second idle-only attempt is preserved:
game player 5 timed out during a seven-second game-log gap; its cause is unknown.
The retry has 30 valid playback observations (5 idle, 21 walk, 4 idle). Current
foot ground height and both idle stages were not explicitly visually confirmed.
Full-session strict acceptance remains incomplete because the first idle-only
ped did not move; the explicitly selected retry passes. Final assessment:
`assessment-final-reviewed.json` in the current animation plan. Preserve both scopes.
Waiting for an anchor can truncate final idle; helper exit 0 alone does not
establish complete playback.

The next work identified then was helper-facing/speed calibration, then bounded render interpolation
with a separate network cadence. The existing 50 ms polling loop writes target
positions directly; the fixed walking clip still has uncalibrated stride. These
follow-ups have not been installed. The findings tracker contains the full audit,
remaining limits and recovery checkpoint. No second real game client was tested.

Experimental separate Windows x64 client and one custom UDP server. On October
11 at 15:15, one real Steam game client displayed a synthetic remote character:
the user confirmed appearance, movement and disappearance. The log records one
creation, 30 changing transforms and deletion. The character floated and had no
animation; the helper deliberately circles the player and the current proxy is
frozen, collision-disabled and moved by coordinate writes. Ground placement and
walking animation were unfinished in that run, as are two real game clients and playable
multiplayer. The game exited normally at 15:24:10, but no End stop was logged;
the strict reader correctly rejects the complete stop/exit sequence for this run.
The user reports pressing End; shortcut registration in this run is unverified.

The completed position candidate has actual game readback evidence. At
**20:00:47-20:01:17**, synthetic player 5/ped 8706 had **29 distinct paired
transforms**, exact requested/observed X/Y/Z agreement and **zero Z error**.
The helper exited 0 with 575 distinct synthetic updates and no errors. The game
logged creation and deletion, End stop at **20:07:25**, and normal shutdown/exit
at **20:08:08**. The strict reader passes without issues or failures. The user
confirmed feet on the ground, sliding around the player, with no floating or
sinking. Grounding is confirmed at this test location; matching body-center
coordinates do not establish general terrain height. That run had no animation.
The position candidate matched 87 passing mocks, with all 16 installed hashes checked
before the run and no source/native change. Current plan/baseline:
`Build/definitive-manual-test/20261011-195314-position-host-ready/`;
final audit: `audit-200818423094-final/`; assessment: `assessment-final-reviewed.json`
in that directory. The current
pointer is `current-position-test.json` in its parent directory.
An earlier manual session activated at 16:51:39 and registered End at 16:53:21,
then exited normally at 16:53:28. No peer ran or join occurred; position was then
untested. The earlier command launcher failed while enumerating locked F:, and
the previous server's scheduled expiry had passed. Evidence is at
`Build/definitive-diagnostics/20261011-165657-position-stop-no-peer/`.
At the 18:23 resume, sandbox CIM/device/event access was denied and a server
command-session fallback failed its UDP welcome probe, then was interrupted.
Another user-reported crash left the log at 18:57:26 boot-script scanning, without
entry/bridge/join markers. Windows confirms an unclean restart at 19:09, but no newly
grouped dump identity; latest 141 records repeat older dumps. Cause remains unknown.
The user reported an undervolt afterward; its gameplay effect is unverified.
With full access, the agent collected fresh readiness, audited source/startup
files, and verified the restarted server PID 5248 by UDP welcome/requested leave
at 19:36. It expired at 20:13:01 local time and its log confirms a stop with zero
players; the process/endpoint were absent at 20:14. The current bounded peer completed.
The agent subsequently started and verified the fresh server described above.

The manual base-game control completed: the user confirmed gameplay and normal
exit with the loader bypassed. No new boot/dump identity or CLEO-log change was
found. The exact `version.dll` was restored at 19:50:56 with all 16 hashes verified;
fresh RequireReady passed. Online/manual and Home/End are available again.
This one control does not identify the earlier crash cause or establish stability.
Control evidence: `Build/definitive-manual-test/20261011-194145-base-game-control/post-control/`.
Current position plan/baseline:
`Build/definitive-manual-test/20261011-195314-position-host-ready/test-plan.json`;
current pointer: `current-position-test.json`. The agent handles commands directly;
manual outdoor/Home readiness and fresh join/self-echo preceded the 30-second peer.
Recovery and diagnostic limits remain in the root findings tracker.

Automated game testing was stopped after two PC freezes/forced restarts, until
the user's October 12 authorization described above. On October
4 at 06:21 +02:00, Windows reported the Radeon RX Vega healthy (status OK, device
code 0). A manual Diagnostic session completed at 07:08:50 and exited normally at
07:12:21. The October 4 Smoke session also passed: bridge guards, 75 local-player
samples and controlled stop, followed by normal exit at 07:26:55. The installed
mode remains Online after the successful one-client submission/echo test. The cause of the
earlier freezes and general startup stability remain unresolved.
The October 11 13:36 manual Online run joined the local server and passed actual
coordinate submission/echo checks: 23 telemetry records, 22 self-echoes and at
least 331 successful submissions, including changing negative Y values. The
signed-bit conversion fix now has joined game-runtime evidence. Ctrl+Shift+End
stopped the adapter, the server recorded requested departure, and the game exited
normally. This earlier session did not test remote peds; the later 15:15 proxy
experiment is described above. Two real game clients remain unverified.
Another manual launch froze on October 11 around 13:29. Windows recorded a fresh
LiveKernelEvent 141. The recovered CLEO log ends during boot-script scanning,
before adapter discovery or bridge loading. The loader/runtime had already loaded,
so their involvement cannot be ruled out. GPU status is healthy after reboot;
the crash trigger is unresolved. Game testing remained manual in those trials.
The remote experiments then started only after a stable outdoor save was reached.

The initial launch was followed by a reported PC crash and Windows graphics-engine
timeout. Its cause is unresolved. The user confirmed normal play worked after
reboot; the post-reboot log also confirms the smoke adapter was active. The mod
remains installed at the user's request. Evidence is in the root findings tracker.

The October 11 launches at 14:18:42 and 14:21:52 froze at the same recorded CLEO
boot-script scanning boundary as 13:29, before adapter entry or a server join.
Both have fresh 141 dump identities; GPU queries after reboot remain healthy.
Online now starts **armed and inactive**. Hold **Ctrl+Shift+Home** once after
loading a save to enable bridge loading and the client connection. Hold
**Ctrl+Shift+End** to cancel while armed or stop afterward. This gate has mock
coverage, installed-hash verification and actual Home/End game-runtime evidence.
It does not delay the ASI loader, CLEO or plugin boot stages.
The manual 14:38-14:43 run now verifies Home and End in GTA: 31 logged seconds
armed before activation, successful bridge guards/joins, changing self-echoed
coordinates, then Ctrl+Shift+End stop and script disposal. Idle timeouts caused
two successful rejoins; their cause is not confirmed. Normal host shutdown/Exit
is verified at 14:58:39, and all 16 owned hashes pass after game closure. Earlier
freeze causes remain unresolved. The later synthetic proxy was visually observed,
but grounded motion and animation were unverified at that point; the later
position and animation trials above retain their separate acceptance evidence.

## Build and verify

PowerShell 7, Visual Studio C++ tools, x64 Python 3.9+ and Node.js:

```powershell
./Client/definitive/build.ps1
python -B -m unittest discover -s Tests/definitive -p 'test_*.py'
node Tests/definitive/test_game_adapter.cjs
python -B -m unittest discover -s utils/definitive_edition -p 'test_*.py'
pwsh -NoProfile -File Tests/definitive/test_loader_toggle.ps1
```

The independent C++17 build uses `/W4 /WX` and runs 293 protocol/math checks.
Outputs are under ignored `Build/definitive-online`. Thirty-three Python tests exercise
server/native behavior; 180 Node mocks check controller/startup/activation/readback/animation/smoothing and bounded gait-probe behavior. Mocks
do not execute GTA commands. Native tests skip when the x64 bridge is not built.
The utility suite passes 153 tests: 17 preflight, 29 Diagnostic-log, 69 mocked
synthetic-peer and 38 remote-log checks. The loader-toggle suite passes 22 fixture
checks. Those two suites do not load the bridge or launch a real game.

## Startup investigation

Collect evidence without starting GTA or capturing its window:

```powershell
pwsh -NoProfile -File ./utils/definitive_edition/collect_diagnostics.ps1 -RequireReady
```

The collector saves JSON/Markdown, the installed manifest, CLEO log and grouped
Windows live-dump event references under `Build/definitive-diagnostics`. It checks
the exact game hash, owned-file hashes and display-device status. Exit 2 means a
prerequisite failed. Exit 0 only means those prerequisites passed; it does not
establish stability. Older queued WER reports are grouped by dump identity so a
replayed event is not counted as a new crash. Protected dumps still need analysis.

The earlier device-code-22 blocker is now cleared, and the manual Diagnostic
stage passed, followed by the current Smoke bridge/local-read stage. F11 also
toggles fullscreen in this game, so the adapter stop shortcut is now
**Ctrl+Shift+End**. It reads held keys without consuming them; F11 alone no longer
stops the adapter. The new shortcut passed its actual game test on October 11.
No game launch is part of collection.

For the next manual comparison, launch through Steam and load an outdoor save,
then play briefly **without pressing Home**. The log should record
`awaiting_activation` and one `adapter_armed` event with `bridge_loaded: false`,
`game_adapter_active: false` and `network_active: false`. Waiting polls held keys
and checks readiness only when the activation chord is held; no bridge, player
coordinate or network operation occurs during that wait.

After the game is stable, hold Ctrl+Shift+Home briefly. Expect
`adapter_activation_requested` before `bridge_load`, then build/runtime, join and
transform markers. Ctrl+Shift+End takes priority if both chords are held.
Cancellation emits `adapter_activation_cancelled`; an armed-only run cannot count
as an Online join or remote-gameplay pass. This is one activation per script
instance; stopping exits the script, and another comparison uses a fresh manual
launch. No hot reload. Record whether a freeze occurs before or after Home, and
preserve the log before another launch. A post-Home failure could involve bridge,
local reads or networking; the key does not isolate server execution alone.

The one-client Online submission/echo stage passed on October 11 at 13:36.
Acceptance required `joined`, bounded `online_transform` events with increasing
successful submission counts and changing server-echoed own-player coordinates,
then `adapter_stopped` and normal exit. A join alone would not prove the float-bit
submission correction. No synthetic peer was used for that session. Startup
isolation remains necessary; the next gameplay gate is remote-ped rendering.

For a startup comparison, a reversible loader bypass is available with GTA closed:

```powershell
pwsh -NoProfile -File ./Client/definitive/set-loader.ps1 -Action Status
pwsh -NoProfile -File ./Client/definitive/set-loader.ps1 -Action Disable -WhatIf
pwsh -NoProfile -File ./Client/definitive/set-loader.ps1 -Action Disable
# After the comparison, close GTA before restoring:
pwsh -NoProfile -File ./Client/definitive/set-loader.ps1 -Action Enable
```

Disable renames only the owned `version.dll` to `version.dll.de-online-disabled`;
Enable restores it. The helper validates all 16 owned hashes, refuses linked paths
and collisions, and saves a durable pre-rename journal. It preserves mod bytes,
the installed mode and manifest. Status reads only; no game is launched. Restore
the loader before packaging changes, updating, changing modes or uninstalling.
The real installation remains **Enabled / Online**; no bypass was applied.

Diagnostic remains available for repeating the earlier isolation stage. It logs when
CLEO dispatches the ordinary script after loading a save/new game; it cannot log
a failure earlier in the loader/runtime. Progress to Smoke and Online separately
after a stable diagnostic session. Resume/load a save, play briefly, and exit
normally; reaching only the title menu does not dispatch the adapter. The log
should contain `adapter_phase` with `phase: entry`, `mode: diagnostic`, followed
by `adapter_diagnostic_complete` with `bridge_loaded: false`,
`game_adapter_active: false` and `network_active: false`. Diagnostic exits by
itself, so no stop shortcut is necessary in this profile.

Check a completed session from its text log, or start a bounded watcher before
manually launching the game:

```powershell
python -B utils/definitive_edition/session_log.py
python -B utils/definitive_edition/session_log.py --watch-seconds 300
```

The reader accepts ordered Diagnostic entry/completion markers with strictly
false activity flags. It reports adapter failures, bridge-loading evidence and
shutdown messages separately. Exit 0 means the script markers passed; exit 2
means incomplete/failed/timed out. No marker proves gameplay stability. Watch
mode excludes old markers even if unrelated data is appended or the header is
rewritten. The one-shot reader describes the whole existing log and makes no
freshness claim. It reads at most 16 MiB and never launches/inspects the game.
Twenty-nine fixture tests cover these acceptance and stale-evidence cases.

```powershell
./Client/definitive/package.ps1 -Mode Diagnostic
./Client/definitive/set-mode.ps1 -Mode Diagnostic
./Client/definitive/update.ps1
```

These updates keep the mod installed and preserve generated data. `uninstall.ps1
-ValidateOnly` performs only path/link/hash checks and returns before removal is
proposed or executed. Smoke/Online now log each bridge load, build check and runtime
binding boundary, to narrow any later startup failure.

## Local game test

The manual Diagnostic and Smoke stages passed on October 4. Online submission
and later remote-player stages still need runtime evidence; the cause of the
earlier freezes remains unresolved.

The only accepted game SHA-256 is
`63e633bb51c35fe35ca98f503b62e7ddb8403abe4c9d501cfa9162892453846f`.
Fixed numeric version: `1.0.113.21181`; display string: `1.0.113.49697469`.

```powershell
./Client/definitive/package.ps1 -Mode Smoke
./Client/definitive/install.ps1 -Mode Smoke -WhatIf
./Client/definitive/install.ps1 -Mode Smoke
```

Packaging verifies pinned official dependency archives and command definitions.
Installation verifies all 16 files and refuses an existing runtime/loader/CLEO
folder. It records each owned destination for removal. Close GTA before installing.
The commercial executable, classic GTA and installed MTA are unchanged.

Launch through Steam and load an outdoor save. Smoke mode only reads and logs CJ's
position/heading in `Gameface/Binaries/Win64/cleo_redux.log`. Hold Ctrl+Shift+End to stop the
adapter cleanly. Smoke does not connect to the server or create remote peds.

For the online experiment, close GTA first:

```powershell
./Client/definitive/set-mode.ps1 -Mode Online
pwsh -NoProfile -File ./utils/definitive_edition/start-server.ps1
```

The helper starts one hidden server bound to `127.0.0.1:22040` for 30 minutes.
It checks the listening log and owning UDP endpoint, refuses an occupied port,
and saves logs/session metadata under `Build/definitive-online-test`. Run it
again after restarting Windows or after the server expires. It does not install
an automatic startup task or launch GTA. Optional `-DurationSeconds 3600` allows
a longer session; `-PythonPath` selects a Python executable.

For an Online test, manually launch through Steam, load an outdoor save,
wait while armed, hold Ctrl+Shift+Home to activate, then walk for about a minute,
hold Ctrl+Shift+End briefly and exit normally. Check
`joined` and changing `online_transform` submitted/echoed values, then the stop
record. Run this stage without the synthetic peer.

After successful one-client submission/echo verification, a separate later test
can add a synthetic peer while the manually launched game is stable outdoors and
the fresh log shows changing submitted/self-echoed coordinates. Keep GTA running
until this experiment finishes:

```powershell
python -B utils/definitive_edition/game_peer.py --seconds 30
```

The helper is a synthetic peer circling five units from the received player. It
is not a second GTA client. Inspect game-side `joined`, `remote_ped_created`,
`remote_transform_applied` and `remote_ped_deleted` records. Visual confirmation
and two real DE clients are separate acceptance gates. Helper output explicitly
sets `synthetic: true` and `game_rendering_verified: false`.
After peer departure, observe whether the ped disappears, then hold
Ctrl+Shift+End briefly and exit normally. Preserve the game and helper logs.
The remote reader excludes pre-run records when given a saved baseline:

```powershell
python -B utils/definitive_edition/remote_session_log.py --baseline <saved-pre-run-cleo-log>
```

Exit 0 requires one joined Online session with matching remote creation, at least
two changed finite transforms, deletion before controlled stop, and normal host
shutdown/exit. Exit 2 means incomplete, invalid or failed evidence; before any
remote experiment, incomplete is expected. A reader pass verifies recorded command
order, not rendering or two-client multiplayer.

The interrupted October 11 preparation remains under
`Build/definitive-manual-test/20261011-141248-remote/` with its startup-interruption
record. A fresh baseline was prepared at `20261011-150105-remote/`; that plan's
30-second peer completed at 15:15:41. Actual server identity, peer outputs, fresh
game log and the user's visual observation are preserved under
`Build/definitive-diagnostics/20261011-152031-remote-audit/`.
`Build/definitive-manual-test/current-remote-test.json` points to the latest result.
Keep historical prepared server metadata separate from the actual restarted
server identity. The final 15:24 snapshot verifies normal exit and all 16 owned
hashes, but no End stop; keep that missing controlled-stop evidence explicit.
The newer body-center trial verifies matching coordinate readback and visible
grounding at one location, with a complete End/exit sequence. Bounded locomotion
animation now has observed idle/walk/idle playback and visible walking leg motion
in the completed 20:38 retry, followed by controlled stop and normal exit.
The October 12 straight-lane profile adds five-stage playback/readback and one
return-leg facing frame. The later smoothing trial verifies bounded interpolation
and includes sampled outbound/return poses. The new read-only gait probe is
prepared separately. Lag/sliding, complete visual facing/stride calibration,
visible smoothness and two real clients remain unfinished.

Ctrl+Shift+End stops the adapter. Close GTA before editing/removing scripts. Forced hot
reload is unsupported because CLEO documents no JavaScript unload-cleanup callback.
The bridge is pinned while CLEO retains its reset callback. Reset disconnects
transport; actual entity/reset cleanup still needs game tests.

Use `set-mode.ps1 -Mode Smoke` after closing GTA to return to read-only operation.
For removal, run `uninstall.ps1 -WhatIf`, then `uninstall.ps1`. It verifies all
installed hashes before deleting owned files and retains generated/unrelated
files. Modified files cause removal to stop for review.

## Scope and limits

- One fixed game endpoint `127.0.0.1:22040`, 16-session limit, 20 Hz snapshots,
  finite/bounded transform validation and five-second idle removal.
- Native UDP transport with explicit serialization, session checks, retries,
  departure handling and stale/wrapped sequence checks.
- Reused MTA math headers. Classic hooks, `netc`, Lua/resources, CEF and the
  existing MTA server protocol are outside this independent client.
- CLEO game-native controller with model streaming, owned proxies and controlled
  cleanup/reconnect logic. Local reads, remote creation/movement/deletion and paired
  remote coordinate readback now have runtime log evidence. Visual grounding and
  two real clients remain separate acceptance gates.
- Remotes are inert model-7 peds with collision disabled. Walking/idle animation
  has observed actual playback and visible walking leg motion. Stride alignment
  and smoothness remain unverified. Bounded interpolation is installed with mock
  coverage and actual intermediate-pose/cadence evidence. Visible smoothness and
  stride matching remain unverified. Vehicles, combat, chat,
  interiors and world sync are unfinished.
  Use outdoor free roam for the first test.

The server relays client-reported transforms. Accounts, encryption, gameplay
authority and anti-cheat are absent. LAN/Internet play is unverified. This protocol
cannot connect to existing MTA servers.
Joined sessions receive snapshots immediately, but their player state is published
only after an accepted position UPDATE. This prevents a temporary remote proxy at
the map origin while a new peer is waiting for its first usable position. An
explicitly submitted zero position remains valid.

Wire version 1 has a little-endian `DES1` header followed by JOIN, WELCOME,
UPDATE, SNAPSHOT or LEAVE payloads. Maximum packet: 512 bytes; full snapshot:
346 bytes. Bridge ABI v2 adds an executable guard, CLEO lifecycle binding and
integer float-bit calls, avoiding game pointers and foreign output allocations.
Call from one owning thread outside the loader lock. See [Bridge.h](Bridge.h).

Repository code/MTA headers retain the repository license. CLEO is separately
licensed freeware; ASI Loader uses MIT. Notices accompany the local bundle. Sanny
definitions have no declared license in the inspected repository; pinned attribution
is preserved. Public redistribution compatibility remains unresolved.

See [DEFINITIVE_EDITION_PORT.md](../../DEFINITIVE_EDITION_PORT.md) for the complete
audit and tracker. Commit messages should state goals, motivation, implementation
reasoning, tests performed and remaining limitations.


### API5 scoped Present association reference

`-MtaCoreRenderFrameBoundaryReference` separately admits the read-only API5 observer in Smoke mode. It also requires `-MtaCoreRenderReference -MtaCoreRenderRoute D3D12 -MtaCoreRenderGpuReference -MtaCoreRenderCustomPresentReference`. Every source flag defaults to false; ordinary mode changes clear this additional lease, and historical configurations cannot infer it from API4 recognition.

The worker requests API5 explicitly and transports 110 unsigned scalar fields. The first 84 preserve API4, with version5, followed by scoped natural Present status, endpoint profile, CPU buffer/queue association facts, original and query HRESULTs, split64 lifecycle counters, thread IDs and query stage. API1 through API4 retain their separate factories and snapshot widths.

`ObservedCoherent=2`, endpoint profile1 and facts255 identify a logged coherent Present-entry association sample. Reader outputs `native_present_observed` and `buffer_association_observed` count only valid sampled coherent reports without native failure or any whole-log issue; cumulative counters cannot reconstruct unsampled successes. A later fatal callback can retain an earlier boundary cache without becoming a new positive sample. These facts do not establish GPU completion, resource state, visible presentation, a drawing boundary or permission to issue GPU commands. GPU ownership, frame-boundary verification, drawing and full startup remain unverified.

Core, the backend and installed files remain retained while cleanup is pending. Cancellation and cleanup deadlines never authorize unloading active hooks. The frontend only polls scalar caches and requests worker cancellation; it receives no native interface pointers and does not call Present, draw, copy or engine input functions.

The separate `ReadGameRenderEndpointDiagnosticV1` export preserves this wire
and every earlier ABI. Its 20 unsigned fields distinguish a worker preparation
failure from callback slot ownership, because API5 EndpointUnadmitted alone
cannot separate those causes. The exact retained V5 lease and its owner thread
are required; invalid reads preserve caller output, and Release excludes new
readers before destroying the lease. Callback snapshots use held module bases
and guarded memory metadata, with no callback file or loader work.

Core writes at most 32 JSONL records per run to the explicit adapter root's
`mta/logs/render-endpoint.log`, retaining process/run identity and native
generation/sample counters. Counter progress alone is deduplicated and one
record is reserved for End. Missing export, wrong version and file failures
cannot change hook cleanup or relabel an older run's cached diagnostic.
Owner classes, relative offsets and branch categories establish location only;
they neither admit unfamiliar code nor infer GPU state or buffer association.
