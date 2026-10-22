#requires -Version 7.0
# Run only an owned ordinary host and sparse SEC_IMAGE; no game launch/calls.
[CmdletBinding()]
param()
$ErrorActionPreference='Stop'
Set-StrictMode -Version Latest
$root=(Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../..')).Path
$id=(Get-Date -Format 'yyyyMMdd-HHmmss')+'-'+[guid]::NewGuid().ToString('N')
$output=Join-Path $root ('Build/mta-native-weapon-stat-extras/'+$id)
$scratch=Join-Path $root ('Build/tmp/native-weapon-stat-extras-'+$id)
[IO.Directory]::CreateDirectory($output)|Out-Null
[IO.Directory]::CreateDirectory($scratch)|Out-Null
$utf8=[Text.UTF8Encoding]::new($false)
function Sha([string]$p){(Get-FileHash -LiteralPath $p -Algorithm SHA256).Hash.ToLowerInvariant()}
$previous=@{}
foreach($name in @('TEMP','TMP','PATH','INCLUDE','LIB','LIBPATH','VCToolsInstallDir')){$previous[$name]=[Environment]::GetEnvironmentVariable($name,'Process')}
$record=[ordered]@{schema='mta_native_weapon_stat_extra_tests_v1';success=$false;failure=$null;sources=@();results=@();architecture='x64';
    nativeEngineCodeExecuted=$false;gameLaunched=$false;gameAttached=$false;engineMemoryWritten=$false;hookInstalled=$false;
    completeGameReady=$false;completeWeaponInfoReady=$false;nativeWeaponComboCountStorageTested=$false;generatedFixture=$null;sourcesUnchanged=$true;scratch=$scratch;scratchFilesRemaining=$null;
    scope='Existing exact-width weapon primitives extended with five timing scalars, native fire type, bounded aim indices, known default-combo IDs and checked native model-table identities. Uses the actual sparse OS image, native reader, existing same-owner access methods and fault/CAS seams. Combo count matches the classic raw BYTE storage intent, preserves the DefaultCombo neighbor and does not claim gameplay behavior for arbitrary values; no native function, game, hook or foreign process executes.'}
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
    $names=@('Client/game_de/NativeWeaponInfoExtraProfile.h','Client/game_de/NativeWeaponInfoProfile.h','Client/game_de/NativeWeaponInfoReader.h','Client/game_de/NativeWeaponInfoReader.cpp',
        'Client/game_de/CWeaponInfoReadDE.h','Client/game_de/CWeaponInfoReadDE.cpp','Client/sdk/game/CWeaponInfo.h','Client/game_sa/CWeaponStatSA.h',
        'Client/game_de/NativeImageAccess.h','Client/game_de/NativeImageAccess.cpp','Client/sdk/game/CGameAdapter.h','Client/sdk/game/CGameClockService.h',
        'Shared/sdk/CControllerState.h','Shared/sdk/CControllerActions.h','Shared/sdk/CControlStateSynthesis.h',
        'Tests/definitive/NativeWeaponInfoReaderTests.cpp','Tests/definitive/test_native_weapon_info.ps1','Tests/definitive/NativeWeaponStatExtraTests.cpp','Tests/definitive/test_native_weapon_stat_extras.ps1',
        'Client/game_de/NativeWeaponInfoWriteAccess.h','Client/game_de/NativeWeaponInfoWriteMemory.h','Client/game_de/NativeWeaponInfoWriteMemory.cpp',
        'Client/game_de/NativeWeaponInfoWriteProfile.h','Client/game_de/CWeaponInfoWriteDE.h','Client/game_de/CWeaponInfoWriteDE.cpp',
        'Build/definitive-recovery/mta-world-services-batch-20261021/weapon-native/native-weapon-info-qualification.json',
        'Build/definitive-recovery/mta-weapon-stat-native-batch-20261021/native-proof/native-weapon-stat-extra-qualification.json',
        'Build/definitive-recovery/mta-native-weapon-combo-count-20261022/combo-count-storage-qualified.json')
    $record.sources=@($names|ForEach-Object{[ordered]@{path=$_;sha256=(Sha (Join-Path $root $_))}})
    # Generate only the SDK mutations and pointer accessor left intentionally
    # pure by the read-only production slice. No implemented getter is replaced.
    $header=[IO.File]::ReadAllText((Join-Path $root 'Client/game_de/CWeaponInfoReadDE.h'))
    $provided=@([regex]::Matches($header,'(?<name>\w+)\s*\([^;{}]*\)\s*override\s*;')|ForEach-Object{$_.Groups['name'].Value})
    if($provided.Count -ne 31){throw 'Expected thirty-one actual SDK scalar/flag getter overrides'}
    $sdk=[IO.File]::ReadAllText((Join-Path $root 'Client/sdk/game/CWeaponInfo.h'))
    $sdk=[regex]::Replace($sdk,'(?s)/\*.*?\*/|//[^\r\n]*','')
    $methods=@()
    foreach($match in [regex]::Matches($sdk,'virtual\s+(?<signature>[^;{}]+);')){
        $signature=[regex]::Replace($match.Groups['signature'].Value,'\s*=\s*0\s*$','').Trim()
        $name=[regex]::Match($signature,'(?<name>\w+)\s*\(').Groups['name'].Value
        if(-not $name){throw 'Unrecognized weapon SDK declaration'}
        if($provided -contains $name){continue}
        $signature=[regex]::Replace($signature,'\s*=\s*[^,)]+','')
        $methods+=([regex]::Replace($signature,'\s+',' ')+' override { Unqualified(); }')
    }
    $generated=Join-Path $scratch 'WeaponInfoReadFixture.generated.h'
    @('#pragma once','#pragma warning(push)','#pragma warning(disable : 4100)',
        'namespace WeaponTestHost { class WeaponFixtureBase : public mta::game_de::CWeaponInfoReadDE { public:',
        'WeaponFixtureBase(const mta::game_de::NativeWeaponInfoReader& reader, std::size_t slot) noexcept : CWeaponInfoReadDE(reader, slot) {}')+
        $methods+@('}; }','#pragma warning(pop)')|Set-Content -LiteralPath $generated -Encoding utf8NoBOM
    $savedGenerated=Join-Path $output 'WeaponInfoReadFixture.generated.h'
    Copy-Item -LiteralPath $generated -Destination $savedGenerated
    $record.generatedFixture=@{path=$savedGenerated;sha256=(Sha $savedGenerated);productionGetters=$provided;rejectingOverrides=$methods.Count}
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
    $exe=Join-Path $output 'native-weapon-stat-extra-tests.exe'
    $arguments=@('/nologo','/std:c++17','/EHsc','/W4','/WX','/O2','/MD','/DNOMINMAX','/DWIN32_LEAN_AND_MEAN',
        ('/I'+(Join-Path $root 'Client/sdk')),('/I'+(Join-Path $root 'Shared/sdk')),('/I'+$scratch),
        (Join-Path $PSScriptRoot 'NativeWeaponStatExtraTests.cpp'),('/Fo'+$scratch+[IO.Path]::DirectorySeparatorChar),
        ('/Fd'+(Join-Path $scratch 'compiler.pdb')),('/Fe'+$exe),'/link','bcrypt.lib')
    $result=[ordered]@{compiler=$compiler;compilerSha256=(Sha $compiler);compile=$null;test=$null;checks=0}
    $record.results+=$result
    $result.compile=RunOwned $compiler $arguments $output 'compile' 60000
    if($result.compile.exitCode -ne 0 -or $result.compile.timedOut){throw 'Native weapon-stat extra-field fixture compilation failed.'}
    $result.executableSha256=Sha $exe
    $result.test=RunOwned $exe @($output) $output 'test' 30000
    $summary=[regex]::Match([IO.File]::ReadAllText($result.test.stdout),'(?m)^Passed (\d+) native weapon-stat extra-field checks \(64-bit\)\r?$')
    if($summary.Success){$result.checks=[int]$summary.Groups[1].Value}
    if($result.test.exitCode -ne 0 -or $result.test.timedOut -or $result.checks -le 0){throw 'Native weapon-stat extra-field checks failed.'}
    $record.nativeWeaponComboCountStorageTested=[IO.File]::ReadAllText($result.test.stdout).Contains('Production native weapon combo-count byte storage fixture completed')
    if(-not $record.nativeWeaponComboCountStorageTested){throw 'Native combo-count storage fixture marker absent.'}
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
if(-not $record.success){throw ('Native weapon-stat extra-field validation failed; see '+$receipt)}
