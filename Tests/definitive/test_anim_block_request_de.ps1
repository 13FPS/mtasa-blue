#!/usr/bin/env pwsh
#requires -Version 7.0
# Compile shared actual animation request and abstract DE SDK dispatch in standard assert/release modes.
[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$taskRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../..')).Path
$id = (Get-Date -Format 'yyyyMMdd-HHmmss') + '-' + [guid]::NewGuid().ToString('N')
$outputRoot = Join-Path $taskRoot ('Build/mta-anim-block-request-de/' + $id)
$scratchRoot = Join-Path $taskRoot ('Build/tmp/anim-block-request-de-' + $id)
[IO.Directory]::CreateDirectory($outputRoot) | Out-Null
[IO.Directory]::CreateDirectory($scratchRoot) | Out-Null
$previousEnvironment = @{}
foreach ($name in @('TEMP','TMP','VCToolsInstallDir','INCLUDE','LIB','LIBPATH','PATH')) {
    $previousEnvironment[$name] = [Environment]::GetEnvironmentVariable($name,'Process')
}
$record = [ordered]@{
    schema='mta_anim_block_request_de_tests_v1';directory=$outputRoot;success=$false;
    recordedAt=[DateTimeOffset]::Now.ToString('o');sourceHashes=@();sourcesUnchanged=$true;
    generatedFixture=$null;results=@();failure=$null;scratchDirectory=$scratchRoot;scratchRemoved=$false;
    scope='Actual shared animation-block Request and actual CAnimBlockRequestDE SDK dispatch with real borrowed CGame/CStreaming interfaces. Two focused x64 variants preserve standard assertions versus NDEBUG observation elision, DWORD wrap, unused blocking lookup, request/ref/load order and exception prefixes. No historical animation/model aggregate suite or complete animation factory claim.';
    nativeEngineCodeExecuted=$false;gameLaunched=$false;gameAttached=$false;installationChanged=$false;
    completeAnimationFactoryReady=$false;nativeAddRefImplemented=$false;classicInterfacesFabricated=$false;
    actualSharedAnimationBlockRequestPolicyTested=$false;actualAnimationBlockRequestSdkTested=$false
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
    $productionSources = @('Client/game_de/CAnimBlockRequestDE.cpp','Client/game_de/CAnimBlockDE.cpp','Client/game_de/CStreamingDE.cpp')
    $sourceNames = $productionSources + @(
        'Client/game_de/CAnimBlockRequestDE.h','Client/game_de/CAnimBlockDE.h','Client/game_de/NativeAnimationBlockAccess.h',
        'Client/sdk/game/CAnimBlockRequestPolicy.h','Client/sdk/game/CAnimBlock.h','Client/sdk/game/CGame.h','Client/sdk/game/CStreaming.h',
        'Client/game_sa/CAnimBlockSA.cpp','Client/game_sa/CAnimBlockSA.h',
        'Client/game_de/CStreamingDE.h','Client/sdk/game/CModelRequestPolicy.h','Client/sdk/game/CAnimBlock.h','Client/sdk/game/Common.h',
        'Client/game_de/NativeStreamingModelAccess.h','Client/game_de/NativeModelLoadedReader.h','Client/game_de/NativeModelLoadedProfile.h',
        'Client/game_de/NativeStreamingModelGroupsReader.h','Client/game_de/NativeStreamingModelGroupsProfile.h',
        'Client/game_de/NativeModelBankReader.h','Client/game_de/NativeModelBankProfile.h','Client/game_de/NativePedPoolReader.h','Client/game_de/NativePedPoolProfile.h',
        'Client/sdk/game/CGameClockService.h','Client/sdk/game/CGameAdapter.h','Shared/sdk/CVector.h',
        'Shared/sdk/SharedUtil.Defines.h','Shared/sdk/SharedUtil.IntTypes.h',
        'Tests/definitive/AnimBlockRequestDETests.cpp','Tests/definitive/test_anim_block_request_de.ps1')
    $record.sourceHashes = @($sourceNames | Sort-Object -Unique | ForEach-Object {
        [ordered]@{path=$_;sha256=(Sha (Join-Path $taskRoot $_))}
    })
    $blockProvided = @('GetIndex','IsLoaded','GetIDOffset','GetAnimationCount','Request','AddRef')
    $streamingProvided = @('RequestModel','RemoveModel','HasModelLoaded','LoadAllRequestedModels','LoadScene',
        'GetNumPedsLoaded','IsModelInLoadedPedGroup','GetNumLoadedVehicles','IsModelInLoadedVehicleGroup')
    $gameMethods = @(InterfaceMethods 'Client/sdk/game/CGame.h' | Where-Object { $_.name -notin @('GetStreaming','GetModelInfo','GetBaseIDforIFP') })
    $blockMethods = @(InterfaceMethods 'Client/sdk/game/CAnimBlock.h' | Where-Object { $blockProvided -notcontains $_.name })
    $streamingMethods = @(InterfaceMethods 'Client/sdk/game/CStreaming.h' | Where-Object { $streamingProvided -notcontains $_.name })
    $generated = Join-Path $scratchRoot 'AnimBlockRequestFixture.generated.h'
    $lines = @('#pragma once','#pragma warning(push)','#pragma warning(disable : 4100)','namespace fixture {',
        'class GameFixtureBase : public CGame { public:')
    $lines += @($gameMethods | ForEach-Object { $_.signature + ' override { Unqualified(); }' })
    $lines += @('};','class BlockFixtureBase : public mta::game_de::CAnimBlockRequestDE { public:',
        'BlockFixtureBase(mta::game_de::NativeAnimationBlockAccess& owner, const mta::game_de::NativeAnimationBlockIdentity& identity, CGame& game) : CAnimBlockRequestDE(owner,identity,game) {}')
    $lines += @($blockMethods | ForEach-Object { $_.signature + ' override { Unqualified(); }' })
    $lines += @('};','class StreamingFixtureBase : public mta::game_de::CStreamingDE { public:',
        'explicit StreamingFixtureBase(mta::game_de::NativeStreamingModelAccess& owner) noexcept : CStreamingDE(owner) {}')
    $lines += @($streamingMethods | ForEach-Object { $_.signature + ' override { Unqualified(); }' })
    $lines += @('};','}','#pragma warning(pop)')
    $lines | Set-Content -LiteralPath $generated -Encoding utf8NoBOM
    $durableGenerated = Join-Path $outputRoot 'AnimBlockRequestFixture.generated.h'
    Copy-Item -LiteralPath $generated -Destination $durableGenerated
    $record.generatedFixture = [ordered]@{path=$durableGenerated;sha256=(Sha $generated);gameRejectingOverrides=$gameMethods.Count;
        blockRejectingOverrides=$blockMethods.Count;streamingRejectingOverrides=$streamingMethods.Count;
        productionBlockMethodsNotOverridden=$blockProvided;nativeAddRefStillMandatory=$true}

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
        $executable = Join-Path $directory 'anim_block_request_de_tests.exe'
        $assertFlag = if ($variant -eq 'Assertions') { '/UNDEBUG' } else { '/DNDEBUG' }
        $arguments = @('/nologo','/std:c++17','/EHsc','/W4','/WX','/O2','/MD','/DNOMINMAX','/DWIN32_LEAN_AND_MEAN',$assertFlag,
            ('/I'+(Join-Path $taskRoot 'Shared/sdk')),('/I'+(Join-Path $taskRoot 'Client/sdk')),('/I'+$scratchRoot),
            (Join-Path $PSScriptRoot 'AnimBlockRequestDETests.cpp')) + @($productionSources | ForEach-Object { Join-Path $taskRoot $_ }) +
            @(('/Fo'+$scratchRoot+[IO.Path]::DirectorySeparatorChar),('/Fd'+(Join-Path $scratchRoot 'fixture.pdb')),('/Fe'+$executable),'/link','bcrypt.lib')
        $result = [ordered]@{architecture='x64';variant=$variant;environment=$environmentResult;compile=$null;test=$null;checks=0;
            pointerBits=$null;assertions=$null;executable=$executable;executableSha256=$null;success=$false;
            actualSharedAnimationBlockRequestPolicyTested=$false;actualAnimationBlockRequestSdkTested=$false}
        $record.results += $result
        $result.compile = RunOwned $compiler $arguments $directory 'compile' 60000
        if ($result.compile.exitCode -ne 0 -or $result.compile.timedOut) { throw "Actual animation Request SDK $variant compilation failed." }
        $result.executableSha256 = Sha $executable
        $result.test = RunOwned $executable @() $directory 'test' 10000
        $stdout = [IO.File]::ReadAllText($result.test.stdout)
        $summary = [regex]::Match($stdout.TrimEnd(),
            '(?:^|\r?\n)Passed (\d+) actual animation request SDK checks \((\d+)-bit; assertions=(\d+)\)$')
        if ($summary.Success) {
            $result.checks = [int]$summary.Groups[1].Value
            $result.pointerBits = [int]$summary.Groups[2].Value
            $result.assertions = [int]$summary.Groups[3].Value
        }
        $expectedAssertions = if ($variant -eq 'Assertions') { 1 } else { 0 }
        $result.actualSharedAnimationBlockRequestPolicyTested = $stdout.Contains('Production shared animation block request policy fixture completed')
        $result.actualAnimationBlockRequestSdkTested = $stdout.Contains('Production actual animation block request SDK fixture completed')
        $result.success = $result.test.exitCode -eq 0 -and -not $result.test.timedOut -and $result.checks -gt 0 -and
            $result.pointerBits -eq 64 -and $result.assertions -eq $expectedAssertions -and
            $result.actualSharedAnimationBlockRequestPolicyTested -and $result.actualAnimationBlockRequestSdkTested
        if (-not $result.success) { throw "Actual animation Request SDK $variant checks failed or execution evidence was missing." }
    }
    $record.actualSharedAnimationBlockRequestPolicyTested = @($record.results | Where-Object { -not $_.actualSharedAnimationBlockRequestPolicyTested }).Count -eq 0
    $record.actualAnimationBlockRequestSdkTested = @($record.results | Where-Object { -not $_.actualAnimationBlockRequestSdkTested }).Count -eq 0
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
if (-not $record.success) { throw "Actual animation block request SDK checks failed; see $outputRoot" }
