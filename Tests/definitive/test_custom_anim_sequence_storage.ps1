#!/usr/bin/env pwsh
#requires -Version 7.0
# Actual shared SDK destruction and actual classic backend storage algorithm.
[CmdletBinding()]
param([switch]$CompileX64)
$ErrorActionPreference='Stop'
Set-StrictMode -Version Latest
$taskRoot=(Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../..')).Path
$id=(Get-Date -Format 'yyyyMMdd-HHmmss')+'-'+[guid]::NewGuid().ToString('N')
$outputRoot=Join-Path $taskRoot ('Build/mta-custom-animation-storage/'+$id)
$scratchRoot=Join-Path $taskRoot ('Build/tmp/custom-animation-storage-'+$id)
[IO.Directory]::CreateDirectory($outputRoot)|Out-Null
[IO.Directory]::CreateDirectory($scratchRoot)|Out-Null
$previousEnvironment=@{}
foreach($name in @('TEMP','TMP','VCToolsInstallDir','INCLUDE','LIB','LIBPATH','PATH')){
    $previousEnvironment[$name]=[Environment]::GetEnvironmentVariable($name,'Process')
}
$record=[ordered]@{
    schema='mta_custom_animation_sequence_storage_tests_v1';directory=$outputRoot;success=$false;
    recordedAt=[DateTimeOffset]::Now.ToString('o');sourceHashes=@();sourcesUnchanged=$true;
    results=@();checks=0;failure=$null;scratchDirectory=$scratchRoot;scratchRemoved=$false;
    actualAnimSequenceStorageTested=$false;actualAnimHierarchySequenceSdkLifetimeTested=$false;
    scope='Execute production CAnimSequenceStorageSA with actual SA record declarations on x86, including all-capacity initialization and frame-neutral storage free; execute actual shared SDK polymorphic CPU deletion and visible time-read refusal. Optional x64 is compile-only source coverage. No manager/engine/native ABI or complete DE factory execution.';
    nativeEngineCodeExecuted=$false;classicX64NativeRuntimeTested=$false;managerVtableDispatchTested=$false;
    gameLaunched=$false;gameAttached=$false;installationChanged=$false;fullMtaReady=$false
}
function Sha([string]$Path) { (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToLowerInvariant() }
function AddFailure([string]$Message) {
    $record.success = $false
    if ($null -eq $record.failure) { $record.failure = $Message }
    else { $record.failure += [Environment]::NewLine + $Message }
}
function RunOwned([string]$Executable,[string[]]$Arguments,[string]$Directory,[string]$Name,[int]$DeadlineMs) {
    $stdout = Join-Path $Directory ($Name + '.log')
    $stderr = Join-Path $Directory ($Name + '-errors.log')
    $command = Join-Path $Directory ($Name + '-command.json')
    [ordered]@{executable=$Executable;sha256=(Sha $Executable);arguments=$Arguments;workingDirectory=$Directory;
        deadlineMs=$DeadlineMs;temporaryDirectory=$scratchRoot} | ConvertTo-Json -Depth 4 |
        Set-Content -LiteralPath $command -Encoding utf8NoBOM
    $info = [Diagnostics.ProcessStartInfo]::new()
    $info.FileName = $Executable
    $info.WorkingDirectory = $Directory
    $info.UseShellExecute = $false
    $info.CreateNoWindow = $true
    $info.RedirectStandardOutput = $true
    $info.RedirectStandardError = $true
    $info.Environment['TEMP'] = $scratchRoot
    $info.Environment['TMP'] = $scratchRoot
    foreach ($argument in $Arguments) { $info.ArgumentList.Add($argument) }
    $process = [Diagnostics.Process]::new()
    $process.StartInfo = $info
    try {
        if (-not $process.Start()) { throw "Could not start owned $Name process." }
        $readOut = $process.StandardOutput.ReadToEndAsync()
        $readErr = $process.StandardError.ReadToEndAsync()
        $timedOut = -not $process.WaitForExit($DeadlineMs)
        if ($timedOut) {
            # Only the fixture process tree created by this invocation is owned.
            $process.Kill($true)
            if (-not $process.WaitForExit(10000)) { throw "Owned $Name process did not terminate after timeout." }
        }
        [IO.File]::WriteAllText($stdout,$readOut.GetAwaiter().GetResult(),[Text.UTF8Encoding]::new($false))
        [IO.File]::WriteAllText($stderr,$readErr.GetAwaiter().GetResult(),[Text.UTF8Encoding]::new($false))
        [ordered]@{name=$Name;command=$command;commandSha256=(Sha $command);exitCode=$process.ExitCode;
            timedOut=$timedOut;stdout=$stdout;stderr=$stderr;stdoutSha256=(Sha $stdout);stderrSha256=(Sha $stderr)}
    } finally { $process.Dispose() }
}
try {
    # Resolve only the actual test/header include graph. The two delegation
    # CPPs are additionally hashed, but their normal TU compile has its own
    # receipt; this helper does not claim to link the game_sa engine vtable.
    $sourceNames=@('Tests/definitive/CustomAnimSequenceStorageTests.cpp',
        [IO.Path]::GetRelativePath($taskRoot,$PSCommandPath).Replace('\','/'))
    $pending=[Collections.Generic.Queue[string]]::new()
    foreach($source in $sourceNames){$pending.Enqueue($source)}
    $observed=[Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
    while($pending.Count){
        $source=$pending.Dequeue()
        if(-not $observed.Add($source)){continue}
        $sourcePath=Join-Path $taskRoot $source
        if(-not(Test-Path -LiteralPath $sourcePath -PathType Leaf)){throw "Actual storage input missing: $source"}
        foreach($match in [regex]::Matches([IO.File]::ReadAllText($sourcePath),'(?m)^\s*#\s*include\s*[<"](?<path>[^>"]+)[>"]')){
            $includeName=$match.Groups['path'].Value
            $candidates=@((Join-Path (Split-Path -Parent $sourcePath) $includeName),
                (Join-Path $taskRoot ('Shared/sdk/'+$includeName)),
                (Join-Path $taskRoot ('Client/sdk/'+$includeName)),
                (Join-Path $taskRoot ('Client/game_sa/'+$includeName)))
            foreach($candidate in $candidates){
                if(Test-Path -LiteralPath $candidate -PathType Leaf){
                    $relative=[IO.Path]::GetRelativePath($taskRoot,(Resolve-Path -LiteralPath $candidate).Path).Replace('\','/')
                    if($relative.StartsWith('../')){throw 'Actual header escaped workspace'}
                    $pending.Enqueue($relative)
                    break
                }
            }
        }
    }
    foreach($source in @('Client/game_sa/CAnimManagerSA.cpp','Client/game_sa/CAnimBlendSequenceSA.cpp')){[void]$observed.Add($source)}
    $record.sourceHashes=@($observed|Sort-Object|ForEach-Object{[ordered]@{path=$_;sha256=(Sha(Join-Path $taskRoot $_))}})
    $vswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
    $discovery=RunOwned $vswhere @('-latest','-products','*','-requires','Microsoft.VisualStudio.Component.VC.Tools.x86.x64','-property','installationPath') $outputRoot 'vswhere' 10000
    $visualStudio=[IO.File]::ReadAllText($discovery.stdout).Trim()
    if($discovery.exitCode -ne 0 -or $discovery.timedOut -or -not $visualStudio){throw 'Visual Studio x86/x64 tools required'}
    $developerCommand=Join-Path $visualStudio 'Common7/Tools/VsDevCmd.bat'
    $architectures=@('x86')
    if($CompileX64){$architectures+=@('x64')}
    foreach($architecture in $architectures){
        foreach($name in $previousEnvironment.Keys){[Environment]::SetEnvironmentVariable($name,$previousEnvironment[$name],'Process')}
        $environmentName='capture-msvc-'+$architecture+'.cmd'
        @('@echo off',('call "{0}" -no_logo -arch={1} -host_arch=x64 >nul' -f $developerCommand,$architecture),
            'if errorlevel 1 exit /b 1','set VCToolsInstallDir','set INCLUDE','set LIB','set PATH','exit /b 0')|
            Set-Content -LiteralPath (Join-Path $outputRoot $environmentName) -Encoding ascii
        $environmentResult=RunOwned $env:ComSpec @('/d','/s','/c',$environmentName) $outputRoot ('environment-'+$architecture) 30000
        if($environmentResult.exitCode -ne 0 -or $environmentResult.timedOut){throw "MSVC $architecture initialization failed"}
        foreach($entry in [IO.File]::ReadAllLines($environmentResult.stdout)){
            if($entry -match '^(VCToolsInstallDir|INCLUDE|LIB|LIBPATH|PATH)=(.*)$'){
                [Environment]::SetEnvironmentVariable($Matches[1],$Matches[2],'Process')
            }
        }
        $compiler=Join-Path $env:VCToolsInstallDir ('bin/Hostx64/'+$architecture+'/cl.exe')
        $executable=Join-Path $scratchRoot ('custom-storage-'+$architecture+'.exe')
        $arguments=@('/nologo','/std:c++17','/EHsc','/W4','/WX','/O2','/MD','/DNDEBUG','/DNOMINMAX','/DWIN32_LEAN_AND_MEAN',
            ('/I'+(Join-Path $taskRoot 'Client/sdk')),('/I'+(Join-Path $taskRoot 'Client/game_sa')),
            (Join-Path $PSScriptRoot 'CustomAnimSequenceStorageTests.cpp'),
            ('/Fo'+(Join-Path $scratchRoot ('custom-storage-'+$architecture+'.obj'))),
            ('/Fd'+(Join-Path $scratchRoot ('compiler-'+$architecture+'.pdb'))),('/Fe'+$executable))
        $result=[ordered]@{architecture=$architecture;compileOnly=($architecture -eq 'x64');environment=$environmentResult;
            compile=$null;test=$null;executableSha256=$null;checks=0;pointerBits=$null;success=$false}
        $record.results+=@($result)
        $result.compile=RunOwned $compiler $arguments $outputRoot ('compile-'+$architecture) 60000
        if($result.compile.exitCode -ne 0 -or $result.compile.timedOut){throw "Actual storage/lifetime $architecture compilation failed"}
        $result.executableSha256=Sha $executable
        if($architecture -eq 'x64'){
            # The actual classic game remains32-bit. Compile its source layout
            # here without claiming an executable native x64 classic backend.
            $result.success=$true
            continue
        }
        $result.test=RunOwned $executable @() $outputRoot ('test-'+$architecture) 10000
        $stdout=[IO.File]::ReadAllText($result.test.stdout)
        $summary=[regex]::Match($stdout.TrimEnd(),'(?:^|\r?\n)Passed (\d+) custom animation storage/lifetime checks \((\d+)-bit; no native engine code executed\)$')
        if($summary.Success){$result.checks=[int]$summary.Groups[1].Value;$result.pointerBits=[int]$summary.Groups[2].Value}
        $record.actualAnimSequenceStorageTested=$stdout.Contains('Actual classic backend sequence storage policy fixture completed')
        $record.actualAnimHierarchySequenceSdkLifetimeTested=$stdout.Contains('Actual hierarchy and sequence SDK polymorphic CPU lifetime fixture completed')
        $result.success=$result.test.exitCode -eq 0 -and -not $result.test.timedOut -and $result.checks -gt 0 -and $result.pointerBits -eq 32 -and
            $record.actualAnimSequenceStorageTested -and $record.actualAnimHierarchySequenceSdkLifetimeTested
        if(-not $result.success){throw 'Actual storage/lifetime execution or evidence failed'}
        $record.checks=$result.checks
    }
    $record.success=$record.results.Count -eq $architectures.Count -and @($record.results|Where-Object{-not $_.success}).Count -eq 0 -and
        $record.actualAnimSequenceStorageTested -and $record.actualAnimHierarchySequenceSdkLifetimeTested
} catch { AddFailure ($_.Exception.ToString()) }
finally {
    foreach ($name in $previousEnvironment.Keys) {
        [Environment]::SetEnvironmentVariable($name,$previousEnvironment[$name],'Process')
    }
    foreach ($entry in $record.sourceHashes) {
        try { if ((Sha (Join-Path $taskRoot $entry.path)) -ne $entry.sha256) { $record.sourcesUnchanged = $false } }
        catch { $record.sourcesUnchanged = $false; AddFailure ($_.Exception.ToString()) }
    }
    $record.success = $record.success -and $record.sourcesUnchanged
    try {
        # Validate this invocation's flat scratch directory before deleting its
        # generated fixture and compiler scratch. Durable evidence stays above.
        $resolvedScratch = (Resolve-Path -LiteralPath $scratchRoot).Path.TrimEnd('\') + '\'
        $expectedParent = (Resolve-Path -LiteralPath (Join-Path $taskRoot 'Build/tmp')).Path.TrimEnd('\') + '\'
        if (-not $resolvedScratch.StartsWith($expectedParent,[StringComparison]::OrdinalIgnoreCase) -or
            ((Get-Item -LiteralPath $scratchRoot -Force).Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) { throw 'Task scratch escaped Build/tmp or became a reparse point.' }
        $entries = @(Get-ChildItem -LiteralPath $scratchRoot -Force)
        foreach ($entry in $entries) {
            if ($entry.PSIsContainer -or ($entry.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0 -or
                -not (Resolve-Path -LiteralPath $entry.FullName).Path.StartsWith($resolvedScratch,[StringComparison]::OrdinalIgnoreCase)) { throw 'Unexpected scratch directory/link/path preserved.' }
        }
        foreach ($entry in $entries) { Remove-Item -LiteralPath $entry.FullName -Force }
        [IO.Directory]::Delete($scratchRoot,$false)
        $record.scratchRemoved = $true
    } catch { AddFailure ($_.Exception.ToString()) }
    $record.finishedAt = [DateTimeOffset]::Now.ToString('o')
    $record | ConvertTo-Json -Depth 10 | Set-Content -LiteralPath (Join-Path $outputRoot 'test-result.json') -Encoding utf8NoBOM
}
[ordered]@{success=$record.success;receipt=(Join-Path $outputRoot 'test-result.json');failure=$record.failure} | ConvertTo-Json -Compress
if (-not $record.success) { throw "Custom animation storage/lifetime checks failed; see $outputRoot" }
