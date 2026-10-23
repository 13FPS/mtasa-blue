#requires -Version 7.0
# Execute only a typed call seam over owned images, never GTA entry code.
[CmdletBinding()]
param()
$ErrorActionPreference='Stop'
Set-StrictMode -Version Latest
$root=(Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../..')).Path
$id=(Get-Date -Format 'yyyyMMdd-HHmmss')+'-'+[guid]::NewGuid().ToString('N')
$output=Join-Path $root ('Build/mta-borrowed-ped-factory-de/'+$id)
$scratch=Join-Path $root ('Build/tmp/borrowed-ped-factory-de-'+$id)
[IO.Directory]::CreateDirectory($output)|Out-Null
[IO.Directory]::CreateDirectory($scratch)|Out-Null
$utf8=[Text.UTF8Encoding]::new($false)
function Sha([string]$p){(Get-FileHash -LiteralPath $p -Algorithm SHA256).Hash.ToLowerInvariant()}
$previous=@{}
foreach($name in @('TEMP','TMP','PATH','INCLUDE','LIB','LIBPATH','VCToolsInstallDir')){$previous[$name]=[Environment]::GetEnvironmentVariable($name,'Process')}
$record=[ordered]@{schema='mta_borrowed_ped_factory_tests_v1';success=$false;failure=$null;sources=@();results=@();architecture='x64';
    nativeEngineCodeExecuted=$false;nativePoolObservedInGta=$false;actualBorrowedPedFactoryLifetimeTested=$false;nativeBorrowedPedFactoryControllerTested=$false;explicitBorrowedPedRetirementContractTested=$false;
    gameLaunched=$false;gameAttached=$false;engineMemoryWritten=$false;hookInstalled=$false;
    completeGameReady=$false;completePoolsReady=$false;localPlayerRegistered=$false;sourcesUnchanged=$true;scratch=$scratch;scratchFilesRemaining=$null;
    scope='Actual typed borrowed wrapper lifecycle over the canonical CPoolsDE registry and the existing native reference Memory/Controller on owned SEC_IMAGE/pool/bank pages. Complete concrete rejecting test-only SDK caller required. One acquired token; exact constructor binding; canonical guarded publication; withdrawal before typed destruction and one-shot Release; foreign associations retain the original; constructor failure and entered native uncertainty preserve debt. Only existing final registrar/unregistrar boundaries substituted. Historical fixture mains are compiled for scaffold reuse but not executed. No native ped/player allocator, complete product factory or second reference registry.'}
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
    $names=@('Client/game_de/CPedBorrowedFactoryDE.h','Client/game_de/CPoolsDE.h','Client/game_de/CPoolsDE.cpp',
        'Client/game_de/CPedVitalsDE.h','Client/game_de/CPedVitalsDE.cpp',
        'Client/game_de/NativePedReferenceController.h','Client/game_de/NativePedReferenceController.cpp',
        'Tests/definitive/PedBorrowedFactoryDETests.cpp','Tests/definitive/test_ped_borrowed_factory_de.ps1',
        'Tests/definitive/Write-PedBorrowedFactoryFixture.ps1','Tests/definitive/Write-PedPoolRegistryFixture.ps1',
        'Build/definitive-recovery/mta-borrowed-ped-factory-20261023/borrowed-factory-qualified.json',
        'Build/definitive-recovery/mta-ped-reference-batch-20261021/native-proof/ped-reference-qualified.json',
        'Build/definitive-recovery/mta-ped-pool-handle-batch-20261021/native-proof/ped-pool-qualified.json')
    # Capture the exact bounded local include graph used by the real SDK and
    # native-owner interfaces, including the rejecting fixture generator's SDK
    # inputs. System headers remain the compiler/toolchain's own dependency.
    $pending=[Collections.Generic.Queue[string]]::new()
    foreach($name in $names){$pending.Enqueue($name)}
    $seen=[Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
    while($pending.Count){
        $name=$pending.Dequeue()
        if(-not $seen.Add($name)){continue}
        $path=Join-Path $root $name
        if([IO.Path]::GetExtension($path) -notin @('.h','.cpp','.inl')){continue}
        foreach($match in [regex]::Matches([IO.File]::ReadAllText($path),'#include\s*["<]([^">]+)[">]')){
            $include=$match.Groups[1].Value
            $candidates=@((Join-Path (Split-Path -Parent $path) $include),(Join-Path $root ('Client/sdk/'+$include)),(Join-Path $root ('Shared/sdk/'+$include)))
            foreach($candidate in $candidates){
                if(-not (Test-Path -LiteralPath $candidate -PathType Leaf)){continue}
                $full=[IO.Path]::GetFullPath($candidate)
                if(-not $full.StartsWith($root.TrimEnd('\')+'\',[StringComparison]::OrdinalIgnoreCase)){throw 'Local include escaped repository.'}
                $relative=[IO.Path]::GetRelativePath($root,$full).Replace('\','/')
                $pending.Enqueue($relative)
                break
            }
        }
    }
    $record.sources=@($seen|Sort-Object|ForEach-Object{[ordered]@{path=$_;sha256=(Sha (Join-Path $root $_))}})
    . (Join-Path $PSScriptRoot 'Write-PedBorrowedFactoryFixture.ps1')
    $generated=Join-Path $scratch 'PedBorrowedFactoryFixture.generated.h'
    $record['generatedFixture']=Write-PedBorrowedFactoryFixture $root $generated
    $durable=Join-Path $output 'PedBorrowedFactoryFixture.generated.h'
    Copy-Item -LiteralPath $generated -Destination $durable
    $record['generatedFixture']['durablePath']=$durable
    $record['generatedFixture']['durableSha256']=Sha $durable
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
    $exe=Join-Path $output 'ped-borrowed-factory-tests.exe'
    $arguments=@('/nologo','/std:c++17','/EHsc','/W4','/WX','/O2','/MD','/DNOMINMAX','/DWIN32_LEAN_AND_MEAN',
        ('/I'+(Join-Path $root 'Client/sdk')),('/I'+(Join-Path $root 'Shared/sdk')),('/I'+$scratch),
        (Join-Path $PSScriptRoot 'PedBorrowedFactoryDETests.cpp'),(Join-Path $root 'Client/game_de/CPoolsDE.cpp'),(Join-Path $root 'Client/game_de/CPedVitalsDE.cpp'),
        (Join-Path $root 'Client/game_de/NativePedReferenceController.cpp'),('/Fo'+$scratch+[IO.Path]::DirectorySeparatorChar),
        ('/Fd'+(Join-Path $scratch 'compiler.pdb')),('/Fe'+$exe),'/link','bcrypt.lib')
    $result=[ordered]@{compiler=$compiler;compilerSha256=(Sha $compiler);compile=$null;test=$null;checks=0}
    $record.results+=$result
    $result.compile=RunOwned $compiler $arguments $output 'compile' 60000
    if($result.compile.exitCode -ne 0 -or $result.compile.timedOut){throw 'Actual borrowed ped factory fixture compilation failed.'}
    $result.executableSha256=Sha $exe
    $result.test=RunOwned $exe @($output) $output 'test' 30000
    $summary=[regex]::Match([IO.File]::ReadAllText($result.test.stdout),'(?m)^Passed (\d+) actual borrowed ped factory checks \(64-bit; no native engine code executed\)\r?$')
    if($summary.Success){$result.checks=[int]$summary.Groups[1].Value}
    if($result.test.exitCode -ne 0 -or $result.test.timedOut -or $result.checks -le 0){throw 'Actual borrowed ped factory checks failed.'}
    $stdout=[IO.File]::ReadAllText($result.test.stdout)
    $record.actualBorrowedPedFactoryLifetimeTested=$stdout.Contains('Production typed borrowed ped factory lifetime fixture completed')
    $record.nativeBorrowedPedFactoryControllerTested=$stdout.Contains('Production borrowed ped factory existing native controller fixture completed')
    if(-not $record.actualBorrowedPedFactoryLifetimeTested -or -not $record.nativeBorrowedPedFactoryControllerTested){throw 'Typed factory/controller executed markers missing.'}
    # One owned child proves the destructor's explicit-lifetime fail-fast path;
    # it installs a process-local terminate handler and never enters GTA code.
    $child=Join-Path $output 'unretired-child'
    [IO.Directory]::CreateDirectory($child)|Out-Null
    $record['explicitRetirementContract']=RunOwned $exe @($child,'--unterminated') $child 'unretired-contract' 30000
    $contract=$record.explicitRetirementContract
    $record.explicitBorrowedPedRetirementContractTested=($contract.exitCode -eq 91 -and -not $contract.timedOut -and
        [IO.File]::ReadAllText($contract.stdout).Contains('Intentional unretired borrowed factory contract check'))
    if(-not $record.explicitBorrowedPedRetirementContractTested){throw 'Owned explicit-retirement destructor contract check failed.'}
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
if(-not $record.success){throw ('Borrowed ped factory validation failed; see '+$receipt)}
