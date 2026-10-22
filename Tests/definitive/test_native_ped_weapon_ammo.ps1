#requires -Version 7.0
# Test exact raw clip/total DWORD intent on current owned ped weapon slots; never GTA code.
[CmdletBinding()]
param()
$ErrorActionPreference='Stop'
Set-StrictMode -Version Latest
$root=(Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../..')).Path
$id=(Get-Date -Format 'yyyyMMdd-HHmmss')+'-'+[guid]::NewGuid().ToString('N')
$output=Join-Path $root ('Build/mta-native-ped-weapon-ammo/'+$id)
$scratch=Join-Path $root ('Build/tmp/native-ped-weapon-ammo-'+$id)
[IO.Directory]::CreateDirectory($output)|Out-Null
[IO.Directory]::CreateDirectory($scratch)|Out-Null
$utf8=[Text.UTF8Encoding]::new($false)
function Sha([string]$p){(Get-FileHash -LiteralPath $p -Algorithm SHA256).Hash.ToLowerInvariant()}
$previous=@{}
foreach($name in @('TEMP','TMP','PATH','INCLUDE','LIB','LIBPATH','VCToolsInstallDir')){$previous[$name]=[Environment]::GetEnvironmentVariable($name,'Process')}
$record=[ordered]@{schema='mta_native_ped_weapon_ammo_tests_v1';success=$false;failure=$null;sources=@();results=@();architecture='x64';
    nativeEngineCodeExecuted=$false;nativePoolObservedInGta=$false;nativePedWeaponAmmoPrimitiveTested=$false;
    gameLaunched=$false;gameAttached=$false;engineMemoryWritten=$false;hookInstalled=$false;
    completeGameReady=$false;completePoolsReady=$false;localPlayerRegistered=$false;sourcesUnchanged=$true;scratch=$scratch;scratchFilesRemaining=$null;
    scope='Independent raw ammo clip/total DWORD storage matching actual CWeaponSA setters on qualified ped-owned slots0..12. Shared context preserves current registered ped/type identity and native GT/GC/state. One complete DWORD CAS without native017B clamp, clip adjustment or state reset; other row bytes unowned. Shared type/state/ammo mutation admission, no entered replay/rollback, standalone allocation or complete CWeapon publication.'}
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
        'Client/game_de/NativePedMetadataAccess.h','Client/game_de/NativePedMetadataMemory.h','Client/game_de/NativePedMetadataMemory.cpp','Client/game_de/NativePedMetadataProfile.h',
        'Client/game_de/NativePedReferenceToken.h','Shared/sdk/enums/EntityType.h',
        'Client/game_de/NativePedWeaponFieldsAccess.h','Client/game_de/NativePedWeaponFieldsMemory.h','Client/game_de/NativePedWeaponFieldsMemory.cpp','Client/game_de/NativePedWeaponProfile.h','Client/game_de/NativePedWeaponContext.h','Client/game_de/NativePedWeaponContext.cpp',
        'Tests/definitive/NativePedWeaponFieldsTests.cpp','Tests/definitive/test_native_ped_weapon_fields.ps1',
        'Build/definitive-recovery/mta-native-weapon-instance-fields-20261023/native-proof/weapon-instance-fields-qualified.json',
        'Client/game_de/NativeGameStateMemory.h','Client/game_de/NativeGameStateMemory.cpp','Client/game_de/NativeGameStateProfile.h',
        'Client/game_de/NativeStartGameCalls.h','Client/game_de/NativeStartGameCalls.cpp','Client/game_de/NativeStartGameProfile.h',
        'Client/game_de/NativeApplicationFrameProfile.h','Client/game_de/NativeGamePauseProfile.h','Client/game_de/NativeBorrowedGcProfile.h',
        'Tests/definitive/NativePedTransformReaderTests.cpp','Tests/definitive/NativePedMetadataTests.cpp','Tests/definitive/test_native_ped_metadata.ps1',
        'Build/definitive-recovery/mta-ped-pool-handle-batch-20261021/native-proof/ped-pool-qualified.json',
        'Build/definitive-recovery/mta-ped-transform-batch-20261021/native-proof/ped-transform-qualified.json',
        'Build/definitive-recovery/mta-ped-metadata-proof-20261021/ped-metadata-qualified.json')
    $names += @(
        'Client/game_de/NativePedWeaponAmmoAccess.h',
        'Client/game_de/NativePedWeaponAmmoMemory.h',
        'Client/game_de/NativePedWeaponAmmoMemory.cpp',
        'Client/game_de/NativePedWeaponAmmoProfile.h',
        'Tests/definitive/NativePedWeaponAmmoTests.cpp',
        'Tests/definitive/test_native_ped_weapon_ammo.ps1',
        'Build/definitive-recovery/mta-native-ped-weapon-ammo-20261023/ped-weapon-ammo-qualified.json',
        'Build/definitive-recovery/mta-native-ped-weapon-ammo-20261023/named-weapon-prefix.json',
        'Build/definitive-recovery/mta-native-ped-weapon-ammo-20261023/named-ammo-cases.json',
        'Build/definitive-recovery/mta-native-ped-weapon-ammo-20261023/ammo-functions.json',
        'Build/definitive-recovery/mta-native-ped-weapon-ammo-20261023/function-010d6080.txt',
        'Build/definitive-recovery/mta-native-ped-weapon-ammo-20261023/function-010d7173.txt',
        'Build/definitive-recovery/mta-native-ped-weapon-ammo-20261023/function-010d76a0.txt',
        'Build/definitive-recovery/mta-native-ped-weapon-ammo-20261023/function-010d77b8.txt',
        'Build/definitive-recovery/mta-native-ped-weapon-ammo-20261023/function-010e7620.txt',
        'Build/definitive-recovery/mta-native-ped-weapon-ammo-20261023/function-010e7d50.txt',
        'Build/definitive-recovery/mta-native-ped-weapon-ammo-20261023/function-01222990.txt',
        'Build/definitive-recovery/mta-native-ped-weapon-ammo-20261023/function-01222d80.txt',
        'Client/game_sa/CWeaponSA.h',
        'Build/definitive-recovery/mta-native-weapon-instance-fields-20261023/weapon-fields-source-ready.json'
    )
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
    $exe=Join-Path $output 'native-ped-weapon-ammo-tests.exe'
    $arguments=@('/nologo','/std:c++17','/EHsc','/W4','/WX','/O2','/MD','/DNOMINMAX','/DWIN32_LEAN_AND_MEAN',
        ('/I'+(Join-Path $root 'Client/sdk')),('/I'+(Join-Path $root 'Shared/sdk')),('/I'+$scratch),
        (Join-Path $PSScriptRoot 'NativePedWeaponAmmoTests.cpp'),('/Fo'+$scratch+[IO.Path]::DirectorySeparatorChar),
        ('/Fd'+(Join-Path $scratch 'compiler.pdb')),('/Fe'+$exe),'/link','bcrypt.lib')
    $result=[ordered]@{compiler=$compiler;compilerSha256=(Sha $compiler);compile=$null;test=$null;checks=0}
    $record.results+=$result
    $result.compile=RunOwned $compiler $arguments $output 'compile' 60000
    if($result.compile.exitCode -ne 0 -or $result.compile.timedOut){throw 'Native ped-weapon-ammo fixture compilation failed.'}
    $result.executableSha256=Sha $exe
    $result.test=RunOwned $exe @($output) $output 'test' 30000
    $summary=[regex]::Match([IO.File]::ReadAllText($result.test.stdout),'(?m)^Passed (\d+) native ped-weapon-ammo checks \(64-bit; no native engine code executed\)\r?$')
    if($summary.Success){$result.checks=[int]$summary.Groups[1].Value}
    $record.nativePedWeaponAmmoPrimitiveTested=[IO.File]::ReadAllText($result.test.stdout).Contains('Production native ped weapon ammo primitive fixture completed')
    if($result.test.exitCode -ne 0 -or $result.test.timedOut -or $result.checks -le 0 -or -not $record.nativePedWeaponAmmoPrimitiveTested){throw 'Native ped-weapon-ammo checks failed.'}
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
if(-not $record.success){throw ('Native ped-weapon-ammo validation failed; see '+$receipt)}
