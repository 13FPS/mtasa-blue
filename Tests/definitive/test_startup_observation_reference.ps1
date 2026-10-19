#!/usr/bin/env pwsh
#requires -Version 7.0
# Exercise production RunLifecycle with a deterministic fake owner and clock.
# Native application callbacks and live GTA startup require separate evidence.
[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$taskRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../..')).Path
$id = (Get-Date -Format 'yyyyMMdd-HHmmss') + '-' + [guid]::NewGuid().ToString('N')
$outputRoot = Join-Path $taskRoot ('Build/mta-startup-observation-reference/' + $id)
$scratchRoot = Join-Path $taskRoot ('Build/tmp/startup-observation-reference-' + $id)
[IO.Directory]::CreateDirectory($outputRoot) | Out-Null
[IO.Directory]::CreateDirectory($scratchRoot) | Out-Null
$previousTemp = $env:TEMP
$previousTmp = $env:TMP
$record = [ordered]@{
    schema='mta_startup_observation_reference_tests_v1';directory=$outputRoot;architecture='x64';success=$false;
    recordedAt=[DateTimeOffset]::Now.ToString('o');sourceHashes=@();sourcesUnchanged=$true;
    compile=$null;test=$null;checks=0;executable=$null;executableSha256=$null;failure=$null;
    scope='Deterministic fake owner and clock exercising production composite startup observer RunLifecycle only; no native application callback or live GTA startup readiness is established.';
    sourceSnapshotScope='Production header-template RunLifecycle, recursive value dependencies and fixture; Core runtime wrappers are pinned but not compiled or executed by this fake-owner test. Not a complete normal Core compiler dependency trace.';
    productionLifecycleTested=$true;fakeOwnerAndClock=$true;engineCodeExecuted=$false;
    gameLaunched=$false;gameAttached=$false;gameProcessEnumerated=$false;gameHooksInstalled=$false;
    nativeLifecycleReady=$false;clientDllLoaded=$false;installationChanged=$false;fullMtaReady=$false
}

function Sha([string]$Path) { (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToLowerInvariant() }
function AddFailure([string]$Message) {
    $record.success = $false
    if ($null -eq $record.failure) { $record.failure = $Message }
    else { $record.failure += [Environment]::NewLine + $Message }
}
function RunBounded([string]$Executable,[string[]]$Arguments,[string]$Name,[int]$DeadlineMs) {
    $stdout = Join-Path $outputRoot ($Name + '.log')
    $stderr = Join-Path $outputRoot ($Name + '-errors.log')
    $command = Join-Path $outputRoot ($Name + '-command.json')
    $executableHash = Sha $Executable
    [ordered]@{
        executable=$Executable;sha256=$executableHash;arguments=$Arguments;workingDirectory=$outputRoot;
        deadlineMs=$DeadlineMs;temporaryDirectory=$scratchRoot
    } | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath $command -Encoding utf8NoBOM
    $info = [Diagnostics.ProcessStartInfo]::new()
    $info.FileName = $Executable
    $info.WorkingDirectory = $outputRoot
    $info.UseShellExecute = $false
    $info.CreateNoWindow = $true
    $info.RedirectStandardOutput = $true
    $info.RedirectStandardError = $true
    foreach ($argument in $Arguments) { $info.ArgumentList.Add($argument) }
    $process = [Diagnostics.Process]::new()
    $process.StartInfo = $info
    $timer = [Diagnostics.Stopwatch]::StartNew()
    try {
        if (-not $process.Start()) { throw "Could not launch owned $Name process." }
        $readOut = $process.StandardOutput.ReadToEndAsync()
        $readErr = $process.StandardError.ReadToEndAsync()
        $timedOut = -not $process.WaitForExit($DeadlineMs)
        if ($timedOut) {
            # Only the process tree created by this fixture belongs to the run.
            $process.Kill($true)
            $process.WaitForExit()
        }
        [IO.File]::WriteAllText($stdout,$readOut.GetAwaiter().GetResult(),[Text.UTF8Encoding]::new($false))
        [IO.File]::WriteAllText($stderr,$readErr.GetAwaiter().GetResult(),[Text.UTF8Encoding]::new($false))
        $timer.Stop()
        [ordered]@{
            name=$Name;executable=$Executable;executableSha256=$executableHash;command=$command;commandSha256=(Sha $command);
            exitCode=$process.ExitCode;timedOut=$timedOut;elapsedMs=$timer.ElapsedMilliseconds;
            stdout=$stdout;stderr=$stderr;stdoutSha256=(Sha $stdout);stderrSha256=(Sha $stderr)
        }
    } finally { $timer.Stop(); $process.Dispose() }
}

try {
    # The header template is the production lifecycle under test. Runtime wrappers
    # need the real Win32/module boundary and belong to the normal Core build.
    $compileSourceNames = @('Tests/definitive/StartupObservationReferenceTests.cpp')
    $sourceNames = $compileSourceNames + @(
        'Client/core/DefinitiveStartupObservationReference.h','Client/core/DefinitiveStartupObservationReference.cpp',
        'Client/core/CGameAdapterModule.h','Client/core/CGameAdapterModule.cpp',
        'Client/sdk/game/CGameStartupObservation.h',
        'Client/core/DefinitiveApplicationFrameReference.h','Client/core/DefinitiveApplicationFrameReference.cpp',
        'Client/core/DefinitiveClockReference.h','Client/core/DefinitiveClockReference.cpp',
        'Client/core/DefinitiveInputReference.h','Client/core/DefinitiveGuiFrontendReference.h',
        'Client/core/DefinitiveGuiSurfaceReference.h','Client/core/DefinitiveGuiStartupReference.h',
        'Client/sdk/game/CGameApplicationFrameObservation.h','Client/sdk/game/CGameClockService.h',
        'Client/sdk/game/CGameAdapter.h','Shared/sdk/CControllerState.h',
        'Shared/sdk/CControlStateSynthesis.h','Shared/sdk/CControllerActions.h',
        'Tests/definitive/test_startup_observation_reference.ps1')
    $record.sourceHashes = @($sourceNames | Sort-Object -Unique | ForEach-Object {
        [ordered]@{path=$_;sha256=(Sha (Join-Path $taskRoot $_))}
    })
    $env:TEMP = $scratchRoot
    $env:TMP = $scratchRoot
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
    $visualStudio = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    if (-not $visualStudio) { throw 'Visual Studio x64 tools are required.' }
    $developerCommand = Join-Path $visualStudio 'Common7/Tools/VsDevCmd.bat'
    $environment = & $env:ComSpec /d /s /c ('call "{0}" -no_logo -arch=x64 -host_arch=x64 >nul && set' -f $developerCommand)
    if ($LASTEXITCODE -ne 0) { throw 'MSVC x64 environment initialization failed.' }
    foreach ($entry in $environment) {
        if ($entry -match '^([^=]+)=(.*)$') { [Environment]::SetEnvironmentVariable($Matches[1],$Matches[2],'Process') }
    }
    # The developer environment can replace TEMP/TMP; keep compiler scratch owned.
    $env:TEMP = $scratchRoot
    $env:TMP = $scratchRoot
    $compiler = Join-Path $env:VCToolsInstallDir 'bin/Hostx64/x64/cl.exe'
    $record.executable = Join-Path $outputRoot 'startup_observation_reference_tests.exe'
    $arguments = @('/nologo','/std:c++17','/EHsc','/W4','/WX','/O2','/MD','/DNOMINMAX','/DWIN32_LEAN_AND_MEAN',
        ('/I'+(Join-Path $taskRoot 'Client/sdk')),('/I'+(Join-Path $taskRoot 'Shared/sdk')))
    $arguments += @($compileSourceNames | ForEach-Object { Join-Path $taskRoot $_ })
    $arguments += @(('/Fo'+$outputRoot+[IO.Path]::DirectorySeparatorChar),('/Fe'+$record.executable),'/link','/NOLOGO')
    $record.compile = RunBounded $compiler $arguments 'compile' 60000
    if ($record.compile.exitCode -ne 0 -or $record.compile.timedOut) { throw 'The x64 startup observation reference fixture compilation failed.' }
    $record.executableSha256 = Sha $record.executable
    $record.test = RunBounded $record.executable @() 'test' 15000
    $text = [IO.File]::ReadAllText($record.test.stdout)
    $summary = [regex]::Match($text.TrimEnd(), '(?:^|\r?\n)Passed (\d+) startup observation reference checks$')
    if ($summary.Success) { $record.checks = [int]$summary.Groups[1].Value }
    $completed = $text -match '(?m)^Production startup observation reference lifecycle fixture completed\r?$'
    if ($record.test.exitCode -ne 0 -or $record.test.timedOut -or -not $completed -or $record.checks -le 0) {
        throw 'The production startup observation reference lifecycle checks failed or completion markers were missing.'
    }
    $record.success = $true
} catch { AddFailure ($_.Exception.ToString()) }
finally {
    $env:TEMP = $previousTemp
    $env:TMP = $previousTmp
    foreach ($entry in $record.sourceHashes) {
        try {
            if ((Sha (Join-Path $taskRoot $entry.path)) -ne $entry.sha256) { $record.sourcesUnchanged = $false }
        } catch { $record.sourcesUnchanged = $false; AddFailure ($_.Exception.ToString()) }
    }
    $record.success = $record.success -and $record.sourcesUnchanged
    try {
        # Preserve logs and binaries; remove only files in this run's resolved scratch.
        $resolvedScratch = (Resolve-Path -LiteralPath $scratchRoot).Path.TrimEnd('\') + '\'
        $expectedParent = (Resolve-Path -LiteralPath (Join-Path $taskRoot 'Build/tmp')).Path.TrimEnd('\') + '\'
        if (-not $resolvedScratch.StartsWith($expectedParent,[StringComparison]::OrdinalIgnoreCase)) { throw 'Task scratch escaped Build/tmp.' }
        foreach ($file in @(Get-ChildItem -LiteralPath $scratchRoot -Recurse -File -Force)) {
            $resolvedFile = (Resolve-Path -LiteralPath $file.FullName).Path
            if (-not $resolvedFile.StartsWith($resolvedScratch,[StringComparison]::OrdinalIgnoreCase)) { throw 'Scratch file escaped the task directory.' }
            if (($file.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) { throw 'Task scratch contains a reparse-point file.' }
            Remove-Item -LiteralPath $resolvedFile -Force
        }
        $record.scratchFilesRemaining = @(Get-ChildItem -LiteralPath $scratchRoot -Recurse -File -Force).Count
    } catch { AddFailure ($_.Exception.ToString()) }
    $record.scratchDirectory = $scratchRoot
    $record.finishedAt = [DateTimeOffset]::Now.ToString('o')
    $record | ConvertTo-Json -Depth 9 | Set-Content -LiteralPath (Join-Path $outputRoot 'test-result.json') -Encoding utf8NoBOM
}
[ordered]@{success=$record.success;receipt=(Join-Path $outputRoot 'test-result.json');failure=$record.failure} | ConvertTo-Json -Compress
if (-not $record.success) { throw "Startup observation reference lifecycle checks failed; see $outputRoot" }
