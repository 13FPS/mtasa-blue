#requires -Version 7.0
# Read-only production access and actual SDK forwarding; no engine code executes.
[CmdletBinding()]
param()
$ErrorActionPreference='Stop'
Set-StrictMode -Version Latest
$root=(Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../..')).Path
$id=(Get-Date -Format 'yyyyMMdd-HHmmss')+'-'+[guid]::NewGuid().ToString('N')
$output=Join-Path $root ('Build/mta-native-gravity-read/'+$id)
$scratch=Join-Path $root ('Build/tmp/native-gravity-read-'+$id)
[IO.Directory]::CreateDirectory($output)|Out-Null
[IO.Directory]::CreateDirectory($scratch)|Out-Null
$utf8=[Text.UTF8Encoding]::new($false)
function Sha([string]$p){(Get-FileHash -LiteralPath $p -Algorithm SHA256).Hash.ToLowerInvariant()}
$previous=@{}
foreach($name in @('TEMP','TMP','PATH','INCLUDE','LIB','LIBPATH','VCToolsInstallDir')){$previous[$name]=[Environment]::GetEnvironmentVariable($name,'Process')}
$record=[ordered]@{schema='mta_native_gravity_read_tests_v1';success=$false;failure=$null;sources=@();result=$null;sourcesUnchanged=$true;
    nativeEngineCodeExecuted=$false;gameLaunched=$false;gameAttached=$false;engineMemoryWritten=$false;installationChanged=$false;
    completeGameReady=$false;gravitySetterImplemented=$false;scratch=$scratch;scratchFilesRemaining=$null;
    scope='Actual gravity-read helper over owned modeled PE memory plus ordinary-host shipping page refusal; actual CGameTimingDE::GetGravity dispatch with controlled reader result. Exact three acceleration code pins, current finite coefficient bits, signed zero, nonfinite/overlap/refusal preservation, PE/code identity changes. No production timing-reader thread-admission mock is claimed as a live result; SetGravity stays pure.'}
function RunOwned([string]$exe,[string[]]$arguments,[string]$directory,[string]$name,[int]$deadline){
    $info=[Diagnostics.ProcessStartInfo]::new($exe)
    $info.WorkingDirectory=$directory;$info.UseShellExecute=$false;$info.CreateNoWindow=$true;$info.RedirectStandardOutput=$true;$info.RedirectStandardError=$true
    $info.Environment['TEMP']=$scratch;$info.Environment['TMP']=$scratch
    foreach($argument in $arguments){$info.ArgumentList.Add($argument)}
    $command=Join-Path $directory ($name+'-command.json')
    [IO.File]::WriteAllText($command,([ordered]@{executable=$exe;sha256=(Sha $exe);arguments=$arguments;timeoutMs=$deadline}|ConvertTo-Json -Depth 4),$utf8)
    $process=[Diagnostics.Process]::new();$process.StartInfo=$info
    try{
        if(-not $process.Start()){throw ('Cannot start owned '+$name)}
        $out=$process.StandardOutput.ReadToEndAsync();$err=$process.StandardError.ReadToEndAsync()
        $timedOut=-not $process.WaitForExit($deadline)
        if($timedOut){$process.Kill($true);if(-not $process.WaitForExit(10000)){throw 'Owned child did not terminate.'}}
        $stdout=Join-Path $directory ($name+'.log');$stderr=Join-Path $directory ($name+'-errors.log')
        [IO.File]::WriteAllText($stdout,$out.GetAwaiter().GetResult(),$utf8);[IO.File]::WriteAllText($stderr,$err.GetAwaiter().GetResult(),$utf8)
        return [ordered]@{exitCode=$process.ExitCode;timedOut=$timedOut;stdout=$stdout;stderr=$stderr;command=$command;commandSha256=(Sha $command);stdoutSha256=(Sha $stdout);stderrSha256=(Sha $stderr)}
    }finally{$process.Dispose()}
}
try{
    $names=@('Client/game_de/NativeGravityReadProfile.h','Client/game_de/NativeGravityReadAccess.h','Client/game_de/NativeGravityReadAccess.cpp',
        'Client/game_de/CGameTimingDE.h','Client/game_de/CGameTimingDE.cpp','Client/game_de/NativeGameTimingReader.h','Client/game_de/NativeGameTimingReader.cpp',
        'Client/game_de/NativeImageAccess.h','Client/game_de/NativeImageAccess.cpp','Client/sdk/game/CGame.h','Client/sdk/game/CGameAdapter.h',
        'Client/sdk/game/CGameClockService.h','Client/game_de/NativeGameTimingAccess.h','Shared/sdk/CControllerState.h',
        'Tests/definitive/NativeGravityReadTests.cpp','Tests/definitive/test_native_gravity_read.ps1')
    $record.sources=@($names|ForEach-Object{[ordered]@{path=$_;sha256=(Sha (Join-Path $root $_))}})
    # Generate rejecting implementations only for the still-missing SDK methods;
    # the tested GetGravity method must come from the production translation unit.
    $providedText=[IO.File]::ReadAllText((Join-Path $root 'Client/game_de/CGameTimingDE.h'))
    $provided=@([regex]::Matches($providedText,'(?<name>\w+)\s*\([^;{}]*\)\s*(?:const\s*)?(?:noexcept\s*)?override\s*;')|ForEach-Object{$_.Groups['name'].Value})
    if($provided -notcontains 'GetGravity' -or $provided -contains 'SetGravity'){throw 'Gravity getter-only SDK scope changed.'}
    $text=[IO.File]::ReadAllText((Join-Path $root 'Client/sdk/game/CGame.h'))
    $text=[regex]::Replace($text,'(?s)/\*.*?\*/|//[^\r\n]*','')
    $lines=@('#pragma once','#pragma warning(push)','#pragma warning(disable : 4100)','namespace fixture {','class TimingFixture final : public mta::game_de::CGameTimingDE { public:',
        'TimingFixture(const mta::game_de::NativeGameTimingReader& reader, mta::game_de::NativeGameTimingAccess& owner) noexcept : CGameTimingDE(reader, owner) {}')
    $rejecting=0
    foreach($match in [regex]::Matches($text,'virtual\s+(?<signature>[^;{}]+);')){
        $signature=[regex]::Replace($match.Groups['signature'].Value,'\s*=\s*0\s*$','').Trim()
        $name=[regex]::Match($signature,'(?<name>\w+)\s*\(').Groups['name'].Value
        if(-not $name){throw ('Cannot derive rejecting method: '+$signature)}
        if($provided -contains $name){continue}
        $signature=[regex]::Replace($signature,'\s*=\s*[^,)]+','')
        $refusal=if($signature -match '\bnoexcept\b'){'UnqualifiedNoexcept();'}else{'Unqualified();'}
        $lines+=([regex]::Replace($signature,'\s+',' ')+' override { '+$refusal+' }');$rejecting++
    }
    $lines+=@('};','}','#pragma warning(pop)')
    $generated=Join-Path $scratch 'GravityTimingFixture.generated.h'
    [IO.File]::WriteAllText($generated,($lines -join "`n")+"`n",$utf8)
    $durableGenerated=Join-Path $output 'GravityTimingFixture.generated.h';Copy-Item -LiteralPath $generated -Destination $durableGenerated
    $record.generatedFixture=[ordered]@{path=$durableGenerated;sha256=(Sha $generated);rejectingMethods=$rejecting;productionMethods=$provided}
    $env:TEMP=$scratch;$env:TMP=$scratch
    $vswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
    $vs=& $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    if($LASTEXITCODE -ne 0 -or -not $vs){throw 'MSVC x64 toolchain unavailable.'}
    $batch=Join-Path $output 'capture-msvc.cmd'
    @('@echo off',('call "{0}" -no_logo -arch=x64 -host_arch=x64 >nul' -f (Join-Path $vs 'Common7/Tools/VsDevCmd.bat')),
        'if errorlevel 1 exit /b 1','set VCToolsInstallDir','set INCLUDE','set LIB','set PATH','exit /b 0')|Set-Content -LiteralPath $batch -Encoding ascii
    $setup=RunOwned $env:ComSpec @('/d','/s','/c','capture-msvc.cmd') $output 'environment' 30000
    $record.environment=$setup
    if($setup.exitCode -ne 0 -or $setup.timedOut){throw 'MSVC environment failed.'}
    foreach($line in [IO.File]::ReadAllLines($setup.stdout)){if($line -match '^(VCToolsInstallDir|INCLUDE|LIB|LIBPATH|PATH)=(.*)$'){[Environment]::SetEnvironmentVariable($Matches[1],$Matches[2],'Process')}}
    $compiler=Join-Path $env:VCToolsInstallDir 'bin/Hostx64/x64/cl.exe';$exe=Join-Path $output 'native-gravity-read-tests.exe'
    $arguments=@('/nologo','/std:c++17','/EHsc','/W4','/WX','/O2','/MD','/DNOMINMAX','/DWIN32_LEAN_AND_MEAN',
        ('/I'+(Join-Path $root 'Client/sdk')),('/I'+(Join-Path $root 'Shared/sdk')),('/I'+$scratch),
        (Join-Path $PSScriptRoot 'NativeGravityReadTests.cpp'),(Join-Path $root 'Client/game_de/NativeGravityReadAccess.cpp'),
        (Join-Path $root 'Client/game_de/NativeImageAccess.cpp'),(Join-Path $root 'Client/game_de/CGameTimingDE.cpp'),
        ('/Fo'+$scratch+[IO.Path]::DirectorySeparatorChar),('/Fd'+(Join-Path $scratch 'gravity.pdb')),('/Fe'+$exe),'/link','bcrypt.lib')
    $result=[ordered]@{compiler=$compiler;compilerSha256=(Sha $compiler);compile=$null;test=$null;checks=0};$record.result=$result
    $result.compile=RunOwned $compiler $arguments $output 'compile' 60000
    if($result.compile.exitCode -ne 0 -or $result.compile.timedOut){throw 'Gravity read fixture compilation failed.'}
    $result.executableSha256=Sha $exe;$result.test=RunOwned $exe @() $output 'test' 10000
    $summary=[regex]::Match([IO.File]::ReadAllText($result.test.stdout),'(?m)^Passed (\d+) native gravity read checks \(64-bit; owned-image-and-sdk\)\r?$')
    if($summary.Success){$result.checks=[int]$summary.Groups[1].Value}
    if($result.test.exitCode -ne 0 -or $result.test.timedOut -or $result.checks -le 0){throw 'Gravity read assertions failed.'}
    $record.success=$true
}catch{$record.failure=$_.Exception.ToString()}
finally{
    foreach($name in $previous.Keys){[Environment]::SetEnvironmentVariable($name,$previous[$name],'Process')}
    foreach($source in $record.sources){if((Sha (Join-Path $root $source.path)) -ne $source.sha256){$record.sourcesUnchanged=$false}}
    $record.success=$record.success -and $record.sourcesUnchanged
    $resolved=[IO.Path]::GetFullPath($scratch);$allowed=[IO.Path]::GetFullPath((Join-Path $root 'Build/tmp')).TrimEnd('\')+'\'
    if(-not $resolved.StartsWith($allowed,[StringComparison]::OrdinalIgnoreCase)){throw 'Scratch escaped the workspace.'}
    foreach($file in @(Get-ChildItem -LiteralPath $scratch -Force)){if(-not $file.PSIsContainer -and -not ($file.Attributes -band [IO.FileAttributes]::ReparsePoint)){Remove-Item -LiteralPath $file.FullName -Force}}
    $record.scratchFilesRemaining=@(Get-ChildItem -LiteralPath $scratch -Force).Count
    if(-not $record.scratchFilesRemaining){Remove-Item -LiteralPath $scratch}
    $receipt=Join-Path $output 'test-result.json';[IO.File]::WriteAllText($receipt,($record|ConvertTo-Json -Depth 12)+"`n",$utf8)
}
@{success=$record.success;path=$receipt;sha256=(Sha $receipt);result=$record.result}|ConvertTo-Json -Depth 6 -Compress
if(-not $record.success){throw ('Gravity read validation failed; see '+$receipt)}
