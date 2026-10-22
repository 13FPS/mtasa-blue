#requires -Version 7.0
# Run genuine native request admission and partial-effect accounting on owned OS pages.
[CmdletBinding()]
param()
$ErrorActionPreference='Stop'
Set-StrictMode -Version Latest
$root=(Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../..')).Path
$id=(Get-Date -Format 'yyyyMMdd-HHmmss')+'-'+[guid]::NewGuid().ToString('N')
$output=Join-Path $root ('Build/mta-native-streaming-resources/'+$id)
$scratch=Join-Path $root ('Build/tmp/native-streaming-resources-'+$id)
[IO.Directory]::CreateDirectory($output)|Out-Null
[IO.Directory]::CreateDirectory($scratch)|Out-Null
$utf8=[Text.UTF8Encoding]::new($false)
function Sha([string]$p){(Get-FileHash -LiteralPath $p -Algorithm SHA256).Hash.ToLowerInvariant()}
$previous=@{}
foreach($name in @('TEMP','TMP','PATH','INCLUDE','LIB','LIBPATH','VCToolsInstallDir')){$previous[$name]=[Environment]::GetEnvironmentVariable($name,'Process')}
$record=[ordered]@{schema='mta_native_streaming_resources_tests_v1';success=$false;failure=$null;sources=@();results=@();architecture='x64';
    nativeEngineCodeExecuted=$false;nativePoolObservedInGta=$false;nativeStreamingResourcesPrimitiveTested=$false;
    gameLaunched=$false;gameAttached=$false;engineMemoryWritten=$false;hookInstalled=$false;
    completeGameReady=$false;completePoolsReady=$false;localPlayerRegistered=$false;modelInfoSdkPublished=$false;nativeModelObjectDereferenced=$false;modelReferenceClaimed=$false;rwObjectPublished=$false;sourcesUnchanged=$true;scratch=$scratch;scratchFilesRemaining=$null;
scope='Genuine non-DFF Request1288560 with full32flags/explicit thirdzero; exact resource categories excluding four list sentinels. Current native owner/GT/noGC/PLAYING,typed root/IMGFile membership,selected filename/pool generation or DAT softpath,real cache/buffer and common list graph bookends. Native cache rebuild,decode,recursive retry and publication remain with the real entry; entered failure never replays or restores state. Owned image fixtures execute no engine instruction.'}
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
    $names+=@('Client/game_de/NativeStreamingResourceProfile.h','Client/game_de/NativeStreamingResourceContext.h','Client/game_de/NativeStreamingResourceContext.cpp','Client/game_de/NativeStreamingResourceCalls.h','Client/game_de/NativeStreamingResourceCalls.cpp','Tests/definitive/NativeStreamingResourcesTests.cpp','Tests/definitive/test_native_streaming_resources.ps1','Build/definitive-recovery/mta-native-streaming-resources-proof-20261022/native-proof/resource-request-contracts-qualified.json','Build/definitive-recovery/mta-streaming-ids-static-20261019-103008/streaming-file-ids-evidence.json')
    $record.sources=@($names|ForEach-Object{[ordered]@{path=$_;sha256=(Sha (Join-Path $root $_))}})
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
    $exe=Join-Path $output 'native-streaming-resources-tests.exe'
    $arguments=@('/nologo','/std:c++17','/EHsc','/W4','/WX','/O2','/MD','/DNOMINMAX','/DWIN32_LEAN_AND_MEAN',
        ('/I'+(Join-Path $root 'Client/sdk')),('/I'+(Join-Path $root 'Shared/sdk')),('/I'+$scratch),
        (Join-Path $PSScriptRoot 'NativeStreamingResourcesTests.cpp'),('/Fo'+$scratch+[IO.Path]::DirectorySeparatorChar),
        ('/Fd'+(Join-Path $scratch 'compiler.pdb')),('/Fe'+$exe),'/link','bcrypt.lib')
    $result=[ordered]@{compiler=$compiler;compilerSha256=(Sha $compiler);compile=$null;test=$null;checks=0}
    $record.results+=$result
    $result.compile=RunOwned $compiler $arguments $output 'compile' 60000
    if($result.compile.exitCode -ne 0 -or $result.compile.timedOut){throw 'Native streaming-resources fixture compilation failed.'}
    $result.executableSha256=Sha $exe
    $result.test=RunOwned $exe @($output) $output 'test' 60000
    $summary=[regex]::Match([IO.File]::ReadAllText($result.test.stdout),'(?m)^Passed (\d+) native streaming-resources checks \(64-bit; no native engine code executed\)\r?$')
    if($summary.Success){$result.checks=[int]$summary.Groups[1].Value}
    $record.nativeStreamingResourcesPrimitiveTested=[IO.File]::ReadAllText($result.test.stdout).Contains('Production native streaming resources primitive fixture completed')
    if($result.test.exitCode -ne 0 -or $result.test.timedOut -or $result.checks -le 0 -or -not $record.nativeStreamingResourcesPrimitiveTested){throw 'Native streaming-resources checks failed.'}
    $record.success=$true
}catch{$record.failure=$_.Exception.ToString()}
finally{
    foreach($name in $previous.Keys){[Environment]::SetEnvironmentVariable($name,$previous[$name],'Process')}
    foreach($source in $record.sources){if((Sha (Join-Path $root $source.path)) -ne $source.sha256){$record.sourcesUnchanged=$false}}
    $record.success=$record.success -and $record.sourcesUnchanged
    $resolved=[IO.Path]::GetFullPath($scratch)
    $allowed=[IO.Path]::GetFullPath((Join-Path $root 'Build/tmp')).TrimEnd('\')+'\'
    if(-not $resolved.StartsWith($allowed,[StringComparison]::OrdinalIgnoreCase)){throw 'Compiler scratch escaped workspace.'}
    foreach($file in @(Get-ChildItem -LiteralPath $scratch -Force)){
        if(-not $file.PSIsContainer -and -not ($file.Attributes -band [IO.FileAttributes]::ReparsePoint)){Remove-Item -LiteralPath $file.FullName -Force}
    }
    $record.scratchFilesRemaining=@(Get-ChildItem -LiteralPath $scratch -Force).Count
    if(-not $record.scratchFilesRemaining){Remove-Item -LiteralPath $scratch}
    $receipt=Join-Path $output 'test-result.json';[IO.File]::WriteAllText($receipt,($record|ConvertTo-Json -Depth 12)+"`n",$utf8)
}
@{success=$record.success;path=$receipt;sha256=(Sha $receipt);results=$record.results}|ConvertTo-Json -Depth 6 -Compress
if(-not $record.success){throw ('Native streaming-resources validation failed; see '+$receipt)}
