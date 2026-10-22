#!/usr/bin/env pwsh
#requires -Version 7.0
# Exercise actual selected-ped CWeapon SDK forwarding; no engine code executes.
[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$taskRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../..')).Path
$id = (Get-Date -Format 'yyyyMMdd-HHmmss') + '-' + [guid]::NewGuid().ToString('N')
$outputRoot = Join-Path $taskRoot ('Build/mta-weapon-native-de/' + $id)
$scratchRoot = Join-Path $taskRoot ('Build/tmp/weapon-native-de-' + $id)
[IO.Directory]::CreateDirectory($outputRoot) | Out-Null
[IO.Directory]::CreateDirectory($scratchRoot) | Out-Null
$previousEnvironment = @{}
foreach ($name in @('TEMP','TMP','VCToolsInstallDir','INCLUDE','LIB','LIBPATH','PATH')) {
    $previousEnvironment[$name] = [Environment]::GetEnvironmentVariable($name,'Process')
}
$record = [ordered]@{
    schema='mta_weapon_native_sdk_tests_v1';directory=$outputRoot;success=$false;
    recordedAt=[DateTimeOffset]::Now.ToString('o');sourceHashes=@();sourcesUnchanged=$true;
    generatedFixture=$null;result=$null;failure=$null;scratchDirectory=$scratchRoot;scratchRemoved=$false;
    actualPedWeaponSdkSliceTested=$false;actualWeaponCpuMetadataSdkTested=$false;actualWeaponCurrentSlotDelegationSdkTested=$false;
    scope='Actual CWeapon borrowed-ped SetAsCurrentWeapon delegates once, preserves null no-op and propagates ped refusal with no duplicate native owner; fire-time/reload remain separate. Actual abstract CWeaponNativeDE nine-method SDK slice and inherited borrowed CPU metadata. Exact immutable token/slot, fullDWORD ammo, representable type/state enum getters, typed refusal outside unfixed enum language ranges, real-owner GetInfo pointer/null forwarding, all explicit native refusal statuses and accepted-effect/no-replay behavior. Other actual SDK methods reject in the generated fixture. No native field proof, complete weapon factory/lifecycle, firing, reload or standalone native weapon is claimed.';
    nativeEngineCodeExecuted=$false;gameLaunched=$false;gameAttached=$false;installationChanged=$false;
    completeWeaponReady=$false;completeCGameReady=$false;fullMtaReady=$false
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
    $productionSources = @('Client/game_de/CWeaponNativeDE.cpp','Client/game_de/CWeaponPolicyDE.cpp')
    $sourceNames = $productionSources + @('Client/game_de/CWeaponNativeDE.h','Client/game_de/CWeaponPolicyDE.h',
        'Client/game_de/NativeGameTimingAccess.h','Client/game_de/NativePedWeaponFieldsAccess.h','Client/game_de/NativePedWeaponAmmoAccess.h',
        'Client/game_de/NativePedReferenceToken.h','Client/game_de/NativePedPoolReader.h','Client/game_de/NativePedPoolProfile.h',
        'Client/sdk/game/CWeapon.h','Client/sdk/game/CWeaponInfo.h','Client/sdk/game/Common.h','Client/sdk/game/CGameClockService.h','Client/sdk/game/CGameAdapter.h',
        'Client/sdk/game/CPed.h','Client/sdk/game/CPhysical.h','Client/sdk/game/CEntity.h','Client/sdk/game/CPedIK.h','Client/sdk/game/CPedSound.h','Shared/sdk/SharedUtil.Defines.h','Shared/sdk/SharedUtil.IntTypes.h','Shared/sdk/CVector.h','Shared/sdk/enums/PedPieceTypes.h','Client/game_sa/CWeaponSA.h','Client/game_sa/CWeaponSA.cpp','Client/game_sa/CWeaponInfoSA.h',
        'Tests/definitive/WeaponNativeDETests.cpp','Tests/definitive/test_weapon_native_de.ps1')
    $record.sourceHashes = @($sourceNames | Sort-Object -Unique | ForEach-Object {
        [ordered]@{path=$_;sha256=(Sha (Join-Path $taskRoot $_))}
    })
    $provided = @('SetAsCurrentWeapon','GetPed','GetSlot','GetType','SetType','GetState','SetState','GetAmmoInClip','SetAmmoInClip','GetAmmoTotal','SetAmmoTotal','GetInfo')
    $declaration = [IO.File]::ReadAllText((Join-Path $taskRoot 'Client/game_de/CWeaponNativeDE.h'))
    foreach ($method in @('GetType','SetType','GetState','SetState','GetAmmoInClip','SetAmmoInClip','GetAmmoTotal','SetAmmoTotal','GetInfo')) {
        if ($declaration -notmatch ('\b'+$method+'\s*\([^;{}]*\)\s*(?:const\s*)?override\s*;')) {
            throw "Actual weapon SDK override missing: $method"
        }
    }
    $rejecting = @(InterfaceMethods 'Client/sdk/game/CWeapon.h' | Where-Object { $provided -notcontains $_.name })
    $generated = Join-Path $scratchRoot 'WeaponNativeFixture.generated.h'
    $lines = @('#pragma once','#pragma warning(push)','#pragma warning(disable : 4100)','namespace fixture {',
        'class WeaponFixture final : public mta::game_de::CWeaponNativeDE { public:',
        'WeaponFixture(mta::game_de::NativeGameTimingAccess& access,const mta::game_de::NativePedReferenceToken& token,CPed* owner,eWeaponSlot slot) noexcept : CWeaponNativeDE(access,token,owner,slot) {}')
    $lines += @($rejecting | ForEach-Object { $_.signature + ' override { Unqualified(); }' })
    $lines += @('};','class PedSlotFixture final : public CPed { public:',
        'unsigned slotCalls = 0; eWeaponSlot lastSlot = WEAPONSLOT_TYPE_UNARMED; bool refuse = false;',
        'void SetCurrentWeaponSlot(eWeaponSlot slot) override { ++slotCalls; lastSlot = slot; if (refuse) throw std::runtime_error("fixture ped slot refused"); }')
    $allPedMethods = @(foreach ($header in @('Client/sdk/game/CEntity.h','Client/sdk/game/CPhysical.h','Client/sdk/game/CPed.h')) {
        InterfaceMethods $header | Where-Object { $_.name -ne 'SetCurrentWeaponSlot' }
    })
    $seen = @{}
    $pedMethods = @($allPedMethods | Where-Object {
        $key = [regex]::Replace($_.signature,'\s+','')
        if ($seen.ContainsKey($key)) { $false } else { $seen[$key] = $true; $true }
    })
    $lines += @($pedMethods | ForEach-Object { $_.signature + ' override { Unqualified(); }' })
    $lines += @('};','}','#pragma warning(pop)')
    $lines | Set-Content -LiteralPath $generated -Encoding utf8NoBOM
    $durableGenerated = Join-Path $outputRoot 'WeaponNativeFixture.generated.h'
    Copy-Item -LiteralPath $generated -Destination $durableGenerated
    $record.generatedFixture = [ordered]@{path=$durableGenerated;sha256=(Sha $generated);rejectingOverrides=$rejecting.Count;
        productionMethodsNotOverridden=$provided;testedMethods=$provided}
    $env:TEMP = $scratchRoot
    $env:TMP = $scratchRoot
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
    $discovery = RunOwned $vswhere @('-latest','-products','*','-requires','Microsoft.VisualStudio.Component.VC.Tools.x86.x64','-property','installationPath') $outputRoot 'vswhere' 10000
    $visualStudio = [IO.File]::ReadAllText($discovery.stdout).Trim()
    if ($discovery.exitCode -ne 0 -or $discovery.timedOut -or -not $visualStudio) { throw 'Visual Studio x64 tools required.' }
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
    $executable = Join-Path $outputRoot 'weapon_native_de_tests.exe'
    $arguments = @('/nologo','/std:c++17','/EHsc','/W4','/WX','/O2','/MD','/DNOMINMAX','/DWIN32_LEAN_AND_MEAN',
        ('/I'+(Join-Path $taskRoot 'Shared/sdk')),('/I'+(Join-Path $taskRoot 'Client/sdk')),('/I'+$scratchRoot),
        (Join-Path $PSScriptRoot 'WeaponNativeDETests.cpp')) + @($productionSources | ForEach-Object { Join-Path $taskRoot $_ }) +
        @(('/Fo'+$scratchRoot+[IO.Path]::DirectorySeparatorChar),('/Fd'+(Join-Path $scratchRoot 'fixture.pdb')),('/Fe'+$executable))
    $record.result = [ordered]@{architecture='x64';environment=$environmentResult;compile=$null;test=$null;checks=0;
        pointerBits=$null;executable=$executable;executableSha256=$null}
    $record.result.compile = RunOwned $compiler $arguments $outputRoot 'compile' 60000
    if ($record.result.compile.exitCode -ne 0 -or $record.result.compile.timedOut) { throw 'Actual ped-owned CWeapon SDK compilation failed.' }
    $record.result.executableSha256 = Sha $executable
    $record.result.test = RunOwned $executable @() $outputRoot 'test' 10000
    $text = [IO.File]::ReadAllText($record.result.test.stdout).TrimEnd()
    $summary = [regex]::Match($text,'(?:^|\r?\n)Passed (\d+) actual ped-owned CWeapon SDK checks \((\d+)-bit\)$')
    if ($summary.Success) {
        $record.result.checks = [int]$summary.Groups[1].Value
        $record.result.pointerBits = [int]$summary.Groups[2].Value
    }
    $record.actualPedWeaponSdkSliceTested = $text.Contains('Production actual ped-owned CWeapon SDK slice fixture completed')
    $record.actualWeaponCpuMetadataSdkTested = $text.Contains('Production actual CWeapon CPU metadata SDK fixture completed')
    $record.actualWeaponCurrentSlotDelegationSdkTested = $text.Contains('Production actual CWeapon current-slot delegation SDK fixture completed')
    $record.success = $record.result.test.exitCode -eq 0 -and -not $record.result.test.timedOut -and
        $record.result.checks -gt 0 -and $record.result.pointerBits -eq 64 -and $record.actualPedWeaponSdkSliceTested -and $record.actualWeaponCpuMetadataSdkTested -and $record.actualWeaponCurrentSlotDelegationSdkTested
    if (-not $record.success) { throw 'Actual ped-owned CWeapon SDK checks or completion evidence failed.' }
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
if (-not $record.success) { throw "Actual ped-owned CWeapon SDK checks failed; see $outputRoot" }
