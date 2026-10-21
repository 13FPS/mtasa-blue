#requires -Version 7.0
# Execute actual production Windows memory operations only on an owned PE image.
[CmdletBinding()]
param()
$ErrorActionPreference='Stop'
Set-StrictMode -Version Latest
$root=(Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../..')).Path
$id=(Get-Date -Format 'yyyyMMdd-HHmmss')+'-'+[guid]::NewGuid().ToString('N')
$output=Join-Path $root ('Build/mta-native-weather-win32/'+$id)
$scratch=Join-Path $root ('Build/tmp/native-weather-win32-'+$id)
[IO.Directory]::CreateDirectory($output)|Out-Null
[IO.Directory]::CreateDirectory($scratch)|Out-Null
$utf8=[Text.UTF8Encoding]::new($false)
function Sha([string]$path){(Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLowerInvariant()}
$oldEnvironment=@{}
foreach($name in @('TEMP','TMP','PATH','INCLUDE','LIB','LIBPATH','VCToolsInstallDir')){$oldEnvironment[$name]=[Environment]::GetEnvironmentVariable($name,'Process')}
$record=[ordered]@{schema='mta_native_weather_win32_tests_v1';success=$false;failure=$null;sources=@();results=@();architecture='x64';
    nativeEngineCodeExecuted=$false;gameLaunched=$false;gameAttached=$false;engineMemoryWritten=$false;ownedImageMemoryWritten=$true;hookInstalled=$false;
    completeGameReady=$false;completeWeatherReady=$false;nativeLifecycleReady=$false;sourcesUnchanged=$true;scratch=$scratch;scratchFilesRemaining=$null;
    scope='Actual shipping Windows platform on a tiny owned sparse SEC_IMAGE: aligned64 and CPU-conditional128 atomic exchanges, compare conflicts, neighboring-byte preservation, image/private/protection/alignment refusal, real scalar CAS, all eight field-congruent private64KiB interior slots, allocation-base identity, simultaneous retention, wrong-range refusal and exact release. No native game instruction or process executes.'}
function RunOwned([string]$exe,[string[]]$arguments,[string]$label,[int]$timeout){
    $info=[Diagnostics.ProcessStartInfo]::new($exe)
    $info.WorkingDirectory=$output;$info.UseShellExecute=$false;$info.CreateNoWindow=$true;$info.RedirectStandardOutput=$true;$info.RedirectStandardError=$true
    $info.Environment['TEMP']=$scratch;$info.Environment['TMP']=$scratch
    foreach($argument in $arguments){$info.ArgumentList.Add($argument)}
    $process=[Diagnostics.Process]::new();$process.StartInfo=$info
    [IO.File]::WriteAllText((Join-Path $output ($label+'-command.json')),([ordered]@{executable=$exe;sha256=(Sha $exe);arguments=$arguments;timeoutMs=$timeout}|ConvertTo-Json -Depth 4),$utf8)
    try{
        if(-not $process.Start()){throw ('Could not start owned '+$label)}
        $out=$process.StandardOutput.ReadToEndAsync();$err=$process.StandardError.ReadToEndAsync()
        $timedOut=-not $process.WaitForExit($timeout)
        if($timedOut){$process.Kill($true);if(-not $process.WaitForExit(10000)){throw 'Owned child did not terminate.'}}
        $stdout=Join-Path $output ($label+'.log');$stderr=Join-Path $output ($label+'-errors.log')
        [IO.File]::WriteAllText($stdout,$out.GetAwaiter().GetResult(),$utf8);[IO.File]::WriteAllText($stderr,$err.GetAwaiter().GetResult(),$utf8)
        return [ordered]@{exitCode=$process.ExitCode;timedOut=$timedOut;stdout=$stdout;stderr=$stderr;stdoutSha256=(Sha $stdout);stderrSha256=(Sha $stderr)}
    }finally{$process.Dispose()}
}
try{
    $names=@('Client/game_de/NativeWeatherRainProfile.h','Client/game_de/NativeWeatherRainBridge.h','Client/game_de/NativeWeatherRainBridge.cpp','Client/game_de/NativeWeatherRainLeaf.h','Client/game_de/NativeWeatherRainLeaf.cpp','Client/game_de/NativeWeatherOverrideWin32.h','Client/game_de/NativeWeatherOverrideWin32.cpp',
        'Client/game_de/NativeWeatherOverrideOwner.h','Client/game_de/NativeWeatherOverrideAtomic.h','Client/game_de/NativeWeatherOverrideAtomic.cpp','Client/game_de/NativeWeatherOverridePlacement.h','Client/game_de/NativeWeatherOverridePlacement.cpp','Client/game_de/NativeWeatherOverrideSlot.h','Client/game_de/NativeWeatherOverrideSlot.cpp',
        'Client/game_de/NativeWeatherOverridePlan.h','Client/game_de/NativeWeatherOverridePlan.cpp','Client/game_de/NativeWeatherOverrideProfile.cpp',
        'Client/game_de/NativeWeatherScalarProfile.h','Client/game_de/NativeWeatherScalarProfile.cpp',
        'Client/game_de/NativeWeatherTransitionProfile.h','Client/game_de/NativeWeatherTransitionProfile.cpp',
        'Client/game_de/NativeClockProfile.h','Client/game_de/NativeImageAccess.h','Client/game_de/NativeImageAccess.cpp',
        'Client/sdk/game/CGameClockService.h','Client/sdk/game/CGameAdapter.h','Shared/sdk/CControllerState.h','Shared/sdk/CControlStateSynthesis.h',
        'Tests/definitive/NativeWeatherOverrideWin32Tests.cpp','Tests/definitive/test_native_weather_override_win32.ps1')
    $record.sources=@($names|ForEach-Object{[ordered]@{path=$_;sha256=(Sha (Join-Path $root $_))}})
    $env:TEMP=$scratch;$env:TMP=$scratch
    $vswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
    $vs=& $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    if($LASTEXITCODE -ne 0 -or -not $vs){throw 'MSVC x64 toolchain unavailable.'}
    $batch=Join-Path $output 'capture-msvc.cmd'
    @('@echo off',('call "{0}" -no_logo -arch=x64 -host_arch=x64 >nul' -f (Join-Path $vs 'Common7/Tools/VsDevCmd.bat')),
        'if errorlevel 1 exit /b 1','set VCToolsInstallDir','set INCLUDE','set LIB','set PATH','exit /b 0')|Set-Content -LiteralPath $batch -Encoding ascii
    $setup=RunOwned $env:ComSpec @('/d','/s','/c','capture-msvc.cmd') 'environment' 30000
    $setup['script']=$batch;$setup['scriptSha256']=Sha $batch;$record.environment=$setup
    if($setup.exitCode -ne 0 -or $setup.timedOut){throw 'MSVC environment initialization failed.'}
    foreach($line in [IO.File]::ReadAllLines($setup.stdout)){
        if($line -match '^(VCToolsInstallDir|INCLUDE|LIB|LIBPATH|PATH)=(.*)$'){[Environment]::SetEnvironmentVariable($Matches[1],$Matches[2],'Process')}
    }
    $compiler=Join-Path $env:VCToolsInstallDir 'bin/Hostx64/x64/cl.exe'
    $exe=Join-Path $output 'native-weather-win32-tests.exe'
    $cpp=@('Client/game_de/NativeWeatherRainBridge.cpp','Client/game_de/NativeWeatherRainLeaf.cpp','Client/game_de/NativeWeatherOverrideWin32.cpp','Client/game_de/NativeWeatherOverrideAtomic.cpp','Client/game_de/NativeWeatherOverridePlacement.cpp','Client/game_de/NativeWeatherOverrideSlot.cpp',
        'Client/game_de/NativeWeatherOverridePlan.cpp','Client/game_de/NativeWeatherOverrideProfile.cpp',
        'Client/game_de/NativeWeatherScalarProfile.cpp','Client/game_de/NativeWeatherTransitionProfile.cpp','Client/game_de/NativeImageAccess.cpp',
        'Tests/definitive/NativeWeatherOverrideWin32Tests.cpp')
    $arguments=@('/nologo','/std:c++17','/EHsc','/W4','/WX','/O2','/MD','/DNOMINMAX','/DWIN32_LEAN_AND_MEAN',
        ('/I'+(Join-Path $root 'Client/sdk')),('/I'+(Join-Path $root 'Shared/sdk')))+
        @($cpp|ForEach-Object{Join-Path $root $_})+@(('/Fo'+$scratch+[IO.Path]::DirectorySeparatorChar),('/Fd'+(Join-Path $scratch 'fixture.pdb')),('/Fe'+$exe),'/link','bcrypt.lib')
    $result=[ordered]@{compiler=$compiler;compilerSha256=(Sha $compiler);compile=$null;test=$null;checks=0}
    $record.results+=$result
    $result.compile=RunOwned $compiler $arguments 'compile' 60000
    if($result.compile.exitCode -ne 0 -or $result.compile.timedOut){throw 'Actual Windows weather platform fixture compilation failed.'}
    $result.executableSha256=Sha $exe
    $result.test=RunOwned $exe @($output) 'test' 30000
    $summary=[regex]::Match([IO.File]::ReadAllText($result.test.stdout),'(?m)^Passed (\d+) actual Windows weather platform checks \(64-bit\)\r?$')
    if($summary.Success){$result.checks=[int]$summary.Groups[1].Value}
    if($result.test.exitCode -ne 0 -or $result.test.timedOut -or $result.checks -le 0){throw 'Actual Windows weather platform checks failed.'}
    $record.success=$true
}catch{$record.failure=$_.Exception.ToString()}
finally{
    foreach($name in $oldEnvironment.Keys){[Environment]::SetEnvironmentVariable($name,$oldEnvironment[$name],'Process')}
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
@{success=$record.success;path=$receipt;sha256=(Sha $receipt);checks=($record.results.checks|Measure-Object -Sum).Sum;failure=$record.failure}|ConvertTo-Json -Depth 6 -Compress
if(-not $record.success){throw ('Actual Windows weather platform validation failed; see '+$receipt)}
