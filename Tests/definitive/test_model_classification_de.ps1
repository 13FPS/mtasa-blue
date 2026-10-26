#!/usr/bin/env pwsh
#requires -Version 7.0
# Compile actual model SDK archive/classification overrides against an owned CPU fixture.
# This executes no native code, game allocation, wrapper factory or native registry owner.
[CmdletBinding()]
param(
    [ValidateSet('Full','ArchiveExceptions','VehicleType')]
    [string]$Scope = 'Full'
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$taskRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../..')).Path
$id = (Get-Date -Format 'yyyyMMdd-HHmmss') + '-' + [guid]::NewGuid().ToString('N')
$outputRoot = Join-Path $taskRoot ('Build/mta-model-classification-sdk/' + $Scope + '/' + $id)
$scratchRoot = Join-Path $taskRoot ('Build/tmp/model-classification-sdk-' + $Scope + '-' + $id)
[IO.Directory]::CreateDirectory($outputRoot) | Out-Null
[IO.Directory]::CreateDirectory($scratchRoot) | Out-Null
$previousEnvironment = @{}
foreach ($name in @('TEMP','TMP','VCToolsInstallDir','INCLUDE','LIB','LIBPATH','PATH')) {
    $previousEnvironment[$name] = [Environment]::GetEnvironmentVariable($name,'Process')
}
$record = [ordered]@{
    schema='mta_model_classification_sdk_tests_v1';directory=$outputRoot;success=$false;
    recordedAt=[DateTimeOffset]::Now.ToString('o');sourceHashes=@();sourcesUnchanged=$true;
    results=@();actualModelClassificationSdkTested=$false;actualModelTypeSdkTested=$false;actualModelArchiveSdkTested=$false;actualModelArchiveExceptionSdkTested=$false;actualModelVehicleTypeSdkTested=$false;failure=$null;scratchDirectory=$scratchRoot;scratchFilesRemaining=$null;scratchRemoved=$false;
    selectedScope=$Scope;scope='VehicleType executes only actual GetVehicleType gate, ordered archive/type/subtype, raw DWORD-to-BYTE and refusal checks on x86/x64. ArchiveExceptions remains archive-only; Full preserves its existing cases and includes both focused groups on x64. No native/reporting matrix replay or native model factory.';
    ownedBuffersOnly=$true;nativeEngineCodeExecuted=$false;gameLaunched=$false;gameAttached=$false;
    foreignProcessAccessed=$false;nativeLifecycleReady=$false;fullMtaReady=$false;
    clientDllLoaded=$false;installationChanged=$false
}

function Sha([string]$Path) { (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToLowerInvariant() }
function AddFailure([string]$Message) {
    $record.success = $false
    if ($null -eq $record.failure) { $record.failure = $Message }
    else { $record.failure += [Environment]::NewLine + $Message }
}
function RunOwned([string]$Executable,[string[]]$Arguments,[string]$Directory,[string]$Name,[int]$DeadlineMs) {
    $stdout = Join-Path $Directory ($Name + '.log')
    $stderr = Join-Path $Directory ($Name + '-errors.log')
    $command = Join-Path $Directory ($Name + '-command.json')
    $executableHash = Sha $Executable
    [ordered]@{
        executable=$Executable;sha256=$executableHash;arguments=$Arguments;workingDirectory=$Directory;
        deadlineMs=$DeadlineMs;temporaryDirectory=$scratchRoot
    } | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath $command -Encoding utf8NoBOM
    $info = [Diagnostics.ProcessStartInfo]::new()
    $info.FileName = $Executable
    $info.WorkingDirectory = $Directory
    $info.UseShellExecute = $false
    $info.CreateNoWindow = $true
    $info.RedirectStandardOutput = $true
    $info.RedirectStandardError = $true
    $info.Environment['TEMP'] = $scratchRoot
    $info.Environment['TMP'] = $scratchRoot
    foreach ($argument in $Arguments) { $info.ArgumentList.Add($argument) }
    $process = [Diagnostics.Process]::new()
    $process.StartInfo = $info
    $timer = [Diagnostics.Stopwatch]::StartNew()
    try {
        if (-not $process.Start()) { throw "Could not start owned $Name process." }
        $readOut = $process.StandardOutput.ReadToEndAsync()
        $readErr = $process.StandardError.ReadToEndAsync()
        $timedOut = -not $process.WaitForExit($DeadlineMs)
        if ($timedOut) {
            # Only this run's actual child process tree is eligible for termination.
            $process.Kill($true)
            if (-not $process.WaitForExit(10000)) { throw "Owned $Name process did not terminate after timeout." }
        }
        [IO.File]::WriteAllText($stdout,$readOut.GetAwaiter().GetResult(),[Text.UTF8Encoding]::new($false))
        [IO.File]::WriteAllText($stderr,$readErr.GetAwaiter().GetResult(),[Text.UTF8Encoding]::new($false))
        $timer.Stop()
        [ordered]@{
            name=$Name;executable=$Executable;executableSha256=$executableHash;
            command=$command;commandSha256=(Sha $command);exitCode=$process.ExitCode;
            timedOut=$timedOut;elapsedMs=$timer.ElapsedMilliseconds;
            stdout=$stdout;stderr=$stderr;stdoutSha256=(Sha $stdout);stderrSha256=(Sha $stderr)
        }
    } finally { $timer.Stop(); $process.Dispose() }
}

try {
    $sourceNames = @(
        'Client/game_de/CModelInfoDE.h','Client/game_de/CModelInfoDE.cpp','Client/game_de/NativeStreamingModelAccess.h',
        'Client/game_de/NativeModelLoadedReader.h','Client/game_de/NativeModelLoadedProfile.h','Client/game_de/NativeModelBankReader.h',
        'Client/game_de/NativePedPoolReader.h','Client/game_de/NativePedPoolProfile.h',
        'Client/sdk/game/CStreaming.h','Client/sdk/game/CModelRequestPolicy.h','Client/sdk/game/CGameClockService.h','Client/sdk/game/CGameAdapter.h',
        'Shared/sdk/SharedUtil.Defines.h','Shared/sdk/SharedUtil.IntTypes.h','Shared/sdk/CVector.h',
        'Tests/definitive/ModelClassificationDETests.cpp','Tests/definitive/test_model_classification_de.ps1')
    $sourceNames += @(
        'Client/sdk/game/CModelInfo.h',
        'Client/sdk/game/Common.h',
        'Shared/sdk/enums/VehicleDummies.h',
        'Shared/sdk/enums/ResizableVehicleWheelGroup.h',
        'Client/game_de/NativeModelClassificationAccess.h',
        'Client/game_de/NativeModelClassificationCalls.h',
        'Client/game_de/NativeModelClassificationProfile.h'
    )
    $sourceNames += @('Client/game_de/NativeModelTypeLeaf.h')
    $sourceNames += @(
        'Client/game_de/NativeStreamingModelGroupsReader.h',
        'Client/game_de/NativeStreamingModelGroupsProfile.h'
    )
    $sourceNames += @(
        'Build/definitive-recovery/mta-model-archive-exceptions-sdk-20261026/model-archive-exception-source-ready.json',
        'Build/definitive-recovery/mta-model-archive-exceptions-sdk-tests-20261026/model-archive-exceptions-sdk-tests-source-ready.json',
        'Build/definitive-recovery/mta-model-vehicle-subtype-native-20261026/model-vehicle-subtype-api-source-ready.json',
        'Build/definitive-recovery/mta-model-vehicle-type-sdk-20261026/model-vehicle-type-sdk-source-ready.json',
        'Build/definitive-recovery/mta-model-vehicle-type-sdk-tests-20261026/model-vehicle-type-sdk-tests-source-ready.json'
    )
    $record.sourceHashes = @($sourceNames | Sort-Object -Unique | ForEach-Object {
        [ordered]@{path=$_;sha256=(Sha (Join-Path $taskRoot $_))}
    })
    $env:TEMP = $scratchRoot
    $env:TMP = $scratchRoot
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
    $discovery = RunOwned $vswhere @('-latest','-products','*','-requires','Microsoft.VisualStudio.Component.VC.Tools.x86.x64','-property','installationPath') $outputRoot 'vswhere' 10000
    $visualStudio = [IO.File]::ReadAllText($discovery.stdout).Trim()
    if ($discovery.exitCode -ne 0 -or $discovery.timedOut -or -not $visualStudio) { throw 'Visual Studio x86/x64 tools are required.' }
    $developerCommand = Join-Path $visualStudio 'Common7/Tools/VsDevCmd.bat'
    $architectures = @(if ($Scope -ne 'Full') { 'x86'; 'x64' } else { 'x64' })
    foreach ($architecture in $architectures) {
        $directory = Join-Path $outputRoot $architecture
        [IO.Directory]::CreateDirectory($directory) | Out-Null
        # Capture toolchain paths only; a full environment dump could contain
        # unrelated credentials and does not belong in durable build evidence.
        # Keep cmd's quoting inside an owned batch file. ProcessStartInfo's
        # argument escaping otherwise treats the quoted CALL path as C argv.
        $environmentScript = Join-Path $directory 'capture-msvc.cmd'
        @('@echo off',
            ('call "{0}" -no_logo -arch={1} -host_arch=x64 >nul' -f $developerCommand,$architecture),
            'if errorlevel 1 exit /b 1',
            'set VCToolsInstallDir','set INCLUDE','set LIB','set PATH','exit /b 0') |
            Set-Content -LiteralPath $environmentScript -Encoding ascii
        $environmentResult = RunOwned $env:ComSpec @('/d','/s','/c','capture-msvc.cmd') $directory 'msvc-environment' 30000
        $environmentResult['script'] = $environmentScript
        $environmentResult['scriptSha256'] = Sha $environmentScript
        if ($environmentResult.exitCode -ne 0 -or $environmentResult.timedOut) { throw "$architecture MSVC environment initialization failed." }
        foreach ($entry in [IO.File]::ReadAllLines($environmentResult.stdout)) {
            if ($entry -match '^(VCToolsInstallDir|INCLUDE|LIB|LIBPATH|PATH)=(.*)$') {
                [Environment]::SetEnvironmentVariable($Matches[1],$Matches[2],'Process')
            }
        }
        $env:TEMP = $scratchRoot
        $env:TMP = $scratchRoot
        $compiler = Join-Path $env:VCToolsInstallDir "bin/Hostx64/$architecture/cl.exe"
        $executable = Join-Path $directory 'model_classification_sdk_tests.exe'
        $arguments = @('/nologo','/std:c++17','/EHsc','/W4','/WX','/O2','/MD','/DNOMINMAX','/DWIN32_LEAN_AND_MEAN',
            ('/I'+(Join-Path $taskRoot 'Shared/sdk')),('/I'+(Join-Path $taskRoot 'Client/sdk')),
            (Join-Path $PSScriptRoot 'ModelClassificationDETests.cpp'),(Join-Path $taskRoot 'Client/game_de/CModelInfoDE.cpp'),
            ('/Fo'+$scratchRoot+[IO.Path]::DirectorySeparatorChar),('/Fd'+(Join-Path $scratchRoot 'compiler.pdb')),('/Fe'+$executable))
        $result = [ordered]@{architecture=$architecture;environment=$environmentResult;compile=$null;test=$null;
            checks=0;pointerBits=$null;executable=$executable;executableSha256=$null;actualModelClassificationSdkTested=$false;actualModelTypeSdkTested=$false;actualModelArchiveSdkTested=$false;actualModelArchiveExceptionSdkTested=$false;actualModelVehicleTypeSdkTested=$false}
        $record.results += $result
        $result.compile = RunOwned $compiler $arguments $directory 'compile' 60000
        if ($result.compile.exitCode -ne 0 -or $result.compile.timedOut) { throw "$architecture actual MTA model-classification SDK fixture compilation failed." }
        $result.executableSha256 = Sha $executable
        $testArguments = @(if ($Scope -eq 'ArchiveExceptions') { '--archive-exceptions' } elseif ($Scope -eq 'VehicleType') { '--vehicle-type' })
        $result.test = RunOwned $executable $testArguments $directory 'test' 10000
        $text = [IO.File]::ReadAllText($result.test.stdout)
        $summaryPattern = if ($Scope -eq 'VehicleType') { '(?:^|\r?\n)Passed (\d+) actual MTA model vehicle-type SDK checks \((\d+)-bit\)$' }
                          elseif ($Scope -eq 'ArchiveExceptions') { '(?:^|\r?\n)Passed (\d+) actual MTA model archive-exception SDK checks \((\d+)-bit\)$' }
                          else { '(?:^|\r?\n)Passed (\d+) actual MTA model-classification SDK checks \((\d+)-bit\)$' }
        $summary = [regex]::Match($text.TrimEnd(), $summaryPattern)
        if ($summary.Success) {
            $result.checks = [int]$summary.Groups[1].Value
            $result.pointerBits = [int]$summary.Groups[2].Value
        }
        $result.actualModelClassificationSdkTested = $text.Contains('Production MTA model classification SDK fixture completed')
        $result.actualModelTypeSdkTested = $text.Contains('Production MTA model type SDK fixture completed')
        $result.actualModelArchiveSdkTested = $text.Contains('Production MTA model archive SDK fixture completed')
        $result.actualModelArchiveExceptionSdkTested = $text.Contains('Production MTA model archive exception SDK fixture completed')
        $result.actualModelVehicleTypeSdkTested = $text.Contains('Production MTA model vehicle type SDK fixture completed')
        if ($Scope -eq 'Full' -and -not ($result.actualModelClassificationSdkTested -and $result.actualModelTypeSdkTested -and $result.actualModelArchiveSdkTested)) {
            throw 'Default Full classification/type/archive success coverage did not execute.'
        }
        $expectedBits = if ($architecture -eq 'x86') { 32 } else { 64 }
        $requiredCoverage = if ($Scope -eq 'VehicleType') { $result.actualModelVehicleTypeSdkTested }
                            elseif ($Scope -eq 'ArchiveExceptions') { $result.actualModelArchiveExceptionSdkTested }
                            else { $result.actualModelArchiveExceptionSdkTested -and $result.actualModelVehicleTypeSdkTested }
        if ($result.test.exitCode -ne 0 -or $result.test.timedOut -or $result.checks -le 0 -or $result.pointerBits -ne $expectedBits -or -not $requiredCoverage) {
            throw "$architecture actual MTA model-classification SDK checks failed or summary/pointer-width evidence was missing."
        }
    }
    $record.actualModelArchiveExceptionSdkTested = @($record.results | Where-Object { -not $_.actualModelArchiveExceptionSdkTested }).Count -eq 0
    $record.actualModelVehicleTypeSdkTested = @($record.results | Where-Object { -not $_.actualModelVehicleTypeSdkTested }).Count -eq 0
    $requiredCoverage = if ($Scope -eq 'VehicleType') { $record.actualModelVehicleTypeSdkTested }
                        elseif ($Scope -eq 'ArchiveExceptions') { $record.actualModelArchiveExceptionSdkTested }
                        else { $record.actualModelArchiveExceptionSdkTested -and $record.actualModelVehicleTypeSdkTested }
    $record.success = $record.results.Count -eq $architectures.Count -and $requiredCoverage
    $record.actualModelArchiveSdkTested = $record.success -and $Scope -eq 'Full' -and $record.results[0].actualModelArchiveSdkTested
    $record.actualModelClassificationSdkTested = $record.success -and $Scope -eq 'Full' -and $record.results[0].actualModelClassificationSdkTested
    $record.actualModelTypeSdkTested = $record.success -and $Scope -eq 'Full' -and $record.results[0].actualModelTypeSdkTested
} catch { AddFailure ($_.Exception.ToString()) }
finally {
    foreach ($name in $previousEnvironment.Keys) {
        [Environment]::SetEnvironmentVariable($name,$previousEnvironment[$name],'Process')
    }
    foreach ($entry in $record.sourceHashes) {
        try {
            if ((Sha (Join-Path $taskRoot $entry.path)) -ne $entry.sha256) { $record.sourcesUnchanged = $false }
        } catch { $record.sourcesUnchanged = $false; AddFailure ($_.Exception.ToString()) }
    }
    $record.success = $record.success -and $record.sourcesUnchanged
    try {
        # Compiler scratch is flat and disposable. Preserve any unexpected
        # directory/link rather than recursing into paths not created here.
        $resolvedScratch = (Resolve-Path -LiteralPath $scratchRoot).Path.TrimEnd('\') + '\'
        $expectedParent = (Resolve-Path -LiteralPath (Join-Path $taskRoot 'Build/tmp')).Path.TrimEnd('\') + '\'
        if (-not $resolvedScratch.StartsWith($expectedParent,[StringComparison]::OrdinalIgnoreCase) -or
            ((Get-Item -LiteralPath $scratchRoot -Force).Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) {
            throw 'Task scratch escaped Build/tmp or became a reparse point.'
        }
        $entries = @(Get-ChildItem -LiteralPath $scratchRoot -Force)
        foreach ($entry in $entries) {
            if ($entry.PSIsContainer -or ($entry.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) {
                throw 'Unexpected scratch directory/link preserved; cleanup is incomplete.'
            }
            $resolvedFile = (Resolve-Path -LiteralPath $entry.FullName).Path
            if (-not $resolvedFile.StartsWith($resolvedScratch,[StringComparison]::OrdinalIgnoreCase)) { throw 'Scratch file escaped the task directory.' }
        }
        foreach ($entry in $entries) { Remove-Item -LiteralPath $entry.FullName -Force }
        $record.scratchFilesRemaining = @(Get-ChildItem -LiteralPath $scratchRoot -Force).Count
        [IO.Directory]::Delete($scratchRoot,$false)
        $record.scratchRemoved = $true
    } catch { AddFailure ($_.Exception.ToString()) }
    $record.finishedAt = [DateTimeOffset]::Now.ToString('o')
    $record | ConvertTo-Json -Depth 10 | Set-Content -LiteralPath (Join-Path $outputRoot 'test-result.json') -Encoding utf8NoBOM
}
[ordered]@{success=$record.success;receipt=(Join-Path $outputRoot 'test-result.json');failure=$record.failure} | ConvertTo-Json -Compress
if (-not $record.success) { throw "Actual MTA model archive/classification SDK checks failed; see $outputRoot" }
