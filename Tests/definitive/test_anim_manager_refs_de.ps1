#!/usr/bin/env pwsh
#requires -Version 7.0
# Compile only actual integer manager reference SDK dispatch on both CPU pointer widths.
[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$taskRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../..')).Path
$id = (Get-Date -Format 'yyyyMMdd-HHmmss') + '-' + [guid]::NewGuid().ToString('N')
$outputRoot = Join-Path $taskRoot ('Build/mta-anim-manager-refs-de/' + $id)
$scratchRoot = Join-Path $taskRoot ('Build/tmp/anim-manager-refs-de-' + $id)
[IO.Directory]::CreateDirectory($outputRoot) | Out-Null
[IO.Directory]::CreateDirectory($scratchRoot) | Out-Null
$previousEnvironment = @{}
foreach ($name in @('TEMP','TMP','VCToolsInstallDir','INCLUDE','LIB','LIBPATH','PATH')) {
    $previousEnvironment[$name] = [Environment]::GetEnvironmentVariable($name,'Process')
}
$record = [ordered]@{
    schema='mta_anim_manager_refs_de_tests_v1';directory=$outputRoot;success=$false;
    recordedAt=[DateTimeOffset]::Now.ToString('o');sourceHashes=@();sourcesUnchanged=$true;
    results=@();failure=$null;scratchDirectory=$scratchRoot;scratchFilesRemaining=$null;scratchRemoved=$false;
    scope='Actual CAnimManagerDE Add/Get/WithoutDelete integer references through the counter-only same-complete-owner constructor and raw counter identity/service APIs. Signed manager promotion, unsigned block getter comparison, raw16 affinity, full signed index admission, copied generation, status/refusal propagation and no destructor cleanup. Remaining 51 manager methods abort. Existing strict block fixture definitions only; no historical cases, CGame/native engine or complete manager.';
    ownedBuffersOnly=$true;nativeEngineCodeExecuted=$false;gameLaunched=$false;gameAttached=$false;
    completeAnimationManagerReady=$false;fullMtaReady=$false;installationChanged=$false;
    actualAnimationManagerRefsSdkTested=$false;actualSignedManagerUnsignedBlockRefsSdkTested=$false

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
    $productionSources=@('Client/game_de/CAnimManagerDE.cpp','Client/game_de/CAnimBlockDE.cpp','Client/game_de/CAnimBlockRequestDE.cpp')
    $sourceNames=$productionSources+@('Tests/definitive/AnimManagerRefsDETests.cpp','Tests/definitive/Write-AnimationManagerRefsFixture.ps1',
        'Tests/definitive/Write-AnimBlockRefsFixture.ps1','Tests/definitive/AnimBlockNameDETests.cpp',
        'Tests/definitive/test_anim_manager_refs_de.ps1',
        'Build/definitive-recovery/mta-animation-manager-refs-access-20261024/animation-manager-refs-access-source-ready.json',
        'Build/definitive-recovery/mta-animation-manager-refs-sdk-20261024/animation-manager-refs-sdk-source-ready.json',
        'Build/definitive-recovery/mta-animation-manager-refs-sdk-tests-20261024/animation-manager-refs-fixture-source-ready.json',
        'Build/definitive-recovery/mta-animation-manager-refs-sdk-tests-20261024/animation-manager-refs-sdk-tests-source-ready.json')
    $pending=[Collections.Generic.Queue[string]]::new()
    $seen=[Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
    foreach($name in $sourceNames){$pending.Enqueue($name)}
    while($pending.Count){
        $name=$pending.Dequeue();if(-not $seen.Add($name)){continue}
        $path=[IO.Path]::GetFullPath((Join-Path $taskRoot $name))
        if(-not $path.StartsWith($taskRoot+'\',[StringComparison]::OrdinalIgnoreCase) -or $path -match '(?i)(?:^|[\\/])r-engineer(?:[\\/]|$)'){throw 'Local SDK graph left the allowed source scope'}
        if(-not (Test-Path -LiteralPath $path -PathType Leaf)){throw ('Missing actual source input: '+$name)}
        foreach($match in [regex]::Matches([IO.File]::ReadAllText($path),'(?m)^\s*#\s*include\s*[<"](?<path>[^>"\n]+)[>"]')){
            $include=$match.Groups['path'].Value
            foreach($candidate in @((Join-Path (Split-Path $path) $include),(Join-Path $taskRoot ('Shared/sdk/'+$include)),(Join-Path $taskRoot ('Client/sdk/'+$include)))){
                if(Test-Path -LiteralPath $candidate -PathType Leaf){$pending.Enqueue([IO.Path]::GetRelativePath($taskRoot,[IO.Path]::GetFullPath($candidate)).Replace('\','/'));break}
            }
        }
    }
    $sourceNames=@($seen)
    $record.sourceHashes = @($sourceNames | Sort-Object -Unique | ForEach-Object {
        [ordered]@{path=$_;sha256=(Sha (Join-Path $taskRoot $_))}
    })
    . (Join-Path $PSScriptRoot 'Write-AnimationManagerRefsFixture.ps1')
    . (Join-Path $PSScriptRoot 'Write-AnimBlockRefsFixture.ps1')
    $generated=Join-Path $scratchRoot 'AnimationManagerRefsFixture.generated.h'
    $record['generatedFixture']=Write-AnimationManagerRefsFixture $taskRoot $generated
    $durableGenerated=Join-Path $outputRoot 'AnimationManagerRefsFixture.generated.h'
    Copy-Item -LiteralPath $generated -Destination $durableGenerated
    if((Sha $durableGenerated) -ne $record.generatedFixture.sha256){throw 'Generated actual SDK fixture copy changed.'}
    $record.generatedFixture['durableCopy']=$durableGenerated
    $unsignedGenerated=Join-Path $scratchRoot 'AnimationManagerUnsignedBlockRefs.generated.h'
    $record['unsignedBlockFixture']=Write-AnimBlockRefsFixture $taskRoot $unsignedGenerated
    $unsignedDurable=Join-Path $outputRoot 'AnimationManagerUnsignedBlockRefs.generated.h'
    Copy-Item -LiteralPath $unsignedGenerated -Destination $unsignedDurable
    if((Sha $unsignedDurable) -ne $record.unsignedBlockFixture.sha256){throw 'Unsigned block fixture durable copy changed'}
    $record.unsignedBlockFixture['durableCopy']=$unsignedDurable
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
        $executable = Join-Path $directory 'anim_manager_refs_de_tests.exe'
        $arguments = @('/nologo','/std:c++17','/EHsc','/W4','/WX','/O2','/MD','/DNOMINMAX','/DWIN32_LEAN_AND_MEAN',
            ('/I'+(Join-Path $taskRoot 'Shared/sdk')),('/I'+(Join-Path $taskRoot 'Client/sdk')),('/I'+$scratchRoot),
            (Join-Path $PSScriptRoot 'AnimManagerRefsDETests.cpp'),(Join-Path $taskRoot 'Client/game_de/CAnimManagerDE.cpp'),
            (Join-Path $taskRoot 'Client/game_de/CAnimBlockDE.cpp'),(Join-Path $taskRoot 'Client/game_de/CAnimBlockRequestDE.cpp'),
            ('/Fo'+$scratchRoot+[IO.Path]::DirectorySeparatorChar),('/Fd'+(Join-Path $scratchRoot 'compiler.pdb')),('/Fe'+$executable))
        $result = [ordered]@{architecture=$architecture;environment=$environmentResult;compile=$null;test=$null;
            checks=0;pointerBits=$null;executable=$executable;executableSha256=$null;actualAnimationManagerRefsSdkTested=$false;actualSignedManagerUnsignedBlockRefsSdkTested=$false}
        $record.results += $result
        $result.compile = RunOwned $compiler $arguments $directory 'compile' 60000
        if ($result.compile.exitCode -ne 0 -or $result.compile.timedOut) { throw "$architecture actual animation manager reference SDK fixture compilation failed." }
        $result.executableSha256 = Sha $executable
        $result.test = RunOwned $executable @() $directory 'test' 10000
        $text = [IO.File]::ReadAllText($result.test.stdout)
        $summary = [regex]::Match($text.TrimEnd(), '(?:^|\r?\n)Passed (\d+) animation manager reference SDK checks \((\d+)-bit; no native engine code executed\)$')
        if ($summary.Success) {
            $result.checks = [int]$summary.Groups[1].Value
            $result.pointerBits = [int]$summary.Groups[2].Value
        }
        $result.actualAnimationManagerRefsSdkTested=$text.Contains('Actual animation manager raw reference SDK fixture completed')
        $result.actualSignedManagerUnsignedBlockRefsSdkTested=$text.Contains('Actual manager signed and block unsigned reference fixture completed')
        $expectedBits = if ($architecture -eq 'x86') { 32 } else { 64 }
        if ($result.test.exitCode -ne 0 -or $result.test.timedOut -or $result.checks -le 0 -or $result.pointerBits -ne $expectedBits -or -not $result.actualAnimationManagerRefsSdkTested -or -not $result.actualSignedManagerUnsignedBlockRefsSdkTested) {
            throw "$architecture actual animation manager reference SDK checks failed or summary/pointer-width evidence was missing."
        }
    }
    $record.actualAnimationManagerRefsSdkTested=@($record.results|Where-Object{-not $_.actualAnimationManagerRefsSdkTested}).Count -eq 0
    $record.actualSignedManagerUnsignedBlockRefsSdkTested=@($record.results|Where-Object{-not $_.actualSignedManagerUnsignedBlockRefsSdkTested}).Count -eq 0
    $record.success = $record.results.Count -eq 2 -and $record.actualAnimationManagerRefsSdkTested -and $record.actualSignedManagerUnsignedBlockRefsSdkTested
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
if (-not $record.success) { throw "Actual animation manager reference SDK checks failed; see $outputRoot" }
