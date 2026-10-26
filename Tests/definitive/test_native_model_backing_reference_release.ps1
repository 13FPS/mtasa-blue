#requires -Version 7.0
# Run explicit backing-reference release through the existing owned-image final-release fixture.
[CmdletBinding()]
param()
$ErrorActionPreference='Stop'
Set-StrictMode -Version Latest
$root=(Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../..')).Path
$id=(Get-Date -Format 'yyyyMMdd-HHmmss')+'-'+[guid]::NewGuid().ToString('N')
$output=Join-Path $root ('Build/mta-native-model-backing-reference-release-controller/'+$id)
$scratch=Join-Path $root ('Build/tmp/native-model-backing-reference-release-controller-'+$id)
[IO.Directory]::CreateDirectory($output)|Out-Null
[IO.Directory]::CreateDirectory($scratch)|Out-Null
$utf8=[Text.UTF8Encoding]::new($false)
function Sha([string]$p){(Get-FileHash -LiteralPath $p -Algorithm SHA256).Hash.ToLowerInvariant()}
$previous=@{}
foreach($name in @('TEMP','TMP','PATH','INCLUDE','LIB','LIBPATH','VCToolsInstallDir')){$previous[$name]=[Environment]::GetEnvironmentVariable($name,'Process')}
$record=[ordered]@{schema='mta_native_model_backing_reference_release_controller_tests_v1';success=$false;failure=$null;sources=@();results=@();architecture='x64';
    nativeEngineCodeExecuted=$false;nativePoolObservedInGta=$false;nativeModelBackingReferenceReleaseControllerTested=$false;nativeModelBackingReferenceWeakOriginTested=$false;
    gameLaunched=$false;gameAttached=$false;engineMemoryWritten=$false;hookInstalled=$false;
    completeGameReady=$false;completePoolsReady=$false;localPlayerRegistered=$false;modelInfoSdkPublished=$false;nativeModelObjectDereferenced=$true;modelReferenceClaimed=$false;rwObjectPublished=$false;sourcesUnchanged=$true;scratch=$scratch;scratchFilesRemaining=$null;
scope='Actual backing controller explicit capability release after real owned-image acquisition. Fresh native count chooses actual interior Adjust(-1), final KeepResident or final RemoveAtZero without fallback. Durable acquisition requires an already-issued root weak serial; release matches actual captured root/map/index/serial. Check correct/wrong helper origin, serial-zero acquisition refusal, same-address serial replacement, terminal binding-loss debt, Released acknowledgement, entered/accepted fault debt, no replay/reacquisition and CPU-only destruction. Historical mains remain unused; actual Controller.cpp links separately. No native engine code, cloned counter algorithm, weak allocation or SDK Remove hook implementation.'}
function RunOwned([string]$exe,[string[]]$arguments,[string]$directory,[string]$label,[int]$timeout){
    $info=[Diagnostics.ProcessStartInfo]::new($exe)
    $info.WorkingDirectory=$directory;$info.UseShellExecute=$false;$info.CreateNoWindow=$true;$info.RedirectStandardOutput=$true;$info.RedirectStandardError=$true
    $info.Environment['TEMP']=$scratch;$info.Environment['TMP']=$scratch
    foreach($argument in $arguments){$info.ArgumentList.Add($argument)}
    $process=[Diagnostics.Process]::new();$process.StartInfo=$info
    [IO.File]::WriteAllText((Join-Path $directory ($label+'-command.json')),([ordered]@{executable=$exe;sha256=(Sha $exe);arguments=$arguments;timeoutMs=$timeout}|ConvertTo-Json -Depth 4),$utf8)
    try{
        if(-not $process.Start()){throw ('Could not start owned '+$label)}
        $out=$process.StandardOutput.ReadToEndAsync();$err=$process.StandardError.ReadToEndAsync()
        $timedOut=-not $process.WaitForExit($timeout)
        if($timedOut){$process.Kill($true);if(-not $process.WaitForExit(10000)){throw 'Owned child did not terminate.'}}
        $stdout=Join-Path $directory ($label+'.log');$stderr=Join-Path $directory ($label+'-errors.log')
        [IO.File]::WriteAllText($stdout,$out.GetAwaiter().GetResult(),$utf8);[IO.File]::WriteAllText($stderr,$err.GetAwaiter().GetResult(),$utf8)
        return [ordered]@{exitCode=$process.ExitCode;timedOut=$timedOut;stdout=$stdout;stderr=$stderr;stdoutSha256=(Sha $stdout);stderrSha256=(Sha $stderr)}
    }finally{$process.Dispose()}
}
try{
    $names=@('Client/game_de/NativePedPoolReader.h','Client/game_de/NativePedPoolReader.cpp','Client/game_de/NativePedPoolProfile.h',
        'Client/game_de/NativePedTransformReader.h','Client/game_de/NativePedTransformReader.cpp','Client/game_de/NativePedTransformProfile.h',
        'Shared/sdk/CVector.h','Shared/sdk/CVector4D.h','Shared/sdk/SharedUtil.Defines.h','Client/sdk/game/CPoolSlotPolicy.h',
        'Client/game_de/NativeImageAccess.h','Client/game_de/NativeImageAccess.cpp','Client/sdk/game/CGameAdapter.h','Client/sdk/game/CGameClockService.h',
        'Shared/sdk/CControllerState.h','Shared/sdk/CControllerActions.h','Shared/sdk/CControlStateSynthesis.h',
        'Client/game_de/NativeModelLoadedReader.h','Client/game_de/NativeModelLoadedReader.cpp','Client/game_de/NativeModelLoadedProfile.h',
        'Client/game_de/NativeModelBankReader.h','Client/game_de/NativeModelBankReader.cpp','Client/game_de/NativeModelBankProfile.h','Client/game_de/NativeStreamingFileIdsProfile.h',
        'Client/game_de/NativePedReferenceToken.h','Shared/sdk/enums/EntityType.h',
        'Client/game_de/NativeGameStateMemory.h','Client/game_de/NativeGameStateMemory.cpp','Client/game_de/NativeGameStateProfile.h',
        'Client/game_de/NativeStartGameCalls.h','Client/game_de/NativeStartGameCalls.cpp','Client/game_de/NativeStartGameProfile.h',
        'Client/game_de/NativeApplicationFrameProfile.h','Client/game_de/NativeGamePauseProfile.h','Client/game_de/NativeBorrowedGcProfile.h',
        'Tests/definitive/NativePedTransformReaderTests.cpp','Tests/definitive/NativeModelBankTests.cpp','Tests/definitive/NativeStreamingRequestTests.cpp','Tests/definitive/test_native_streaming_request.ps1',
        'Build/definitive-recovery/mta-ped-pool-handle-batch-20261021/native-proof/ped-pool-qualified.json',
        'Build/definitive-recovery/mta-ped-transform-batch-20261021/native-proof/ped-transform-qualified.json',
        'Build/definitive-recovery/mta-native-model-loaded-proof-20261022/model-loaded-qualified.json',
        'Client/game_de/NativeStreamingModelContext.h','Client/game_de/NativeStreamingModelContext.cpp',
        'Client/game_de/NativeModelReferenceMap.h','Client/game_de/NativeStreamingUpgradeProfile.h',
        'Client/game_de/NativeModelReferenceTransactions.h','Client/game_de/NativeModelReferenceTransactions.cpp',
        'Client/game_de/NativeModelReferenceHeap.h','Client/game_de/NativeModelReferenceTransactionProfile.h',
        'Tests/definitive/NativeModelBackingReferenceReleaseControllerTests.cpp','Tests/definitive/test_native_model_backing_reference_release.ps1',
        'Build/definitive-recovery/mta-native-model-reference-transactions-20261022/native-reference-transaction-qualified.json',
        'Build/definitive-recovery/mta-native-model-reference-lifetime-proof-20261022/native-reference-lifetime-qualified.json',
        'Client/game_de/NativeModelReferenceMemory.h','Client/game_de/NativeModelReferenceMemory.cpp','Client/game_de/NativeModelReferenceProfile.h',
        'Tests/definitive/NativeModelReferenceTests.cpp','Tests/definitive/test_native_model_reference.ps1',
        'Build/definitive-recovery/mta-native-model-reference-adjust-proof-20261022/reference-adjust-qualified.json',
        'Build/definitive-recovery/mta-native-model-reference-proof-20261022/model-reference-paths-qualified.json',
        'Client/game_de/NativeStreamingRequestCalls.h',
        'Client/game_de/NativeStreamingRequestCalls.cpp',
        'Client/game_de/NativeStreamingRequestProfile.h',
        'Client/game_de/NativeUObjectMembershipAccess.h',
        'Client/game_de/NativeUObjectMembershipProfile.h',
        'Client/game_de/NativeStartupProvidersProfile.h',
        'Client/game_de/NativeStartupWorldProfile.h',
        'Client/game_de/NativeClassChainAccess.h',
        'Client/game_de/NativeUiClassMapProfile.h',
        'Client/sdk/game/CModelRequestPolicy.h',
        'Client/sdk/game/CAnimBlock.h',
        'Build/definitive-recovery/mta-native-streaming-request-proof-20261022/native-proof/streaming-request-qualified.json')
    $names += @('Client/game_de/NativeModelBackingReferenceController.cpp','Client/game_de/NativeModelBackingReferenceController.h',
        'Client/game_de/NativeModelBackingReferenceAccess.h','Client/game_de/NativeModelReferenceAcquisitionReceipt.h',
        'Tests/definitive/NativeModelReferenceAcquireTests.cpp','Tests/definitive/test_native_model_acquire.ps1',
        'Tests/definitive/NativeModelReferenceFinalReleaseTests.cpp','Tests/definitive/test_native_model_final_release.ps1',
        'Build/definitive-recovery/mta-native-model-final-release-20261022/final-release-qualified.json',
        'Build/definitive-recovery/mta-native-model-removal-transactions-20261022/native-remove-qualified.json',
        'Build/definitive-recovery/mta-model-backing-reference-release-20261026/model-backing-reference-count-source-ready.json',
        'Build/definitive-recovery/mta-model-backing-reference-release-20261026/model-backing-reference-release-api-source-ready.json',
        'Build/definitive-recovery/mta-model-backing-reference-release-20261026/model-backing-reference-release-controller-source-ready.json',
        'Build/definitive-recovery/mta-model-backing-reference-release-20261026/model-backing-reference-zero-loss-source-ready.json',
        'Build/definitive-recovery/mta-model-backing-reference-release-20261026/model-backing-reference-release-affinity-source-ready.json',
        'Build/definitive-recovery/mta-model-backing-reference-release-20261026/model-backing-reference-weak-origin-current-source-ready.json',
        'Build/definitive-recovery/mta-model-backing-reference-release-tests-20261026/model-backing-reference-release-tests-current-source-ready.json',
        'Build/definitive-recovery/mta-model-backing-reference-release-tests-20261026/model-backing-reference-release-origin-tests-source-ready.json',
        'Build/definitive-recovery/mta-model-backing-reference-release-tests-20261026/model-backing-reference-release-weak-marker-source-ready.json')
    # The reused fixture includes actual production source, so hash the whole
    # resolved include graph rather than only the directly compiled pair.
    $pending=[Collections.Generic.Queue[string]]::new()
    $seen=[Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
    foreach($name in $names){$pending.Enqueue($name)}
    while($pending.Count){
        $name=$pending.Dequeue();if(-not $seen.Add($name)){continue}
        $path=[IO.Path]::GetFullPath((Join-Path $root $name))
        if(-not $path.StartsWith($root+'\',[StringComparison]::OrdinalIgnoreCase) -or $path -match '(?i)(?:^|[\\/])r-engineer(?:[\\/]|$)'){throw 'Controller fixture graph left allowed scope.'}
        if(-not (Test-Path -LiteralPath $path -PathType Leaf)){throw ('Missing actual source: '+$name)}
        foreach($match in [regex]::Matches([IO.File]::ReadAllText($path),'(?m)^\s*#\s*include\s*[<"](?<path>[^>"\n]+)[>"]')){
            $include=$match.Groups['path'].Value
            foreach($candidate in @((Join-Path (Split-Path $path) $include),(Join-Path $root ('Shared/sdk/'+$include)),(Join-Path $root ('Client/sdk/'+$include)))){
                if(Test-Path -LiteralPath $candidate -PathType Leaf){$pending.Enqueue([IO.Path]::GetRelativePath($root,[IO.Path]::GetFullPath($candidate)).Replace('\','/'));break}
            }
        }
    }
    $record.sources=@($seen|Sort-Object|ForEach-Object{[ordered]@{path=$_;sha256=(Sha (Join-Path $root $_))}})
    $env:TEMP=$scratch;$env:TMP=$scratch
    $vswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
    $vs=& $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    if($LASTEXITCODE -ne 0 -or -not $vs){throw 'MSVC x64 toolchain unavailable.'}
    $batch=Join-Path $output 'capture-msvc.cmd'
    @('@echo off',('call "{0}" -no_logo -arch=x64 -host_arch=x64 >nul' -f (Join-Path $vs 'Common7/Tools/VsDevCmd.bat')),
        'if errorlevel 1 exit /b 1','set VCToolsInstallDir','set INCLUDE','set LIB','set PATH','exit /b 0')|Set-Content -LiteralPath $batch -Encoding ascii
    $setup=RunOwned $env:ComSpec @('/d','/s','/c','capture-msvc.cmd') $output 'environment' 30000
    $setup['script']=$batch;$setup['scriptSha256']=Sha $batch;$record.environment=$setup
    if($setup.exitCode -ne 0 -or $setup.timedOut){throw 'MSVC environment initialization failed.'}
    foreach($line in [IO.File]::ReadAllLines($setup.stdout)){
        if($line -match '^(VCToolsInstallDir|INCLUDE|LIB|LIBPATH|PATH)=(.*)$'){[Environment]::SetEnvironmentVariable($Matches[1],$Matches[2],'Process')}
    }
    $compiler=Join-Path $env:VCToolsInstallDir 'bin/Hostx64/x64/cl.exe'
    $exe=Join-Path $output 'native-model-backing-reference-release-controller-tests.exe'
    $arguments=@('/nologo','/std:c++17','/EHsc','/W4','/WX','/O2','/MD','/DNOMINMAX','/DWIN32_LEAN_AND_MEAN',
        ('/I'+(Join-Path $root 'Client/sdk')),('/I'+(Join-Path $root 'Shared/sdk')),('/I'+$scratch),
        (Join-Path $PSScriptRoot 'NativeModelBackingReferenceReleaseControllerTests.cpp'),
        (Join-Path $root 'Client/game_de/NativeModelBackingReferenceController.cpp'),('/Fo'+$scratch+[IO.Path]::DirectorySeparatorChar),
        ('/Fd'+(Join-Path $scratch 'compiler.pdb')),('/Fe'+$exe),'/link','bcrypt.lib')
    $result=[ordered]@{compiler=$compiler;compilerSha256=(Sha $compiler);compile=$null;test=$null;checks=0}
    $record.results+=$result
    $result.compile=RunOwned $compiler $arguments $output 'compile' 60000
    if($result.compile.exitCode -ne 0 -or $result.compile.timedOut){throw 'Native model backing reference release controller fixture compilation failed.'}
    $result.executableSha256=Sha $exe
    $result.test=RunOwned $exe @($output) $output 'test' 60000
    $summary=[regex]::Match([IO.File]::ReadAllText($result.test.stdout),'(?m)^Passed (\d+) native model backing reference release controller checks \(64-bit; no native engine code executed\)\r?$')
    if($summary.Success){$result.checks=[int]$summary.Groups[1].Value}
    $record.nativeModelBackingReferenceReleaseControllerTested=[IO.File]::ReadAllText($result.test.stdout).Contains('Production native model backing reference release controller fixture completed')
    $record.nativeModelBackingReferenceWeakOriginTested=[IO.File]::ReadAllText($result.test.stdout).Contains('Production native model backing reference weak-origin fixture completed')
    if($result.test.exitCode -ne 0 -or $result.test.timedOut -or $result.checks -le 0 -or -not $record.nativeModelBackingReferenceReleaseControllerTested -or -not $record.nativeModelBackingReferenceWeakOriginTested){throw 'Native model backing reference release controller checks failed or a required coverage marker was missing.'}
    $record.success=$true
}catch{$record.failure=$_.Exception.ToString()}
finally{
    foreach($name in $previous.Keys){[Environment]::SetEnvironmentVariable($name,$previous[$name],'Process')}
    foreach($source in $record.sources){if((Sha (Join-Path $root $source.path)) -ne $source.sha256){$record.sourcesUnchanged=$false}}
    $record.success=$record.success -and $record.sourcesUnchanged
    $resolved=[IO.Path]::GetFullPath($scratch)
    $allowed=[IO.Path]::GetFullPath((Join-Path $root 'Build/tmp')).TrimEnd('\')+'\'
    if(-not $resolved.StartsWith($allowed,[StringComparison]::OrdinalIgnoreCase) -or
        ((Get-Item -LiteralPath $scratch -Force).Attributes -band [IO.FileAttributes]::ReparsePoint)){throw 'Compiler scratch escaped workspace or became a link.'}
    $entries=@(Get-ChildItem -LiteralPath $scratch -Force)
    foreach($file in $entries){
        if($file.PSIsContainer -or ($file.Attributes -band [IO.FileAttributes]::ReparsePoint) -or
            -not $file.FullName.StartsWith($resolved.TrimEnd('\')+'\',[StringComparison]::OrdinalIgnoreCase)){throw 'Unexpected scratch directory/link or path escape preserved.'}
    }
    foreach($file in $entries){Remove-Item -LiteralPath $file.FullName -Force}
    $record.scratchFilesRemaining=@(Get-ChildItem -LiteralPath $scratch -Force).Count
    if(-not $record.scratchFilesRemaining){Remove-Item -LiteralPath $scratch}
    $receipt=Join-Path $output 'test-result.json';[IO.File]::WriteAllText($receipt,($record|ConvertTo-Json -Depth 12)+"`n",$utf8)
}
@{success=$record.success;path=$receipt;sha256=(Sha $receipt);results=$record.results}|ConvertTo-Json -Depth 6 -Compress
if(-not $record.success){throw ('Native model backing reference release controller validation failed; see '+$receipt)}
