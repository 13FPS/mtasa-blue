#!/usr/bin/env pwsh
#requires -Version 7.0
# Compile actual CPed oxygen/ducking methods and CPU wrapper identity; native repair remains pure.
[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$taskRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../..')).Path
$id = (Get-Date -Format 'yyyyMMdd-HHmmss') + '-' + [guid]::NewGuid().ToString('N')
$outputRoot = Join-Path $taskRoot ('Build/mta-ped-oxygen-state-de/' + $id)
$scratchRoot = Join-Path $taskRoot ('Build/tmp/ped-oxygen-state-de-' + $id)
[IO.Directory]::CreateDirectory($outputRoot) | Out-Null
[IO.Directory]::CreateDirectory($scratchRoot) | Out-Null
$previousEnvironment = @{}
foreach ($name in @('TEMP','TMP','VCToolsInstallDir','INCLUDE','LIB','LIBPATH','PATH')) {
    $previousEnvironment[$name] = [Environment]::GetEnvironmentVariable($name,'Process')
}
$record = [ordered]@{
    schema='mta_ped_oxygen_state_de_tests_v1';directory=$outputRoot;success=$false;
    recordedAt=[DateTimeOffset]::Now.ToString('o');sourceHashes=@();sourcesUnchanged=$true;
    generatedFixture=$null;result=$null;failure=$null;scratchDirectory=$scratchRoot;scratchRemoved=$false;
    scope='Actual CPedStateDE oxygen getter/setter and named IsDucking query borrow the same immutable registered token. Preserves raw breath bits, true/false queries, all explicit owner refusal statuses, accepted-effect/no-retry forwarding, independent full DWORD CPU wrapper identity and protected signed custom-move bookkeeping (default0) under refused/absent native access. Seat remains pure because classic has no initialized default; no native custom-animation setter is claimed. Production SetDucking, target rotation, native repair and complete ped factory remain unavailable. No native engine code or process executes.';
    nativeEngineCodeExecuted=$false;gameLaunched=$false;gameAttached=$false;installationChanged=$false;
    completeCGameReady=$false;fullMtaReady=$false;completePedReady=$false;completePoolsReady=$false;nativeRepairQualified=$false;actualPedOxygenStateSdkTested=$false;actualPedWrapperTypeSdkTested=$false;actualPedCustomMoveBookkeepingSdkTested=$false
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
    $productionSources = @('Client/game_de/CPedVitalsDE.cpp','Client/game_de/CPedMetadataDE.cpp','Client/game_de/CPedMutationDE.cpp','Client/game_de/CPedPhysicsDE.cpp','Client/game_de/CPedRotationDE.cpp','Client/game_de/CPedStateDE.cpp')
    $sourceNames = $productionSources + @('Client/game_de/CPedVitalsDE.h','Client/game_de/CPedMetadataDE.h','Client/game_de/CPedMutationDE.h','Client/game_de/NativePedMutationAccess.h','Client/game_de/NativePedPhysicalScalarsAccess.h','Client/game_de/CPedPhysicsDE.h','Client/game_de/NativePedOxygenAccess.h','Client/game_de/NativePedDuckingAccess.h','Client/game_de/CPedStateDE.h','Client/game_de/NativePedRotationAccess.h','Client/game_de/CPedRotationDE.h','Client/game_de/NativePedObservationAccess.h','Client/game_de/NativePedTransformReader.h','Client/sdk/game/CEntityPhysicsState.h','Shared/sdk/CMatrix.h','Client/game_de/NativePedVelocityAccess.h','Client/game_de/NativePedMetadataAccess.h','Client/game_de/NativePedVitalsAccess.h',
        'Client/game_de/NativePedReferenceToken.h','Client/game_de/NativePedPoolReader.h','Client/game_de/NativePedPoolProfile.h',
        'Client/sdk/game/CPed.h','Client/sdk/game/CPhysical.h','Client/sdk/game/CEntity.h','Client/sdk/game/CPedIK.h',
        'Client/sdk/game/CWeaponInfo.h','Client/sdk/game/CPedSound.h','Client/sdk/game/Common.h','Client/sdk/game/CPoolSlotPolicy.h',
        'Client/sdk/game/CGameAdapter.h','Client/sdk/game/CGameClockService.h','Shared/sdk/enums/PedState.h','Shared/sdk/enums/PedMoveState.h',
        'Shared/sdk/enums/PedPieceTypes.h','Shared/sdk/enums/EntityType.h','Shared/sdk/SharedUtil.Defines.h','Shared/sdk/CVector.h',
        'Shared/sdk/CVector4D.h','Shared/sdk/SharedUtil.IntTypes.h','Client/game_sa/CPedSA.h','Client/game_sa/CPedSA.cpp','Client/game_sa/CPlayerPedSA.cpp','Client/game_sa/CPoolsSA.cpp','Client/game_sa/CEntitySA.h','Client/game_sa/CEntitySA.cpp','Client/mods/deathmatch/logic/CClientPed.cpp','Tests/definitive/PedStateDETests.cpp')
    # Pin this actual runner so renaming a copied fixture cannot leave a
    # nonexistent source prerequisite before compilation starts.
    $sourceNames += [IO.Path]::GetRelativePath($taskRoot,$PSCommandPath).Replace('\','/')
    $record.sourceHashes = @($sourceNames | Sort-Object -Unique | ForEach-Object {
        [ordered]@{path=$_;sha256=(Sha (Join-Path $taskRoot $_))}
    })
    $provided = @('GetHealth','SetHealth','GetArmor','SetArmor','GetModelIndex','GetEntityType','SetMoveSpeed','SetTurnSpeed','GetStoredPointer','SetStoredPointer','GetPosition','GetMatrix','GetMoveSpeed','GetTurnSpeed','GetMass','SetMass','GetTurnMass','SetTurnMass','GetAirResistance','SetAirResistance','GetElasticity','SetElasticity','GetCenterOfMass','SetCenterOfMass','GetCurrentRotation','SetCurrentRotation','GetOxygenLevel','SetOxygenLevel','IsDucking','GetType','GetCustomMoveAnim')
    $declaration = [IO.File]::ReadAllText((Join-Path $taskRoot 'Client/game_de/CPedVitalsDE.h')) + [IO.File]::ReadAllText((Join-Path $taskRoot 'Client/game_de/CPedMetadataDE.h')) + [IO.File]::ReadAllText((Join-Path $taskRoot 'Client/game_de/CPedMutationDE.h')) + [IO.File]::ReadAllText((Join-Path $taskRoot 'Client/game_de/CPedPhysicsDE.h')) + [IO.File]::ReadAllText((Join-Path $taskRoot 'Client/game_de/CPedRotationDE.h')) + [IO.File]::ReadAllText((Join-Path $taskRoot 'Client/game_de/CPedStateDE.h'))
    foreach ($method in $provided) {
        if ($declaration -notmatch ('\b'+$method+'\s*\([^;{}]*\)\s*(?:const\s*)?(?:noexcept\s*)?override\s*;')) {
            throw "Production CPed SDK override is missing: $method"
        }
    }
    $allMethods = @(foreach ($header in @('Client/sdk/game/CEntity.h','Client/sdk/game/CPhysical.h','Client/sdk/game/CPed.h')) {
        InterfaceMethods $header | Where-Object { $provided -notcontains $_.name }
    })
    # CPed repeats some inherited pure declarations. Emit one matching override
    # per signature while retaining every overload and const/noexcept qualifier.
    $seen = @{}
    $pedMethods = @($allMethods | Where-Object {
        $key = [regex]::Replace($_.signature,'\s+','')
        if ($seen.ContainsKey($key)) { $false } else { $seen[$key] = $true; $true }
    })
    $generated = Join-Path $scratchRoot 'PedStateFixture.generated.h'
    $lines = @('#pragma once','#pragma warning(push)','#pragma warning(disable : 4100)','namespace fixture {',
        ('inline constexpr unsigned RejectingMethods = {0};' -f $pedMethods.Count),
        'class PedStateFixture final : public mta::game_de::CPedStateDE { public:',
        'PedStateFixture(mta::game_de::NativePedVitalsAccess& vitals,mta::game_de::NativePedMetadataAccess& metadata,mta::game_de::NativePedMutationAccess& mutation,mta::game_de::NativePedObservationAccess& observation,mta::game_de::NativePedRotationAccess& rotation,mta::game_de::NativePedOxygenAccess& oxygen,mta::game_de::NativePedDuckingAccess& ducking,const mta::game_de::NativePedReferenceToken& token,std::uint32_t wrapperType) noexcept : CPedStateDE(vitals,metadata,mutation,observation,rotation,oxygen,ducking,token,wrapperType) {}',
        'void RecordCustomMoveAnimForFixture(int value) noexcept { m_iCustomMoveAnim = value; }',
        'mta::game_de::ClockResult RepairLastGoodPhysicsState(const CMatrix&) noexcept override { Unqualified(); }')
    $lines += @($pedMethods | ForEach-Object { $_.signature + ' override { Unqualified(); }' })
    $lines += @('};','}','#pragma warning(pop)')
    $lines | Set-Content -LiteralPath $generated -Encoding utf8NoBOM
    $durableGenerated = Join-Path $outputRoot 'PedStateFixture.generated.h'
    Copy-Item -LiteralPath $generated -Destination $durableGenerated
    $record.generatedFixture = [ordered]@{path=$durableGenerated;sha256=(Sha $generated);rejectingOverrides=$pedMethods.Count;
        interfaces=@('CPed','CPhysical','CEntity');productionMethodsNotOverridden=$provided;testedMethods=@('GetOxygenLevel','SetOxygenLevel','IsDucking','GetType','GetCustomMoveAnim')}
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
    $executable = Join-Path $outputRoot 'ped_oxygen_state_de_tests.exe'
    $arguments = @('/nologo','/std:c++17','/EHsc','/W4','/WX','/O2','/MD','/DNOMINMAX','/DWIN32_LEAN_AND_MEAN',
        ('/I'+(Join-Path $taskRoot 'Shared/sdk')),('/I'+(Join-Path $taskRoot 'Client/sdk')),('/I'+$scratchRoot),
        (Join-Path $PSScriptRoot 'PedStateDETests.cpp')) + @($productionSources | ForEach-Object { Join-Path $taskRoot $_ }) +
        @(('/Fo'+$outputRoot+[IO.Path]::DirectorySeparatorChar),('/Fe'+$executable),'/link','bcrypt.lib')
    $record.result = [ordered]@{architecture='x64';environment=$environmentResult;compile=$null;test=$null;checks=0;
        pointerBits=$null;executable=$executable;executableSha256=$null}
    $record.result.compile = RunOwned $compiler $arguments $outputRoot 'compile' 60000
    if ($record.result.compile.exitCode -ne 0 -or $record.result.compile.timedOut) { throw 'Actual abstract CPed oxygen/state SDK fixture compilation failed.' }
    $record.result.executableSha256 = Sha $executable
    $record.result.test = RunOwned $executable @() $outputRoot 'test' 10000
    $summary = [regex]::Match([IO.File]::ReadAllText($record.result.test.stdout).TrimEnd(),
        '(?:^|\r?\n)Passed (\d+) actual CPed oxygen/state SDK checks \((\d+)-bit\)$')
    if ($summary.Success) {
        $record.result.checks = [int]$summary.Groups[1].Value
        $record.result.pointerBits = [int]$summary.Groups[2].Value
    }
    $record.success = $record.result.test.exitCode -eq 0 -and -not $record.result.test.timedOut -and
        $record.result.checks -gt 0 -and $record.result.pointerBits -eq 64
    $record.actualPedOxygenStateSdkTested = $record.success -and [IO.File]::ReadAllText($record.result.test.stdout).Contains('Production MTA oxygen and ducking ped SDK fixture completed')
    $record.actualPedWrapperTypeSdkTested = $record.success -and [IO.File]::ReadAllText($record.result.test.stdout).Contains('Production MTA ped wrapper type SDK fixture completed')
    $record.actualPedCustomMoveBookkeepingSdkTested = $record.success -and [IO.File]::ReadAllText($record.result.test.stdout).Contains('Production MTA ped custom-move bookkeeping SDK fixture completed')
    $record.success = $record.success -and $record.actualPedOxygenStateSdkTested -and $record.actualPedWrapperTypeSdkTested -and $record.actualPedCustomMoveBookkeepingSdkTested
    if (-not $record.success) { throw 'Actual abstract CPed oxygen/state SDK checks failed or summary evidence was missing.' }
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
if (-not $record.success) { throw "Actual abstract CPed oxygen/state SDK checks failed; see $outputRoot" }
