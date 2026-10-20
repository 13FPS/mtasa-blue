# Experimental DE launcher startup

The opt-in Release/x64 **Client Launcher** uses MTA's existing
`LoadLibrarySafe` and `ExecuteDoWinMain` dispatch to load an adjacent
`mta/de_preflight.dll`. This loader checks files without starting GTA, loading
Core or modifying the installation. Explicit host launch now uses that same
validator before asking Steam to start the installed native MTA owner. Full
DE gameplay and multiplayer remain unfinished.

Generate the normal MTA projects with `utils/premake5.exe
--with-definitive-adapter vs2026`, then build **Client Launcher** in
Release/x64. The validator is a project dependency. Outputs are isolated in
`Bin/mta-launcher/x64/`; default builds continue to exclude the x64 launcher.
The classic Win32 launcher and loader retain their existing startup path.

Invoke the executable with redirected standard output to receive one JSON
status record:

```powershell
& './Bin/mta-launcher/x64/Multi Theft Auto.exe' --de-preflight `
    --game-exe 'C:\absolute\game\SanAndreas.exe' `
    --bundle 'C:\absolute\owned-bundle\Win64' `
    --manifest 'C:\absolute\package\manifest.json' | Write-Output
```

Use a **package manifest**, not `de-online-install-manifest.json`. The latter
contains destination and installation metadata outside this input schema.
Arguments must name normalized absolute paths; spaces and Unicode paths are
supported. The bundle can be an owned staging copy. Its 23 fixed ownership
paths must all match the supplied package hashes, and native modules must be
AMD64 PE32+ DLLs. The game must match the supported executable fingerprint.
Linked paths, missing files, invalid PE bounds and oversized inputs fail.
The manifest is a file-coherence input, not a trusted signature or proof that
the binaries implement MTA gameplay.

Exit 0 reports `preflight_valid` and 23 verified files. Exit 1 rejects an
unsupported launcher invocation, 2 rejects loading the adjacent validator,
3 rejects its entry point, and 4 rejects arguments or file validation.
Failures remain headless. A valid preflight explicitly reports
`launchReady: false`, `hostLaunchReady: false`, `hostLaunchRequested: false`
and `fullMtaReady: false`. An opt-in x64 launcher without an explicit DE
selection also exits without starting a game.

To request actual host startup, select `--de-launch` with the same three path
arguments. Here `--bundle` must be the real game's `Gameface/Binaries/Win64`
directory and `--manifest` must describe its installed native 60/64-file
package. The native host and retained runtime must be explicitly enabled; a
disarmed INI, a staging copy or a different installation receipt is refused.
The launcher verifies every installed receipt destination and hash, the exact
game fingerprint, the registered Steam library's app 1547000 and its installed
appmanifest. An already running `SanAndreas.exe` is refused.

Only then does MTA's existing nonblocking shell helper request the fixed
`steam://run/1547000` route. Steam/Rockstar handle authentication. Successful
request acceptance reports `hostLaunchReady: true` and
`hostLaunchRequested: true`; `launchReady`, `gameLaunched`, `coreLoaded` and
`fullMtaReady` remain false because this external launcher cannot prove an
in-game startup result. Exit 5 means preparation passed but COM or shell
dispatch failed. No direct game execution, injection, network client or
installation writes are performed.

The installed client also supports an explicit selection with no workspace
package paths:

```powershell
& './Bin/mta-launcher/x64/Multi Theft Auto.exe' --de-launch-installed
```

This reads the current user's registered Steam root and its library list,
requires exactly one library registering app 1547000, and resolves the game's
directory from that library's installed appmanifest. The `installdir` value
must be one safe directory name. The adjacent
`de-online-install-manifest.json` must then describe exactly the native
60/64-file owner, with matching game identity, absolute destinations, file
hashes, AMD64 modules and the same enabled native runtime configuration/INI
policy used by the explicit path route. The receipt is installation ownership
evidence, not a package provenance signature or proof of complete gameplay.
No workspace package manifest is synthesized or required.

Extra arguments and mixed DE selections are refused before discovery.
`--de-launch` retains its required three path arguments; the new selection
does not provide fallback behavior for an incomplete old invocation. The
installed selection uses the same fixed Steam request and honest readiness
fields. Its `DE_PrepareInstalledHostLaunchV1` export returns the same value-only
112-byte preparation result without dispatching Steam, allowing read-only
offline validation of the real registered installation.

Focused verification lives in `Tests/definitive/test_de_launcher_preflight.ps1`.
The normal launcher/validator check passes 25 cases and 254 assertions, with
17 build-policy guards preserving classic Win32 and default x64 exclusion.
Run the preflight suite
against normal build outputs and an owned copy of the supported game/bundle;
it does not execute any inspected engine module or launch GTA.
`test_de_host_launch_preparation.ps1` additionally invokes the normal-built
preparation export read-only against a matching, closed, registered native
installation, exercises owned refusal fixtures through the actual launcher,
and checks the shared Steam locator against bounded VDF inputs. Its owned host
never calls the shell helper; the real game executable is copied and hashed
only.
