#requires -Version 7.0
# Test only the prepared dependency HALF; no parent physical attachment or detach ACK.
[CmdletBinding()]
param()
$ErrorActionPreference='Stop'
Set-StrictMode -Version Latest
$root=(Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../..')).Path
$id=(Get-Date -Format 'yyyyMMdd-HHmmss')+'-'+[guid]::NewGuid().ToString('N')
$output=Join-Path $root ('Build/mta-native-prepared-array-dependencies/'+$id)
$scratch=Join-Path $root ('Build/tmp/native-prepared-array-dependencies-'+$id)
[IO.Directory]::CreateDirectory($output)|Out-Null
[IO.Directory]::CreateDirectory($scratch)|Out-Null
$utf8=[Text.UTF8Encoding]::new($false)
function Sha([string]$p){(Get-FileHash -LiteralPath $p -Algorithm SHA256).Hash.ToLowerInvariant()}
$previous=@{}
foreach($name in @('TEMP','TMP','PATH','INCLUDE','LIB','LIBPATH','VCToolsInstallDir')){$previous[$name]=[Environment]::GetEnvironmentVariable($name,'Process')}
$record=[ordered]@{schema='mta_native_prepared_array_dependency_half_tests_v1';success=$false;failure=$null;directory=$output;sources=@();results=@();architecture='x64';
    nativePreparedArrayDependencyHalfTested=$false;actualParentDetachAcknowledged=$false;publicFactoryImplemented=$false;nativeEngineCodeExecuted=$false;gameLaunched=$false;gameAttached=$false;installationChanged=$false;
    completeGameReady=$false;completePedReady=$false;remoteDataOwnerReady=$false;runtimeEndTested=$false;fourSlotLimitTested=$false;runtimeModulePinTested=$false;sourcesUnchanged=$true;scratch=$scratch;scratchRemoved=$false;
    scope='Prepared dependency HALF only: issued array plus deduplicated actual frame holds, one qualified borrowed epoch, blocked free/fair retirement and frame-edge freeze, copied intent, span guards and uncertainty retention. Action-return-Ok never proves parent attachment/detach or releases holds. Held cases run in separate child processes; their process exit is not production End/module/native-resource recovery.'}
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
        if($timedOut){$process.Kill($true);if(-not $process.WaitForExit(10000)){throw 'Owned child did not terminate'}}
        $stdout=Join-Path $directory ($label+'.log');$stderr=Join-Path $directory ($label+'-errors.log')
        [IO.File]::WriteAllText($stdout,$out.GetAwaiter().GetResult(),$utf8);[IO.File]::WriteAllText($stderr,$err.GetAwaiter().GetResult(),$utf8)
        return [ordered]@{exitCode=$process.ExitCode;timedOut=$timedOut;stdout=$stdout;stderr=$stderr;stdoutSha256=(Sha $stdout);stderrSha256=(Sha $stderr)}
    }finally{$process.Dispose()}
}
try{
    $pending=[Collections.Generic.Queue[string]]::new()
    $seen=[Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
    foreach($name in @('Tests/definitive/NativePreparedArrayDependencyTests.cpp','Tests/definitive/test_native_prepared_array_dependencies.ps1','Client/game_de/NativeAnimationSequenceName.cpp','Client/game_de/CKeyGenDE.cpp')){$pending.Enqueue($name)}
    while($pending.Count){
        $name=$pending.Dequeue()
        if(-not $seen.Add($name)){continue}
        $path=[IO.Path]::GetFullPath((Join-Path $root $name))
        if(-not $path.StartsWith($root+'\',[StringComparison]::OrdinalIgnoreCase) -or $path -match '(?i)(?:^|[\\/])r-engineer(?:[\\/]|$)'){throw 'Local include left the allowed source graph'}
        if(-not (Test-Path -LiteralPath $path -PathType Leaf)){throw ('Missing actual source input: '+$name)}
        foreach($match in [regex]::Matches([IO.File]::ReadAllText($path),'(?m)^\s*#\s*include\s*[<"](?<path>[^>"\n]+)[>"]')){
            $include=$match.Groups['path'].Value
            foreach($candidate in @((Join-Path (Split-Path $path) $include),(Join-Path $root ('Shared/sdk/'+$include)),(Join-Path $root ('Client/sdk/'+$include)))){
                if(Test-Path -LiteralPath $candidate -PathType Leaf){$pending.Enqueue([IO.Path]::GetRelativePath($root,[IO.Path]::GetFullPath($candidate)).Replace('\','/'));break}
            }
        }
    }
    $names=@($seen)+@(
        'Build/definitive-recovery/mta-private-prepared-dependencies-20261025/prepared-dependency-headers-source-ready.json',
        'Build/definitive-recovery/mta-private-prepared-dependencies-20261025/prepared-dependency-native-source-ready.json',
        'Build/definitive-recovery/mta-private-prepared-span-fix-20261025/prepared-dependency-span-source-ready.json')
    $record.sources=@($names|Sort-Object -Unique|ForEach-Object{[ordered]@{path=$_;sha256=(Sha (Join-Path $root $_))}})
    $env:TEMP=$scratch;$env:TMP=$scratch
    $vswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
    $vs=& $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    if($LASTEXITCODE -ne 0 -or -not $vs){throw 'MSVC x64 toolchain unavailable'}
    $batch=Join-Path $output 'capture-msvc.cmd'
    @('@echo off',('call "{0}" -no_logo -arch=x64 -host_arch=x64 >nul' -f (Join-Path $vs 'Common7/Tools/VsDevCmd.bat')),
        'if errorlevel 1 exit /b 1','set VCToolsInstallDir','set INCLUDE','set LIB','set PATH','exit /b 0')|Set-Content -LiteralPath $batch -Encoding ascii
    $setup=RunOwned $env:ComSpec @('/d','/s','/c','capture-msvc.cmd') $output 'environment' 30000
    $setup['script']=$batch;$setup['scriptSha256']=Sha $batch;$record.environment=$setup
    if($setup.exitCode -ne 0 -or $setup.timedOut){throw 'MSVC environment initialization failed'}
    foreach($line in [IO.File]::ReadAllLines($setup.stdout)){if($line -match '^(VCToolsInstallDir|INCLUDE|LIB|LIBPATH|PATH)=(.*)$'){[Environment]::SetEnvironmentVariable($Matches[1],$Matches[2],'Process')}}
    $compiler=Join-Path $env:VCToolsInstallDir 'bin/Hostx64/x64/cl.exe'
    $exe=Join-Path $output 'native-prepared-array-dependencies-tests.exe'
    $arguments=@('/nologo','/std:c++17','/EHsc','/W4','/WX','/O2','/MD','/DNOMINMAX','/DWIN32_LEAN_AND_MEAN',('/I'+(Join-Path $root 'Shared/sdk')),
        ('/I'+(Join-Path $root 'Client/sdk')),(Join-Path $PSScriptRoot 'NativePreparedArrayDependencyTests.cpp'),(Join-Path $root 'Client/game_de/NativeAnimationSequenceName.cpp'),(Join-Path $root 'Client/game_de/CKeyGenDE.cpp'),('/Fo'+$scratch+'\'),('/Fd'+(Join-Path $scratch 'compiler.pdb')),
        ('/Fe'+$exe),'/link','bcrypt.lib')
    $compile=RunOwned $compiler $arguments $output 'compile' 120000
    if($compile.exitCode -ne 0 -or $compile.timedOut){throw 'Actual prepared dependency HALF fixture compilation failed'}
    $allPassed=$true
    foreach($case in @('baseline','held','foreign','owner','span','stride')){
        $caseDirectory=Join-Path $output $case
        [IO.Directory]::CreateDirectory($caseDirectory)|Out-Null
        $test=RunOwned $exe @($caseDirectory,$case) $caseDirectory 'test' 60000
        $text=[IO.File]::ReadAllText($test.stdout)
        $summary=[regex]::Match($text,('(?m)^Passed (\d+) prepared dependency HALF checks \('+[regex]::Escape($case)+'; no parent attach/detach ACK\)\r?$'))
        $count=if($summary.Success){[int]$summary.Groups[1].Value}else{0}
        $passed=$test.exitCode -eq 0 -and -not $test.timedOut -and $count -gt 0 -and $text.Contains('Production prepared dependency HALF fixture completed')
        $record.results+=@{scenario=$case;test=$test;checks=$count;passed=$passed;separateChildProcess=$true;heldProcessExitIsRecovery=$false}
        $allPassed=$allPassed -and $passed
    }
    $record.compile=$compile
    $record.executable=@{path=$exe;sha256=(Sha $exe)}
    $record.nativePreparedArrayDependencyHalfTested=$allPassed
    $record.success=$allPassed
    if(-not $record.success){throw 'Actual prepared dependency HALF fixture checks failed'}
}catch{$record.failure=$_.Exception.ToString();$record.success=$false}
finally{
    foreach($entry in $record.sources){if((Sha (Join-Path $root $entry.path)) -cne $entry.sha256){$record.sourcesUnchanged=$false}}
    foreach($name in $previous.Keys){[Environment]::SetEnvironmentVariable($name,$previous[$name],'Process')}
    try{
        $absolute=[IO.Path]::GetFullPath($scratch);$allowed=[IO.Path]::GetFullPath((Join-Path $root 'Build/tmp')).TrimEnd('\')+'\'
        if(-not $absolute.StartsWith($allowed,[StringComparison]::OrdinalIgnoreCase) -or (Get-Item -LiteralPath $absolute -Force).Attributes -band [IO.FileAttributes]::ReparsePoint){throw 'Owned flat scratch left task directory'}
        $entries=@(Get-ChildItem -LiteralPath $absolute -Force)
        foreach($entry in $entries){if($entry.PSIsContainer -or $entry.Attributes -band [IO.FileAttributes]::ReparsePoint){throw 'Unexpected nested or linked scratch preserved'}}
        foreach($entry in $entries){Remove-Item -LiteralPath $entry.FullName -Force}
        [IO.Directory]::Delete($absolute,$false);$record.scratchRemoved=$true
    }catch{$record.success=$false;$record.failure=($record.failure+[Environment]::NewLine+$_.Exception.ToString()).Trim()}
    $record.success=$record.success -and $record.sourcesUnchanged -and $record.scratchRemoved
    $record.finishedAt=[DateTimeOffset]::Now.ToString('o')
    $record|ConvertTo-Json -Depth 14|Set-Content -LiteralPath (Join-Path $output 'test-result.json') -Encoding utf8NoBOM
}
@{success=$record.success;receipt=(Join-Path $output 'test-result.json');failure=$record.failure}|ConvertTo-Json -Compress
if(-not $record.success){throw ('Native prepared dependency HALF tests failed; see '+$output)}
