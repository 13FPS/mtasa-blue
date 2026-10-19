#!/usr/bin/env pwsh
#requires -Version 7.0
[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$taskRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../..')).Path
$id = (Get-Date -Format 'yyyyMMdd-HHmmss') + '-' + [guid]::NewGuid().ToString('N')
$outputRoot = Join-Path $taskRoot ('Build/mta-clock-service/' + $id)
$scratchRoot = Join-Path $taskRoot ('Build/tmp/clock-service-' + $id)
[IO.Directory]::CreateDirectory($outputRoot) | Out-Null
[IO.Directory]::CreateDirectory($scratchRoot) | Out-Null
$previousTemp = $env:TEMP
$previousTmp = $env:TMP
$record = [ordered]@{schema='mta_clock_service_tests_v1';directory=$outputRoot;architecture='x64';success=$false;
    compileRuns=@();testRun=$null;checks=0;failure=$null;sourceHashes=@();sourcesUnchanged=$true;
    existingSdkClockImplementationTested=$false;productionNativeCallbackTested=$false;wrongExecutableRefusalVerified=$false;
    productionFreezePatchTested=$false;readOnlyGameTimeReaderTested=$false;nativeCGameTimingPrerequisiteTested=$false;nativeCGameTimingWriterTested=$false;nativeStoredFPSReaderTested=$false;executionArtifacts=@();
    gameLaunched=$false;gameAttached=$false;gameClockWritten=$false;gameHooksInstalled=$false;fullMtaReady=$false}
function Sha([string]$Path) { (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToLowerInvariant() }
function RunBounded([string]$Executable,[string[]]$Arguments,[string]$Name,[int]$DeadlineMs) {
    $stdout = Join-Path $outputRoot ($Name + '.log')
    $stderr = Join-Path $outputRoot ($Name + '-errors.log')
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
    $timedOut = $false
    try {
        if (-not $process.Start()) { throw "Failed to launch owned $Name process." }
        $readOut = $process.StandardOutput.ReadToEndAsync()
        $readErr = $process.StandardError.ReadToEndAsync()
        $timedOut = -not $process.WaitForExit($DeadlineMs)
        if ($timedOut) {
            # Only this newly launched compiler/test tree belongs to the run.
            $process.Kill($true)
            $process.WaitForExit()
        }
        [IO.File]::WriteAllText($stdout,$readOut.GetAwaiter().GetResult(),[Text.UTF8Encoding]::new($false))
        [IO.File]::WriteAllText($stderr,$readErr.GetAwaiter().GetResult(),[Text.UTF8Encoding]::new($false))
        [ordered]@{name=$Name;exitCode=$process.ExitCode;timedOut=$timedOut;stdout=$stdout;stderr=$stderr;
            stdoutSha256=(Sha $stdout);stderrSha256=(Sha $stderr)}
    } finally { $process.Dispose() }
}
try {
    if ([IntPtr]::Size -ne 8) { throw 'Clock service verification requires native x64 PowerShell.' }
    $nativeFiles = @(Get-ChildItem -LiteralPath (Join-Path $taskRoot 'Client/game_de') -File |
        Where-Object { $_.Name -like 'NativeClock*' -and $_.Extension -in @('.h','.cpp') })
    if (-not ($nativeFiles.Name -contains 'NativeClockService.cpp')) { throw 'Production native clock runtime is not ready.' }
    $sourceNames = @('Client/game_de/CClockDE.h','Client/game_de/CClockDE.cpp','Client/game_de/NativeImageAccess.h','Client/game_de/NativeImageAccess.cpp',
        'Client/game_de/NativeGameTimeReader.h','Client/game_de/NativeGameTimeReader.cpp','Client/sdk/game/CGameTimeService.h',
        'Client/game_de/NativeGameTimingReader.h','Client/game_de/NativeGameTimingReader.cpp','Client/game_de/NativeGameTimingProfile.h',
        'Client/game_de/NativeGameFPSProfile.h',
        'Client/game_de/NativeGameTimingAccess.h',
        'Client/game_de/CGameTimingDE.h','Client/game_de/CGameTimingDE.cpp','Client/sdk/game/CGame.h','Client/sdk/game/CWeaponInfo.h',
        'Client/sdk/game/Common.h','Shared/sdk/SString.h','Shared/sdk/SharedUtil.IntTypes.h','Shared/sdk/enums/SystemState.h',
        'Shared/sdk/enums/EntityType.h','Shared/sdk/enums/PedPieceTypes.h',
        'Client/game_de/NativeServiceAdmission.h',
        'Client/sdk/game/CClock.h','Client/sdk/game/CGameClockService.h','Client/sdk/game/CGameAdapter.h','Shared/sdk/CControllerState.h',
        'Shared/sdk/CControlStateSynthesis.h','Tests/definitive/ClockServiceTests.cpp','Tests/definitive/test_clock_service.ps1')
    $sourceNames += @($nativeFiles | ForEach-Object { [IO.Path]::GetRelativePath($taskRoot,$_.FullName).Replace('\','/') })
    $detoursNames = @('detours.cpp','modules.cpp','disasm.cpp','image.cpp','creatwth.cpp')
    $sourceNames += @($detoursNames + @('detours.h') | ForEach-Object { 'vendor/detours/4.0.1/src/' + $_ })
    $record.sourceHashes = @($sourceNames | Sort-Object -Unique | ForEach-Object {
        [ordered]@{path=$_;sha256=(Sha (Join-Path $taskRoot $_))}
    })
    $record.sourceSnapshotScope = 'Declared clock runtime, SDK and Detours inputs; not an independently recovered compiler-read trace.'
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
    $visualStudio = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    if (-not $visualStudio) { throw 'Visual Studio x64 C++ tools are required.' }
    $dev = Join-Path $visualStudio 'Common7/Tools/VsDevCmd.bat'
    $environment = & $env:ComSpec /d /s /c ('call "{0}" -no_logo -arch=x64 -host_arch=x64 >nul && set' -f $dev)
    if ($LASTEXITCODE -ne 0) { throw 'MSVC environment initialization failed.' }
    foreach ($entry in $environment) { if ($entry -match '^([^=]+)=(.*)$') { [Environment]::SetEnvironmentVariable($Matches[1],$Matches[2],'Process') } }
    $env:TEMP = $scratchRoot
    $env:TMP = $scratchRoot
    $compiler = Join-Path $env:VCToolsInstallDir 'bin/Hostx64/x64/cl.exe'
    $record.compiler = [ordered]@{path=$compiler;sha256=(Sha $compiler)}
    $common = @('/nologo','/std:c++17','/EHsc','/O2','/MD','/DNOMINMAX','/DWIN32_LEAN_AND_MEAN','/DDETOURS_MTA_VM_TRANSACTION_NODES',
        ('/I'+(Join-Path $taskRoot 'Client/sdk')),('/I'+(Join-Path $taskRoot 'Shared/sdk')),('/I'+(Join-Path $taskRoot 'vendor/detours/4.0.1/src')))
    $detoursSources = @($detoursNames | ForEach-Object { Join-Path $taskRoot ('vendor/detours/4.0.1/src/' + $_) })
    $detoursArguments = $common + @('/w','/c') + $detoursSources + @(('/Fo'+$outputRoot+[IO.Path]::DirectorySeparatorChar))
    $record.compileRuns += RunBounded $compiler $detoursArguments 'detours-compile' 60000
    if ($record.compileRuns[-1].exitCode -ne 0 -or $record.compileRuns[-1].timedOut) { throw 'Owned Detours compilation failed.' }
    $objects = @($detoursNames | ForEach-Object { Join-Path $outputRoot ([IO.Path]::ChangeExtension($_,'.obj')) })
    $executable = Join-Path $outputRoot 'clock_service_tests.exe'
    $runtimeDependencies = @($nativeFiles | Where-Object { $_.Extension -eq '.cpp' -and $_.Name -ne 'NativeClockService.cpp' } | ForEach-Object FullName)
    # The fixture includes the exact production runtime translation unit, like
    # the native movement tests. Its shipping executable gate stays unchanged.
    $arguments = $common + @('/W4','/WX',(Join-Path $PSScriptRoot 'ClockServiceTests.cpp'),
        (Join-Path $taskRoot 'Client/game_de/CClockDE.cpp'),(Join-Path $taskRoot 'Client/game_de/CGameTimingDE.cpp'),
        (Join-Path $taskRoot 'Client/game_de/NativeImageAccess.cpp')) +
        $runtimeDependencies + @(('/Fo'+$outputRoot+[IO.Path]::DirectorySeparatorChar),('/Fe'+$executable),'/link','bcrypt.lib','user32.lib') + $objects
    [ordered]@{compiler=$record.compiler;detoursArguments=$detoursArguments;arguments=$arguments} | ConvertTo-Json -Depth 5 |
        Set-Content -LiteralPath (Join-Path $outputRoot 'compile-command.json') -Encoding utf8NoBOM
    $record.compileRuns += RunBounded $compiler $arguments 'compile' 60000
    if ($record.compileRuns[-1].exitCode -ne 0 -or $record.compileRuns[-1].timedOut) { throw 'Clock SDK/runtime fixture compilation failed.' }
    $record.testRun = RunBounded $executable @($outputRoot) 'test' 30000
    $text = [IO.File]::ReadAllText($record.testRun.stdout) + [IO.File]::ReadAllText($record.testRun.stderr)
    if ($text -match 'Passed (\d+) production clock service checks') { $record.checks = [int]$Matches[1] }
    if ($record.testRun.exitCode -ne 0 -or $record.testRun.timedOut -or $record.checks -eq 0) { throw 'Clock service fixture failed.' }
    if (-not $text.Contains('Production native clock runtime fixture completed')) { throw 'Actual production native runtime fixture did not complete.' }
    $record.existingSdkClockImplementationTested = $true
    $record.productionNativeCallbackTested = $true
    $record.wrongExecutableRefusalVerified = $true
    $record.productionFreezePatchTested = $text.Contains('Native freeze patch capability: verified with production review gate')
    $record.readOnlyGameTimeReaderTested = $text.Contains('Production read-only game-time reader fixture completed')
    if (-not $record.readOnlyGameTimeReaderTested) { throw 'Production read-only native time reader fixture did not complete.' }
    $record.nativeCGameTimingPrerequisiteTested = $text.Contains('Production CGame native timing prerequisite fixture completed')
    if (-not $record.nativeCGameTimingPrerequisiteTested) { throw 'Actual native CGame timing prerequisite fixture did not complete.' }
    $record.nativeStoredFPSReaderTested = $text.Contains('Production native stored FPS reader fixture completed')
    if (-not $record.nativeStoredFPSReaderTested) { throw 'Actual native stored FPS reader fixture did not complete.' }
    $record.nativeCGameTimingWriterTested = $text.Contains('Production native CGame timing writer fixture completed')
    if (-not $record.nativeCGameTimingWriterTested) { throw 'Actual native CGame timing writer fixture did not complete.' }
    $record.nativeFixtureScope = 'Owned SEC_IMAGE with exact native pins and actual Detours, callback, native memory, protection and peer operations. This does not admit the test executable as GTA or execute the game clock update.'
    $record.executable = [ordered]@{path=$executable;bytes=(Get-Item -LiteralPath $executable).Length;sha256=(Sha $executable)}
    foreach ($entry in $record.sourceHashes) { if ((Sha (Join-Path $taskRoot $entry.path)) -ne $entry.sha256) { $record.sourcesUnchanged = $false } }
    $record.success = $record.sourcesUnchanged
} catch { $record.failure = $_.Exception.ToString() }
finally {
    $env:TEMP = $previousTemp
    $env:TMP = $previousTmp
    # Remove only disposable files in this task's resolved scratch directory.
    # Binaries, compiler commands, source pins and logs remain durable evidence.
    $resolvedScratch = [IO.Path]::GetFullPath($scratchRoot).TrimEnd('\') + '\'
    $expectedParent = [IO.Path]::GetFullPath((Join-Path $taskRoot 'Build/tmp')).TrimEnd('\') + '\'
    if (-not $resolvedScratch.StartsWith($expectedParent,[StringComparison]::OrdinalIgnoreCase)) { throw 'Task scratch escaped Build/tmp.' }
    foreach ($file in @(Get-ChildItem -LiteralPath $scratchRoot -Recurse -File)) {
        if (-not [IO.Path]::GetFullPath($file.FullName).StartsWith($resolvedScratch,[StringComparison]::OrdinalIgnoreCase)) { throw 'Scratch file escaped the task directory.' }
        Remove-Item -LiteralPath $file.FullName -Force
    }
    $record.scratchDirectory = $scratchRoot
    $record.scratchFilesRemaining = @(Get-ChildItem -LiteralPath $scratchRoot -Recurse -File).Count
    $record.executionArtifacts = @(Get-ChildItem -LiteralPath $outputRoot -File | Where-Object { $_.Extension -in @('.exe','.bin','.obj') } | ForEach-Object {
        [ordered]@{path=$_.FullName;bytes=$_.Length;sha256=(Sha $_.FullName)}
    })
    $record | ConvertTo-Json -Depth 10 | Set-Content -LiteralPath (Join-Path $outputRoot 'test-result.json') -Encoding utf8NoBOM
}
[ordered]@{success=$record.success;receipt=(Join-Path $outputRoot 'test-result.json');checks=$record.checks;failure=$record.failure} | ConvertTo-Json -Compress
if (-not $record.success) { throw "Clock service checks failed; see $outputRoot" }
