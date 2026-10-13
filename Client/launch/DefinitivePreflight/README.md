# Experimental DE launcher preflight

The opt-in Release/x64 **Client Launcher** uses MTA's existing
`LoadLibrarySafe` and `ExecuteDoWinMain` dispatch to load an adjacent
`mta/de_preflight.dll`. This loader checks files without starting GTA, loading
Core or modifying the installation. It is a startup prerequisite, not a
playable DE client.

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
Failures remain headless. A valid result explicitly reports
`launchReady: false` and `fullMtaReady: false`; full game, rendering and
multiplayer startup remain gated. An opt-in x64 launcher without the explicit
preflight flag also exits without starting a game.

Focused verification lives in `Tests/definitive/test_de_launcher_preflight.ps1`.
The normal launcher/validator check passes 25 cases and 254 assertions, with
17 build-policy guards preserving classic Win32 and default x64 exclusion.
Run the preflight suite
against normal build outputs and an owned copy of the supported game/bundle;
it does not execute any inspected engine module or launch GTA.
