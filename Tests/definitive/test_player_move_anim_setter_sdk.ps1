#!/usr/bin/env pwsh
#requires -Version 7.0
# Focused actual inherited movement setter; each module-reference case has an independent process.
[CmdletBinding()]
param()
$ErrorActionPreference='Stop'
Set-StrictMode -Version Latest
$taskRoot=(Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../..')).Path
$id=(Get-Date -Format 'yyyyMMdd-HHmmss')+'-'+[guid]::NewGuid().ToString('N')
$outputRoot=Join-Path $taskRoot ('Build/mta-player-moveanim-setter-sdk/'+$id)
$scratchRoot=Join-Path $taskRoot ('Build/tmp/movesetter-'+$id)
[IO.Directory]::CreateDirectory($outputRoot)|Out-Null
[IO.Directory]::CreateDirectory($scratchRoot)|Out-Null
$previousEnvironment=@{}
foreach($name in @('TEMP','TMP','VCToolsInstallDir','INCLUDE','LIB','LIBPATH','PATH')){$previousEnvironment[$name]=[Environment]::GetEnvironmentVariable($name,'Process')}
$markers=@('actualPlayerMoveAnimSetterSdkTested','actualPlayerMoveAnimSetterRequestSdkTested','actualPlayerMoveAnimSetterPrefixSdkTested','actualPlayerMoveAnimSetterOwnerSdkTested','actualPlayerMoveAnimSetterAbstractSdkTested')
$caseCounts=[ordered]@{'invalid-default'=8;'loaded'=8;'request'=8;'request-incomplete'=8;'affinity'=8;'module-intent'=8;'already-loaded-block'=6;'request-refusal-ref'=8;
    'prefix-ref'=6;'prefix-enable'=6;'prefix-publish'=6;'prefix-write'=6;'prefix-reapply'=6;'prefix-request'=6;'prefix-load'=6;
    'observe-null-hierarchy'=4;'observe-wrong-group'=4;'observe-wrong-child'=4;'observe-missing-block'=4;'observe-refused-group'=4;
    'observe-null-children'=5;'observe-children-uninitialized'=5;'observe-children-changed'=5}
$expectedChecks=139
$record=[ordered]@{schema='mta_player_move_anim_setter_sdk_tests_v1';directory=$outputRoot;success=$false;recordedAt=[DateTimeOffset]::Now.ToString('o');
    sourceHashes=@();sourcesUnchanged=$true;generatedFixture=$null;results=@();failure=$null;scratchDirectory=$scratchRoot;scratchRemoved=$false;
    scope='Actual inherited CPlayerPedMoveAnimDE setter with strict real player/manager/CGame/streaming interfaces and actual block factory/Request dispatch. Twenty-three focused independent cases per architecture preserve module-wide reference intent without resetting production state. Historical getter/manager matrices are not executed. Native endpoints are explicit doubles; no mapped game function executes.';
    nativeEngineCodeExecuted=$false;gameLaunched=$false;gameAttached=$false;installationChanged=$false;completeCGameReady=$false;completePedReady=$false;fullMtaReady=$false;productionFactoryPublished=$false}
foreach($marker in $markers){$record[$marker]=$false}
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

. (Join-Path $PSScriptRoot 'Write-PlayerMoveAnimSetterFixture.ps1')
try {
    # The actual manager constructs these real facade families. Link their
    # implementations and shared name hash even though old matrices do not run.
    $productionSources=@('Client/game_de/CPedVitalsDE.cpp','Client/game_de/CPedMetadataDE.cpp','Client/game_de/CPedMutationDE.cpp','Client/game_de/CPedPhysicsDE.cpp','Client/game_de/CPedRotationDE.cpp','Client/game_de/CPedStateDE.cpp','Client/game_de/CPedFlagsDE.cpp','Client/game_de/CPedBehaviorDE.cpp','Client/game_de/CPedCombatDE.cpp','Client/game_de/CPedWeaponSelectionDE.cpp','Client/game_de/CPedPoseDE.cpp','Client/game_de/CPedWeaponStatsDE.cpp','Client/game_de/CPedWaterDE.cpp','Client/game_de/CPedOrientationDE.cpp','Client/game_de/CPedAreaDE.cpp','Client/game_de/CPedFireDE.cpp','Client/game_de/CPedCollisionDE.cpp','Client/game_de/CPedTasksDE.cpp','Client/game_de/CPedGogglesDE.cpp','Client/game_de/CPedVisibilityDE.cpp','Client/game_de/CPedOnScreenDE.cpp','Client/game_de/CPedGiveWeaponDE.cpp','Client/game_de/CPedClearWeaponDE.cpp','Client/game_de/CPedClothesDE.cpp','Client/game_de/CPedRebuildDE.cpp','Client/game_de/CPlayerPedMoveAnimDE.cpp','Client/game_de/CAnimBlockDE.cpp','Client/game_de/CAnimBlockRequestDE.cpp','Client/game_de/CAnimManagerDE.cpp','Client/game_de/CAnimBlendStaticAssociationDE.cpp','Client/game_de/CKeyGenDE.cpp','Client/game_de/CStreamingDE.cpp','Client/game_de/CAnimBlendAssocGroupDE.cpp','Client/game_de/CAnimBlendAssociationDE.cpp','Client/game_de/CAnimBlendHierarchyDE.cpp','Client/game_de/CAnimBlendSequenceDE.cpp','Client/game_de/NativeClassicAnimationNameHash.cpp')
    $sourceNames=$productionSources+@('Tests/definitive/PlayerMoveAnimSetterSdkTests.cpp','Tests/definitive/test_player_move_anim_setter_sdk.ps1',
        'Tests/definitive/Write-PlayerMoveAnimSetterFixture.ps1','Tests/definitive/Write-PlayerMoveAnimFixture.ps1',
        'Tests/definitive/Write-AnimationBlockFactoryFixture.ps1','Tests/definitive/Write-AnimationManagerFixture.ps1',
        'Tests/definitive/Write-PedRebuildFixture.ps1','Tests/definitive/Write-PedClothesFixture.ps1','Tests/definitive/Write-PedClearWeaponFixture.ps1',
        'Client/sdk/game/CPlayerPed.h','Client/sdk/game/CGame.h','Client/sdk/game/CStreaming.h','Client/sdk/game/CPlayerMoveAnimPolicy.h',
        'Tests/definitive/PlayerMoveAnimDETests.cpp','Tests/definitive/PedPlayerSharedBaseDETests.cpp','Tests/definitive/PedClearWeaponDETests.cpp')
    $pending=[Collections.Generic.Queue[string]]::new()
    foreach($source in $sourceNames){$pending.Enqueue($source)}
    $observed=[Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
    while($pending.Count) {
        $source=$pending.Dequeue()
        if(-not $observed.Add($source)){continue}
        $path=Join-Path $taskRoot $source
        if(-not(Test-Path -LiteralPath $path -PathType Leaf)){throw "SDK input missing: $source"}
        foreach($match in [regex]::Matches([IO.File]::ReadAllText($path),'(?m)^\s*#\s*include\s*[<"](?<path>[^>"]+)[>"]')) {
            $name=$match.Groups['path'].Value
            foreach($candidate in @((Join-Path (Split-Path -Parent $path) $name),(Join-Path $taskRoot ('Shared/sdk/'+$name)),(Join-Path $taskRoot ('Client/sdk/'+$name)))) {
                if(Test-Path -LiteralPath $candidate -PathType Leaf) {
                    $relative=[IO.Path]::GetRelativePath($taskRoot,(Resolve-Path -LiteralPath $candidate).Path).Replace('\','/')
                    if($relative.StartsWith('../')){throw 'SDK input escaped the workspace.'}
                    $pending.Enqueue($relative);break
                }
            }
        }
    }
    $record.sourceHashes=@($observed|Sort-Object|ForEach-Object{[ordered]@{path=$_;sha256=Sha (Join-Path $taskRoot $_)}})
    $generated=Join-Path $scratchRoot 'PlayerMoveAnimSetterFixture.generated.h'
    $record.generatedFixture=Write-PlayerMoveAnimSetterFixture $taskRoot $generated
    $record.generatedFixture['durableFiles']=@(Get-ChildItem -LiteralPath $scratchRoot -Filter '*.generated.h'|ForEach-Object{
        $copy=Join-Path $outputRoot $_.Name
        Copy-Item -LiteralPath $_.FullName -Destination $copy
        [ordered]@{path=$copy;sha256=Sha $copy}
    })
    $env:TEMP=$scratchRoot;$env:TMP=$scratchRoot
    $vswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
    $discovery=RunOwned $vswhere @('-latest','-products','*','-requires','Microsoft.VisualStudio.Component.VC.Tools.x86.x64','-property','installationPath') $outputRoot 'vswhere' 10000
    $visualStudio=[IO.File]::ReadAllText($discovery.stdout).Trim()
    if($discovery.exitCode -ne 0 -or $discovery.timedOut -or -not $visualStudio){throw 'MSVC x86/x64 tools are required.'}
    $developerCommand=Join-Path $visualStudio 'Common7/Tools/VsDevCmd.bat'
    foreach($architecture in @('x86','x64')) {
        $directory=Join-Path $outputRoot $architecture
        [IO.Directory]::CreateDirectory($directory)|Out-Null
        $batch=Join-Path $directory 'capture-msvc.cmd'
        @('@echo off',('call "{0}" -no_logo -arch={1} -host_arch=x64 >nul' -f $developerCommand,$architecture),
            'if errorlevel 1 exit /b 1','set VCToolsInstallDir','set INCLUDE','set LIB','set PATH','exit /b 0')|Set-Content -LiteralPath $batch -Encoding ascii
        $environment=RunOwned $env:ComSpec @('/d','/s','/c','capture-msvc.cmd') $directory 'msvc-environment' 30000
        if($environment.exitCode -ne 0 -or $environment.timedOut){throw "$architecture MSVC environment initialization failed."}
        foreach($entry in [IO.File]::ReadAllLines($environment.stdout)) {
            if($entry -match '^(VCToolsInstallDir|INCLUDE|LIB|LIBPATH|PATH)=(.*)$'){[Environment]::SetEnvironmentVariable($Matches[1],$Matches[2],'Process')}
        }
        $compiler=Join-Path $env:VCToolsInstallDir "bin/Hostx64/$architecture/cl.exe"
        $exe=Join-Path $directory 'player_move_anim_setter_sdk_tests.exe'
        $arguments=@('/nologo','/std:c++17','/EHsc','/W4','/WX','/O2','/MD','/DWIN32','/DNOMINMAX','/DWIN32_LEAN_AND_MEAN',
            ('/I'+(Join-Path $taskRoot 'Shared/sdk')),('/I'+(Join-Path $taskRoot 'Client/sdk')),('/I'+$scratchRoot),
            (Join-Path $PSScriptRoot 'PlayerMoveAnimSetterSdkTests.cpp'))+@($productionSources|ForEach-Object{Join-Path $taskRoot $_})+
            @(('/Fo'+$scratchRoot+[IO.Path]::DirectorySeparatorChar),('/Fd'+(Join-Path $scratchRoot 'compiler.pdb')),('/Fe'+$exe),'/link','bcrypt.lib')
        $result=[ordered]@{architecture=$architecture;environment=$environment;compile=$null;cases=@();checks=0;pointerBits=$null;executable=$exe;executableSha256=$null}
        $record.results+=@($result)
        $result.compile=RunOwned $compiler $arguments $directory 'compile' 120000
        if($result.compile.exitCode -ne 0 -or $result.compile.timedOut){throw "$architecture setter SDK compilation failed."}
        $result.executableSha256=Sha $exe
        $bits=if($architecture -eq 'x86'){32}else{64}
        foreach($caseName in $caseCounts.Keys) {
            # Reference intent is backend-module lifetime state. Separate
            # processes preserve that contract without a test-only reset API.
            $test=RunOwned $exe @($caseName) $directory $caseName 10000
            $stdout=[IO.File]::ReadAllText($test.stdout)
            $summary=[regex]::Match($stdout.TrimEnd(),'(?:^|\r?\n)Passed (\d+) player move animation setter SDK checks \((\d+)-bit; no native engine code executed\)$')
            $case=[ordered]@{name=$caseName;test=$test;checks=0;pointerBits=$null;completed=$stdout.Contains('Actual inherited player move animation setter SDK case completed: '+$caseName)}
            if($summary.Success){$case.checks=[int]$summary.Groups[1].Value;$case.pointerBits=[int]$summary.Groups[2].Value}
            $result.cases+=@($case)
            if($test.exitCode -ne 0 -or $test.timedOut -or -not $case.completed -or $case.checks -ne $caseCounts[$caseName] -or $case.pointerBits -ne $bits){throw "$architecture setter SDK case failed: $caseName"}
            $result.checks+=$case.checks
        }
        $result.pointerBits=$bits
        if($result.cases.Count -ne 23 -or $result.checks -ne $expectedChecks){throw "$architecture setter SDK aggregate count mismatch."}
        foreach($marker in $markers){$result[$marker]=$true}
    }
    foreach($marker in $markers){$record[$marker]=@($record.results|Where-Object{-not $_[$marker]}).Count -eq 0}
    $record.success=$record.results.Count -eq 2
} catch {AddFailure ($_.Exception.ToString())}
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
[ordered]@{success=$record.success;receipt=(Join-Path $outputRoot 'test-result.json');failure=$record.failure}|ConvertTo-Json -Compress
if(-not $record.success){throw "Player move animation setter SDK checks failed; see $outputRoot"}
