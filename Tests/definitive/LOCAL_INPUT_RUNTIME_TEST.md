# EXPERIMENTAL: retained Core and local input

This combined check covers MTA's retained menu, Settings bindings and local
on-foot input. Only forwards, backwards, left, right, walk and sprint are
supported by this input service. Full `CGame`, networking and multiplayer startup
remain incomplete. Run this once the candidate and capture tooling are installed,
published and explicitly ready. The user performs the GTA actions.

Launch GTA through Steam in **Windowed** mode and load a clear, flat street on
foot. Leave F9 untouched until the operator saves the fresh Baseline capture.

1. Hold **Ctrl+Shift+F9** for three seconds, then release every key.
2. Open **Settings → Binds**. Set the primary **Forwards** key to **I** and click
   **Apply (session only)**. Close the menu with **Quit**.
3. Leave every key released for three seconds. Check movement with **I**, then
   actual sprinting with **I+Left Shift**. Release the keys.
4. Hold the full **Ctrl+Shift+F9** chord for three seconds, then release every key.
   Check that the second menu opens and Forwards still shows **I**.
5. Stage **O** for Forwards, click **Cancel**, then reopen Settings and confirm
   Forwards remains **I**. Close Settings and click the menu's **Quit**.
6. Check that **I** still moves the player. Release every key.
7. Hold **Ctrl+Shift+End** for three seconds, then release every key. Keep GTA
   focused for five seconds. Check that normal **W** movement, sprinting and
   camera controls have returned. Leave GTA open for the Actual capture.

Report what happened, including any missing second menu, pointer problem, input
failure or failed restoration. The menu's Quit closes its view; the final End
chord stops the combined runtime. There is no fixed trial timer or 45-second
waiting step. After Actual evidence is saved, exit GTA normally when requested
so the operator can capture Shutdown evidence.

The operator uses PowerShell 7.5 and
[`capture_local_input_runtime.ps1`](capture_local_input_runtime.ps1). Every mode
requires explicit `-InstalledReady` and `-ExpectedReadySha256` pins.

| Mode | Additional pinned input | Capture point |
| --- | --- | --- |
| `Baseline` | None | Fresh GTA process, before the first F9 chord. |
| `Actual` | `-Baseline`, `-ExpectedBaselineSha256` | Runtime stopped, GTA still open. |
| `Shutdown` | `-Actual`, `-ExpectedActualSha256` | User has exited GTA normally. |

Use the exact path and SHA256 returned by the preceding capture. The tool saves
all fourteen raw logs before protocol validation. It reads process/module and
installed-file identity, copies evidence into the workspace, and sends no input,
loads no trial DLL, changes no installed file and performs no game activation.

Keep protocol, resource cleanup and the verbatim human report separate. Positive
machine evidence requires two actual native-admitted views, two retirements and
an active local input lease between views on the same Core/binding owner, followed
by explicit Stop and qualified cleanup. Shortcut events alone cannot establish
native admission. Telemetry does not establish a raw Quit click or visual success.

Runtime416 and local-input32 have independent publication caches. Match their
session, owner, command, stage and generation evidence in order; do not treat two
captures as one simultaneous snapshot. `providerPinnedExpected=1` can legitimately
leave the backend module loaded after native leases, hooks and scopes retire.
Failed observations remain failed even when cleanup and normal exit succeed.
