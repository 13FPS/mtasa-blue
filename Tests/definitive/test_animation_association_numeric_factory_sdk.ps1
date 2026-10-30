#!/usr/bin/env pwsh
#requires -Version 7.0
# Exercise actual numeric null/refusal transport and common CPU deletion; successful issued views belong the actual owner fixture.
[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$taskRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../..')).Path
$id = (Get-Date -Format 'yyyyMMdd-HHmmss') + '-' + [guid]::NewGuid().ToString('N')
$outputRoot = Join-Path $taskRoot ('Build/mta-animation-association-numeric-factory-sdk/' + $id)
$scratchRoot = Join-Path $taskRoot ('Build/tmp/animation-association-numeric-factory-sdk-' + $id)
[IO.Directory]::CreateDirectory($outputRoot) | Out-Null
[IO.Directory]::CreateDirectory($scratchRoot) | Out-Null
$previousEnvironment = @{}
foreach ($name in @('TEMP','TMP','VCToolsInstallDir','INCLUDE','LIB','LIBPATH','PATH')) {
    $previousEnvironment[$name] = [Environment]::GetEnvironmentVariable($name,'Process')
}
$record = [ordered]@{
    schema='mta_animation_association_numeric_factory_sdk_tests_v1';directory=$outputRoot;success=$false;
    recordedAt=[DateTimeOffset]::Now.ToString('o');sourceHashes=@();sourcesUnchanged=$true;
    results=@();failure=$null;scratchDirectory=$scratchRoot;scratchFilesRemaining=$null;scratchRemoved=$false;
    scope='Actual numeric Create full-ID/null/refusal/CPU-OOM transport and actual DE/classic common CPU deletion. No successful code view forged, native engine code executed or historical matrix replay. Root actual-owner scope validates genuine typed issuance and parent/H-child code lifetime.';
    ownedBuffersOnly=$true;nativeEngineCodeExecuted=$false;gameLaunched=$false;gameAttached=$false;
    completeAnimationHierarchyReady=$false;fullMtaReady=$false;installationChanged=$false;
    actualAnimationAssociationNumericFactorySdkTested=$false

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
    $executableHash = Sha $Executable
    [ordered]@{
        executable=$Executable;sha256=$executableHash;arguments=$Arguments;workingDirectory=$Directory;
        deadlineMs=$DeadlineMs;temporaryDirectory=$scratchRoot
    } | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath $command -Encoding utf8NoBOM
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
    $timer = [Diagnostics.Stopwatch]::StartNew()
    try {
        if (-not $process.Start()) { throw "Could not start owned $Name process." }
        $readOut = $process.StandardOutput.ReadToEndAsync()
        $readErr = $process.StandardError.ReadToEndAsync()
        $timedOut = -not $process.WaitForExit($DeadlineMs)
        if ($timedOut) {
            # Only this run's actual child process tree is eligible for termination.
            $process.Kill($true)
            if (-not $process.WaitForExit(10000)) { throw "Owned $Name process did not terminate after timeout." }
        }
        [IO.File]::WriteAllText($stdout,$readOut.GetAwaiter().GetResult(),[Text.UTF8Encoding]::new($false))
        [IO.File]::WriteAllText($stderr,$readErr.GetAwaiter().GetResult(),[Text.UTF8Encoding]::new($false))
        $timer.Stop()
        [ordered]@{
            name=$Name;executable=$Executable;executableSha256=$executableHash;
            command=$command;commandSha256=(Sha $command);exitCode=$process.ExitCode;
            timedOut=$timedOut;elapsedMs=$timer.ElapsedMilliseconds;
            stdout=$stdout;stderr=$stderr;stdoutSha256=(Sha $stdout);stderrSha256=(Sha $stderr)
        }
    } finally { $timer.Stop(); $process.Dispose() }
}

try {
    $productionSources=@('Client/game_de/CAnimManagerDE.cpp','Client/game_de/CAnimBlendAssocGroupDE.cpp','Client/game_de/CAnimBlockDE.cpp','Client/game_de/CAnimBlockRequestDE.cpp','Client/game_de/CAnimBlendStaticAssociationDE.cpp','Client/game_de/CKeyGenDE.cpp','Client/game_de/CAnimBlendHierarchyDE.cpp','Client/game_de/NativeClassicAnimationNameHash.cpp','Client/game_de/CAnimBlendSequenceDE.cpp','Client/game_de/CAnimBlendAssociationDE.cpp')
    $sourceNames=$productionSources+@('Tests/definitive/AnimationAssociationNumericFactorySdkTests.cpp','Tests/definitive/test_animation_association_numeric_factory_sdk.ps1','Tests/definitive/Write-AnimationAssociationNumericFactoryFixture.ps1','Tests/definitive/Write-AnimationManagerFixture.ps1','Tests/definitive/Write-ClassicAssociationCpuLifetimeFixture.ps1','Client/game_sa/CAnimBlendAssociationSA.h')
    $pending=[Collections.Generic.Queue[string]]::new()
    $seen=[Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
    foreach($name in $sourceNames){$pending.Enqueue($name)}
    while($pending.Count){
        $name=$pending.Dequeue();if(-not $seen.Add($name)){continue}
        $path=[IO.Path]::GetFullPath((Join-Path $taskRoot $name))
        if(-not $path.StartsWith($taskRoot+'\',[StringComparison]::OrdinalIgnoreCase) -or $path -match '(?i)(?:^|[\\/])r-engineer(?:[\\/]|$)'){throw 'SDK graph left allowed scope'}
        if(-not (Test-Path -LiteralPath $path -PathType Leaf)){throw ('Missing actual source: '+$name)}
        foreach($match in [regex]::Matches([IO.File]::ReadAllText($path),'(?m)^\s*#\s*include\s*[<"](?<path>[^>"\n]+)[>"]')){
            $include=$match.Groups['path'].Value
            foreach($candidate in @((Join-Path (Split-Path $path) $include),(Join-Path $taskRoot ('Shared/sdk/'+$include)),(Join-Path $taskRoot ('Client/sdk/'+$include)))){
                if(Test-Path -LiteralPath $candidate -PathType Leaf){$pending.Enqueue([IO.Path]::GetRelativePath($taskRoot,[IO.Path]::GetFullPath($candidate)).Replace('\','/'));break}
            }
        }
    }
    $record.sourceHashes=@($seen|Sort-Object|ForEach-Object{[ordered]@{path=$_;sha256=(Sha (Join-Path $taskRoot $_))}})
    . (Join-Path $PSScriptRoot 'Write-AnimationAssociationNumericFactoryFixture.ps1')
    $generated=Join-Path $scratchRoot 'AnimationAssociationNumericFactoryManagerFixture.generated.h'
    $record['generatedFixture']=Write-AnimationAssociationNumericFactoryFixture $taskRoot $generated
    if($record.generatedFixture.rejectingMethods -ne 68 -or @($record.generatedFixture.inheritedFactories).Count -ne 1){throw 'Actual numeric factory manager partition changed'}
    . (Join-Path $PSScriptRoot 'Write-ClassicAssociationCpuLifetimeFixture.ps1')
    $classic=Join-Path $scratchRoot 'ClassicAssociationCpuLifetime.generated.h'
    $record['classicCpuFixture']=Write-ClassicAssociationCpuLifetimeFixture $taskRoot $classic
    foreach($file in @($generated,$classic)){Copy-Item -LiteralPath $file -Destination (Join-Path $outputRoot ([IO.Path]::GetFileName($file)))}

    $env:TEMP = $scratchRoot
    $env:TMP = $scratchRoot
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
    $discovery = RunOwned $vswhere @('-latest','-products','*','-requires','Microsoft.VisualStudio.Component.VC.Tools.x86.x64','-property','installationPath') $outputRoot 'vswhere' 10000
    $visualStudio = [IO.File]::ReadAllText($discovery.stdout).Trim()
    if ($discovery.exitCode -ne 0 -or $discovery.timedOut -or -not $visualStudio) { throw 'Visual Studio x86/x64 tools are required.' }
    $developerCommand = Join-Path $visualStudio 'Common7/Tools/VsDevCmd.bat'
    foreach ($architecture in @('x86','x64')) {
        $directory = Join-Path $outputRoot $architecture
        [IO.Directory]::CreateDirectory($directory) | Out-Null
        # Capture toolchain paths only; a full environment dump could contain
        # unrelated credentials and does not belong in durable build evidence.
        # Keep cmd's quoting inside an owned batch file. ProcessStartInfo's
        # argument escaping otherwise treats the quoted CALL path as C argv.
        $environmentScript = Join-Path $directory 'capture-msvc.cmd'
        @('@echo off',
            ('call "{0}" -no_logo -arch={1} -host_arch=x64 >nul' -f $developerCommand,$architecture),
            'if errorlevel 1 exit /b 1',
            'set VCToolsInstallDir','set INCLUDE','set LIB','set PATH','exit /b 0') |
            Set-Content -LiteralPath $environmentScript -Encoding ascii
        $environmentResult = RunOwned $env:ComSpec @('/d','/s','/c','capture-msvc.cmd') $directory 'msvc-environment' 30000
        $environmentResult['script'] = $environmentScript
        $environmentResult['scriptSha256'] = Sha $environmentScript
        if ($environmentResult.exitCode -ne 0 -or $environmentResult.timedOut) { throw "$architecture MSVC environment initialization failed." }
        foreach ($entry in [IO.File]::ReadAllLines($environmentResult.stdout)) {
            if ($entry -match '^(VCToolsInstallDir|INCLUDE|LIB|LIBPATH|PATH)=(.*)$') {
                [Environment]::SetEnvironmentVariable($Matches[1],$Matches[2],'Process')
            }
        }
        $env:TEMP = $scratchRoot
        $env:TMP = $scratchRoot
        $compiler = Join-Path $env:VCToolsInstallDir "bin/Hostx64/$architecture/cl.exe"
        $executable = Join-Path $scratchRoot ('animation_association_numeric_factory_sdk_tests-' + $architecture + '.exe')
        $arguments = @('/nologo','/std:c++17','/EHsc','/W4','/WX','/O2','/MD','/DNOMINMAX','/DWIN32_LEAN_AND_MEAN',
            ('/I'+(Join-Path $taskRoot 'Shared/sdk')),('/I'+(Join-Path $taskRoot 'Client/sdk')),('/I'+$scratchRoot),
            (Join-Path $PSScriptRoot 'AnimationAssociationNumericFactorySdkTests.cpp'))+@($productionSources|ForEach-Object{Join-Path $taskRoot $_})+@(
            ('/Fo'+$scratchRoot+[IO.Path]::DirectorySeparatorChar),('/Fd'+(Join-Path $scratchRoot 'compiler.pdb')),('/Fe'+$executable))
        $result = [ordered]@{architecture=$architecture;environment=$environmentResult;compile=$null;test=$null;
            checks=0;pointerBits=$null;executable=$executable;executableSha256=$null;actualAnimationAssociationNumericFactorySdkTested=$false}
        $record.results += $result
        $result.compile = RunOwned $compiler $arguments $directory 'compile' 60000
        if ($result.compile.exitCode -ne 0 -or $result.compile.timedOut) { throw "$architecture actual numeric association factory SDK fixture compilation failed." }
        $result.executableSha256 = Sha $executable
        $result.test = RunOwned $executable @() $directory 'test' 10000
        $text = [IO.File]::ReadAllText($result.test.stdout)
        $summary = [regex]::Match($text.TrimEnd(), '(?:^|\r?\n)Passed (\d+) numeric association factory SDK checks \((\d+)-bit; no successful code view forged\)$')
        if ($summary.Success) {
            $result.checks = [int]$summary.Groups[1].Value
            $result.pointerBits = [int]$summary.Groups[2].Value
        }
        $result.actualAnimationAssociationNumericFactorySdkTested=$text.Contains('Actual numeric association factory SDK fixture completed')
        $result['actualAssociationCpuBaseDeletionTested']=$text.Contains('Actual association CPU base-deletion fixture completed')

        $expectedBits = if ($architecture -eq 'x86') { 32 } else { 64 }
        if ($result.test.exitCode -ne 0 -or $result.test.timedOut -or $result.checks -le 0 -or $result.pointerBits -ne $expectedBits -or -not $result.actualAnimationAssociationNumericFactorySdkTested -or -not $result.actualAssociationCpuBaseDeletionTested) {
            throw "$architecture actual numeric association factory SDK checks failed or summary/pointer-width evidence was missing."
        }
    }
    $record.actualAnimationAssociationNumericFactorySdkTested=@($record.results|Where-Object{-not $_.actualAnimationAssociationNumericFactorySdkTested}).Count -eq 0

    $record['actualAssociationCpuBaseDeletionTested']=@($record.results|Where-Object{-not $_.actualAssociationCpuBaseDeletionTested}).Count -eq 0
    $record.success = $record.results.Count -eq 2 -and $record.actualAnimationAssociationNumericFactorySdkTested -and $record.actualAssociationCpuBaseDeletionTested
} catch { AddFailure ($_.Exception.ToString()) }
finally {
    foreach ($name in $previousEnvironment.Keys) {
        [Environment]::SetEnvironmentVariable($name,$previousEnvironment[$name],'Process')
    }
    foreach ($entry in $record.sourceHashes) {
        try {
            if ((Sha (Join-Path $taskRoot $entry.path)) -ne $entry.sha256) { $record.sourcesUnchanged = $false }
        } catch { $record.sourcesUnchanged = $false; AddFailure ($_.Exception.ToString()) }
    }
    $record.success = $record.success -and $record.sourcesUnchanged
    try {
        # Compiler scratch is flat and disposable. Preserve any unexpected
        # directory/link rather than recursing into paths not created here.
        $resolvedScratch = (Resolve-Path -LiteralPath $scratchRoot).Path.TrimEnd('\') + '\'
        $expectedParent = (Resolve-Path -LiteralPath (Join-Path $taskRoot 'Build/tmp')).Path.TrimEnd('\') + '\'
        if (-not $resolvedScratch.StartsWith($expectedParent,[StringComparison]::OrdinalIgnoreCase) -or
            ((Get-Item -LiteralPath $scratchRoot -Force).Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) {
            throw 'Task scratch escaped Build/tmp or became a reparse point.'
        }
        $entries = @(Get-ChildItem -LiteralPath $scratchRoot -Force)
        foreach ($entry in $entries) {
            if ($entry.PSIsContainer -or ($entry.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) {
                throw 'Unexpected scratch directory/link preserved; cleanup is incomplete.'
            }
            $resolvedFile = (Resolve-Path -LiteralPath $entry.FullName).Path
            if (-not $resolvedFile.StartsWith($resolvedScratch,[StringComparison]::OrdinalIgnoreCase)) { throw 'Scratch file escaped the task directory.' }
        }
        foreach ($entry in $entries) { Remove-Item -LiteralPath $entry.FullName -Force }
        $record.scratchFilesRemaining = @(Get-ChildItem -LiteralPath $scratchRoot -Force).Count
        [IO.Directory]::Delete($scratchRoot,$false)
        $record.scratchRemoved = $true
    } catch { AddFailure ($_.Exception.ToString()) }
    $record.finishedAt = [DateTimeOffset]::Now.ToString('o')
    $record | ConvertTo-Json -Depth 10 | Set-Content -LiteralPath (Join-Path $outputRoot 'test-result.json') -Encoding utf8NoBOM
}
[ordered]@{success=$record.success;receipt=(Join-Path $outputRoot 'test-result.json');failure=$record.failure} | ConvertTo-Json -Compress
if (-not $record.success) { throw "Actual numeric association factory SDK checks failed; see $outputRoot" }
