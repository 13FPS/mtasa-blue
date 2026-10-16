#!/usr/bin/env pwsh
#requires -Version 7.0
[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$taskRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../..')).Path
$taskId = (Get-Date -Format 'yyyyMMdd-HHmmss') + '-' + [guid]::NewGuid().ToString('N')
$outputRoot = Join-Path $taskRoot ('Build/mta-frontend-input-lease/' + $taskId)
$scratchRoot = Join-Path $taskRoot ('Build/tmp/frontend-input-lease-' + $taskId)
New-Item -ItemType Directory -Path $outputRoot,$scratchRoot | Out-Null
$previousTemp = $env:TEMP
$previousTmp = $env:TMP
$env:TEMP = $scratchRoot
$env:TMP = $scratchRoot
$record = [ordered]@{schema='mta_frontend_input_lease_test_v1';success=$false;directory=$outputRoot;checks=0;
    gameLaunched=$false;gameHooksInstalled=$false;gameInputWritten=$false;error=$null}
try {
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
    $visualStudio = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    if (-not $visualStudio) { throw 'Visual Studio x64 tools are required.' }
    $developerCommand = Join-Path $visualStudio 'Common7/Tools/VsDevCmd.bat'
    $environmentCommand = 'call "{0}" -no_logo -arch=x64 -host_arch=x64 >nul && set' -f $developerCommand
    $compilerEnvironment = & $env:ComSpec /d /s /c $environmentCommand
    if ($LASTEXITCODE -ne 0) { throw 'Compiler environment initialization failed.' }
    foreach ($entry in $compilerEnvironment) {
        if ($entry -match '^([^=]+)=(.*)$') { [Environment]::SetEnvironmentVariable($Matches[1],$Matches[2],'Process') }
    }
    # The environment helper may reset TEMP/TMP; compiler scratch remains task
    # local so the receipt and logs survive ordinary system-temp cleanup.
    $env:TEMP = $scratchRoot
    $env:TMP = $scratchRoot
    $sourceNames = @('Client/sdk/game/CGameFrontendInputLease.h','Client/sdk/game/CGameAdapter.h',
        'Client/game_de/FrontendInputLeasePolicy.h','Client/game_de/NativeMovementInput.h','Client/game_de/NativeMovementInput.cpp',
        'Client/game_de/NativeInputRouting.h','Client/game_de/NativePadReader.h','Client/game_de/NativePadReader.cpp',
        'Shared/sdk/CControllerState.h','Shared/sdk/CControlStateSynthesis.h','Shared/sdk/enums/PedState.h',
        'utils/definitive_edition/FrontendInputLeaseTests.cpp','utils/definitive_edition/test_frontend_input_lease.ps1',
        'vendor/detours/4.0.1/src/detours.h','vendor/detours/4.0.1/src/detours.cpp','vendor/detours/4.0.1/src/modules.cpp',
        'vendor/detours/4.0.1/src/disasm.cpp','vendor/detours/4.0.1/src/image.cpp','vendor/detours/4.0.1/src/creatwth.cpp')
    $sourceHashes = @($sourceNames | ForEach-Object {
        [ordered]@{path=$_;sha256=(Get-FileHash -LiteralPath (Join-Path $taskRoot $_) -Algorithm SHA256).Hash.ToLowerInvariant()}
    })
    $record.sourceHashes = $sourceHashes
    $compiler = Join-Path $env:VCToolsInstallDir 'bin/Hostx64/x64/cl.exe'
    $common = @('/nologo','/std:c++17','/EHsc','/O2','/MD','/DNOMINMAX','/DWIN32_LEAN_AND_MEAN',
        ('/I'+(Join-Path $taskRoot 'Shared/sdk')),('/I'+(Join-Path $taskRoot 'vendor/detours/4.0.1/src')))
    $detoursSources = @('detours.cpp','modules.cpp','disasm.cpp','image.cpp','creatwth.cpp') | ForEach-Object {
        Join-Path $taskRoot ('vendor/detours/4.0.1/src/'+$_)
    }
    $arguments = $common + @('/w','/c') + $detoursSources + @('/Fo'+$outputRoot+[IO.Path]::DirectorySeparatorChar)
    & $compiler @arguments *> (Join-Path $outputRoot 'detours-compile.log')
    $record.detoursCompilerExit = $LASTEXITCODE
    if ($LASTEXITCODE -ne 0) { throw 'Detours fixture compilation failed.' }
    $objects = @('detours.obj','modules.obj','disasm.obj','image.obj','creatwth.obj') | ForEach-Object { Join-Path $outputRoot $_ }
    $executable = Join-Path $outputRoot 'frontend_input_lease_tests.exe'
    $arguments = $common + @('/W4','/WX',(Join-Path $PSScriptRoot 'FrontendInputLeaseTests.cpp'),
        (Join-Path $taskRoot 'Client/game_de/NativePadReader.cpp'),('/Fo'+$outputRoot+[IO.Path]::DirectorySeparatorChar),
        ('/Fe'+$executable),'/link','bcrypt.lib','user32.lib') + $objects
    & $compiler @arguments *> (Join-Path $outputRoot 'compile.log')
    $record.compilerExit = $LASTEXITCODE
    if ($LASTEXITCODE -ne 0) { throw 'Native frontend lease compilation failed.' }
    $process = Start-Process -FilePath $executable -WindowStyle Hidden -PassThru -RedirectStandardOutput (Join-Path $outputRoot 'test.log') `
        -RedirectStandardError (Join-Path $outputRoot 'test-errors.log')
    try {
        $record.testTimedOut = -not $process.WaitForExit(30000)
        if ($record.testTimedOut) { $process.Kill(); $process.WaitForExit() }
        $record.testExit = $process.ExitCode
    } finally { $process.Dispose() }
    $output = Get-Content -LiteralPath (Join-Path $outputRoot 'test.log') -Raw
    if ($output -match 'Passed (\d+) native frontend lease checks') { $record.checks = [int]$Matches[1] }
    $record.sourcesUnchanged = $true
    foreach ($entry in $sourceHashes) {
        if ((Get-FileHash -LiteralPath (Join-Path $taskRoot $entry.path) -Algorithm SHA256).Hash.ToLowerInvariant() -ne $entry.sha256) {
            $record.sourcesUnchanged = $false
        }
    }
    $record.executable = [ordered]@{path=$executable;sha256=(Get-FileHash -LiteralPath $executable -Algorithm SHA256).Hash.ToLowerInvariant()}
    $record.success = $record.testExit -eq 0 -and -not $record.testTimedOut -and $record.checks -gt 0 -and $record.sourcesUnchanged
    if (-not $record.success) { throw 'Native frontend lease checks failed.' }
} catch {
    $record.error = $_.Exception.Message
} finally {
    $env:TEMP = $previousTemp
    $env:TMP = $previousTmp
    # Delete only this run's explicitly resolved scratch files, never build
    # evidence. The directory itself can remain empty for a restart audit.
    $resolvedScratch = [IO.Path]::GetFullPath($scratchRoot)
    $expectedScratchParent = [IO.Path]::GetFullPath((Join-Path $taskRoot 'Build/tmp')) + [IO.Path]::DirectorySeparatorChar
    if (-not $resolvedScratch.StartsWith($expectedScratchParent, [StringComparison]::OrdinalIgnoreCase)) {
        throw 'Task scratch escaped Build/tmp.'
    }
    Get-ChildItem -LiteralPath $resolvedScratch -File -Recurse | ForEach-Object { Remove-Item -LiteralPath $_.FullName -Force }
    $record.scratchFilesRemaining = @(Get-ChildItem -LiteralPath $resolvedScratch -File -Recurse).Count
    $record | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath (Join-Path $outputRoot 'test-result.json') -Encoding utf8
}
if (-not $record.success) { throw "Native frontend lease checks failed; see $outputRoot" }
$record | ConvertTo-Json -Depth 8 -Compress
