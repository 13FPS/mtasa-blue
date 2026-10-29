#requires -Version 7.0
# Run only synchronous temporary-clump foundation checks in four owned processes.
[CmdletBinding()]
param()
$ErrorActionPreference='Stop'
Set-StrictMode -Version Latest
$root=(Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../..')).Path
$id=(Get-Date -Format 'yyyyMMdd-HHmmss')+'-'+[guid]::NewGuid().ToString('N')
$output=Join-Path $root ('Build/mta-native-temporary-model-clump/'+$id)
$scratch=Join-Path $root ('Build/tmp/native-temporary-model-clump-'+$id)
[IO.Directory]::CreateDirectory($output)|Out-Null
[IO.Directory]::CreateDirectory($scratch)|Out-Null
$phaseImageDirectories=[Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
$utf8=[Text.UTF8Encoding]::new($false)
function Sha([string]$p){(Get-FileHash -LiteralPath $p -Algorithm SHA256).Hash.ToLowerInvariant()}
$previous=@{}
foreach($name in @('TEMP','TMP','PATH','INCLUDE','LIB','LIBPATH','VCToolsInstallDir')){$previous[$name]=[Environment]::GetEnvironmentVariable($name,'Process')}
$record=[ordered]@{schema='mta_native_temporary_model_clump_tests_v1';success=$false;failure=$null;sources=@();results=@();architecture='x64';
    nativeEngineCodeExecuted=$false;nativePoolObservedInGta=$false;nativeTemporaryModelClumpPrimitiveTested=$false;
    gameLaunched=$false;gameAttached=$false;engineMemoryWritten=$false;hookInstalled=$false;
    completeGameReady=$false;completeGroupFactoryReady=$false;jointStaticInitializerIntegrated=$false;engineActorCreated=$false;nativeModelLifetimeClaimed=$false;nativeHeapFreed=$false;publishedGroupMutated=$false;sourcesUnchanged=$true;scratch=$scratch;scratchFilesRemaining=$null;scratchRemoved=$false;
    scope='Actual synchronous original model-clump factory/setup/prepare/deferred-retirement boundary over existing owned model/world/UObject fixtures. Typed row callback is visited only; no actual FEB5F0 bridge is claimed. Preserves original wrapper and conditional setup; journaled null/nonnull/missing-ack outcomes, owner-zero container, no cross-callback lease, retry or heapfree. Four fresh processes select happy/preentry/unknown/race phases; historical fixture mains compile but are never called.'}
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
    # Pin this exact local include graph; compiling the reused owned-image
    # scaffold does not replay its historical main or claim its old matrix.
    $pending=[Collections.Generic.Queue[string]]::new()
    foreach($name in @('Tests/definitive/NativeTemporaryModelClumpTests.cpp','Tests/definitive/test_native_temporary_model_clump.ps1','Tests/definitive/NativeStaticAssociationInitializeCallsTestBridge.cpp','Tests/definitive/NativeStaticAssociationInitializeReadersTestBridge.cpp')){$pending.Enqueue($name)}
    $seen=[Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
    while($pending.Count){
        $name=$pending.Dequeue();if(-not $seen.Add($name)){continue}
        $path=[IO.Path]::GetFullPath((Join-Path $root $name))
        if(-not $path.StartsWith($root+'\',[StringComparison]::OrdinalIgnoreCase) -or $path -match '(?i)(?:^|[\\/])r-engineer(?:[\\/]|$)'){throw 'Temporary-clump fixture source escaped workspace'}
        foreach($match in [regex]::Matches([IO.File]::ReadAllText($path),'(?m)^\s*#\s*include\s*[<"](?<path>[^>"\n]+)[>"]')){
            $include=$match.Groups['path'].Value
            foreach($candidate in @((Join-Path (Split-Path $path) $include),(Join-Path $root ('Shared/sdk/'+$include)),(Join-Path $root ('Client/sdk/'+$include)))){
                if(Test-Path -LiteralPath $candidate -PathType Leaf){$pending.Enqueue([IO.Path]::GetRelativePath($root,[IO.Path]::GetFullPath($candidate)).Replace('\','/'));break}
            }
        }
    }
    $names=@($seen)+@('Build/definitive-recovery/mta-temporary-model-clump-20261029/temporary-model-clump-source-ready.json')
    $record.sources=@($names|Sort-Object -Unique|ForEach-Object{[ordered]@{path=$_;sha256=(Sha (Join-Path $root $_))}})
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
    $exe=Join-Path $scratch 'native-temporary-model-clump-tests.exe'
    $arguments=@('/nologo','/std:c++17','/EHsc','/W4','/WX','/O2','/MD','/DNOMINMAX','/DWIN32_LEAN_AND_MEAN',
        ('/I'+(Join-Path $root 'Client/sdk')),('/I'+(Join-Path $root 'Shared/sdk')),('/I'+$scratch),
        (Join-Path $PSScriptRoot 'NativeTemporaryModelClumpTests.cpp'),(Join-Path $PSScriptRoot 'NativeStaticAssociationInitializeCallsTestBridge.cpp'),(Join-Path $PSScriptRoot 'NativeStaticAssociationInitializeReadersTestBridge.cpp'),('/Fo'+$scratch+[IO.Path]::DirectorySeparatorChar),
        ('/Fd'+(Join-Path $scratch 'compiler.pdb')),('/Fe'+$exe),'/link','bcrypt.lib')
    $compile=RunOwned $compiler $arguments $output 'compile' 60000
    if($compile.exitCode -ne 0 -or $compile.timedOut){throw 'Native temporary-clump fixture compilation failed.'}
    $executableSha=Sha $exe
    foreach($phase in @('happy','preentry','unknown','race')){
        # Each fresh process creates a fixed-name sparse image with CREATE_NEW.
        # Keep that image private to its phase while reusing the compiled exe.
        $phaseImage=[IO.Path]::GetFullPath((Join-Path $scratch ('image-'+$phase)))
        if(Test-Path -LiteralPath $phaseImage){throw 'Phase image directory already exists.'}
        [IO.Directory]::CreateDirectory($phaseImage)|Out-Null
        [void]$phaseImageDirectories.Add($phaseImage)
        $result=[ordered]@{phase=$phase;imageDirectory=$phaseImage;compiler=$compiler;compilerSha256=(Sha $compiler);compile=$compile;test=$null;checks=0;executableSha256=$executableSha}
        $record.results+=$result
        $result.test=RunOwned $exe @($phaseImage,$phase) $output ('test-'+$phase) 30000
        $stdout=[IO.File]::ReadAllText($result.test.stdout)
        $summary=[regex]::Match($stdout,'(?m)^Passed (\d+) native temporary-clump checks \(64-bit; no native engine code executed\)\r?$')
        if($summary.Success){$result.checks=[int]$summary.Groups[1].Value}
        if($result.test.exitCode -ne 0 -or $result.test.timedOut -or $result.checks -le 0 -or
            -not $stdout.Contains('Production synchronous temporary model clump fixture completed')){throw ('Native temporary-clump '+$phase+' checks failed.')}
    }
    $record.nativeTemporaryModelClumpPrimitiveTested=$record.results.Count -eq 4
    $record.success=$true
}catch{$record.failure=$_.Exception.ToString()}
finally{
    foreach($name in $previous.Keys){[Environment]::SetEnvironmentVariable($name,$previous[$name],'Process')}
    foreach($source in $record.sources){if((Sha (Join-Path $root $source.path)) -ne $source.sha256){$record.sourcesUnchanged=$false}}
    $record.success=$record.success -and $record.sourcesUnchanged
    try{
        $resolved=[IO.Path]::GetFullPath($scratch)
        $allowed=[IO.Path]::GetFullPath((Join-Path $root 'Build/tmp')).TrimEnd('\')+'\'
        if(-not $resolved.StartsWith($allowed,[StringComparison]::OrdinalIgnoreCase)){throw 'Compiler scratch escaped workspace.'}
        for($parent=$resolved;$parent;$parent=[IO.Path]::GetDirectoryName($parent)){
            if((Test-Path -LiteralPath $parent) -and ((Get-Item -LiteralPath $parent -Force).Attributes -band [IO.FileAttributes]::ReparsePoint)){throw 'Linked compiler scratch ancestor refused.'}
        }
        # Validate the complete owned tree before removing anything. Only the
        # explicitly created phase directories may contain one flat file layer.
        $filesToRemove=[Collections.Generic.List[string]]::new()
        $directoriesToRemove=[Collections.Generic.List[string]]::new()
        foreach($entry in @(Get-ChildItem -LiteralPath $scratch -Force)){
            if($entry.Attributes -band [IO.FileAttributes]::ReparsePoint){throw 'Linked scratch child preserved.'}
            $child=[IO.Path]::GetFullPath($entry.FullName)
            if(-not $child.StartsWith($resolved+'\',[StringComparison]::OrdinalIgnoreCase)){throw 'Scratch child escaped exact task directory.'}
            if($entry.PSIsContainer){
                if(-not $phaseImageDirectories.Contains($child)){throw 'Unexpected scratch directory preserved.'}
                foreach($phaseFile in @(Get-ChildItem -LiteralPath $child -Force)){
                    if($phaseFile.PSIsContainer -or ($phaseFile.Attributes -band [IO.FileAttributes]::ReparsePoint)){throw 'Nested or linked phase child preserved.'}
                    $phaseChild=[IO.Path]::GetFullPath($phaseFile.FullName)
                    if(-not $phaseChild.StartsWith($child+'\',[StringComparison]::OrdinalIgnoreCase)){throw 'Phase child escaped its exact owned directory.'}
                    $filesToRemove.Add($phaseChild)
                }
                $directoriesToRemove.Add($child)
            }else{
                $filesToRemove.Add($child)
            }
        }
        foreach($file in $filesToRemove){Remove-Item -LiteralPath $file -Force}
        foreach($directory in $directoriesToRemove){
            if(@(Get-ChildItem -LiteralPath $directory -Force).Count){throw 'Owned phase directory is not empty.'}
            Remove-Item -LiteralPath $directory -Force
        }
        $record.scratchFilesRemaining=@(Get-ChildItem -LiteralPath $scratch -Force).Count
        if($record.scratchFilesRemaining){throw 'Owned scratch files remain.'}
        Remove-Item -LiteralPath $scratch
        $record.scratchRemoved=$true
    }catch{$record.success=$false;$record.failure=($record.failure+';scratch:'+ $_.Exception.Message)}
    $receipt=Join-Path $output 'test-result.json';[IO.File]::WriteAllText($receipt,($record|ConvertTo-Json -Depth 12)+"`n",$utf8)
}
@{success=$record.success;path=$receipt;sha256=(Sha $receipt);results=$record.results}|ConvertTo-Json -Depth 6 -Compress
if(-not $record.success){throw ('Native temporary-clump validation failed; see '+$receipt)}
