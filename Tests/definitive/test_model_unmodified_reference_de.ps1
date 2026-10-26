#!/usr/bin/env pwsh
#requires -Version 7.0
# Exercise the actual acquire-side SDK layer through inherited request policy and real streaming dispatch.
[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$taskRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../..')).Path
$id = (Get-Date -Format 'yyyyMMdd-HHmmss') + '-' + [guid]::NewGuid().ToString('N')
$outputRoot = Join-Path $taskRoot ('Build/mta-model-unmodified-unload-sdk/' + $id)
$scratchRoot = Join-Path $taskRoot ('Build/tmp/model-unmodified-unload-sdk-' + $id)
[IO.Directory]::CreateDirectory($outputRoot) | Out-Null
[IO.Directory]::CreateDirectory($scratchRoot) | Out-Null
$previousEnvironment = @{}
foreach ($name in @('TEMP','TMP','VCToolsInstallDir','INCLUDE','LIB','LIBPATH','PATH')) {
    $previousEnvironment[$name] = [Environment]::GetEnvironmentVariable($name,'Process')
}
$record = [ordered]@{
    schema='mta_model_unmodified_reference_sdk_tests_v1';directory=$outputRoot;success=$false;
    recordedAt=[DateTimeOffset]::Now.ToString('o');sourceHashes=@();sourcesUnchanged=$true;
    results=@();failure=$null;scratchDirectory=$scratchRoot;scratchFilesRemaining=$null;scratchRemoved=$false;
    scope='Preserve the prior62 unmodified reference/Core composition checks and add actual final UnloadUnused one-shot current-token dispatch. Owner boundary replies prove false/true forwarding, all Clock refusals including written output, and no CPU/pending/readiness/request/load/acquire/release/renew/report substitution. No native count model or native removal implementation in this SDK fixture.';
    ownedBuffersOnly=$true;nativeEngineCodeExecuted=$false;gameLaunched=$false;gameAttached=$false;
    completeModelInfoReady=$false;fullMtaReady=$false;installationChanged=$false;
    actualModelUnmodifiedReferenceSdkTested=$false;actualUnmodifiedDiagnosticsCompositionSdkTested=$false;actualModelUnmodifiedUnloadUnusedSdkTested=$false

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

function InterfaceMethods([string]$RelativePath) {
    $text = [IO.File]::ReadAllText((Join-Path $taskRoot $RelativePath))
    $text = [regex]::Replace($text,'(?s)/\*.*?\*/|//[^\r\n]*','')
    foreach ($match in [regex]::Matches($text,'virtual\s+(?<signature>[^;{}]+);')) {
        $signature = [regex]::Replace($match.Groups['signature'].Value,'\s*=\s*0\s*$','').Trim()
        $name = [regex]::Match($signature,'(?<name>\w+)\s*\(').Groups['name'].Value
        if (-not $name) { throw "Cannot derive rejecting fixture method from $signature" }
        # Defaults are caller syntax, not part of an overriding signature.
        $signature = [regex]::Replace($signature,'\s*=\s*[^,)]+','')
        [ordered]@{name=$name;signature=([regex]::Replace($signature,'\s+',' '))}
    }
}

try {
    $productionSources=@('Client/game_de/CModelInfoUnmodifiedDE.cpp','Client/game_de/CModelInfoDiagnosticsDE.cpp','Client/game_de/CModelInfoReferenceDE.cpp','Client/game_de/CModelInfoRequestDE.cpp','Client/game_de/CModelInfoDE.cpp','Client/game_de/CStreamingDE.cpp')
    $sourceNames=$productionSources+@('Tests/definitive/ModelUnmodifiedReferenceDETests.cpp','Tests/definitive/test_model_unmodified_reference_de.ps1',
        'Tests/definitive/test_model_request_de.ps1',
        'Tests/definitive/ModelDiagnosticsDETests.cpp','Tests/definitive/test_model_diagnostics_de.ps1',
        'Build/definitive-recovery/mta-model-unmodified-reference-20261026/model-unmodified-family-api-source-ready.json',
        'Build/definitive-recovery/mta-model-unmodified-reference-20261026/model-unmodified-reference-sdk-source-ready.json',
        'Build/definitive-recovery/mta-model-unmodified-reference-sdk-tests-20261026/model-unmodified-reference-sdk-tests-source-ready.json',
        'Build/definitive-recovery/mta-model-unmodified-diagnostics-composition-20261026/model-unmodified-diagnostics-composition-source-ready.json',
        'Build/definitive-recovery/mta-model-unmodified-diagnostics-sdk-tests-20261026/model-unmodified-diagnostics-sdk-tests-source-ready.json',
        'Build/definitive-recovery/mta-model-unused-family-native-20261026/model-unused-family-api-source-ready.json',
        'Build/definitive-recovery/mta-model-unmodified-unload-sdk-20261026/model-unmodified-unload-sdk-source-ready.json',
        'Build/definitive-recovery/mta-model-unmodified-unload-sdk-tests-20261026/model-unmodified-unload-sdk-tests-source-ready.json')
    $pending=[Collections.Generic.Queue[string]]::new()
    $seen=[Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
    foreach($name in $sourceNames){$pending.Enqueue($name)}
    while($pending.Count){
        $name=$pending.Dequeue();if(-not $seen.Add($name)){continue}
        $path=[IO.Path]::GetFullPath((Join-Path $taskRoot $name))
        if(-not $path.StartsWith($taskRoot+'\',[StringComparison]::OrdinalIgnoreCase) -or $path -match '(?i)(?:^|[\\/])r-engineer(?:[\\/]|$)'){throw 'SDK graph left allowed scope'}
        if(-not (Test-Path -LiteralPath $path -PathType Leaf)){throw ('Missing actual source: '+$name)}
        foreach($match in [regex]::Matches([IO.File]::ReadAllText($path),'(?m)^\s*#\s*include\s*[<"](?<path>[^>"\n]+)[>"]')){
            $include=$match.Groups['path'].Value
            foreach($candidate in @((Join-Path (Split-Path $path) $include),(Join-Path $taskRoot ('Shared/sdk/'+$include)),(Join-Path $taskRoot ('Client/sdk/'+$include)),(Join-Path $taskRoot ('vendor/'+$include)))){
                if(Test-Path -LiteralPath $candidate -PathType Leaf){$pending.Enqueue([IO.Path]::GetRelativePath($taskRoot,[IO.Path]::GetFullPath($candidate)).Replace('\','/'));break}
            }
        }
    }
    $record.sourceHashes=@($seen|Sort-Object|ForEach-Object{[ordered]@{path=$_;sha256=(Sha (Join-Path $taskRoot $_))}})
    $modelProvided = @('GetInterface','GetRwObject','GetModel','GetModelType','IsVehicle','IsAllocatedInArchive','IsBoat','IsCar','IsPlane','IsHeli',
        'Request','IsLoaded','ModelAddRef','RemoveRef','GetRefCount','IsValid','SetCustomModel','RestoreOriginalModel','SetColModel','RestoreColModel','MakeCustomModel','UnloadUnused')
    $streamingProvided = @('RequestSpecialModel','LoadSceneCollision','GetModelStreamingFlags','RequestModel','RemoveModel','HasModelLoaded','LoadAllRequestedModels','LoadScene',
        'GetNumPedsLoaded','IsModelInLoadedPedGroup','GetNumLoadedVehicles','IsModelInLoadedVehicleGroup')
    $gameMethods = @(InterfaceMethods 'Client/sdk/game/CGame.h' | Where-Object { $_.name -notin @('GetStreaming','GetBaseIDforTXD','IsASyncLoadingEnabled') })
    $modelMethods = @(InterfaceMethods 'Client/sdk/game/CModelInfo.h' | Where-Object { $modelProvided -notcontains $_.name })
    $streamingMethods = @(InterfaceMethods 'Client/sdk/game/CStreaming.h' | Where-Object { $streamingProvided -notcontains $_.name })
    $coreMethods = @(InterfaceMethods 'Client/sdk/core/CCoreInterface.h' | Where-Object { $_.name -ne 'LogEvent' })
    $testedMethods = @('Request','IsLoaded','ModelAddRef','RemoveRef','GetRefCount')
    $headerText = [IO.File]::ReadAllText((Join-Path $taskRoot 'Client/game_de/CModelInfoRequestDE.h'))
    foreach ($method in $testedMethods) {
        if ($headerText -notmatch ('\b'+[regex]::Escape($method)+'\s*\([^;{}]*\)\s*override\s*;')) { throw "Actual model SDK override is missing: $method" }
    }
    $generated = Join-Path $scratchRoot 'ModelUnmodifiedReferenceFixture.generated.h'
    $lines = @('#pragma once','#pragma warning(push)','#pragma warning(disable : 4100)','namespace fixture {',
        'class CoreFixtureBase : public CCoreInterface { public:')
    $lines += @($coreMethods | ForEach-Object { $_.signature + ' override { Unqualified(); }' })
    $lines += @('};','class GameFixtureBase : public CGame { public:')
    $lines += @($gameMethods | ForEach-Object { $_.signature + ' override { Unqualified(); }' })
    $lines += @('};','class ModelFixtureBase : public mta::game_de::CModelInfoUnmodifiedDE { public:',
        'ModelFixtureBase(std::uint32_t modelKey, mta::game_de::NativeModelClassificationAccess& classification, CGame& game, mta::game_de::NativeModelBackingReferenceAccess& referenceOwner, const mta::game_de::NativeModelBackingReferenceToken& token, CCoreInterface& core) : CModelInfoUnmodifiedDE(modelKey,classification,game,referenceOwner,token,core) {}')
    $lines += @($modelMethods | ForEach-Object { $_.signature + ' override { Unqualified(); }' })
    $lines += @('};','class StreamingFixtureBase : public mta::game_de::CStreamingDE { public:',
        'explicit StreamingFixtureBase(mta::game_de::NativeStreamingModelAccess& owner) noexcept : CStreamingDE(owner) {}')
    $lines += @($streamingMethods | ForEach-Object { $_.signature + ' override { Unqualified(); }' })
    $lines += @('};','}','#pragma warning(pop)')
    $lines | Set-Content -LiteralPath $generated -Encoding utf8NoBOM
    $durableGenerated = Join-Path $outputRoot 'ModelUnmodifiedReferenceFixture.generated.h'
    Copy-Item -LiteralPath $generated -Destination $durableGenerated
    $record.generatedFixture = [ordered]@{path=$durableGenerated;sha256=(Sha $generated);gameRejectingOverrides=$gameMethods.Count;
        modelRejectingOverrides=$modelMethods.Count;streamingRejectingOverrides=$streamingMethods.Count;coreRejectingOverrides=$coreMethods.Count;
        productionMethodsNotOverridden=$modelProvided;testedMethods=$testedMethods;reportingMethodsMasked=$false;actualReadinessInherited=$true;unloadUnusedInherited=$true;
        inheritedReportingMethods=@('ReportMissingAnimationModel','ReportBlockingModelLoad','ReportRequestingAnimationModel')}

    $referenceHeader = [IO.File]::ReadAllText((Join-Path $taskRoot 'Client/game_de/CModelInfoReferenceDE.h'))
    foreach ($method in $testedMethods) {
        if ($referenceHeader -match ('\b'+[regex]::Escape($method)+'\s*\(')) { throw "Acquire layer must inherit actual public policy method: $method" }
    }
    if ($referenceHeader -notmatch 'DoIsLoaded\s*\(\s*\)\s*(?:final\s+)?override' -or $referenceHeader -notmatch 'IncrementNativeModelInterfaceReference\s*\(\s*\)\s*(?:final\s+)?override') { throw 'Acquire layer must provide only its readiness/reference protected hooks.' }
    $unmodifiedHeader = [IO.File]::ReadAllText((Join-Path $taskRoot 'Client/game_de/CModelInfoUnmodifiedDE.h'))
    if ($unmodifiedHeader -notmatch 'CModelInfoUnmodifiedDE\s*:\s*public\s+CModelInfoDiagnosticsDE') { throw 'Unmodified layer must compose the actual diagnostics base.' }
    if ($unmodifiedHeader -notmatch 'UnloadUnused\s*\(\s*\)\s*final\s+override') { throw 'Actual final UnloadUnused must remain inherited by the fixture.' }
    foreach ($method in @('SetCustomModel','RestoreOriginalModel','SetColModel','RestoreColModel','MakeCustomModel')) {
        if ($unmodifiedHeader -notmatch ('\b'+$method+'\s*\([^;{}]*\)\s*final\s+override')) { throw ('Unmodified replacement refusal must be final: '+$method) }
    }
    if ($unmodifiedHeader -notmatch 'const\s+ReplacementState\s+replacements\s*\{\s*\}') { throw 'Unmodified layer must own initialized-empty immutable replacement state.' }
    if ($modelMethods.Count -ne 70 -or $streamingMethods.Count -ne 10) { throw 'Actual model22/70 or streaming12/10 fixture partition changed' }

    $env:TEMP = $scratchRoot
    $env:TMP = $scratchRoot
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
    $discovery = RunOwned $vswhere @('-latest','-products','*','-requires','Microsoft.VisualStudio.Component.VC.Tools.x86.x64','-property','installationPath') $outputRoot 'vswhere' 10000
    $visualStudio = [IO.File]::ReadAllText($discovery.stdout).Trim()
    if ($discovery.exitCode -ne 0 -or $discovery.timedOut -or -not $visualStudio) { throw 'Visual Studio x86/x64 tools are required.' }
    $developerCommand = Join-Path $visualStudio 'Common7/Tools/VsDevCmd.bat'
    foreach ($architecture in @('x86','x64')) {
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
        $executable = Join-Path $directory 'model_unmodified_reference_sdk_tests.exe'
        $arguments = @('/nologo','/std:c++17','/EHsc','/W4','/WX','/O2','/MD','/DMTA_DEBUG','/DWIN32','/DNOMINMAX','/DWIN32_LEAN_AND_MEAN',
            '/D_CRT_SECURE_NO_WARNINGS','/D_SCL_SECURE_NO_WARNINGS','/D_CRT_NONSTDC_NO_DEPRECATE','/D_TIMESPEC_DEFINED',
            ('/I'+(Join-Path $taskRoot 'Shared/sdk')),('/I'+(Join-Path $taskRoot 'Client/sdk')),('/I'+(Join-Path $taskRoot 'vendor')),('/I'+$scratchRoot),
            (Join-Path $PSScriptRoot 'ModelUnmodifiedReferenceDETests.cpp'),(Join-Path $taskRoot 'Client/game_de/CModelInfoUnmodifiedDE.cpp'),(Join-Path $taskRoot 'Client/game_de/CModelInfoDiagnosticsDE.cpp'),(Join-Path $taskRoot 'Client/game_de/CModelInfoReferenceDE.cpp'),(Join-Path $taskRoot 'Client/game_de/CModelInfoDE.cpp'),
            (Join-Path $taskRoot 'Client/game_de/CModelInfoRequestDE.cpp'),(Join-Path $taskRoot 'Client/game_de/CStreamingDE.cpp'),
            ('/Fo'+$scratchRoot+[IO.Path]::DirectorySeparatorChar),('/Fd'+(Join-Path $scratchRoot 'compiler.pdb')),('/Fe'+$executable),'/link','bcrypt.lib')
        $result = [ordered]@{architecture=$architecture;environment=$environmentResult;compile=$null;test=$null;
            checks=0;pointerBits=$null;executable=$executable;executableSha256=$null;actualModelUnmodifiedReferenceSdkTested=$false;actualUnmodifiedDiagnosticsCompositionSdkTested=$false;actualModelUnmodifiedUnloadUnusedSdkTested=$false}
        $record.results += $result
        $result.compile = RunOwned $compiler $arguments $directory 'compile' 60000
        if ($result.compile.exitCode -ne 0 -or $result.compile.timedOut) { throw "$architecture actual model backing reference SDK fixture compilation failed." }
        $result.executableSha256 = Sha $executable
        $result.test = RunOwned $executable @() $directory 'test' 10000
        $text = [IO.File]::ReadAllText($result.test.stdout)
        $summary = [regex]::Match($text.TrimEnd(), '(?:^|\r?\n)Passed (\d+) actual MTA model unmodified reference SDK checks \((\d+)-bit; unmodified cycles\)$')
        if ($summary.Success) {
            $result.checks = [int]$summary.Groups[1].Value
            $result.pointerBits = [int]$summary.Groups[2].Value
        }
        $result.actualModelUnmodifiedReferenceSdkTested=$text.Contains('Actual MTA model unmodified reference SDK fixture completed')
        $result.actualUnmodifiedDiagnosticsCompositionSdkTested=$text.Contains('Actual MTA unmodified diagnostics composition SDK fixture completed')
        $result.actualModelUnmodifiedUnloadUnusedSdkTested=$text.Contains('Actual MTA model unmodified UnloadUnused SDK fixture completed')

        $expectedBits = if ($architecture -eq 'x86') { 32 } else { 64 }
        if ($result.test.exitCode -ne 0 -or $result.test.timedOut -or $result.checks -le 0 -or $result.pointerBits -ne $expectedBits -or -not $result.actualModelUnmodifiedReferenceSdkTested -or -not $result.actualUnmodifiedDiagnosticsCompositionSdkTested -or -not $result.actualModelUnmodifiedUnloadUnusedSdkTested) {
            throw "$architecture actual MTA model unmodified reference SDK checks failed or summary/pointer-width evidence was missing."
        }
    }
    $record.actualModelUnmodifiedReferenceSdkTested=@($record.results|Where-Object{-not $_.actualModelUnmodifiedReferenceSdkTested}).Count -eq 0
    $record.actualUnmodifiedDiagnosticsCompositionSdkTested=@($record.results|Where-Object{-not $_.actualUnmodifiedDiagnosticsCompositionSdkTested}).Count -eq 0
    $record.actualModelUnmodifiedUnloadUnusedSdkTested=@($record.results|Where-Object{-not $_.actualModelUnmodifiedUnloadUnusedSdkTested}).Count -eq 0

    $record.success = $record.results.Count -eq 2 -and $record.actualModelUnmodifiedReferenceSdkTested -and $record.actualUnmodifiedDiagnosticsCompositionSdkTested -and $record.actualModelUnmodifiedUnloadUnusedSdkTested
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
if (-not $record.success) { throw "Actual MTA model backing reference SDK checks failed; see $outputRoot" }
