#requires -Version 7.0
# Execute only owned ordinary hosts. Controlled image/page dependencies
# authorize no game/native function and writes touch owned fixture memory only.
[CmdletBinding()]
param()
$ErrorActionPreference='Stop'
Set-StrictMode -Version Latest
$root=(Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../..')).Path
$id=(Get-Date -Format 'yyyyMMdd-HHmmss')+'-'+[guid]::NewGuid().ToString('N')
$output=Join-Path $root ('Build/mta-native-weather-scalar/'+$id)
$scratch=Join-Path $root ('Build/tmp/native-weather-scalar-'+$id)
[IO.Directory]::CreateDirectory($output)|Out-Null
[IO.Directory]::CreateDirectory($scratch)|Out-Null
$utf8=[Text.UTF8Encoding]::new($false)
function Sha([string]$p){(Get-FileHash -LiteralPath $p -Algorithm SHA256).Hash.ToLowerInvariant()}
$oldEnvironment=@{}
foreach($name in @('TEMP','TMP','PATH','INCLUDE','LIB','LIBPATH','VCToolsInstallDir')){$oldEnvironment[$name]=[Environment]::GetEnvironmentVariable($name,'Process')}
$record=[ordered]@{schema='mta_native_weather_scalar_tests_v1';success=$false;failure=$null;sources=@();results=@();architecture='x64';
    nativeEngineCodeExecuted=$false;gameLaunched=$false;gameAttached=$false;engineMemoryWritten=$false;hookInstalled=$false;
    completeGameReady=$false;completeWeatherReady=$false;nativeLifecycleReady=$false;sourcesUnchanged=$true;scratch=$scratch;scratchFilesRemaining=$null;
    scope='Actual eight CWeather scalar getters through production SDK methods and native read-only memory. Shipping wrong-image refusal, shared native transition identity plus13 producer/consumer code pins, seven ASLR-aware enum/property name records, RainbowIntensity and RainbowMesh native parameter relationships. Controlled owned memory tests distinct fields, refusal/output preservation, finite out-of-range values, nonfinite refusal, protection races and exactly-once real SDK dispatch. Scalar Set/Reset stay abstract; no engine/native function, hook, game or full weather readiness.'}
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
    $names=@('Client/game_de/NativeWeatherScalarProfile.h','Client/game_de/NativeWeatherScalarProfile.cpp','Client/game_de/NativeWeatherScalarMemory.h',
        'Client/game_de/CWeatherScalarsDE.h','Client/game_de/CWeatherScalarsDE.cpp','Client/game_de/CWeatherTransitionDE.h','Client/game_de/CWeatherTransitionDE.cpp',
        'Client/game_de/NativeWeatherTransitionProfile.h','Client/game_de/NativeWeatherTransitionProfile.cpp',
        'Client/game_de/NativeClockProfile.h','Client/game_de/NativeImageAccess.h','Client/game_de/NativeImageAccess.cpp',
        'Client/sdk/game/CWeather.h','Client/sdk/game/Common.h','Client/sdk/game/CGameAdapter.h','Client/sdk/game/CGameClockService.h',
        'Shared/sdk/CControllerState.h','Shared/sdk/CControlStateSynthesis.h',
        'Tests/definitive/NativeWeatherScalarTests.cpp','Tests/definitive/test_native_weather_scalars.ps1',
        'Build/definitive-recovery/mta-weather-scalars-batch-20261021/proof/scalar-getter-qualification.json')
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
    foreach($mode in @('shipping-wrong-image','controlled-image')){
        $directory=Join-Path $output $mode;[IO.Directory]::CreateDirectory($directory)|Out-Null
        $exe=Join-Path $directory 'native-weather-scalar-tests.exe'
        $arguments=@('/nologo','/std:c++17','/EHsc','/W4','/WX','/O2','/MD','/DNOMINMAX','/DWIN32_LEAN_AND_MEAN')
        if($mode -eq 'controlled-image'){$arguments+='/DMTA_TEST_WEATHER_SCALAR_CONTROLLED=1'}
        $arguments+=@(('/I'+(Join-Path $root 'Client/sdk')),('/I'+(Join-Path $root 'Shared/sdk')),(Join-Path $PSScriptRoot 'NativeWeatherScalarTests.cpp'),(Join-Path $root 'Client/game_de/NativeImageAccess.cpp'),(Join-Path $root 'Client/game_de/CWeatherScalarsDE.cpp'),(Join-Path $root 'Client/game_de/CWeatherTransitionDE.cpp'),
            ('/Fo'+$directory+[IO.Path]::DirectorySeparatorChar),('/Fe'+$exe),'/link','bcrypt.lib')
        $result=[ordered]@{mode=$mode;compiler=$compiler;compilerSha256=(Sha $compiler);compile=$null;test=$null;checks=0}
        $record.results+=$result
        $result.compile=RunOwned $compiler $arguments $directory 'compile' 60000
        if($result.compile.exitCode -ne 0 -or $result.compile.timedOut){throw ('Native profile fixture compilation failed: '+$mode)}
        $result.executableSha256=Sha $exe
        $result.test=RunOwned $exe @() $directory 'test' 10000
        $summary=[regex]::Match([IO.File]::ReadAllText($result.test.stdout),'(?m)^Passed (\d+) native weather scalar checks \(64-bit; ([a-z-]+)\)\r?$')
        if($summary.Success){$result.checks=[int]$summary.Groups[1].Value}
        if($result.test.exitCode -ne 0 -or $result.test.timedOut -or $result.checks -le 0 -or $summary.Groups[2].Value -cne $mode){throw ('Native profile checks failed: '+$mode)}
    }
    $record.success=$record.results.Count -eq 2
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
@{success=$record.success;path=$receipt;sha256=(Sha $receipt);results=$record.results}|ConvertTo-Json -Depth 6 -Compress
if(-not $record.success){throw ('Native weather scalar validation failed; see '+$receipt)}
