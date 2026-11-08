#requires -Version 7.0
# Actual loaded IPL memory/controller checks; final native entries are explicit doubles.
[CmdletBinding()]
param()
$ErrorActionPreference='Stop';Set-StrictMode -Version Latest
$root=(Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../..')).Path
$id=(Get-Date -Format 'yyyyMMdd-HHmmss')+'-'+[guid]::NewGuid().ToString('N')
$output=Join-Path $root ('Build/mta-native-world-ipl-loaded/'+$id)
$scratch=Join-Path $root ('Build/tmp/native-world-ipl-loaded-'+$id)
foreach($path in @($output,$scratch)){[IO.Directory]::CreateDirectory($path)|Out-Null}
function Sha([string]$path){(Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLowerInvariant()}
$previous=@{};foreach($name in @('TEMP','TMP','PATH','INCLUDE','LIB','LIBPATH','VCToolsInstallDir')){$previous[$name]=[Environment]::GetEnvironmentVariable($name,'Process')}
$record=[ordered]@{schema='mta_native_world_ipl_loaded_tests_v1';success=$false;failure=$null;sources=@();commands=@();checks=0;sourcesUnchanged=$true;scratchRemoved=$false;
 gameLaunched=$false;gameAttached=$false;installationChanged=$false;nativeEngineCodeExecuted=$false;fullRemoveWorldReady=$false;
 scope='Actual production loaded IPL Memory/Controller and owned image/world/heap/pools, native entity rows and current typed UE actor/root/component graph. Explicit final Remove/Add/Clear entry doubles; no destructor, mobility or UE API original executes. Whole-bank admission, freed generation/stream acknowledgements and retained unknown nonreplay. Historical state0/load/request mains are not run. FullRemoveWorld remains unsupported.'}
function Run([string]$label,[string]$exe,[string[]]$arguments){
 $info=[Diagnostics.ProcessStartInfo]::new($exe);$info.WorkingDirectory=$output;$info.UseShellExecute=$false;$info.CreateNoWindow=$true
 $info.RedirectStandardOutput=$true;$info.RedirectStandardError=$true;$info.Environment['TEMP']=$scratch;$info.Environment['TMP']=$scratch
 foreach($argument in $arguments){$info.ArgumentList.Add($argument)}
 $command=Join-Path $output ($label+'-command.json');@{executable=$exe;sha256=(Sha $exe);arguments=$arguments;timeoutMs=60000}|ConvertTo-Json -Depth 5|Set-Content -LiteralPath $command -Encoding utf8
 $process=[Diagnostics.Process]::new();$process.StartInfo=$info
 try{
  if(-not $process.Start()){throw "Cannot start owned $label"}
  $out=$process.StandardOutput.ReadToEndAsync();$err=$process.StandardError.ReadToEndAsync();$timeout=-not $process.WaitForExit(60000)
  if($timeout){$process.Kill($true);if(-not $process.WaitForExit(10000)){throw 'Owned child failed to terminate'}}
  $stdout=Join-Path $output ($label+'.log');$stderr=Join-Path $output ($label+'-errors.log')
  [IO.File]::WriteAllText($stdout,$out.GetAwaiter().GetResult());[IO.File]::WriteAllText($stderr,$err.GetAwaiter().GetResult())
  $record.commands+=@{label=$label;command=$command;exitCode=$process.ExitCode;timedOut=$timeout;stdout=$stdout;stdoutSha256=(Sha $stdout);stderr=$stderr;stderrSha256=(Sha $stderr)}
  if($process.ExitCode -ne 0 -or $timeout){throw "$label failed; inspect $stdout"}
 }finally{$process.Dispose()}
}
try{
 $queue=[Collections.Generic.Queue[string]]::new();$seen=[Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
 foreach($name in @('Tests/definitive/NativeWorldIplLoadedTests.cpp','Tests/definitive/test_native_world_ipl_loaded.ps1')){$queue.Enqueue($name)}
 while($queue.Count){
  $name=$queue.Dequeue();if(-not $seen.Add($name)){continue};$path=[IO.Path]::GetFullPath((Join-Path $root $name))
  if(-not $path.StartsWith($root+'\',[StringComparison]::OrdinalIgnoreCase) -or $path -match '(?i)(?:^|[\\/])r-engineer(?:[\\/]|$)'){throw 'Source graph escaped workspace'}
  if(-not (Test-Path -LiteralPath $path -PathType Leaf)){throw "Missing source $name"}
  foreach($match in [regex]::Matches([IO.File]::ReadAllText($path),'(?m)^\s*#\s*include\s*[<"](?<path>[^>"\n]+)[>"]')){
   $include=$match.Groups['path'].Value
   foreach($candidate in @((Join-Path (Split-Path $path) $include),(Join-Path $root ('Client/sdk/'+$include)),(Join-Path $root ('Shared/sdk/'+$include)))){
    if(Test-Path -LiteralPath $candidate -PathType Leaf){$queue.Enqueue([IO.Path]::GetRelativePath($root,[IO.Path]::GetFullPath($candidate)).Replace('\','/'));break}
   }
  }
 }
 $record.sources=@($seen|Sort-Object|ForEach-Object{@{path=$_;sha256=(Sha (Join-Path $root $_))}})
 $env:TEMP=$scratch;$env:TMP=$scratch
 $vswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
 $vs=(& $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath).Trim()
 if($LASTEXITCODE -ne 0 -or -not $vs){throw 'MSVC x64 toolchain unavailable'}
 $batch=Join-Path $output 'capture-msvc.cmd'
 @('@echo off',('call "{0}" -no_logo -arch=x64 -host_arch=x64 >nul' -f (Join-Path $vs 'Common7/Tools/VsDevCmd.bat')),'if errorlevel 1 exit /b 1','set VCToolsInstallDir','set INCLUDE','set LIB','set LIBPATH','set PATH','exit /b 0')|Set-Content -LiteralPath $batch -Encoding ascii
 Run 'environment' $env:ComSpec @('/d','/s','/c','capture-msvc.cmd')
 foreach($line in [IO.File]::ReadAllLines((Join-Path $output 'environment.log'))){if($line -match '^(VCToolsInstallDir|INCLUDE|LIB|LIBPATH|PATH)=(.*)$'){[Environment]::SetEnvironmentVariable($Matches[1],$Matches[2],'Process')}}
 $compiler=Join-Path $env:VCToolsInstallDir 'bin/Hostx64/x64/cl.exe';$exe=Join-Path $output 'native-world-ipl-loaded-tests.exe'
 Run 'compile' $compiler @('/nologo','/std:c++17','/EHsc','/W4','/WX','/O2','/MD','/DNOMINMAX','/DWIN32_LEAN_AND_MEAN',('/I'+(Join-Path $root 'Client/sdk')),('/I'+(Join-Path $root 'Shared/sdk')),('/I'+$scratch),(Join-Path $PSScriptRoot 'NativeWorldIplLoadedTests.cpp'),('/Fo'+$scratch+'\'),('/Fd'+(Join-Path $scratch 'compiler.pdb')),('/Fe'+$exe),'/link','bcrypt.lib')
 $record['executableSha256']=Sha $exe
 Run 'test' $exe @($output)
 $summary=[regex]::Match([IO.File]::ReadAllText((Join-Path $output 'test.log')),'(?m)^Passed (\d+) loaded IPL checks ')
 if(-not $summary.Success){throw 'Focused IPL summary missing'}
 $record.checks=[int]$summary.Groups[1].Value;if($record.checks -le 0){throw 'No focused checks ran'}
 $record.success=$true
}catch{$record.failure=$_.Exception.ToString()}
finally{
 foreach($name in $previous.Keys){[Environment]::SetEnvironmentVariable($name,$previous[$name],'Process')}
 foreach($source in $record.sources){if((Sha (Join-Path $root $source.path)) -cne $source.sha256){$record.sourcesUnchanged=$false}}
 try{
  $resolved=[IO.Path]::GetFullPath($scratch);$allowed=[IO.Path]::GetFullPath((Join-Path $root 'Build/tmp')).TrimEnd('\')+'\'
  if(-not $resolved.StartsWith($allowed,[StringComparison]::OrdinalIgnoreCase)){throw 'Scratch escaped task workspace'}
  for($path=$resolved;$path;$path=[IO.Path]::GetDirectoryName($path)){if((Get-Item -LiteralPath $path -Force).Attributes -band [IO.FileAttributes]::ReparsePoint){throw 'Linked scratch ancestor'}}
  if(@(Get-ChildItem -LiteralPath $resolved -Recurse -Force|Where-Object {$_.Attributes -band [IO.FileAttributes]::ReparsePoint}).Count){throw 'Linked scratch descendant'}
  Remove-Item -LiteralPath $resolved -Recurse -Force;$record.scratchRemoved=$true
 }catch{$record.success=$false;$record.failure=([string]$record.failure)+[Environment]::NewLine+$_.Exception.ToString()}
 $record.success=$record.success -and $record.sourcesUnchanged -and $record.scratchRemoved
 $receipt=Join-Path $output 'test-result.json';$record|ConvertTo-Json -Depth 10|Set-Content -LiteralPath $receipt -Encoding utf8
}
@{success=$record.success;receipt=$receipt;checks=$record.checks;failure=$record.failure}|ConvertTo-Json -Compress
if(-not $record.success){exit 1}
