#!/usr/bin/env pwsh
#requires -Version 7.0
# Test the actual abstract ped orchestration with real shared manager/stat policy.
[CmdletBinding()]
param()
$ErrorActionPreference='Stop'
Set-StrictMode -Version Latest
$taskRoot=(Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../..')).Path
$id=(Get-Date -Format 'yyyyMMdd-HHmmss')+'-'+[guid]::NewGuid().ToString('N')
$outputRoot=Join-Path $taskRoot ('Build/mta-ped-weapon-stats-de/'+$id)
$scratchRoot=Join-Path $taskRoot ('Build/tmp/ped-weapon-stats-de-'+$id)
[IO.Directory]::CreateDirectory($outputRoot)|Out-Null
[IO.Directory]::CreateDirectory($scratchRoot)|Out-Null
$previousEnvironment=@{}
foreach($name in @('TEMP','TMP','VCToolsInstallDir','INCLUDE','LIB','LIBPATH','PATH')){$previousEnvironment[$name]=[Environment]::GetEnvironmentVariable($name,'Process')}
$record=[ordered]@{
    schema='mta_ped_weapon_stats_sdk_tests_v1';directory=$outputRoot;success=$false;recordedAt=[DateTimeOffset]::Now.ToString('o');
    sourceHashes=@();sourcesUnchanged=$true;generatedFixture=$null;result=$null;failure=$null;scratchDirectory=$scratchRoot;scratchRemoved=$false;
    actualPedWeaponStatsSdkTested=$false;nativeEngineCodeExecuted=$false;gameLaunched=$false;installationChanged=$false;
    completeCGameReady=$false;completePedReady=$false;nativeWeaponFactoryClaimed=$false;
    scope='Actual CPedWeaponStatsDE context/weapon/stat orchestration, real inherited current-slot SDK forwarding, actual shared manager selection and real custom CPU records. Exact lazy order, separate GetStats owners, raw skill/range bits, actual-null fallback, virtual range dispatch and refusal/no-replay. No native live row or complete weapon/ped/game factory is fabricated.'
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
    $productionSources=@('Client/game_de/CPedVitalsDE.cpp','Client/game_de/CPedMetadataDE.cpp','Client/game_de/CPedMutationDE.cpp',
        'Client/game_de/CPedPhysicsDE.cpp','Client/game_de/CPedRotationDE.cpp','Client/game_de/CPedStateDE.cpp','Client/game_de/CPedFlagsDE.cpp',
        'Client/game_de/CPedBehaviorDE.cpp','Client/game_de/CPedCombatDE.cpp','Client/game_de/CPedWeaponSelectionDE.cpp','Client/game_de/CPedPoseDE.cpp',
        'Client/game_de/CPedWeaponStatsDE.cpp','Client/game_de/CWeaponPolicyDE.cpp','Client/game_de/CWeaponStatManagerPolicyDE.cpp')
    $pedHeaders=@($productionSources|Where-Object{$_ -match '/CPed'}|ForEach-Object{[IO.Path]::ChangeExtension($_,'.h')})
    $includeRoots=@((Join-Path $taskRoot 'Shared/sdk'),(Join-Path $taskRoot 'Client/sdk'))
    $seen=[Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
    $pending=[Collections.Generic.Queue[string]]::new()
    foreach($name in ($productionSources+@('Tests/definitive/PedWeaponStatsDETests.cpp','Tests/definitive/test_ped_weapon_stats_de.ps1',
            'Client/game_sa/CPedSA.cpp','Client/game_sa/CPedSA.h'))){$pending.Enqueue((Join-Path $taskRoot $name))}
    while($pending.Count){
        $path=[IO.Path]::GetFullPath($pending.Dequeue())
        if(-not $seen.Add($path)){continue}
        if(-not(Test-Path -LiteralPath $path -PathType Leaf)){throw ('Missing focused source: '+$path)}
        if([IO.Path]::GetExtension($path) -eq '.ps1'){continue}
        foreach($match in [regex]::Matches([IO.File]::ReadAllText($path),'(?m)^\s*#\s*include\s*([<"])([^>"\r\n]+)[>"]')){
            $candidates=@()
            if($match.Groups[1].Value -eq '"'){$candidates+=Join-Path ([IO.Path]::GetDirectoryName($path)) $match.Groups[2].Value}
            $candidates+=@($includeRoots|ForEach-Object{Join-Path $_ $match.Groups[2].Value})
            foreach($candidate in $candidates){if(Test-Path -LiteralPath $candidate -PathType Leaf){$pending.Enqueue($candidate);break}}
        }
    }
    $record.sourceHashes=@($seen|Sort-Object|ForEach-Object{[ordered]@{path=[IO.Path]::GetRelativePath($taskRoot,$_).Replace('\','/');sha256=(Sha $_)}})
    $provided=@()
    foreach($name in $pedHeaders){
        $provided+=@([regex]::Matches([IO.File]::ReadAllText((Join-Path $taskRoot $name)),
            '\b(?<name>\w+)\s*\([^;{}]*\)\s*(?:const\s*)?(?:noexcept\s*)?override\s*(?:;|\{)')|ForEach-Object{$_.Groups['name'].Value})
    }
    foreach($name in @('GetCurrentWeaponStat','GetCurrentWeaponRange','GetCurrentWeaponSlot','SetPosition','Teleport')){
        if($provided -notcontains $name){throw ('Actual inherited SDK override missing: '+$name)}
    }
    $seenMethods=@{}
    $pedMethods=@(foreach($header in @('Client/sdk/game/CEntity.h','Client/sdk/game/CPhysical.h','Client/sdk/game/CPed.h')){
        InterfaceMethods $header|Where-Object{
            $key=[regex]::Replace($_.signature,'\s+','')
            if($provided -contains $_.name -or $_.name -eq 'GetWeapon' -or $seenMethods.ContainsKey($key)){$false}
            else{$seenMethods[$key]=$true;$true}
        }
    })
    $gameMethods=@(InterfaceMethods 'Client/sdk/game/CGame.h'|Where-Object{$_.name -notin @('GetPedContext','GetStats','GetWeaponStatManager')})
    $weaponMethods=@(InterfaceMethods 'Client/sdk/game/CWeapon.h'|Where-Object{$_.name -notin @('GetType','GetPed','GetSlot','SetAsCurrentWeapon')})
    $generated=Join-Path $scratchRoot 'PedWeaponStatsFixture.generated.h'
    $lines=@('#pragma once','#pragma warning(push)','#pragma warning(disable : 4100)','namespace fixture {',
        # Generated SDK parameter names include state. Keep this fixture's
        # bookkeeping distinct so strict MSVC warnings still cover that ABI.
        'class Game final : public CGame { Scenario& scenarioState; public: explicit Game(Scenario& scenarioValue) noexcept : scenarioState(scenarioValue) {}',
        'CPed* GetPedContext() override { scenarioState.Event(1); return scenarioState.context; }',
        'CStats* GetStats() override { const bool second=(scenarioState.statsGets++ % 2)!=0; scenarioState.Event(second?7u:5u); return second?scenarioState.valueStats:scenarioState.indexStats; }',
        'CWeaponStatManager* GetWeaponStatManager() override { scenarioState.Event(9); return scenarioState.manager; }')
    $lines+=@($gameMethods|ForEach-Object{$_.signature+' override { Unqualified(); }'})
    $lines+=@('};','class Weapon final : public mta::game_de::CWeaponPolicyDE { Scenario& scenarioState; public:',
        'explicit Weapon(Scenario& scenarioValue) noexcept : CWeaponPolicyDE(nullptr,WEAPONSLOT_TYPE_UNARMED),scenarioState(scenarioValue) {}',
        'eWeaponType GetType() const override { scenarioState.Event(4); return scenarioState.type; }')
    $lines+=@($weaponMethods|ForEach-Object{$_.signature+' override { Unqualified(); }'})
    $lines+=@('};','class Ped : public mta::game_de::CPedWeaponStatsDE { public: Scenario& scenario;',
        'Ped(CGame& game,mta::game_de::NativePedVitalsAccess& vitals,mta::game_de::NativePedMetadataAccess& metadata,mta::game_de::NativePedMutationAccess& mutation,mta::game_de::NativePedObservationAccess& observation,mta::game_de::NativePedRotationAccess& rotation,mta::game_de::NativePedOxygenAccess& oxygen,mta::game_de::NativePedDuckingAccess& ducking,mta::game_de::NativePedBikeFlagsAccess& bike,mta::game_de::NativePedShotFlagsAccess& shot,mta::game_de::NativePedBleedingAccess& bleeding,mta::game_de::NativePedStayAccess& stay,mta::game_de::NativePedFightingStyleAccess& combat,mta::game_de::NativePedCurrentWeaponSlotAccess& selection,mta::game_de::NativePedPlacementAccess& placement,const mta::game_de::NativePedReferenceToken& token,std::uint32_t wrapperType,Scenario& scenario) noexcept : CPedWeaponStatsDE(game,vitals,metadata,mutation,observation,rotation,oxygen,ducking,bike,shot,bleeding,stay,combat,selection,placement,token,wrapperType),scenario(scenario) {}',
        'CWeapon* GetWeapon(eWeaponSlot slot) const noexcept override { scenario.trace.push_back(3); scenario.observedSlot=slot; return scenario.weapon; }',
        'CWeapon* GetWeapon(eWeaponType) const override { Unqualified(); }',
        'mta::game_de::ClockResult RepairLastGoodPhysicsState(const CMatrix&) noexcept override { Unqualified(); }')
    $lines+=@($pedMethods|ForEach-Object{$_.signature+' override { Unqualified(); }'})
    $lines+=@('};','static_assert(!std::is_abstract_v<Game> && !std::is_abstract_v<Weapon> && !std::is_abstract_v<Ped>);','}','#pragma warning(pop)')
    $lines|Set-Content -LiteralPath $generated -Encoding utf8NoBOM
    $saved=Join-Path $outputRoot 'PedWeaponStatsFixture.generated.h'
    Copy-Item -LiteralPath $generated -Destination $saved
    $record.generatedFixture=[ordered]@{path=$saved;sha256=(Sha $generated);productionPedMethods=$provided;testOnlyReceivers=@('CGame','CWeapon','CPed');
        rejectingPedOverrides=$pedMethods.Count;rejectingGameOverrides=$gameMethods.Count;rejectingWeaponOverrides=$weaponMethods.Count}
    $env:TEMP=$scratchRoot;$env:TMP=$scratchRoot
    $vswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
    $discovery=RunOwned $vswhere @('-latest','-products','*','-requires','Microsoft.VisualStudio.Component.VC.Tools.x86.x64','-property','installationPath') $outputRoot 'vswhere' 10000
    $vs=[IO.File]::ReadAllText($discovery.stdout).Trim()
    if($discovery.exitCode -ne 0 -or $discovery.timedOut -or -not $vs){throw 'Visual Studio x64 tools required.'}
    $script=Join-Path $outputRoot 'capture-msvc.cmd'
    @('@echo off',('call "{0}" -no_logo -arch=x64 -host_arch=x64 >nul' -f (Join-Path $vs 'Common7/Tools/VsDevCmd.bat')),
        'if errorlevel 1 exit /b 1','set VCToolsInstallDir','set INCLUDE','set LIB','set PATH','exit /b 0')|Set-Content -LiteralPath $script -Encoding ascii
    $environment=RunOwned $env:ComSpec @('/d','/s','/c','capture-msvc.cmd') $outputRoot 'msvc-environment' 30000
    if($environment.exitCode -ne 0 -or $environment.timedOut){throw 'x64 compiler environment failed.'}
    foreach($line in [IO.File]::ReadAllLines($environment.stdout)){if($line -match '^(VCToolsInstallDir|INCLUDE|LIB|LIBPATH|PATH)=(.*)$'){[Environment]::SetEnvironmentVariable($Matches[1],$Matches[2],'Process')}}
    $compiler=Join-Path $env:VCToolsInstallDir 'bin/Hostx64/x64/cl.exe'
    $exe=Join-Path $outputRoot 'ped-weapon-stats-de-tests.exe'
    $arguments=@('/nologo','/std:c++17','/EHsc','/W4','/WX','/O2','/MD','/DNOMINMAX','/DWIN32_LEAN_AND_MEAN',
        ('/I'+$includeRoots[0]),('/I'+$includeRoots[1]),('/I'+$scratchRoot),(Join-Path $PSScriptRoot 'PedWeaponStatsDETests.cpp'))+
        @($productionSources|ForEach-Object{Join-Path $taskRoot $_})+@((('/Fo'+$scratchRoot+[IO.Path]::DirectorySeparatorChar)),('/Fe'+$exe),'/link','bcrypt.lib')
    $record.result=[ordered]@{architecture='x64';environment=$environment;compile=$null;test=$null;checks=0;pointerBits=$null;executable=$exe;executableSha256=$null}
    $record.result.compile=RunOwned $compiler $arguments $outputRoot 'compile' 90000
    if($record.result.compile.exitCode -ne 0 -or $record.result.compile.timedOut){throw 'Actual ped weapon-stat SDK fixture compilation failed.'}
    $record.result.executableSha256=Sha $exe
    $record.result.test=RunOwned $exe @() $outputRoot 'test' 10000
    $text=[IO.File]::ReadAllText($record.result.test.stdout)
    $summary=[regex]::Match($text,'(?:^|\r?\n)Passed (\d+) actual CPed weapon-stat orchestration SDK checks \((\d+)-bit\)')
    if($summary.Success){$record.result.checks=[int]$summary.Groups[1].Value;$record.result.pointerBits=[int]$summary.Groups[2].Value}
    $record.actualPedWeaponStatsSdkTested=$record.result.test.exitCode -eq 0 -and -not $record.result.test.timedOut -and
        $record.result.checks -gt 0 -and $record.result.pointerBits -eq 64 -and $text.Contains('Production actual MTA ped weapon-stat orchestration SDK fixture completed')
    $record.success=$record.actualPedWeaponStatsSdkTested
    if(-not $record.success){throw 'Actual ped stat fixture failed or executed marker absent.'}
} catch { AddFailure ($_.Exception.ToString()) }
finally {
    foreach($name in $previousEnvironment.Keys){[Environment]::SetEnvironmentVariable($name,$previousEnvironment[$name],'Process')}
    foreach($entry in $record.sourceHashes){try{if((Sha (Join-Path $taskRoot $entry.path)) -cne $entry.sha256){$record.sourcesUnchanged=$false}}catch{$record.sourcesUnchanged=$false;AddFailure($_.Exception.ToString())}}
    try {
        $resolved=[IO.Path]::GetFullPath($scratchRoot)
        $allowed=[IO.Path]::GetFullPath((Join-Path $taskRoot 'Build/tmp'))+[IO.Path]::DirectorySeparatorChar
        if(-not $resolved.StartsWith($allowed,[StringComparison]::OrdinalIgnoreCase)){throw 'Compiler scratch escaped workspace.'}
        foreach($entry in Get-ChildItem -LiteralPath $resolved -Force){if($entry.PSIsContainer -or ($entry.Attributes -band [IO.FileAttributes]::ReparsePoint)){throw 'Unexpected scratch child retained.'};Remove-Item -LiteralPath $entry.FullName -Force}
        [IO.Directory]::Delete($resolved,$false)
        $record.scratchRemoved=$true
    } catch { AddFailure ($_.Exception.ToString()) }
    $record.success=$record.success -and $record.sourcesUnchanged -and $record.scratchRemoved
    $record.finishedAt=[DateTimeOffset]::Now.ToString('o')
    $record|ConvertTo-Json -Depth 10|Set-Content -LiteralPath (Join-Path $outputRoot 'test-result.json') -Encoding utf8NoBOM
}
[ordered]@{success=$record.success;receipt=(Join-Path $outputRoot 'test-result.json');failure=$record.failure}|ConvertTo-Json -Compress
if(-not $record.success){throw ('Actual ped weapon-stat SDK checks failed; see '+$outputRoot)}
