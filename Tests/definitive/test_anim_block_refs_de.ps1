#!/usr/bin/env pwsh
#requires -Version 7.0
# Compile actual block reference SDK dispatch and shared-policy ordering in standard assert/release modes.
[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$taskRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../..')).Path
$id = (Get-Date -Format 'yyyyMMdd-HHmmss') + '-' + [guid]::NewGuid().ToString('N')
$outputRoot = Join-Path $taskRoot ('Build/mta-anim-block-refs-de/' + $id)
$scratchRoot = Join-Path $taskRoot ('Build/tmp/anim-block-refs-de-' + $id)
[IO.Directory]::CreateDirectory($outputRoot) | Out-Null
[IO.Directory]::CreateDirectory($scratchRoot) | Out-Null
$previousEnvironment = @{}
foreach ($name in @('TEMP','TMP','VCToolsInstallDir','INCLUDE','LIB','LIBPATH','PATH')) {
    $previousEnvironment[$name] = [Environment]::GetEnvironmentVariable($name,'Process')
}
$record = [ordered]@{
    schema='mta_anim_block_refs_de_tests_v1';directory=$outputRoot;success=$false;
    recordedAt=[DateTimeOffset]::Now.ToString('o');sourceHashes=@();sourcesUnchanged=$true;
    generatedFixture=$null;results=@();failure=$null;scratchDirectory=$scratchRoot;scratchRemoved=$false;
    scope='Actual CAnimBlockDE AddRef/GetRefs forwarding and actual shared CAnimBlockRequestPolicy coupled to those SDK methods with instrumented request/load/nullable-lookup boundaries. Existing strict remaining virtuals reject raw-interface, hierarchy and Request calls. No positive CGame or CStreaming factory, old suite replay or native engine execution.';
    nativeEngineCodeExecuted=$false;gameLaunched=$false;gameAttached=$false;installationChanged=$false;
    completeAnimationFactoryReady=$false;completeGameReady=$false;classicInterfacesFabricated=$false;
    sharedAnimBlockRequestRefOrderingTested=$false;actualAnimBlockRefsSdkTested=$false
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
. (Join-Path $PSScriptRoot 'Write-AnimBlockRefsFixture.ps1')

try {
    $productionSources = @('Client/game_de/CAnimBlockDE.cpp')
    $sourceNames = $productionSources + @('Tests/definitive/AnimBlockRefsDETests.cpp','Tests/definitive/Write-AnimBlockRefsFixture.ps1',
        'Tests/definitive/AnimBlockNameDETests.cpp',[IO.Path]::GetRelativePath($taskRoot,$PSCommandPath).Replace('\','/'))
    $pending = [Collections.Generic.Queue[string]]::new()
    foreach($source in $sourceNames){$pending.Enqueue($source)}
    $observed = [Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
    while($pending.Count) {
        $source=$pending.Dequeue()
        if(-not $observed.Add($source)){continue}
        $sourcePath=Join-Path $taskRoot $source
        if(-not (Test-Path -LiteralPath $sourcePath -PathType Leaf)){throw "Actual SDK input is missing: $source"}
        foreach($match in [regex]::Matches([IO.File]::ReadAllText($sourcePath),'(?m)^\s*#\s*include\s*[<"](?<path>[^>"]+)[>"]')) {
            $name=$match.Groups['path'].Value
            foreach($candidate in @((Join-Path (Split-Path -Parent $sourcePath) $name),(Join-Path $taskRoot ('Shared/sdk/'+$name)),(Join-Path $taskRoot ('Client/sdk/'+$name)))) {
                if(Test-Path -LiteralPath $candidate -PathType Leaf) {
                    $relative=[IO.Path]::GetRelativePath($taskRoot,(Resolve-Path -LiteralPath $candidate).Path).Replace('\','/')
                    if($relative.StartsWith('../')){throw 'A local SDK input escaped the workspace'}
                    $pending.Enqueue($relative);break
                }
            }
        }
    }
    $sourceNames=@($observed)
    $record.sourceHashes = @($sourceNames | Sort-Object -Unique | ForEach-Object {
        [ordered]@{path=$_;sha256=(Sha (Join-Path $taskRoot $_))}
    })
    $generated=Join-Path $scratchRoot 'AnimBlockRefsFixture.generated.h'
    $record.generatedFixture=Write-AnimBlockRefsFixture $taskRoot $generated
    $durableGenerated=Join-Path $outputRoot 'AnimBlockRefsFixture.generated.h'
    Copy-Item -LiteralPath $generated -Destination $durableGenerated
    $record.generatedFixture['durablePath']=$durableGenerated
    $record.generatedFixture['durableSha256']=Sha $durableGenerated

    $env:TEMP = $scratchRoot
    $env:TMP = $scratchRoot
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
    $discovery = RunOwned $vswhere @('-latest','-products','*','-requires','Microsoft.VisualStudio.Component.VC.Tools.x86.x64','-property','installationPath') $outputRoot 'vswhere' 10000
    $visualStudio = [IO.File]::ReadAllText($discovery.stdout).Trim()
    if ($discovery.exitCode -ne 0 -or $discovery.timedOut -or -not $visualStudio) { throw 'Visual Studio x64 tools are required.' }
    $developerCommand = Join-Path $visualStudio 'Common7/Tools/VsDevCmd.bat'
    $environmentScript = Join-Path $outputRoot 'capture-msvc.cmd'
    @('@echo off',('call "{0}" -no_logo -arch=x64 -host_arch=x64 >nul' -f $developerCommand),
        'if errorlevel 1 exit /b 1','set VCToolsInstallDir','set INCLUDE','set LIB','set PATH','exit /b 0') |
        Set-Content -LiteralPath $environmentScript -Encoding ascii
    $environmentResult = RunOwned $env:ComSpec @('/d','/s','/c','capture-msvc.cmd') $outputRoot 'msvc-environment' 30000
    if ($environmentResult.exitCode -ne 0 -or $environmentResult.timedOut) { throw 'x64 MSVC environment initialization failed.' }
    foreach ($entry in [IO.File]::ReadAllLines($environmentResult.stdout)) {
        if ($entry -match '^(VCToolsInstallDir|INCLUDE|LIB|LIBPATH|PATH)=(.*)$') {
            [Environment]::SetEnvironmentVariable($Matches[1],$Matches[2],'Process')
        }
    }
    $compiler = Join-Path $env:VCToolsInstallDir 'bin/Hostx64/x64/cl.exe'
    $record.results = @()
    foreach ($variant in @('Assertions','Release')) {
        $directory = Join-Path $outputRoot $variant
        [IO.Directory]::CreateDirectory($directory) | Out-Null
        $executable = Join-Path $directory 'anim_block_refs_de_tests.exe'
        $assertFlag = if ($variant -eq 'Assertions') { '/UNDEBUG' } else { '/DNDEBUG' }
        $arguments = @('/nologo','/std:c++17','/EHsc','/W4','/WX','/O2','/MD','/DNOMINMAX','/DWIN32_LEAN_AND_MEAN',$assertFlag,
            ('/I'+(Join-Path $taskRoot 'Shared/sdk')),('/I'+(Join-Path $taskRoot 'Client/sdk')),('/I'+$scratchRoot),
            (Join-Path $PSScriptRoot 'AnimBlockRefsDETests.cpp')) + @($productionSources | ForEach-Object { Join-Path $taskRoot $_ }) +
            @(('/Fo'+$scratchRoot+[IO.Path]::DirectorySeparatorChar),('/Fd'+(Join-Path $scratchRoot 'fixture.pdb')),('/Fe'+$executable),'/link','bcrypt.lib')
        $result = [ordered]@{architecture='x64';variant=$variant;environment=$environmentResult;compile=$null;test=$null;checks=0;
            pointerBits=$null;assertions=$null;executable=$executable;executableSha256=$null;success=$false;
            sharedAnimBlockRequestRefOrderingTested=$false;actualAnimBlockRefsSdkTested=$false}
        $record.results += $result
        $result.compile = RunOwned $compiler $arguments $directory 'compile' 60000
        if ($result.compile.exitCode -ne 0 -or $result.compile.timedOut) { throw "Actual block refs SDK/shared-policy $variant compilation failed." }
        $result.executableSha256 = Sha $executable
        $result.test = RunOwned $executable @() $directory 'test' 10000
        $stdout = [IO.File]::ReadAllText($result.test.stdout)
        $summary = [regex]::Match($stdout.TrimEnd(),
            '(?:^|\r?\n)Passed (\d+) animation block refs SDK/shared-policy checks \((\d+)-bit; assertions=(\d+)\)$')
        if ($summary.Success) {
            $result.checks = [int]$summary.Groups[1].Value
            $result.pointerBits = [int]$summary.Groups[2].Value
            $result.assertions = [int]$summary.Groups[3].Value
        }
        $expectedAssertions = if ($variant -eq 'Assertions') { 1 } else { 0 }
        $result.sharedAnimBlockRequestRefOrderingTested = $stdout.Contains('Shared animation block request ref ordering fixture completed')
        $result.actualAnimBlockRefsSdkTested = $stdout.Contains('Actual animation block refs SDK fixture completed')
        $result.success = $result.test.exitCode -eq 0 -and -not $result.test.timedOut -and $result.checks -gt 0 -and
            $result.pointerBits -eq 64 -and $result.assertions -eq $expectedAssertions -and
            $result.sharedAnimBlockRequestRefOrderingTested -and $result.actualAnimBlockRefsSdkTested
        if (-not $result.success) { throw "Actual block refs SDK/shared-policy $variant checks failed or execution evidence was missing." }
    }
    $record.sharedAnimBlockRequestRefOrderingTested = @($record.results | Where-Object { -not $_.sharedAnimBlockRequestRefOrderingTested }).Count -eq 0
    $record.actualAnimBlockRefsSdkTested = @($record.results | Where-Object { -not $_.actualAnimBlockRefsSdkTested }).Count -eq 0
    $record.success = $record.results.Count -eq 2 -and @($record.results | Where-Object { -not $_.success }).Count -eq 0

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
if (-not $record.success) { throw "Actual block refs SDK/shared-policy checks failed; see $outputRoot" }
