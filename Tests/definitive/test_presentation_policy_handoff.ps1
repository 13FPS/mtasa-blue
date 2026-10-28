#!/usr/bin/env pwsh
#requires -Version 7.0
# Actual Core module consumer and SDK mixin, not a complete shipping Core build.
[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$GameDeDll,
    [Parameter(Mandatory)][ValidatePattern('^[0-9a-fA-F]{64}$')][string]$ExpectedGameDeSha256
)
$ErrorActionPreference='Stop'
Set-StrictMode -Version Latest
$taskRoot=(Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../..')).Path
$GameDeDll=(Resolve-Path -LiteralPath $GameDeDll).Path
if((Get-FileHash -LiteralPath $GameDeDll -Algorithm SHA256).Hash -ine $ExpectedGameDeSha256){throw 'Normal-built backend hash mismatch'}
$run=(Get-Date -Format 'yyyyMMdd-HHmmss')+'-'+[guid]::NewGuid().ToString('N')
$outputRoot=Join-Path $taskRoot ('Build/mta-presentation-policy-handoff/'+$run)
$scratch=Join-Path $taskRoot ('Build/tmp/presentation-policy-handoff-'+$run)
[void][IO.Directory]::CreateDirectory($outputRoot)
[void][IO.Directory]::CreateDirectory($scratch)
$sourceNames=@('Client/sdk/game/CGamePresentationPolicy.h','Client/sdk/game/CGameAdapter.h','Client/sdk/game/CGameClockService.h',
    'Client/sdk/game/CGame.h','Client/core/CGameAdapterModule.h','Client/core/CGameAdapterModule.cpp',
    'Client/core/DefinitiveFrontendStartup.h','Client/core/DefinitiveFrontendStartup.cpp','Client/core/Core.cpp',
    'Client/game_de/CGamePresentationPolicyDE.h','Client/game_de/NativePresentationPolicyAccess.h',
    'Client/game_de/GameAdapter.cpp','Client/game_de/NativePresentationPolicyOwner.h','Client/game_de/NativePresentationPolicyOwner.cpp',
    'Tests/definitive/PresentationPolicyHandoffTests.cpp','Tests/definitive/PresentationPolicyHandoffFixture.h',
    'Tests/definitive/PresentationPolicyHandoffFixture.cpp','Tests/definitive/PresentationPolicyHandoffFixture.full.def',
    'Tests/definitive/PresentationPolicyHandoffFixture.partial.def','Tests/definitive/test_presentation_policy_handoff.ps1')
$markers=@('actualCorePresentationExportGroupTested','actualCorePresentationConstructionBorrowTested','actualSdkPresentationMixinTested',
    'actualCorePresentationRetirementTested','actualPresentationBackendRefusalsTested','actualPresentationProcessHeapLifetimeTested')
$sourceHashes=@()
$sourceUnchanged=$false
$scratchRemoved=$false
$compilerExit=$null
$testExit=$null
$checks=$null
$failure=$null
$observed=@{}
foreach($marker in $markers){$observed[$marker]=$false}
$priorTemp=[Environment]::GetEnvironmentVariable('TEMP','Process')
$priorTmp=[Environment]::GetEnvironmentVariable('TMP','Process')
function Sha([string]$path){(Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLowerInvariant()}
function RunOwned([string]$Executable,[string[]]$Arguments,[string]$Name){
    $info=[Diagnostics.ProcessStartInfo]::new()
    $info.FileName=$Executable
    $info.WorkingDirectory=$outputRoot
    $info.UseShellExecute=$false
    $info.CreateNoWindow=$true
    $info.RedirectStandardOutput=$true
    $info.RedirectStandardError=$true
    $info.Environment['TEMP']=$scratch
    $info.Environment['TMP']=$scratch
    foreach($argument in $Arguments){[void]$info.ArgumentList.Add($argument)}
    $process=[Diagnostics.Process]::new()
    $process.StartInfo=$info
    try{
        if(-not $process.Start()){throw ('Failed to launch owned '+$Name)}
        $readOut=$process.StandardOutput.ReadToEndAsync()
        $readErr=$process.StandardError.ReadToEndAsync()
        if(-not $process.WaitForExit(30000)){
            $process.Kill($true)
            if(-not $process.WaitForExit(10000)){throw ('Owned '+$Name+' did not terminate')}
            throw ('Owned '+$Name+' exceeded deadline')
        }
        $out=$readOut.GetAwaiter().GetResult()
        $err=$readErr.GetAwaiter().GetResult()
        [IO.File]::WriteAllText((Join-Path $outputRoot ($Name+'.log')),$out,[Text.UTF8Encoding]::new($false))
        [IO.File]::WriteAllText((Join-Path $outputRoot ($Name+'-errors.log')),$err,[Text.UTF8Encoding]::new($false))
        [ordered]@{exit=$process.ExitCode;text=($out+"`n"+$err)}
    }finally{$process.Dispose()}
}
try{
    $sourceHashes=@($sourceNames|Sort-Object -Unique|ForEach-Object {@{path=$_;sha256=(Sha (Join-Path $taskRoot $_))}})
    $vswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
    $visualStudio=& $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    if(-not $visualStudio){throw 'Existing Visual Studio x64 tools required'}
    $developerCommand=Join-Path $visualStudio 'Common7/Tools/VsDevCmd.bat'
    $compilerEnvironment=& $env:ComSpec /d /s /c ('call "{0}" -no_logo -arch=x64 -host_arch=x64 >nul && set' -f $developerCommand)
    if($LASTEXITCODE -ne 0){throw 'Compiler environment initialization failed'}
    foreach($entry in $compilerEnvironment){if($entry -match '^([^=]+)=(.*)$'){[Environment]::SetEnvironmentVariable($Matches[1],$Matches[2],'Process')}}
    [Environment]::SetEnvironmentVariable('TEMP',$scratch,'Process')
    [Environment]::SetEnvironmentVariable('TMP',$scratch,'Process')
    $compiler=Join-Path $env:VCToolsInstallDir 'bin/Hostx64/x64/cl.exe'
    # Both shipping Release/x64 modules use /MT. Separate fixture CRTs must
    # not share malloc/free ownership; the actual SDK uses the process heap.
    $common=@('/nologo','/std:c++17','/EHsc','/O2','/MT','/W4','/WX','/DNOMINMAX','/DWIN32_LEAN_AND_MEAN',
        ('/I'+(Join-Path $taskRoot 'Shared/sdk')),('/I'+(Join-Path $taskRoot 'Client/sdk')),('/I'+$scratch),
        ('/Fo'+$scratch+[IO.Path]::DirectorySeparatorChar))
    # Unrelated pure CGame slots abort. The actual production mixin supplies
    # DisableVSync; calling through CGame dispatches its shipping method body.
    $sdk=[IO.File]::ReadAllText((Join-Path $taskRoot 'Client/sdk/game/CGame.h'))
    $sdk=[regex]::Replace($sdk,'(?s)/\*.*?\*/|//[^\r\n]*','')
    $methods=@()
    foreach($match in [regex]::Matches($sdk,'virtual\s+(?<signature>[^;{}]+);')){
        $signature=[regex]::Replace($match.Groups['signature'].Value,'\s*=\s*0\s*$','').Trim()
        $name=[regex]::Match($signature,'(?<name>\w+)\s*\(').Groups['name'].Value
        if(-not $name){throw ('Cannot derive rejecting CGame slot: '+$signature)}
        if($name -eq 'DisableVSync'){continue}
        $signature=[regex]::Replace($signature,'\s*=\s*[^,)]+','')
        $methods+=([regex]::Replace($signature,'\s+',' ')+' override { Unrelated(); }')
    }
    $generated=Join-Path $scratch 'PresentationPolicyGameFixture.generated.h'
    $lines=@('#pragma once','#pragma warning(push)','#pragma warning(disable : 4100)','namespace fixture {',
        'class PresentationPolicyGame final : public mta::game_de::CGamePresentationPolicyMixinDE<CGame> { public:',
        'explicit PresentationPolicyGame(const mta::game_de::NativePresentationPolicyBorrow& borrow) : CGamePresentationPolicyMixinDE(borrow) {}')+
        $methods+@('};','static_assert(!std::is_abstract_v<PresentationPolicyGame>);','}','#pragma warning(pop)')
    $lines|Set-Content -LiteralPath $generated -Encoding utf8NoBOM
    Copy-Item -LiteralPath $generated -Destination (Join-Path $outputRoot 'PresentationPolicyGameFixture.generated.h')
    $full=Join-Path $outputRoot 'presentation-fixture-full.dll'
    $partial=Join-Path $outputRoot 'presentation-fixture-partial.dll'
    $fixture=Join-Path $PSScriptRoot 'PresentationPolicyHandoffFixture.cpp'
    # Plain extern-C definitions match the SDK. Exact .def lists preserve
    # the partial group without an unresolved automatic End export.
    $fullExports=Join-Path $PSScriptRoot 'PresentationPolicyHandoffFixture.full.def'
    $partialExports=Join-Path $PSScriptRoot 'PresentationPolicyHandoffFixture.partial.def'
    $one=RunOwned $compiler ($common+@('/LD',$fixture,('/Fe'+$full),'/link','/INCREMENTAL:NO',('/DEF:'+$fullExports),('/IMPLIB:'+(Join-Path $scratch 'full.lib')))) 'compile-full'
    if($one.exit -ne 0){$compilerExit=$one.exit;throw 'Full controlled provider compile failed'}
    $two=RunOwned $compiler ($common+@('/LD','/DMTA_PRESENTATION_FIXTURE_PARTIAL',$fixture,('/Fe'+$partial),'/link','/INCREMENTAL:NO',('/DEF:'+$partialExports),('/IMPLIB:'+(Join-Path $scratch 'partial.lib')))) 'compile-partial'
    if($two.exit -ne 0){$compilerExit=$two.exit;throw 'Partial controlled provider compile failed'}
    $executable=Join-Path $outputRoot 'presentation_policy_handoff_tests.exe'
    $three=RunOwned $compiler ($common+@((Join-Path $PSScriptRoot 'PresentationPolicyHandoffTests.cpp'),
        (Join-Path $taskRoot 'Client/core/CGameAdapterModule.cpp'),('/Fe'+$executable),'/link','/INCREMENTAL:NO','user32.lib')) 'compile-host'
    $compilerExit=$three.exit
    if($compilerExit -ne 0){throw 'Real Core consumer/SDK fixture compile failed'}
    $tested=RunOwned $executable @($full,$partial,$GameDeDll) 'test'
    $testExit=$tested.exit
    if($tested.text -match 'Passed (\d+) focused actual Core presentation handoff checks'){$checks=[int]$Matches[1]}
    foreach($marker in $markers){$observed[$marker]=[regex]::IsMatch($tested.text,'(?m)^'+[regex]::Escape($marker)+'=1\s*$')}
    $sourceUnchanged=(Sha $GameDeDll) -ceq $ExpectedGameDeSha256.ToLowerInvariant()
    foreach($pin in $sourceHashes){if((Sha (Join-Path $taskRoot $pin.path)) -cne $pin.sha256){$sourceUnchanged=$false}}
}catch{$failure=$_.Exception.Message}
finally{
    [Environment]::SetEnvironmentVariable('TEMP',$priorTemp,'Process')
    [Environment]::SetEnvironmentVariable('TMP',$priorTmp,'Process')
    try{
        $parent=[IO.Path]::GetFullPath((Join-Path $taskRoot 'Build/tmp'))
        if([IO.Path]::GetDirectoryName([IO.Path]::GetFullPath($scratch)) -cne $parent){throw 'Scratch escaped task parent'}
        for($ancestor=$scratch;$ancestor;$ancestor=[IO.Path]::GetDirectoryName($ancestor)){
            if((Get-Item -LiteralPath $ancestor -Force).Attributes -band [IO.FileAttributes]::ReparsePoint){throw 'Linked scratch ancestor refused'}
        }
        $items=@(Get-ChildItem -LiteralPath $scratch -Force)
        foreach($item in $items){
            if($item.PSIsContainer -or ($item.Attributes -band [IO.FileAttributes]::ReparsePoint) -or
                [IO.Path]::GetDirectoryName([IO.Path]::GetFullPath($item.FullName)) -cne [IO.Path]::GetFullPath($scratch)){throw 'Unexpected scratch entry retained'}
        }
        foreach($item in $items){Remove-Item -LiteralPath $item.FullName -Force}
        [IO.Directory]::Delete($scratch,$false)
        $scratchRemoved=$true
    }catch{$failure=($failure+';scratch:'+ $_.Exception.Message)}
}
$success=($compilerExit -eq 0 -and $testExit -eq 0 -and $null -ne $checks -and $sourceUnchanged -and $scratchRemoved -and -not $failure -and
    @($markers|Where-Object {-not $observed[$_]}).Count -eq 0)
$result=[ordered]@{schema='mta_presentation_policy_handoff_tests_v1';success=$success;recordedAt=[DateTimeOffset]::Now.ToString('o');
    directory=$outputRoot;compilerExit=$compilerExit;testExit=$testExit;checks=$checks;failure=$failure;sourceHashes=$sourceHashes;
    sourceUnchanged=$sourceUnchanged;scratchRemoved=$scratchRemoved;normalBackend=$GameDeDll;normalBackendSha256=$ExpectedGameDeSha256.ToLowerInvariant();
    controlledProviderDllTested=$success;actualCoreModuleConsumerTested=$success;actualSdkMixinMethodTested=$success;
    fixtureRuntimeLibrary='MultiThreaded';dllIndependentProcessHeapStorageTested=$success;
    shippingCoreRebuilt=$false;shippingFrontendControllerRuntimeTested=$false;fullCoreRebuildAndFrontendHostStillRequired=$true;
    nativePositivePolicyTested=$false;gameLaunched=$false;gameProcessAttached=$false;gameImageRead=$false;fullGameFactoryReady=$false;fullMtaReady=$false}
foreach($marker in $markers){$result[$marker]=$observed[$marker]}
$result|ConvertTo-Json -Depth 8|Set-Content -LiteralPath (Join-Path $outputRoot 'test-result.json') -Encoding utf8NoBOM
if(-not $success){throw ('Presentation handoff tests failed; see '+$outputRoot)}
$result|ConvertTo-Json -Depth 8 -Compress
