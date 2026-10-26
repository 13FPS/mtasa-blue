#!/usr/bin/env pwsh
#requires -Version 7.0
# The real Core virtual receiver supplies logging/reporting, not a second logger.
[CmdletBinding()]
param()
$ErrorActionPreference='Stop'
Set-StrictMode -Version Latest
$taskRoot=(Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../..')).Path
$id=(Get-Date -Format 'yyyyMMdd-HHmmss')+'-'+[guid]::NewGuid().ToString('N')
$outputRoot=Join-Path $taskRoot ('Build/mta-model-diagnostics-de/'+$id)
$scratchRoot=Join-Path $taskRoot ('Build/tmp/model-diagnostics-de-'+$id)
[IO.Directory]::CreateDirectory($outputRoot)|Out-Null
[IO.Directory]::CreateDirectory($scratchRoot)|Out-Null
$previousEnvironment=@{}
foreach($name in @('TEMP','TMP','VCToolsInstallDir','INCLUDE','LIB','LIBPATH','PATH')){
    $previousEnvironment[$name]=[Environment]::GetEnvironmentVariable($name,'Process')
}
$record=[ordered]@{
    schema='mta_model_diagnostics_de_tests_v1';directory=$outputRoot;success=$false;
    recordedAt=[DateTimeOffset]::Now.ToString('o');sourceHashes=@();sourcesUnchanged=$true;
    generatedFixture=$null;result=$null;failure=$null;scratchDirectory=$scratchRoot;scratchRemoved=$false;
    scope='Actual partial CModelInfoDiagnosticsDE through real CCoreInterface inheritance, ReferenceDE capability gate, RequestDE and CStreamingDE. Exact event/report IDs and meaningful payloads, callback refusal ordering, one selected actual delayed completion and isolated genuine debug failure assertion. No prior request/reference matrix or complete game/native factory.';
    actualModelDiagnosticsSdkTested=$false;actualDebugFailureAssertionTested=$false;
    coreVtableChanged=$false;duplicateLoggerProvided=$false;completeModelFactoryReady=$false;
    nativeEngineCodeExecuted=$false;gameLaunched=$false;gameAttached=$false;installationChanged=$false
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
    [ordered]@{executable=$Executable;sha256=(Sha $Executable);arguments=$Arguments;workingDirectory=$Directory;
        deadlineMs=$DeadlineMs;temporaryDirectory=$scratchRoot} | ConvertTo-Json -Depth 4 |
        Set-Content -LiteralPath $command -Encoding utf8NoBOM
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
    try {
        if (-not $process.Start()) { throw "Could not start owned $Name process." }
        $readOut = $process.StandardOutput.ReadToEndAsync()
        $readErr = $process.StandardError.ReadToEndAsync()
        $timedOut = -not $process.WaitForExit($DeadlineMs)
        if ($timedOut) {
            # Only the fixture process tree created by this invocation is owned.
            $process.Kill($true)
            if (-not $process.WaitForExit(10000)) { throw "Owned $Name process did not terminate after timeout." }
        }
        [IO.File]::WriteAllText($stdout,$readOut.GetAwaiter().GetResult(),[Text.UTF8Encoding]::new($false))
        [IO.File]::WriteAllText($stderr,$readErr.GetAwaiter().GetResult(),[Text.UTF8Encoding]::new($false))
        [ordered]@{name=$Name;command=$command;commandSha256=(Sha $command);exitCode=$process.ExitCode;
            timedOut=$timedOut;stdout=$stdout;stderr=$stderr;stdoutSha256=(Sha $stdout);stderrSha256=(Sha $stderr)}
    } finally { $process.Dispose() }
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

# Record the real project header closure, including Core's historical implicit
# SDK prerequisites; system headers are supplied by the discovered x64 toolset.
function SourceClosure([string[]]$Seeds){
    $pending=[Collections.Generic.Queue[string]]::new()
    foreach($seed in $Seeds){$pending.Enqueue((Join-Path $taskRoot $seed))}
    $seen=[Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
    $includeRoots=@((Join-Path $taskRoot 'Shared/sdk'),(Join-Path $taskRoot 'Client/sdk'),(Join-Path $taskRoot 'vendor'))
    while($pending.Count){
        $path=[IO.Path]::GetFullPath($pending.Dequeue())
        if(-not $seen.Add($path)){continue}
        if(-not $path.StartsWith($taskRoot+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)){throw 'Source include escaped workspace'}
        $relative=[IO.Path]::GetRelativePath($taskRoot,$path).Replace('\','/')
        [ordered]@{path=$relative;sha256=(Sha $path)}
        foreach($include in [regex]::Matches([IO.File]::ReadAllText($path),'(?m)^\s*#\s*include\s*([<"])([^>"]+)[>"]')){
            $locations=@()
            if($include.Groups[1].Value -eq '"'){$locations+=Join-Path ([IO.Path]::GetDirectoryName($path)) $include.Groups[2].Value}
            foreach($root in $includeRoots){$locations+=Join-Path $root $include.Groups[2].Value}
            foreach($candidate in $locations){if([IO.File]::Exists($candidate)){$pending.Enqueue($candidate);break}}
        }
    }
}
try{
    $productionSources=@('Client/game_de/CModelInfoDiagnosticsDE.cpp','Client/game_de/CModelInfoReferenceDE.cpp',
        'Client/game_de/CModelInfoRequestDE.cpp','Client/game_de/CModelInfoDE.cpp','Client/game_de/CStreamingDE.cpp')
    $record.sourceHashes=@(SourceClosure ($productionSources+@('Tests/definitive/ModelDiagnosticsDETests.cpp','Tests/definitive/test_model_diagnostics_de.ps1'))|Sort-Object path)
    $modelProvided=@()
    foreach($header in @('Client/game_de/CModelInfoDE.h','Client/game_de/CModelInfoRequestDE.h')){
        $text=[regex]::Replace([IO.File]::ReadAllText((Join-Path $taskRoot $header)),'(?s)/\*.*?\*/|//[^\r\n]*','')
        $modelProvided+=@([regex]::Matches($text,'\b(?<name>\w+)\s*\([^;{}]*\)\s*(?:const\s*)?(?:noexcept\s*)?override\b')|ForEach-Object{$_.Groups['name'].Value})
    }
    $modelProvided=@($modelProvided|Sort-Object -Unique)
    $text=[regex]::Replace([IO.File]::ReadAllText((Join-Path $taskRoot 'Client/game_de/CStreamingDE.h')),'(?s)/\*.*?\*/|//[^\r\n]*','')
    $streamingProvided=@([regex]::Matches($text,'\b(?<name>\w+)\s*\([^;{}]*\)\s*(?:const\s*)?(?:noexcept\s*)?override\b')|ForEach-Object{$_.Groups['name'].Value})
    $gameMethods=@(InterfaceMethods 'Client/sdk/game/CGame.h'|Where-Object{$_.name -notin @('GetStreaming','GetBaseIDforTXD','GetBaseIDforIFP','GetModelInfo')})
    $modelMethods=@(InterfaceMethods 'Client/sdk/game/CModelInfo.h'|Where-Object{$_.name -notin $modelProvided})
    $streamingMethods=@(InterfaceMethods 'Client/sdk/game/CStreaming.h'|Where-Object{$_.name -notin $streamingProvided})
    $coreMethods=@(InterfaceMethods 'Client/sdk/core/CCoreInterface.h'|Where-Object{$_.name -ne 'LogEvent'})
    $generated=Join-Path $scratchRoot 'ModelDiagnosticsFixture.generated.h'
    $lines=@('#pragma once','#pragma warning(push)','#pragma warning(disable : 4100)','namespace fixture {',
        'class CoreFixtureBase : public CCoreInterface { public:')
    $lines+=@($coreMethods|ForEach-Object{$_.signature+' override { Unqualified(); }'})
    $lines+=@('};','class GameFixtureBase : public CGame { public:')
    $lines+=@($gameMethods|ForEach-Object{$_.signature+' override { Unqualified(); }'})
    $lines+=@('};','class ModelFixtureBase : public mta::game_de::CModelInfoDiagnosticsDE { public:',
        'ModelFixtureBase(std::uint32_t key,mta::game_de::NativeModelClassificationAccess& classification,CGame& game,mta::game_de::NativeModelBackingReferenceAccess& owner,const mta::game_de::NativeModelBackingReferenceToken& token,CCoreInterface& core) : CModelInfoDiagnosticsDE(key,classification,game,owner,token,core) {}')
    $lines+=@($modelMethods|ForEach-Object{$_.signature+' override { Unqualified(); }'})
    $lines+=@('};','class StreamingFixtureBase : public mta::game_de::CStreamingDE { public:',
        'explicit StreamingFixtureBase(mta::game_de::NativeStreamingModelAccess& owner) noexcept : CStreamingDE(owner) {}')
    $lines+=@($streamingMethods|ForEach-Object{$_.signature+' override { Unqualified(); }'})
    $lines+=@('};','}','#pragma warning(pop)')
    $lines|Set-Content -LiteralPath $generated -Encoding utf8NoBOM
    $durableGenerated=Join-Path $outputRoot 'ModelDiagnosticsFixture.generated.h'
    Copy-Item -LiteralPath $generated -Destination $durableGenerated
    $record.generatedFixture=[ordered]@{path=$durableGenerated;sha256=(Sha $generated);
        coreRejectingOverrides=$coreMethods.Count;gameRejectingOverrides=$gameMethods.Count;modelRejectingOverrides=$modelMethods.Count;
        streamingRejectingOverrides=$streamingMethods.Count;productionMethodsNotOverridden=$modelProvided;
        coreReceiver='Real CCoreInterface subclass; only existing LogEvent supplied by fixture';
        inheritedReportingMethods=@('ReportMissingAnimationModel','ReportBlockingModelLoad','ReportRequestingAnimationModel');
        requestPolicyMasked=$false;referencePolicyMasked=$false;reportingMethodsMasked=$false}
    $env:TEMP = $scratchRoot
    $env:TMP = $scratchRoot
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
    $discovery = RunOwned $vswhere @('-latest','-products','*','-requires','Microsoft.VisualStudio.Component.VC.Tools.x86.x64','-property','installationPath') $outputRoot 'vswhere' 10000
    $visualStudio = [IO.File]::ReadAllText($discovery.stdout).Trim()
    if ($discovery.exitCode -ne 0 -or $discovery.timedOut -or -not $visualStudio) { throw 'Visual Studio x64 tools are required.' }
    $developerCommand = Join-Path $visualStudio 'Common7/Tools/VsDevCmd.bat'
    $environmentScript = Join-Path $outputRoot 'capture-msvc.cmd'
    @('@echo off',('call "{0}" -no_logo -arch=x64 -host_arch=x64 >nul' -f $developerCommand),
        'if errorlevel 1 exit /b 1','set VCToolsInstallDir','set INCLUDE','set LIB','set PATH','exit /b 0') |
        Set-Content -LiteralPath $environmentScript -Encoding ascii
    $environmentResult = RunOwned $env:ComSpec @('/d','/s','/c','capture-msvc.cmd') $outputRoot 'msvc-environment' 30000
    if ($environmentResult.exitCode -ne 0 -or $environmentResult.timedOut) { throw 'x64 MSVC environment initialization failed.' }
    foreach ($entry in [IO.File]::ReadAllLines($environmentResult.stdout)) {
        if ($entry -match '^(VCToolsInstallDir|INCLUDE|LIB|LIBPATH|PATH)=(.*)$') {
            [Environment]::SetEnvironmentVariable($Matches[1],$Matches[2],'Process')
        }
    }
    $compiler=Join-Path $env:VCToolsInstallDir 'bin/Hostx64/x64/cl.exe'
    $executable=Join-Path $outputRoot 'model_diagnostics_de_tests.exe'
    $arguments=@('/nologo','/std:c++17','/EHsc','/W4','/WX','/O2','/MD','/DMTA_DEBUG','/DWIN32','/DNOMINMAX','/DWIN32_LEAN_AND_MEAN',
        '/D_CRT_SECURE_NO_WARNINGS','/D_SCL_SECURE_NO_WARNINGS','/D_CRT_NONSTDC_NO_DEPRECATE','/D_TIMESPEC_DEFINED',
        ('/I'+(Join-Path $taskRoot 'Shared/sdk')),('/I'+(Join-Path $taskRoot 'Client/sdk')),('/I'+(Join-Path $taskRoot 'vendor')),('/I'+$scratchRoot),
        (Join-Path $PSScriptRoot 'ModelDiagnosticsDETests.cpp'))+@($productionSources|ForEach-Object{Join-Path $taskRoot $_})+
        @(('/Fo'+$scratchRoot+[IO.Path]::DirectorySeparatorChar),('/Fd'+(Join-Path $scratchRoot 'fixture.pdb')),('/Fe'+$executable),'/link','bcrypt.lib')
    $record.result=[ordered]@{architecture='x64';debugAssertionEnabled=$true;environment=$environmentResult;compile=$null;test=$null;assertion=$null;
        checks=0;pointerBits=$null;executable=$executable;executableSha256=$null}
    $record.result.compile=RunOwned $compiler $arguments $outputRoot 'compile' 60000
    if($record.result.compile.exitCode -ne 0 -or $record.result.compile.timedOut){throw 'Actual Core-bound model diagnostic fixture compilation failed.'}
    $record.result.executableSha256=Sha $executable
    $record.result.test=RunOwned $executable @() $outputRoot 'test' 10000
    $stdout=[IO.File]::ReadAllText($record.result.test.stdout)
    $summary=[regex]::Match($stdout.TrimEnd(),'(?:^|\r?\n)Passed (\d+) actual model diagnostics SDK checks \((\d+)-bit\)$')
    if($summary.Success){$record.result.checks=[int]$summary.Groups[1].Value;$record.result.pointerBits=[int]$summary.Groups[2].Value}
    $record.actualModelDiagnosticsSdkTested=$stdout.Contains('Production actual Core-bound model diagnostics SDK fixture completed')
    $record.success=$record.result.test.exitCode -eq 0 -and -not $record.result.test.timedOut -and $record.result.checks -gt 0 -and
        $record.result.pointerBits -eq 64 -and $record.actualModelDiagnosticsSdkTested
    if(-not $record.success){throw 'Actual Core-bound diagnostic SDK checks or execution marker failed.'}
    $record.result.assertion=RunOwned $executable @('--debug-assert') $outputRoot 'debug-assertion' 10000
    $assertOut=[IO.File]::ReadAllText($record.result.assertion.stdout)
    $record.actualDebugFailureAssertionTested=$record.result.assertion.exitCode -ne 0 -and -not $record.result.assertion.timedOut -and
        $assertOut.Contains('Actual Core641 report6641 callback completed before dassert') -and -not $assertOut.Contains('FAIL: genuine failed-load dassert returned')
    $record.success=$record.success -and $record.actualDebugFailureAssertionTested
    if(-not $record.success){throw 'Genuine failed-load debug assertion did not terminate after its real Core callback.'}

} catch { AddFailure ($_.Exception.ToString()) }
finally {
    foreach ($name in $previousEnvironment.Keys) {
        [Environment]::SetEnvironmentVariable($name,$previousEnvironment[$name],'Process')
    }
    foreach ($entry in $record.sourceHashes) {
        try { if ((Sha (Join-Path $taskRoot $entry.path)) -ne $entry.sha256) { $record.sourcesUnchanged = $false } }
        catch { $record.sourcesUnchanged = $false; AddFailure ($_.Exception.ToString()) }
    }
    $record.success = $record.success -and $record.sourcesUnchanged
    try {
        # Validate this invocation's flat scratch directory before deleting its
        # generated fixture and compiler scratch. Durable evidence stays above.
        $resolvedScratch = (Resolve-Path -LiteralPath $scratchRoot).Path.TrimEnd('\') + '\'
        $expectedParent = (Resolve-Path -LiteralPath (Join-Path $taskRoot 'Build/tmp')).Path.TrimEnd('\') + '\'
        if (-not $resolvedScratch.StartsWith($expectedParent,[StringComparison]::OrdinalIgnoreCase) -or
            ((Get-Item -LiteralPath $scratchRoot -Force).Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) { throw 'Task scratch escaped Build/tmp or became a reparse point.' }
        $entries = @(Get-ChildItem -LiteralPath $scratchRoot -Force)
        foreach ($entry in $entries) {
            if ($entry.PSIsContainer -or ($entry.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0 -or
                -not (Resolve-Path -LiteralPath $entry.FullName).Path.StartsWith($resolvedScratch,[StringComparison]::OrdinalIgnoreCase)) { throw 'Unexpected scratch directory/link/path preserved.' }
        }
        foreach ($entry in $entries) { Remove-Item -LiteralPath $entry.FullName -Force }
        [IO.Directory]::Delete($scratchRoot,$false)
        $record.scratchRemoved = $true
    } catch { AddFailure ($_.Exception.ToString()) }
    $record.finishedAt = [DateTimeOffset]::Now.ToString('o')
    $record | ConvertTo-Json -Depth 10 | Set-Content -LiteralPath (Join-Path $outputRoot 'test-result.json') -Encoding utf8NoBOM
}
[ordered]@{success=$record.success;receipt=(Join-Path $outputRoot 'test-result.json');failure=$record.failure} | ConvertTo-Json -Compress
if (-not $record.success) { throw "Actual Core-bound model diagnostics SDK checks failed; see $outputRoot" }
