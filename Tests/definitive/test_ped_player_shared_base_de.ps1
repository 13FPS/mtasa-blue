#!/usr/bin/env pwsh
#requires -Version 7.0
# Compile the real DE ped chain with actual CPlayerPed identity; inherited historical cases never execute.
[CmdletBinding()]
param([ValidateSet('x86','x64')][string]$Architecture='x64')

$expectedPointerBits = if ($Architecture -eq 'x86') { 32 } else { 64 }
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$taskRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../..')).Path
$id = (Get-Date -Format 'yyyyMMdd-HHmmss') + '-' + [guid]::NewGuid().ToString('N')
$outputRoot = Join-Path $taskRoot ('Build/mta-ped-player-shared-base-de/' + $id)
$scratchRoot = Join-Path $taskRoot ('Build/tmp/ped-player-shared-base-de-' + $id)
[IO.Directory]::CreateDirectory($outputRoot) | Out-Null
[IO.Directory]::CreateDirectory($scratchRoot) | Out-Null
$previousEnvironment = @{}
foreach ($name in @('TEMP','TMP','VCToolsInstallDir','INCLUDE','LIB','LIBPATH','PATH')) {
    $previousEnvironment[$name] = [Environment]::GetEnvironmentVariable($name,'Process')
}
$record = [ordered]@{
    schema='mta_ped_player_shared_base_de_tests_v1';directory=$outputRoot;success=$false;
    recordedAt=[DateTimeOffset]::Now.ToString('o');sourceHashes=@();sourcesUnchanged=$true;
    generatedFixture=$null;result=$null;failure=$null;scratchDirectory=$scratchRoot;scratchRemoved=$false;
    scope='Actual CPlayerPed plus complete rejecting test-only leaf over the current CPedClearWeaponDE chain: one CPed/physical/entity identity, sibling RTTI, existing vitals and ClearWeapon owner dispatch, shared StoredPointer/type and explicit refusal. The historical ClearWeapon main is renamed and never invoked. No native player creation, complete production player or CGame factory is claimed.';
    nativeEngineCodeExecuted=$false;gameLaunched=$false;gameAttached=$false;installationChanged=$false;
    completeCGameReady=$false;fullMtaReady=$false;completePedReady=$false;completePoolsReady=$false;nativeRepairQualified=$false;actualSharedPlayerPedBaseSdkTested=$false
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
. (Join-Path $PSScriptRoot 'Write-PedClearWeaponFixture.ps1')

try {
    $productionSources = @('Client/game_de/CPedVitalsDE.cpp','Client/game_de/CPedMetadataDE.cpp','Client/game_de/CPedMutationDE.cpp','Client/game_de/CPedPhysicsDE.cpp','Client/game_de/CPedRotationDE.cpp','Client/game_de/CPedStateDE.cpp','Client/game_de/CPedFlagsDE.cpp','Client/game_de/CPedBehaviorDE.cpp','Client/game_de/CPedCombatDE.cpp','Client/game_de/CPedWeaponSelectionDE.cpp','Client/game_de/CPedPoseDE.cpp','Client/game_de/CPedWeaponStatsDE.cpp','Client/game_de/CPedWaterDE.cpp','Client/game_de/CPedOrientationDE.cpp','Client/game_de/CPedAreaDE.cpp','Client/game_de/CPedFireDE.cpp','Client/game_de/CPedCollisionDE.cpp','Client/game_de/CPedTasksDE.cpp','Client/game_de/CPedGogglesDE.cpp','Client/game_de/CPedVisibilityDE.cpp','Client/game_de/CPedOnScreenDE.cpp','Client/game_de/CPedGiveWeaponDE.cpp','Client/game_de/CPedClearWeaponDE.cpp')
    # Track only the actual local include graph of this SDK fixture and its
    # linked inherited implementation. Native tests and classic callers have
    # their own receipts and are not silently claimed by this forwarding test.
    $sourceNames = $productionSources + @('Tests/definitive/PedPlayerSharedBaseDETests.cpp','Tests/definitive/Write-PedClearWeaponFixture.ps1',
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
    $generated = Join-Path $scratchRoot 'PedClearWeaponFixture.generated.h'
    $record.generatedFixture = Write-PedClearWeaponFixture $taskRoot $generated
    # Extend only this generated rejecting test leaf. Production remains
    # abstract; the existing generator and historical tests are unchanged.
    $fixtureText = [IO.File]::ReadAllText($generated)
    $sealedLeaf = 'class PedClearWeaponFixture final : public mta::game_de::CPedClearWeaponDE'
    if ([regex]::Matches($fixtureText,[regex]::Escape($sealedLeaf)).Count -ne 1) { throw 'Existing strict ped fixture leaf changed.' }
    [IO.File]::WriteAllText($generated,$fixtureText.Replace($sealedLeaf,'class PedClearWeaponFixture : public mta::game_de::CPedClearWeaponDE'),[Text.UTF8Encoding]::new($false))
    $record.generatedFixture['sha256'] = Sha $generated
    $record.generatedFixture['testOnlyExtendableLeaf'] = $true
    $durableGenerated = Join-Path $outputRoot 'PedClearWeaponFixture.generated.h'
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
    @('@echo off',('call "{0}" -no_logo -arch={1} -host_arch=x64 >nul' -f $developerCommand,$Architecture),
        'if errorlevel 1 exit /b 1','set VCToolsInstallDir','set INCLUDE','set LIB','set PATH','exit /b 0') |
        Set-Content -LiteralPath $environmentScript -Encoding ascii
    $environmentResult = RunOwned $env:ComSpec @('/d','/s','/c','capture-msvc.cmd') $outputRoot 'msvc-environment' 30000
    if ($environmentResult.exitCode -ne 0 -or $environmentResult.timedOut) { throw 'x64 MSVC environment initialization failed.' }
    foreach ($entry in [IO.File]::ReadAllLines($environmentResult.stdout)) {
        if ($entry -match '^(VCToolsInstallDir|INCLUDE|LIB|LIBPATH|PATH)=(.*)$') {
            [Environment]::SetEnvironmentVariable($Matches[1],$Matches[2],'Process')
        }
    }
    $compiler = Join-Path $env:VCToolsInstallDir ('bin/Hostx64/'+$Architecture+'/cl.exe')
    $executable = Join-Path $outputRoot 'ped_player_shared_base_de_tests.exe'
    $arguments = @('/nologo','/std:c++17','/EHsc','/W4','/WX','/O2','/MD','/DNOMINMAX','/DWIN32_LEAN_AND_MEAN',
        ('/I'+(Join-Path $taskRoot 'Shared/sdk')),('/I'+(Join-Path $taskRoot 'Client/sdk')),('/I'+$scratchRoot),
        (Join-Path $PSScriptRoot 'PedPlayerSharedBaseDETests.cpp')) + @($productionSources | ForEach-Object { Join-Path $taskRoot $_ }) +
        @(('/Fo'+$scratchRoot+[IO.Path]::DirectorySeparatorChar),('/Fd'+(Join-Path $scratchRoot 'compiler.pdb')),('/Fe'+$executable),'/link','bcrypt.lib')
    $record.result = [ordered]@{architecture=$Architecture;environment=$environmentResult;compile=$null;test=$null;checks=0;
        pointerBits=$null;executable=$executable;executableSha256=$null}
    $record.result.compile = RunOwned $compiler $arguments $outputRoot 'compile' 60000
    if ($record.result.compile.exitCode -ne 0 -or $record.result.compile.timedOut) { throw 'Actual shared player CPed base fixture compilation failed.' }
    $record.result.executableSha256 = Sha $executable
    $record.result.test = RunOwned $executable @() $outputRoot 'test' 10000
    $summary = [regex]::Match([IO.File]::ReadAllText($record.result.test.stdout).TrimEnd(),
        '(?:^|\r?\n)Passed (\d+) actual shared player CPed base checks \((\d+)-bit; no native engine code executed\)$')
    if ($summary.Success) {
        $record.result.checks = [int]$summary.Groups[1].Value
        $record.result.pointerBits = [int]$summary.Groups[2].Value
    }
    $record.success = $record.result.test.exitCode -eq 0 -and -not $record.result.test.timedOut -and
        $record.result.checks -gt 0 -and $record.result.pointerBits -eq $expectedPointerBits
    $record.actualSharedPlayerPedBaseSdkTested = $record.success -and [IO.File]::ReadAllText($record.result.test.stdout).Contains('Actual shared player CPed base fixture completed')
    $record.success = $record.success -and $record.actualSharedPlayerPedBaseSdkTested
    if (-not $record.success) { throw 'Actual shared player CPed base checks failed or summary evidence was missing.' }
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
if (-not $record.success) { throw "Actual shared player CPed base checks failed; see $outputRoot" }
