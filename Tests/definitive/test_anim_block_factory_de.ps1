#!/usr/bin/env pwsh
#requires -Version 7.0
# Compile only focused actual player movement-group SDK dispatch; inherited methods link but their old tests are not replayed.
[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$taskRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../..')).Path
$id = (Get-Date -Format 'yyyyMMdd-HHmmss') + '-' + [guid]::NewGuid().ToString('N')
$outputRoot = Join-Path $taskRoot ('Build/mta-anim-block-factory-de/' + $id)
$scratchRoot = Join-Path $taskRoot ('Build/tmp/anim-block-factory-de-' + $id)
[IO.Directory]::CreateDirectory($outputRoot) | Out-Null
[IO.Directory]::CreateDirectory($scratchRoot) | Out-Null
$previousEnvironment = @{}
foreach ($name in @('TEMP','TMP','VCToolsInstallDir','INCLUDE','LIB','LIBPATH','PATH')) {
    $previousEnvironment[$name] = [Environment]::GetEnvironmentVariable($name,'Process')
}
$record = [ordered]@{
    schema='mta_anim_block_factory_de_tests_v1';directory=$outputRoot;success=$false;
    recordedAt=[DateTimeOffset]::Now.ToString('o');sourceHashes=@();sourcesUnchanged=$true;
    generatedFixture=$null;result=$null;failure=$null;scratchDirectory=$scratchRoot;scratchRemoved=$false;
    scope='Actual CAnimBlockRequestDE CPU products through all three CAnimManagerDE block factories, opaque row/hierarchy SDK dispatch, current copied identity and language-defined unique_ptr<Base> destruction. Explicit borrowed CGame fixture rejects every operation; no successful game/streaming provider or Request execution. No old suite replay.';
    nativeEngineCodeExecuted=$false;gameLaunched=$false;gameAttached=$false;installationChanged=$false;
    completeCGameReady=$false;fullMtaReady=$false;completeAnimationManagerReady=$false;completeAnimationBlockNativeLifetimeOwned=$false;actualAnimBlockFactorySdkTested=$false;actualAnimBlockPolymorphicLifetimeTested=$false;actualAnimBlockOpaqueHierarchySdkTested=$false
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
. (Join-Path $PSScriptRoot 'Write-AnimationBlockFactoryFixture.ps1')

function InterfaceMethods([string]$RelativePath) {
    $text = [IO.File]::ReadAllText((Join-Path $taskRoot $RelativePath))
    $text = [regex]::Replace($text,'(?s)/\*.*?\*/|//[^\r\n]*','')
    foreach ($match in [regex]::Matches($text,'virtual\s+(?<signature>[^;{}]+);')) {
        $signature = [regex]::Replace($match.Groups['signature'].Value,'\s*=\s*0\s*$','').Trim()
        $name = [regex]::Match($signature,'(?<name>\w+)\s*\(').Groups['name'].Value
        if (-not $name) { throw "Cannot derive rejecting fixture method from $signature" }
        # Defaults are caller syntax, not part of an overriding signature.
        $signature = [regex]::Replace($signature,'\s*=\s*[^,)]+','')
        [ordered]@{name=$name;signature=([regex]::Replace($signature,'\s+',' '))}
    }
}


try {
    $productionSources = @('Client/game_de/CAnimBlockDE.cpp','Client/game_de/CAnimBlockRequestDE.cpp','Client/game_de/CAnimManagerDE.cpp')
    # Track only the actual local include graph of this SDK fixture and its
    # linked inherited implementation. Native tests and classic callers have
    # their own receipts and are not silently claimed by this forwarding test.
    $sourceNames = $productionSources + @('Tests/definitive/AnimBlockFactoryDETests.cpp','Tests/definitive/Write-AnimationBlockFactoryFixture.ps1',
        'Tests/definitive/Write-AnimationManagerFixture.ps1','Client/sdk/game/CGame.h',
        [IO.Path]::GetRelativePath($taskRoot,$PSCommandPath).Replace('\','/'))
    $pending = [Collections.Generic.Queue[string]]::new()
    foreach($source in $sourceNames){$pending.Enqueue($source)}
    $observed = [Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
    while($pending.Count) {
        $source = $pending.Dequeue()
        if(-not $observed.Add($source)){continue}
        $sourcePath = Join-Path $taskRoot $source
        if(-not (Test-Path -LiteralPath $sourcePath -PathType Leaf)){throw "Actual SDK input is missing: $source"}
        foreach($match in [regex]::Matches([IO.File]::ReadAllText($sourcePath),'(?m)^\s*#\s*include\s*[<"](?<path>[^>"]+)[>"]')) {
            $includeName=$match.Groups['path'].Value
            $candidates=@((Join-Path (Split-Path -Parent $sourcePath) $includeName),
                (Join-Path $taskRoot ('Shared/sdk/'+$includeName)),(Join-Path $taskRoot ('Client/sdk/'+$includeName)))
            foreach($candidate in $candidates) {
                if(Test-Path -LiteralPath $candidate -PathType Leaf) {
                    $resolved=(Resolve-Path -LiteralPath $candidate).Path
                    $relative=[IO.Path]::GetRelativePath($taskRoot,$resolved).Replace('\','/')
                    if($relative.StartsWith('../')){throw 'A local SDK input escaped the workspace'}
                    $pending.Enqueue($relative)
                    break
                }
            }
        }
    }
    $sourceNames=@($observed)
    $record.sourceHashes = @($sourceNames | Sort-Object -Unique | ForEach-Object {
        [ordered]@{path=$_;sha256=(Sha (Join-Path $taskRoot $_))}
    })
    $generated=Join-Path $scratchRoot 'AnimationBlockFactoryFixture.generated.h'
    $record.generatedFixture=Write-AnimationBlockFactoryFixture $taskRoot $generated
    $gameMethods=@(InterfaceMethods 'Client/sdk/game/CGame.h')
    $gameGenerated=Join-Path $scratchRoot 'AnimationBlockFactoryGameFixture.generated.h'
    $gameLines=@('#pragma once','#pragma warning(push)','#pragma warning(disable : 4100)',
        'namespace animation_block_factory_tests { class AnimationBlockFactoryGameFixture final : public CGame { public:')
    $gameLines+=@($gameMethods | ForEach-Object { $_.signature+' override { UnqualifiedGame(); }' })
    $gameLines+=@('}; }','#pragma warning(pop)')
    [IO.File]::WriteAllLines($gameGenerated,$gameLines,[Text.UTF8Encoding]::new($false))
    $record.generatedFixture['rejectingGameOverrides']=$gameMethods.Count
    $record.generatedFixture['durableFiles']=@('AnimationBlockFactoryFixture.generated.h','AnimationBlockFactoryGameFixture.generated.h' | ForEach-Object {
        $destination=Join-Path $outputRoot $_
        Copy-Item -LiteralPath (Join-Path $scratchRoot $_) -Destination $destination
        [ordered]@{path=$destination;sha256=(Sha $destination)}
    })
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
    $executable = Join-Path $outputRoot 'anim_block_factory_de_tests.exe'
    $arguments = @('/nologo','/std:c++17','/EHsc','/W4','/WX','/O2','/MD','/DNOMINMAX','/DWIN32_LEAN_AND_MEAN',
        ('/I'+(Join-Path $taskRoot 'Shared/sdk')),('/I'+(Join-Path $taskRoot 'Client/sdk')),('/I'+$scratchRoot),
        (Join-Path $PSScriptRoot 'AnimBlockFactoryDETests.cpp')) + @($productionSources | ForEach-Object { Join-Path $taskRoot $_ }) +
        @(('/Fo'+$scratchRoot+[IO.Path]::DirectorySeparatorChar),('/Fd'+(Join-Path $scratchRoot 'compiler.pdb')),('/Fe'+$executable),'/link','bcrypt.lib')
    $record.result = [ordered]@{architecture='x64';environment=$environmentResult;compile=$null;test=$null;checks=0;
        pointerBits=$null;executable=$executable;executableSha256=$null}
    $record.result.compile = RunOwned $compiler $arguments $outputRoot 'compile' 60000
    if ($record.result.compile.exitCode -ne 0 -or $record.result.compile.timedOut) { throw 'Actual animation block product SDK fixture compilation failed.' }
    $record.result.executableSha256 = Sha $executable
    $record.result.test = RunOwned $executable @() $outputRoot 'test' 10000
    $summary = [regex]::Match([IO.File]::ReadAllText($record.result.test.stdout).TrimEnd(),
        '(?:^|\r?\n)Passed (\d+) animation block factory SDK checks \((\d+)-bit; no native engine code executed\)$')
    if ($summary.Success) {
        $record.result.checks = [int]$summary.Groups[1].Value
        $record.result.pointerBits = [int]$summary.Groups[2].Value
    }
    $record.success = $record.result.test.exitCode -eq 0 -and -not $record.result.test.timedOut -and
        $record.result.checks -gt 0 -and $record.result.pointerBits -eq 64
    $record.actualAnimBlockFactorySdkTested = $record.success -and [IO.File]::ReadAllText($record.result.test.stdout).Contains('Actual animation block factory SDK fixture completed')
    $stdout=[IO.File]::ReadAllText($record.result.test.stdout)
    $record['actualAnimBlockPolymorphicLifetimeTested']=$stdout.Contains('Actual animation block polymorphic CPU lifetime fixture completed')
    $record['actualAnimBlockOpaqueHierarchySdkTested']=$stdout.Contains('Actual animation block opaque and hierarchy SDK fixture completed')
    $record.success = $record.success -and $record.actualAnimBlockFactorySdkTested -and $record.actualAnimBlockPolymorphicLifetimeTested -and $record.actualAnimBlockOpaqueHierarchySdkTested
    if (-not $record.success) { throw 'Actual animation block product SDK checks failed or summary evidence was missing.' }
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
if (-not $record.success) { throw "Actual animation block product SDK checks failed; see $outputRoot" }
