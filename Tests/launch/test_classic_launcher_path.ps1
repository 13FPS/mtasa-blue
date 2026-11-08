#!/usr/bin/env pwsh
#requires -Version 7.0
# Run only owned test EXE/DLL builds; never provide the production launcher or loader.
[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$FixtureExe,
    [Parameter(Mandatory)][string]$FixtureDll
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$root = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../..')).Path
$runId = (Get-Date -Format 'yyyyMMdd-HHmmss') + '-' + [guid]::NewGuid().ToString('N')
$scratchBase = [IO.Path]::GetFullPath((Join-Path $root 'Build/tmp'))
$scratch = Join-Path $scratchBase ('classic-launcher-path-' + $runId)
$evidence = Join-Path $root ('Build/definitive-recovery/classic-launcher-path-' + $runId)
$utf8 = [Text.UTF8Encoding]::new($false)

function Sha([string]$Path) {
    return (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToLowerInvariant()
}
function RequireX86Image([string]$Path, [bool]$Dll) {
    $stream = [IO.File]::OpenRead($Path)
    try {
        $reader = [IO.BinaryReader]::new($stream)
        if ($reader.ReadUInt16() -ne 0x5a4d) { throw 'Fixture is not a PE image.' }
        $stream.Position = 0x3c
        $header = $reader.ReadUInt32()
        if ($header -gt $stream.Length - 24) { throw 'Fixture PE header is truncated.' }
        $stream.Position = $header
        if ($reader.ReadUInt32() -ne 0x4550 -or $reader.ReadUInt16() -ne 0x14c) { throw 'Fixtures must be x86 PE images.' }
        $stream.Position = $header + 22
        if ((($reader.ReadUInt16() -band 0x2000) -ne 0) -ne $Dll) { throw 'Fixture image type does not match its role.' }
    } finally { $stream.Dispose() }
}

$exeSource = (Resolve-Path -LiteralPath $FixtureExe).Path
$dllSource = (Resolve-Path -LiteralPath $FixtureDll).Path
# The fixture filenames distinguish these test-only builds from production
# binaries before any process is started or any launcher files are copied.
if ([IO.Path]::GetFileName($exeSource) -cne 'ClassicLauncherPathFixture.exe' -or
    [IO.Path]::GetFileName($dllSource) -cne 'ClassicLauncherLoaderFixture.dll') {
    throw 'Provide the named test-only fixture builds, never the production launcher or loader.'
}
RequireX86Image $exeSource $false
RequireX86Image $dllSource $true
New-Item -ItemType Directory -Path $scratch,$evidence | Out-Null
$cases = [Collections.Generic.List[object]]::new()
$sourceHashes = [ordered]@{}
foreach ($name in @('Client/launch/Main.cpp', 'Client/launch/StdInc.cpp', 'Client/launch/StdInc.h',
    'Tests/launch/ClassicLauncherPathFixture.cpp', 'Tests/launch/ClassicLauncherLoaderFixture.cpp',
    'Tests/launch/test_classic_launcher_path.ps1')) {
    $sourceHashes[$name] = Sha (Join-Path $root $name)
}
$exeHash = Sha $exeSource
$dllHash = Sha $dllSource
$cleaned = $false

function AddOwnedLoader([string]$Directory) {
    [IO.Directory]::CreateDirectory($Directory) | Out-Null
    $destination = Join-Path $Directory 'loader.dll'
    Copy-Item -LiteralPath $dllSource -Destination $destination
    if ((Sha $destination) -ne $dllHash) { throw 'Owned loader copy hash mismatch.' }
    return $destination
}
function InvokeCase([string]$Name, [string]$LauncherDirectory, [string]$ExpectedLoader, [int]$ExpectedExitCode) {
    [IO.Directory]::CreateDirectory($LauncherDirectory) | Out-Null
    $executable = Join-Path $LauncherDirectory 'Multi Theft Auto.exe'
    Copy-Item -LiteralPath $exeSource -Destination $executable
    if ((Sha $executable) -ne $exeHash) { throw 'Owned launcher copy hash mismatch.' }
    $start = [Diagnostics.ProcessStartInfo]::new()
    $start.FileName = $executable
    $start.UseShellExecute = $false
    $start.CreateNoWindow = $true
    $start.WorkingDirectory = $scratch
    $start.ArgumentList.Add('--classic-launcher-fixture')
    $start.Environment['MTA_CLASSIC_LAUNCHER_FIXTURE_DLL'] = $ExpectedLoader
    $process = [Diagnostics.Process]::new()
    $process.StartInfo = $start
    try {
        if (-not $process.Start()) { throw 'Could not start the owned launcher fixture.' }
        if (-not $process.WaitForExit(15000)) {
            $process.Kill($true)
            $process.WaitForExit()
            throw ('Owned launcher fixture timed out: ' + $Name)
        }
        $observedExitCode = $process.ExitCode
        if ($observedExitCode -ne $ExpectedExitCode) {
            throw "$Name returned $observedExitCode; expected $ExpectedExitCode."
        }
        $cases.Add([ordered]@{name=$Name;expected_exit_code=$ExpectedExitCode;observed_exit_code=$observedExitCode;expected_loader=$ExpectedLoader})
    } finally { $process.Dispose() }
}

try {
    $adjacentRoot = Join-Path $scratch 'adjacent with spaces'
    $adjacentLauncher = Join-Path $adjacentRoot 'launcher'
    $adjacentLoader = AddOwnedLoader (Join-Path $adjacentLauncher 'mta')
    $null = AddOwnedLoader (Join-Path $adjacentRoot 'mta')
    InvokeCase 'adjacent-loader-takes-priority' $adjacentLauncher $adjacentLoader 73

    $parentRoot = Join-Path $scratch 'parent fallback with spaces'
    $parentLoader = AddOwnedLoader (Join-Path $parentRoot 'mta')
    InvokeCase 'parent-loader-fallback' (Join-Path $parentRoot 'launcher') $parentLoader 73

    # A working-directory loader must never rescue an absent adjacent/parent
    # directory; the production loader is never available in any fixture tree.
    $null = AddOwnedLoader (Join-Path $scratch 'mta')
    InvokeCase 'working-directory-does-not-supply-loader' (Join-Path $scratch 'missing/launcher') '' 1

    foreach ($name in $sourceHashes.Keys) {
        if ((Sha (Join-Path $root $name)) -ne $sourceHashes[$name]) { throw 'Canonical source changed during the run.' }
    }
    if ((Sha $exeSource) -ne $exeHash -or (Sha $dllSource) -ne $dllHash) { throw 'Source fixture binary changed during the run.' }
} finally {
    $resolvedScratch = [IO.Path]::GetFullPath($scratch)
    if (-not $resolvedScratch.StartsWith($scratchBase + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) {
        throw 'Refusing to clean a scratch directory outside Build/tmp.'
    }
    if (Test-Path -LiteralPath $resolvedScratch) { Remove-Item -LiteralPath $resolvedScratch -Recurse -Force }
    $cleaned = -not (Test-Path -LiteralPath $resolvedScratch)
}

$result = [ordered]@{
    success=$true;created_at=[DateTimeOffset]::Now.ToString('o');cases=$cases;case_count=$cases.Count
    source_hashes=$sourceHashes;fixture_exe_sha256=$exeHash;fixture_dll_sha256=$dllHash;scratch_removed=$cleaned
    game_launched=$false;real_loader_loaded=$false;core_loaded=$false;netc_loaded=$false
    ambient_report_log_written=$false;game_installation_writes=$false
    scope='Actual classic Main.cpp path discovery, owned module loading, and DoWinMain dispatch through a test-only diagnostics/browser wrapper.'
}
[IO.File]::WriteAllText((Join-Path $evidence 'result.json'), ($result | ConvertTo-Json -Depth 8), $utf8)
$result | ConvertTo-Json -Depth 8
