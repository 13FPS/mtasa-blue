#!/usr/bin/env pwsh
#requires -Version 7.0
[CmdletBinding()]
param([ValidateSet('x64','x86')][string[]]$Architectures = @('x64','x86'))
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$taskRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../..')).Path
$taskId = (Get-Date -Format 'yyyyMMdd-HHmmss') + '-' + [guid]::NewGuid().ToString('N')
$outputRoot = Join-Path $taskRoot ('Build/mta-frontend-cursor-lease/' + $taskId)
$scratchRoot = Join-Path $taskRoot ('Build/tmp/frontend-cursor-lease-' + $taskId)
New-Item -ItemType Directory -Path $outputRoot,$scratchRoot | Out-Null
$previousTemp = $env:TEMP
$previousTmp = $env:TMP
$record = [ordered]@{schema='mta_frontend_cursor_lease_policy_test_v1';success=$false;directory=$outputRoot;runs=@();
    realProductionPolicyCompiled=$true;actualCursorHookClassCompiled=$false;actualDetoursTested=$false;
    gameLaunched=$false;gameHooksInstalled=$false;osCursorPositionWritten=$false;osCursorClipWritten=$false;error=$null}
try {
    $sourceNames = @('Client/core/FrontendCursorLeasePolicy.h','utils/definitive_edition/FrontendCursorLeaseTests.cpp',
        'utils/definitive_edition/test_frontend_cursor_lease.ps1')
    $sourceHashes = @($sourceNames | ForEach-Object {
        [ordered]@{path=$_;sha256=(Get-FileHash -LiteralPath (Join-Path $taskRoot $_) -Algorithm SHA256).Hash.ToLowerInvariant()}
    })
    $record.sourceHashes = $sourceHashes
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
    $visualStudio = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    if (-not $visualStudio) { throw 'Visual Studio C++ tools are required.' }
    $developerCommand = Join-Path $visualStudio 'Common7/Tools/VsDevCmd.bat'
    foreach ($architecture in $Architectures) {
        $directory = Join-Path $outputRoot $architecture
        New-Item -ItemType Directory -Path $directory | Out-Null
        $environmentCommand = 'call "{0}" -no_logo -arch={1} -host_arch=x64 >nul && set' -f $developerCommand,$architecture
        $compilerEnvironment = & $env:ComSpec /d /s /c $environmentCommand
        if ($LASTEXITCODE -ne 0) { throw 'Compiler environment initialization failed.' }
        foreach ($entry in $compilerEnvironment) {
            if ($entry -match '^([^=]+)=(.*)$') { [Environment]::SetEnvironmentVariable($Matches[1],$Matches[2],'Process') }
        }
        # Compiler scratch is disposable and task-local. The source hashes,
        # binaries and diagnostic logs remain durable workspace evidence.
        $env:TEMP = $scratchRoot
        $env:TMP = $scratchRoot
        $compiler = Join-Path $env:VCToolsInstallDir ('bin/Hostx64/' + $architecture + '/cl.exe')
        $executable = Join-Path $directory 'frontend_cursor_lease_tests.exe'
        $arguments = @('/nologo','/std:c++17','/EHsc','/W4','/WX','/O2','/MD',
            (Join-Path $PSScriptRoot 'FrontendCursorLeaseTests.cpp'),('/Fo'+$directory+[IO.Path]::DirectorySeparatorChar),('/Fe'+$executable))
        & $compiler @arguments *> (Join-Path $directory 'compile.log')
        $run = [ordered]@{architecture=$architecture;compilerExit=$LASTEXITCODE;testExit=$null;timedOut=$false;checks=0;success=$false}
        $record.runs += $run
        if ($LASTEXITCODE -ne 0) { throw "Cursor lease $architecture policy compilation failed." }
        $process = Start-Process -FilePath $executable -WindowStyle Hidden -PassThru -RedirectStandardOutput (Join-Path $directory 'test.log') `
            -RedirectStandardError (Join-Path $directory 'test-errors.log')
        try {
            $run.timedOut = -not $process.WaitForExit(30000)
            if ($run.timedOut) { $process.Kill(); $process.WaitForExit() }
            $run.testExit = $process.ExitCode
        } finally { $process.Dispose() }
        $output = Get-Content -LiteralPath (Join-Path $directory 'test.log') -Raw
        if ($output -match 'Passed (\d+) frontend cursor lease policy checks') { $run.checks = [int]$Matches[1] }
        $run.executable = [ordered]@{path=$executable;sha256=(Get-FileHash -LiteralPath $executable -Algorithm SHA256).Hash.ToLowerInvariant()}
        $run.success = $run.testExit -eq 0 -and -not $run.timedOut -and $run.checks -gt 0
        if (-not $run.success) { throw "Cursor lease $architecture policy tests failed." }
    }
    $record.sourcesUnchanged = $true
    foreach ($entry in $sourceHashes) {
        if ((Get-FileHash -LiteralPath (Join-Path $taskRoot $entry.path) -Algorithm SHA256).Hash.ToLowerInvariant() -ne $entry.sha256) {
            $record.sourcesUnchanged = $false
        }
    }
    $record.success = $record.sourcesUnchanged -and @($record.runs).Count -eq $Architectures.Count -and @($record.runs | Where-Object{-not $_.success}).Count -eq 0
} catch {
    $record.error = $_.Exception.Message
} finally {
    $env:TEMP = $previousTemp
    $env:TMP = $previousTmp
    # Only this run's resolved scratch files may be cleaned. No durable test
    # evidence or another task's temporary directory belongs to this cleanup.
    $resolvedScratch = [IO.Path]::GetFullPath($scratchRoot)
    $expectedParent = [IO.Path]::GetFullPath((Join-Path $taskRoot 'Build/tmp')) + [IO.Path]::DirectorySeparatorChar
    if (-not $resolvedScratch.StartsWith($expectedParent,[StringComparison]::OrdinalIgnoreCase)) { throw 'Task scratch escaped Build/tmp.' }
    Get-ChildItem -LiteralPath $resolvedScratch -File -Recurse | ForEach-Object { Remove-Item -LiteralPath $_.FullName -Force }
    $record.scratchFilesRemaining = @(Get-ChildItem -LiteralPath $resolvedScratch -File -Recurse).Count
    $record | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath (Join-Path $outputRoot 'test-result.json') -Encoding utf8
}
if (-not $record.success) { throw "Cursor lease policy tests failed; see $outputRoot" }
$record | ConvertTo-Json -Depth 8 -Compress
