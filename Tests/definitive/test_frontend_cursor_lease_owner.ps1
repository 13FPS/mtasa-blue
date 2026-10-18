#!/usr/bin/env pwsh
#requires -Version 7.0
# Compile the actual shared cursor owner, lease policy and GUI lifetime against
# fake resources. Native detours and the visible GTA pointer need separate tests.
[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$taskRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../..')).Path
$id = (Get-Date -Format 'yyyyMMdd-HHmmss') + '-' + [guid]::NewGuid().ToString('N')
$outputRoot = Join-Path $taskRoot ('Build/mta-frontend-cursor-lease-owner/' + $id)
$snapshotRoot = Join-Path $outputRoot 'source-snapshot'
$scratchParent = [IO.Path]::GetFullPath((Join-Path $taskRoot 'Build/tmp'))
$scratchRoot = [IO.Path]::GetFullPath((Join-Path $scratchParent ('frontend-cursor-owner-' + $id)))
New-Item -ItemType Directory -Path $outputRoot,$snapshotRoot,$scratchRoot -Force | Out-Null
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
# CCore and CSetCursorPosHook are declared integration inputs, not separately
# compiled here. Their actual-host/native checks must not inherit this result.
$sourceNames = @(
    'Client/core/FrontendCursorLeaseOwner.h','Client/core/FrontendCursorLeasePolicy.h',
    'Client/core/DefinitiveGuiStartupReference.h','Client/core/CCore.cpp',
    'Client/core/CSetCursorPosHook.h','Client/core/CSetCursorPosHook.cpp',
    'Tests/definitive/FrontendCursorLeaseOwnerTests.cpp',
    'Tests/definitive/test_frontend_cursor_lease_owner.ps1'
)
$hashes = [ordered]@{}
$sourcePins = @()
$runs = @()
$failure = $null
$sourcesUnchanged = $false
$scratchFilesRemaining = $null
try {
    foreach ($name in $sourceNames) {
        $sourcePath = Join-Path $taskRoot $name
        $copyPath = Join-Path $snapshotRoot $name
        $hashes[$name] = Sha $sourcePath
        New-Item -ItemType Directory -Path (Split-Path -Parent $copyPath) -Force | Out-Null
        Copy-Item -LiteralPath $sourcePath -Destination $copyPath
        $sourcePins += [ordered]@{path=$name;bytes=(Get-Item -LiteralPath $sourcePath).Length;sha256=$hashes[$name];savedPath=$copyPath}
        if ((Sha $copyPath) -cne $hashes[$name]) { throw ('Source snapshot copy changed: ' + $name) }
    }
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
        $executable = Join-Path $outputRoot ('frontend_cursor_lease_owner_tests-' + $architecture + '.exe')
        $arguments = @('/nologo','/std:c++17','/EHsc','/O2','/MD','/W4','/WX','/DNOMINMAX','/DWIN32_LEAN_AND_MEAN',
            (Join-Path $taskRoot 'Tests/definitive/FrontendCursorLeaseOwnerTests.cpp'),
            ('/Fo' + (Join-Path $outputRoot ('frontend-cursor-owner-' + $architecture + '.obj'))),('/Fe' + $executable),
            '/link','/NOLOGO')
        [ordered]@{compiler=$compiler;arguments=$arguments;scratch=$scratchRoot} | ConvertTo-Json -Depth 4 |
            Set-Content -LiteralPath (Join-Path $outputRoot ('compile-' + $architecture + '-command.json')) -Encoding UTF8
        $compile = Run ('compile-' + $architecture) $compiler $arguments
        $row = [ordered]@{architecture=$architecture;compile=$compile;test=$null;result=$null;executable=$executable;executableSha256=$null}
        if ($compile.exitCode -ne 0 -or $compile.timedOut) { $runs += $row; throw ('Frontend cursor owner compile failed: ' + $architecture) }
        $row.executableSha256 = Sha $executable
        $test = Run ('test-' + $architecture) $executable @() 15000
        $row.test = $test
        if ($test.exitCode -ne 0 -or $test.timedOut) { $runs += $row; throw ('Frontend cursor owner tests failed: ' + $architecture) }
        $row.result = Get-Content -Raw -LiteralPath $test.stdout | ConvertFrom-Json
        $runs += $row
        if (-not $row.result.success -or $row.result.checks -lt 100 -or -not $row.result.productionCursorOwner -or
            -not $row.result.productionLeasePolicy -or -not $row.result.productionGuiLifetime -or
            -not $row.result.fakeCursorResource -or $row.result.gameLaunched -or $row.result.nativeHooksInstalled -or
            $row.result.actualModuleUnload -or $row.result.fullMtaReady) {
            throw ('Frontend cursor owner fixture reported invalid scope/result: ' + $architecture)
        }
    }
    $sourcesUnchanged = $true
    foreach ($name in $sourceNames) {
        if ((Sha (Join-Path $taskRoot $name)) -cne $hashes[$name]) { $sourcesUnchanged = $false }
    }
    if (-not $sourcesUnchanged) { throw 'Frontend cursor owner source changed during verification.' }
} catch {
    $failure = $_.Exception.Message
} finally {
    [Environment]::SetEnvironmentVariable('TEMP',$savedTemp,'Process')
    [Environment]::SetEnvironmentVariable('TMP',$savedTmp,'Process')
    # Only the current run's known absolute workspace scratch is disposable.
    # Source snapshots, compiler products and diagnostics remain durable.
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
    schema='mta_frontend_cursor_lease_owner_tests_v1';recordedAt=[DateTimeOffset]::Now.ToString('o');directory=$outputRoot
    success=($null -eq $failure -and $runs.Count -eq 2 -and $sourcesUnchanged);failure=$failure
    sourceHashes=$hashes;sourcePins=$sourcePins;sourcesUnchanged=$sourcesUnchanged;runs=$runs
    sourceSnapshotScope='Production cursor ownership, unchanged lease policy and GUI lifetime compiled with fake resources; CCore and the native cursor hook are pinned integration inputs only.'
    scratchDirectory=$scratchRoot;scratchFilesRemaining=$scratchFilesRemaining
    productionCursorOwnerTested=$true;productionLeasePolicyTested=$true;productionGuiLifetimeTested=$true;fakeCursorResource=$true
    gameLaunched=$false;gameAttached=$false;gameHooksInstalled=$false;actualModuleUnload=$false
    installationChanged=$false;visiblePointerVerified=$false;fullMtaReady=$false
}
$target = Join-Path $outputRoot 'test-result.json'
$record | ConvertTo-Json -Depth 12 | Set-Content -LiteralPath $target -Encoding UTF8
[ordered]@{success=$record.success;path=$target;sha256=(Sha $target);checks=@($runs | Where-Object {$null -ne $_.result} | ForEach-Object {$_.result.checks})} | ConvertTo-Json -Depth 4
if (-not $record.success) { exit 1 }
