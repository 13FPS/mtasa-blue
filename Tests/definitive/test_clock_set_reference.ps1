#!/usr/bin/env pwsh
#requires -Version 7.0
# Exercise the production value-only clock Set lifecycle with deterministic owners.
# Game loading, native clock writes and runtime installation are separate work.
[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$taskRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../..')).Path
$id = (Get-Date -Format 'yyyyMMdd-HHmmss') + '-' + [guid]::NewGuid().ToString('N')
$outputRoot = Join-Path $taskRoot ('Build/mta-core-clock-set-reference/' + $id)
$scratchParent = [IO.Path]::GetFullPath((Join-Path $taskRoot 'Build/tmp'))
$scratchRoot = [IO.Path]::GetFullPath((Join-Path $scratchParent ('clock-set-reference-' + $id)))
New-Item -ItemType Directory -Path $outputRoot,$scratchRoot -Force | Out-Null
$savedTemp = [Environment]::GetEnvironmentVariable('TEMP','Process')
$savedTmp = [Environment]::GetEnvironmentVariable('TMP','Process')
function Sha([string] $path) { (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLowerInvariant() }
function Run([string] $name, [string] $binary, [string[]] $arguments, [int] $timeoutMs = 60000) {
    $stdout = Join-Path $outputRoot ($name + '.log')
    $stderr = Join-Path $outputRoot ($name + '-errors.log')
    $start = [Diagnostics.ProcessStartInfo]::new()
    $start.FileName = $binary
    $start.WorkingDirectory = $outputRoot
    $start.UseShellExecute = $false
    $start.CreateNoWindow = $true
    $start.RedirectStandardOutput = $true
    $start.RedirectStandardError = $true
    foreach ($argument in $arguments) { $start.ArgumentList.Add($argument) }
    $process = [Diagnostics.Process]::new()
    $process.StartInfo = $start
    try {
        if (-not $process.Start()) { throw ('Could not start ' + $name) }
        $readOut = $process.StandardOutput.ReadToEndAsync()
        $readErr = $process.StandardError.ReadToEndAsync()
        $timedOut = -not $process.WaitForExit($timeoutMs)
        if ($timedOut) { $process.Kill($true); $process.WaitForExit() }
        [IO.File]::WriteAllText($stdout,$readOut.GetAwaiter().GetResult(),[Text.UTF8Encoding]::new($false))
        [IO.File]::WriteAllText($stderr,$readErr.GetAwaiter().GetResult(),[Text.UTF8Encoding]::new($false))
        [ordered]@{name=$name;exitCode=$process.ExitCode;timedOut=$timedOut;stdout=$stdout;stderr=$stderr;
            stdoutSha256=(Sha $stdout);stderrSha256=(Sha $stderr)}
    } finally { $process.Dispose() }
}
$sourceNames = @(
    'Client/core/DefinitiveClockSetReference.h','Client/core/DefinitiveClockSetReference.cpp',
    'Client/core/DefinitiveClockReference.h',
    'Client/core/DefinitiveInputReference.h','Client/core/DefinitiveGuiFrontendReference.h',
    'Client/core/DefinitiveGuiSurfaceReference.h','Client/core/DefinitiveGuiStartupReference.h',
    'Client/sdk/game/CGameClockService.h','Client/sdk/game/CGameAdapter.h',
    'Shared/sdk/CControllerState.h','Shared/sdk/CControlStateSynthesis.h',
    'Shared/sdk/CControllerActions.h','Tests/definitive/ClockSetReferenceTests.cpp',
    'Tests/definitive/test_clock_set_reference.ps1'
)
$hashes = [ordered]@{}
$runs = @()
$failure = $null
$sourcesUnchanged = $false
$scratchFilesRemaining = $null
try {
    foreach ($name in $sourceNames) { $hashes[$name] = Sha (Join-Path $taskRoot $name) }
    $vswhere = Join-Path ([Environment]::GetEnvironmentVariable('ProgramFiles(x86)')) 'Microsoft Visual Studio/Installer/vswhere.exe'
    $visualStudio = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    if (-not $visualStudio) { throw 'Visual Studio C++ x86/x64 tools are required.' }
    $developerCommand = Join-Path $visualStudio 'Common7/Tools/VsDevCmd.bat'
    foreach ($architecture in @('x86','x64')) {
        $environment = & $env:ComSpec /d /s /c ('call "{0}" -no_logo -arch={1} -host_arch=x64 >nul && set' -f $developerCommand,$architecture)
        if ($LASTEXITCODE -ne 0) { throw 'Compiler environment initialization failed.' }
        foreach ($entry in $environment) {
            if ($entry -match '^([^=]+)=(.*)$') { [Environment]::SetEnvironmentVariable($Matches[1],$Matches[2],'Process') }
        }
        $env:TEMP = $scratchRoot
        $env:TMP = $scratchRoot
        $compiler = Join-Path $env:VCToolsInstallDir ('bin/Hostx64/' + $architecture + '/cl.exe')
        $executable = Join-Path $outputRoot ('clock_set_reference_tests-' + $architecture + '.exe')
        $arguments = @('/nologo','/std:c++17','/EHsc','/O2','/MD','/W4','/WX','/DNOMINMAX','/DWIN32_LEAN_AND_MEAN',
            ('/I' + (Join-Path $taskRoot 'Client/sdk')),('/I' + (Join-Path $taskRoot 'Shared/sdk')),
            (Join-Path $taskRoot 'Tests/definitive/ClockSetReferenceTests.cpp'),
            ('/Fo' + (Join-Path $outputRoot ('clock-set-reference-' + $architecture + '.obj'))),('/Fe' + $executable),
            '/link','/NOLOGO')
        [ordered]@{compiler=$compiler;arguments=$arguments;scratch=$scratchRoot} | ConvertTo-Json -Depth 4 |
            Set-Content -LiteralPath (Join-Path $outputRoot ('compile-' + $architecture + '-command.json')) -Encoding UTF8
        $compile = Run ('compile-' + $architecture) $compiler $arguments
        $row = [ordered]@{architecture=$architecture;compile=$compile;test=$null;result=$null;executable=$executable;executableSha256=$null}
        if ($compile.exitCode -ne 0 -or $compile.timedOut) { $runs += $row; throw ('Clock Set reference compile failed: ' + $architecture) }
        $row.executableSha256 = Sha $executable
        $test = Run ('test-' + $architecture) $executable @() 15000
        $row.test = $test
        if ($test.exitCode -ne 0 -or $test.timedOut) { $runs += $row; throw ('Clock Set reference tests failed: ' + $architecture) }
        $row.result = Get-Content -Raw -LiteralPath $test.stdout | ConvertFrom-Json
        $runs += $row
        if (-not $row.result.success) { throw ('Clock Set reference fixture reported failure: ' + $architecture) }
    }
    $sourcesUnchanged = $true
    foreach ($name in $sourceNames) {
        if ((Sha (Join-Path $taskRoot $name)) -cne $hashes[$name]) { $sourcesUnchanged = $false }
    }
    if (-not $sourcesUnchanged) { throw 'Clock Set reference source changed during verification.' }
} catch {
    $failure = $_.Exception.Message
} finally {
    [Environment]::SetEnvironmentVariable('TEMP',$savedTemp,'Process')
    [Environment]::SetEnvironmentVariable('TMP',$savedTmp,'Process')
    # Only this run's explicitly created workspace scratch is disposable.
    # Evidence and compiler outputs remain under the durable output directory.
    $resolvedScratch = [IO.Path]::GetFullPath($scratchRoot)
    $scratchPrefix = $scratchParent + [IO.Path]::DirectorySeparatorChar
    if (-not $resolvedScratch.StartsWith($scratchPrefix,[StringComparison]::OrdinalIgnoreCase)) { throw 'Scratch path escaped Build/tmp.' }
    if (Test-Path -LiteralPath $resolvedScratch) {
        Remove-Item -LiteralPath $resolvedScratch -Recurse -Force
    }
    $scratchFilesRemaining = if (Test-Path -LiteralPath $resolvedScratch) {
        @(Get-ChildItem -LiteralPath $resolvedScratch -Recurse -File -Force).Count
    } else { 0 }
}
$record = [ordered]@{
    schema='mta_core_clock_set_reference_tests_v1';recordedAt=[DateTimeOffset]::Now.ToString('o');directory=$outputRoot
    success=($null -eq $failure -and $runs.Count -eq 2 -and $sourcesUnchanged);failure=$failure
    sourceHashes=$hashes;sourcesUnchanged=$sourcesUnchanged;runs=$runs
    sourceSnapshotScope='Declared clock Set reference lifecycle, SDK value dependencies and fixture; production Core wrapper is pinned but is not compiled by this deterministic policy test.'
    scratchDirectory=$scratchRoot;scratchFilesRemaining=$scratchFilesRemaining
    productionLifecycleTested=$true;fakeOwnerAndClock=$true
    gameLaunched=$false;gameAttached=$false;gameClockWritten=$false;gameHooksInstalled=$false
    installationChanged=$false;fullMtaReady=$false
}
$target = Join-Path $outputRoot 'test-result.json'
$record | ConvertTo-Json -Depth 12 | Set-Content -LiteralPath $target -Encoding UTF8
[ordered]@{success=$record.success;path=$target;sha256=(Sha $target);checks=@($runs | Where-Object {$null -ne $_.result} | ForEach-Object {$_.result.checks})} | ConvertTo-Json -Depth 4
if (-not $record.success) { exit 1 }
