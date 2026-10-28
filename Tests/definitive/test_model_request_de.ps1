#!/usr/bin/env pwsh
#requires -Version 7.0
# Compile actual model request/ref SDK orchestration; no historical aggregate suite replay.
[CmdletBinding()]
param(
    [ValidateSet('Request','Validity','Dependencies','MakePedModel')]
    [string]$Scope = 'Request'
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$taskRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../..')).Path
$fixtureSource = if ($Scope -eq 'Validity') { 'ModelValidityDETests.cpp' } elseif ($Scope -eq 'Dependencies') { 'ModelDependencyDETests.cpp' } elseif ($Scope -eq 'MakePedModel') { 'ModelMakePedDETests.cpp' } else { 'ModelRequestDETests.cpp' }
$id = (Get-Date -Format 'yyyyMMdd-HHmmss') + '-' + [guid]::NewGuid().ToString('N')
$outputRoot = Join-Path $taskRoot ('Build/mta-model-request-de/' + $id)
$scratchRoot = Join-Path $taskRoot ('Build/tmp/model-request-de-' + $id)
[IO.Directory]::CreateDirectory($outputRoot) | Out-Null
[IO.Directory]::CreateDirectory($scratchRoot) | Out-Null
$previousEnvironment = @{}
foreach ($name in @('TEMP','TMP','VCToolsInstallDir','INCLUDE','LIB','LIBPATH','PATH')) {
    $previousEnvironment[$name] = [Environment]::GetEnvironmentVariable($name,'Process')
}
$record = [ordered]@{
    schema='mta_model_request_de_tests_v1';directory=$outputRoot;success=$false;
    recordedAt=[DateTimeOffset]::Now.ToString('o');sourceHashes=@();sourcesUnchanged=$true;
    generatedFixture=$null;result=$null;failure=$null;scratchDirectory=$scratchRoot;scratchRemoved=$false;
    scope='Actual abstract CModelInfoRequestDE five-method SDK dispatch through genuine CGame/CStreaming interfaces and shared CModelRequestPolicy; async/blocking requests, delayed/exhausted retries, paired-upgrade route, mandatory render/native-reference hooks, pending acknowledgement/cancellation, separate wrapper CPUcounts, CJ exception and exact failure prefixes. No historical policy/classification/streaming aggregate suite executes; no complete native model factory/reference implementation is claimed.';
    nativeEngineCodeExecuted=$false;gameLaunched=$false;gameAttached=$false;installationChanged=$false;
    completeModelFactoryReady=$false;nativeReferenceEffectsProvided=$false;renderDataGateImplemented=$false;
    actualModelRequestSdkTested=$false
}

$record['selectedScope']=$Scope
$record['actualModelValiditySdkTested']=$false
$record['actualModelDependencySdkTested']=$false
$record['actualModelMakePedSdkTested']=$false
$record['actualModelPedCreationOwnerDispatchSdkTested']=$false
$record['actualConcreteStartupModelPreparationTested']=$false
$record['actualSharedPedCreationBoundariesTested']=$false
if ($Scope -eq 'Dependencies') {
    $record.scope='Actual Request dependency order and protected RequestModelDependencies/GetAnimFileIndex through current classification and required game/model/streaming interfaces. Focused vehicle animation wrap/sentinel/base queries, skin7 source lookups, diagnostic defaults/refusals and actual nested asynchronous requests; no request retry/reference or historical matrix replay.'
}
if ($Scope -eq 'Validity') {
    $record.scope='Actual IsValid fixed-TXD occupancy, dynamic file bounds and current opaque-interface policy through existing CGame/CPools/CTxdPool virtuals. One focused CPU fixture; no request/ref/loaded/classification matrix or complete native pool/factory.'
}

if ($Scope -eq 'MakePedModel') {
    $record.scope='Actual MakePedModel creation/publication boundary followed by the existing real streaming special request, source pointer/full model key/channel zero and irreversible accepted prefix on failure. Concrete actual shared44 startup assembly, late service/full key boundaries and x86/x64 only this focused SDK fixture; no old request/validity/dependency matrix, native creation implementation or complete model factory claim.'
    $record['results']=@()
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
    $productionSources = @('Client/game_de/CModelInfoRequestDE.cpp','Client/game_de/CModelInfoDE.cpp')
    if ($Scope -ne 'Validity') { $productionSources += 'Client/game_de/CStreamingDE.cpp' }
    $sourceNames = $productionSources + @(
        'Client/sdk/game/CModelPedCreationPolicy.h','Client/sdk/game/CGameModelPreparationPolicy.h','Client/game_de/CGameModelPreparationDE.h',
        'Client/game_de/CModelInfoRequestDE.h','Client/game_de/CModelInfoDE.h','Client/game_de/CStreamingDE.h',
        'Client/sdk/game/CModelInfo.h','Client/sdk/game/CGame.h','Client/sdk/game/CStreaming.h','Client/sdk/game/CModelRequestPolicy.h',
        'Client/sdk/game/CAnimBlock.h','Client/sdk/game/Common.h','Client/sdk/game/CWeaponInfo.h',
        'Shared/sdk/enums/VehicleDummies.h','Shared/sdk/enums/ResizableVehicleWheelGroup.h',
        'Client/game_de/NativeModelClassificationAccess.h','Client/game_de/NativeModelClassificationCalls.h','Client/game_de/NativeModelClassificationProfile.h',
        'Client/game_de/NativeModelBankReader.h','Client/game_de/NativeModelBankProfile.h',
        'Client/game_de/NativeStreamingModelAccess.h','Client/game_de/NativeModelLoadedReader.h','Client/game_de/NativeModelLoadedProfile.h',
        'Client/game_de/NativeStreamingModelGroupsReader.h','Client/game_de/NativeStreamingModelGroupsProfile.h',
        'Client/game_de/NativePedPoolReader.h','Client/game_de/NativePedPoolProfile.h',
        'Client/sdk/game/CGameClockService.h','Client/sdk/game/CGameAdapter.h','Shared/sdk/CVector.h',
        'Shared/sdk/SharedUtil.Defines.h','Shared/sdk/SharedUtil.IntTypes.h',
        ('Tests/definitive/'+$fixtureSource),'Tests/definitive/test_model_request_de.ps1')
    $sourceNames += @('Client/sdk/game/CPools.h','Client/sdk/game/CTxdPool.h','Client/sdk/game/CPoolClientEntity.h',
        'Client/sdk/game/CBuildingsPool.h','Client/sdk/game/CDummyPool.h','Client/sdk/game/CPtrNodeSingleLinkPool.h')
    if ($Scope -eq 'MakePedModel') {
        $sourceNames += @(
            'Build/definitive-recovery/mta-model-make-ped-sdk-20261026/model-make-ped-sdk-source-ready.json',
            'Build/definitive-recovery/mta-model-make-ped-sdk-tests-20261026/model-make-ped-sdk-tests-source-ready.json')
    }
    if ($Scope -eq 'MakePedModel') {
        $sourceNames += @(
            'Build/definitive-recovery/mta-ped-model-creation-sdk-20261026/ped-model-creation-access-source-ready.json',
            'Build/definitive-recovery/mta-ped-model-creation-sdk-20261026/ped-model-creation-sdk-source-ready.json',
            'Build/definitive-recovery/mta-ped-model-creation-sdk-tests-20261026/ped-model-creation-sdk-tests-source-ready.json')
    }
    $record.sourceHashes = @($sourceNames | Sort-Object -Unique | ForEach-Object {
        [ordered]@{path=$_;sha256=(Sha (Join-Path $taskRoot $_))}
    })
    $modelProvided = @('GetModel','GetModelType','IsVehicle','IsAllocatedInArchive','IsBoat','IsCar','IsPlane','IsHeli',
        'Request','IsLoaded','ModelAddRef','RemoveRef','GetRefCount')
    $streamingProvided = @('RequestModel','RemoveModel','HasModelLoaded','LoadAllRequestedModels','LoadScene',
        'GetNumPedsLoaded','IsModelInLoadedPedGroup','GetNumLoadedVehicles','IsModelInLoadedVehicleGroup')
    if ($Scope -in @('Validity','Dependencies','MakePedModel')) {
        # Inherit the actual methods present in the coherent production headers;
        # do not hide IsValid/GetInterface or a separately added towing policy.
        $modelProvided=@()
        foreach($header in @('Client/game_de/CModelInfoDE.h','Client/game_de/CModelInfoRequestDE.h')) {
            $providedText=[regex]::Replace([IO.File]::ReadAllText((Join-Path $taskRoot $header)),'(?s)/\*.*?\*/|//[^\r\n]*','')
            $modelProvided+=@([regex]::Matches($providedText,'\b(?<name>\w+)\s*\([^;{}]*\)\s*(?:const\s*)?(?:noexcept\s*)?override\b') | ForEach-Object {$_.Groups['name'].Value})
        }
        $modelProvided=@($modelProvided|Sort-Object -Unique)
        if('IsValid' -notin $modelProvided -or 'GetInterface' -notin $modelProvided){throw 'Actual validity/interface production methods missing'}
        if ($Scope -eq 'Validity') {
            $gameMethods=@(InterfaceMethods 'Client/sdk/game/CGame.h'|Where-Object{$_.name -notin @('GetPools','GetBaseIDforTXD','GetCountOfAllFileIDs')})
            $poolMethods=@(InterfaceMethods 'Client/sdk/game/CPools.h'|Where-Object{$_.name -ne 'GetTxdPool'})
            $txdMethods=@(InterfaceMethods 'Client/sdk/game/CTxdPool.h'|Where-Object{$_.name -ne 'IsFreeTextureDictonarySlot'})
        } elseif ($Scope -eq 'Dependencies') {
            $gameMethods=@(InterfaceMethods 'Client/sdk/game/CGame.h'|Where-Object{$_.name -notin @('GetStreaming','GetBaseIDforTXD','GetBaseIDforIFP','GetModelInfo')})
            $dependencyMethods=@(InterfaceMethods 'Client/sdk/game/CModelInfo.h'|Where-Object{$_.name -notin @('Request','IsLoaded')})
            # Preserve all current real streaming overrides in this focused scope.
            $streamingText=[regex]::Replace([IO.File]::ReadAllText((Join-Path $taskRoot 'Client/game_de/CStreamingDE.h')),'(?s)/\*.*?\*/|//[^\r\n]*','')
            $streamingProvided=@([regex]::Matches($streamingText,'\b(?<name>\w+)\s*\([^;{}]*\)\s*(?:const\s*)?(?:noexcept\s*)?override\b')|ForEach-Object{$_.Groups['name'].Value})
        } else {
            $gameMethods=@(InterfaceMethods 'Client/sdk/game/CGame.h'|Where-Object{$_.name -ne 'GetStreaming'})
            # Inherit real RequestSpecialModel; an aborting override here would
            # mask the actual source effect boundary under test.
            $streamingText=[regex]::Replace([IO.File]::ReadAllText((Join-Path $taskRoot 'Client/game_de/CStreamingDE.h')),'(?s)/\*.*?\*/|//[^\r\n]*','')
            $streamingProvided=@([regex]::Matches($streamingText,'\b(?<name>\w+)\s*\([^;{}]*\)\s*(?:const\s*)?(?:noexcept\s*)?override\b')|ForEach-Object{$_.Groups['name'].Value})
            if('MakePedModel' -notin $modelProvided -or 'RequestSpecialModel' -notin $streamingProvided){throw 'Actual MakePedModel/special-model streaming overrides missing'}
        }
    } else {
        $gameMethods = @(InterfaceMethods 'Client/sdk/game/CGame.h' | Where-Object { $_.name -notin @('GetStreaming','IsASyncLoadingEnabled') })
    }
    $modelMethods = @(InterfaceMethods 'Client/sdk/game/CModelInfo.h' | Where-Object { $modelProvided -notcontains $_.name })
    if ($Scope -eq 'Validity') {
        $streamingMethods=@()
    } else {
        $streamingMethods=@(InterfaceMethods 'Client/sdk/game/CStreaming.h' | Where-Object { $streamingProvided -notcontains $_.name })
    }
    $testedMethods = @(if ($Scope -eq 'Validity') { 'IsValid' } elseif ($Scope -eq 'Dependencies') { 'Request','IsLoaded' } elseif ($Scope -eq 'MakePedModel') { 'MakePedModel' } else { 'Request','IsLoaded','ModelAddRef','RemoveRef','GetRefCount' })
    $headerText = [IO.File]::ReadAllText((Join-Path $taskRoot 'Client/game_de/CModelInfoRequestDE.h'))
    foreach ($method in $testedMethods) {
        if ($headerText -notmatch ('\b'+[regex]::Escape($method)+'\s*\([^;{}]*\)\s*override\s*;')) { throw "Actual model SDK override is missing: $method" }
    }
    if ($Scope -eq 'Dependencies') {
        if ($headerText -notmatch 'virtual\s+void\s+RequestModelDependencies\s*\([^;{}]*\)\s*;' -or
            $headerText -match 'RequestModelDependencies\s*\([^;{}]*\)\s*=\s*0') { throw 'Actual nonpure dependency policy is missing' }
        $modelHeader=[IO.File]::ReadAllText((Join-Path $taskRoot 'Client/game_de/CModelInfoDE.h'))
        if ($modelHeader -notmatch 'std::uint32_t\s+GetAnimFileIndex\s*\(\s*\)\s*;') { throw 'Actual protected animation getter is missing' }
    }
    if ($Scope -eq 'MakePedModel') {
        if ($headerText -notmatch 'virtual\s+void\s+CreateAndPublishNativePedModel\s*\(\s*std::uint32_t\s+[^)]*\)\s*;' -or
            $headerText -match 'CreateAndPublishNativePedModel\s*\([^;{}]*\)\s*=\s*0') { throw 'Actual required creation/publication default-refusal boundary missing' }
    }
    $generated = Join-Path $scratchRoot 'ModelRequestFixture.generated.h'
    $lines = @('#pragma once','#pragma warning(push)','#pragma warning(disable : 4100)','namespace fixture {',
        'class GameFixtureBase : public CGame { public:')
    $lines += @($gameMethods | ForEach-Object { $_.signature + ' override { Unqualified(); }' })
    $lines += @('};','class ModelFixtureBase : public mta::game_de::CModelInfoRequestDE { public:',
        'ModelFixtureBase(std::uint32_t model, mta::game_de::NativeModelClassificationAccess& classification, CGame& game) noexcept : CModelInfoRequestDE(model,classification,game) {}')
    $lines += @($modelMethods | ForEach-Object { $_.signature + ' override { Unqualified(); }' })
    if ($Scope -eq 'Validity') {
        $lines += @('};','class PoolsFixtureBase : public CPools { public:')
        $lines += @($poolMethods|ForEach-Object{$_.signature+' override { Unqualified(); }'})
        $lines += @('};','class TxdFixtureBase : public CTxdPool { public:')
        $lines += @($txdMethods|ForEach-Object{$_.signature+' override { Unqualified(); }'})
        $lines += @('};','}','#pragma warning(pop)')
    } else {
        $lines += @('};','class StreamingFixtureBase : public mta::game_de::CStreamingDE { public:',
            'explicit StreamingFixtureBase(mta::game_de::NativeStreamingModelAccess& owner) noexcept : CStreamingDE(owner) {}')
        $lines += @($streamingMethods | ForEach-Object { $_.signature + ' override { Unqualified(); }' })
        if ($Scope -eq 'Dependencies') {
            $lines += @('};','class DependencyFixtureBase : public CModelInfo { public:')
            $lines += @($dependencyMethods|ForEach-Object{$_.signature+' override { Unqualified(); }'})
        }
        $lines += @('};','}','#pragma warning(pop)')
    }
    $lines | Set-Content -LiteralPath $generated -Encoding utf8NoBOM
    $durableGenerated = Join-Path $outputRoot 'ModelRequestFixture.generated.h'
    Copy-Item -LiteralPath $generated -Destination $durableGenerated
    $record.generatedFixture = [ordered]@{path=$durableGenerated;sha256=(Sha $generated);gameRejectingOverrides=$gameMethods.Count;
        modelRejectingOverrides=$modelMethods.Count;streamingRejectingOverrides=$streamingMethods.Count;
        productionMethodsNotOverridden=$modelProvided;testedMethods=$testedMethods;requiredNativeHooksRemainPure=$true}
    if ($Scope -eq 'Validity') {
        $record.generatedFixture['poolsRejectingOverrides']=$poolMethods.Count
        $record.generatedFixture['txdRejectingOverrides']=$txdMethods.Count
        $record.generatedFixture['inheritedPoolsMethods']=@('GetTxdPool')
        $record.generatedFixture['inheritedTxdMethods']=@('IsFreeTextureDictonarySlot')
        $record.generatedFixture['nativePoolOrReferenceFactoryProvided']=$false
    }

    if ($Scope -eq 'Dependencies') {
        $record.generatedFixture['dependencyRejectingOverrides']=$dependencyMethods.Count
        $record.generatedFixture['selectedProtectedOperations']=@('RequestModelDependencies','GetAnimFileIndex')
        $record.generatedFixture['actualDependencyPolicyInherited']=$true
        $record.generatedFixture['actualProtectedAnimationGetterInherited']=$true
        $record.generatedFixture['defaultMissing505ReporterRefusalCovered']=$false
        $record.generatedFixture['optionalAnimationOwnerDefaultRefusalCovered']=$false
    }

    if ($Scope -eq 'MakePedModel') {
        $record.generatedFixture['actualMakePedModelInherited']=$true
        $record.generatedFixture['actualSpecialModelRequestInherited']=$true
        $record.generatedFixture['selectedProtectedOperations']=@('CreateAndPublishNativePedModel')
        $record.generatedFixture['nativePedCreatorImplemented']=$false
        $record.generatedFixture['defaultCreatorRefusalCovered']=$false
    }

    $env:TEMP = $scratchRoot
    $env:TMP = $scratchRoot
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
    $discovery = RunOwned $vswhere @('-latest','-products','*','-requires','Microsoft.VisualStudio.Component.VC.Tools.x86.x64','-property','installationPath') $outputRoot 'vswhere' 10000
    $visualStudio = [IO.File]::ReadAllText($discovery.stdout).Trim()
    if ($discovery.exitCode -ne 0 -or $discovery.timedOut -or -not $visualStudio) { throw 'Visual Studio x64 tools are required.' }
    $developerCommand = Join-Path $visualStudio 'Common7/Tools/VsDevCmd.bat'
    if ($Scope -eq 'MakePedModel') {
        foreach($architecture in @('x86','x64')) {
            $directory=Join-Path $outputRoot $architecture
            [IO.Directory]::CreateDirectory($directory)|Out-Null
            $environmentScript=Join-Path $directory 'capture-msvc.cmd'
            @('@echo off',('call "{0}" -no_logo -arch={1} -host_arch=x64 >nul' -f $developerCommand,$architecture),
                'if errorlevel 1 exit /b 1','set VCToolsInstallDir','set INCLUDE','set LIB','set PATH','exit /b 0')|
                Set-Content -LiteralPath $environmentScript -Encoding ascii
            $environmentResult=RunOwned $env:ComSpec @('/d','/s','/c','capture-msvc.cmd') $directory 'msvc-environment' 30000
            if($environmentResult.exitCode -ne 0 -or $environmentResult.timedOut){throw "$architecture MSVC environment initialization failed."}
            foreach($entry in [IO.File]::ReadAllLines($environmentResult.stdout)) {
                if($entry -match '^(VCToolsInstallDir|INCLUDE|LIB|LIBPATH|PATH)=(.*)$'){
                    [Environment]::SetEnvironmentVariable($Matches[1],$Matches[2],'Process')
                }
            }
            $compiler=Join-Path $env:VCToolsInstallDir "bin/Hostx64/$architecture/cl.exe"
            $executable=Join-Path $directory 'model_make_ped_de_tests.exe'
            $arguments=@('/nologo','/std:c++17','/EHsc','/W4','/WX','/O2','/MD','/DNOMINMAX','/DWIN32_LEAN_AND_MEAN',
                ('/I'+(Join-Path $taskRoot 'Shared/sdk')),('/I'+(Join-Path $taskRoot 'Client/sdk')),('/I'+$scratchRoot),
                (Join-Path $PSScriptRoot $fixtureSource))+@($productionSources|ForEach-Object{Join-Path $taskRoot $_})+
                @(('/Fo'+$scratchRoot+[IO.Path]::DirectorySeparatorChar),('/Fd'+(Join-Path $scratchRoot 'fixture.pdb')),('/Fe'+$executable),'/link','bcrypt.lib')
            $result=[ordered]@{architecture=$architecture;environment=$environmentResult;compile=$null;test=$null;checks=0;
                pointerBits=$null;executable=$executable;executableSha256=$null;actualModelMakePedSdkTested=$false;actualModelPedCreationOwnerDispatchSdkTested=$false;actualConcreteStartupModelPreparationTested=$false;actualSharedPedCreationBoundariesTested=$false}
            $record.results+=$result
            $result.compile=RunOwned $compiler $arguments $directory 'compile' 60000
            if($result.compile.exitCode -ne 0 -or $result.compile.timedOut){throw "$architecture actual MakePedModel SDK compilation failed."}
            $result.executableSha256=Sha $executable
            $result.test=RunOwned $executable @() $directory 'test' 10000
            $stdout=[IO.File]::ReadAllText($result.test.stdout)
            $summary=[regex]::Match($stdout.TrimEnd(),'(?:^|\r?\n)Passed (\d+) actual model MakePedModel SDK checks \((\d+)-bit\)$')
            if($summary.Success){$result.checks=[int]$summary.Groups[1].Value;$result.pointerBits=[int]$summary.Groups[2].Value}
            $result.actualModelMakePedSdkTested=$stdout.Contains('Production actual model MakePedModel SDK fixture completed')
            $result.actualModelPedCreationOwnerDispatchSdkTested=$stdout.Contains('Production actual model ped creation owner dispatch SDK fixture completed')
            $result.actualConcreteStartupModelPreparationTested=$stdout.Contains('Production concrete startup model preparation fixture completed')
            $result.actualSharedPedCreationBoundariesTested=$stdout.Contains('Production shared ped creation original boundaries fixture completed')
            $expectedBits=if($architecture -eq 'x86'){32}else{64}
            if($result.test.exitCode -ne 0 -or $result.test.timedOut -or $result.checks -le 0 -or $result.pointerBits -ne $expectedBits -or
                -not $result.actualModelMakePedSdkTested -or -not $result.actualModelPedCreationOwnerDispatchSdkTested -or -not $result.actualConcreteStartupModelPreparationTested -or -not $result.actualSharedPedCreationBoundariesTested){throw "$architecture actual MakePedModel SDK execution evidence missing or failed."}
        }
        $record.actualModelMakePedSdkTested=@($record.results|Where-Object{-not $_.actualModelMakePedSdkTested}).Count -eq 0
        $record.actualModelPedCreationOwnerDispatchSdkTested=@($record.results|Where-Object{-not $_.actualModelPedCreationOwnerDispatchSdkTested}).Count -eq 0
        $record.actualConcreteStartupModelPreparationTested=@($record.results|Where-Object{-not $_.actualConcreteStartupModelPreparationTested}).Count -eq 0
        $record.actualSharedPedCreationBoundariesTested=@($record.results|Where-Object{-not $_.actualSharedPedCreationBoundariesTested}).Count -eq 0
        $record.generatedFixture.defaultCreatorRefusalCovered=$record.actualModelMakePedSdkTested
        $record.success=$record.results.Count -eq 2 -and $record.actualModelMakePedSdkTested -and $record.actualModelPedCreationOwnerDispatchSdkTested -and $record.actualConcreteStartupModelPreparationTested -and $record.actualSharedPedCreationBoundariesTested
    } else {
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
    $executable = Join-Path $outputRoot $(if ($Scope -eq 'Validity') {'model_validity_de_tests.exe'} elseif ($Scope -eq 'Dependencies') {'model_dependency_de_tests.exe'} else {'model_request_de_tests.exe'})
    $arguments = @('/nologo','/std:c++17','/EHsc','/W4','/WX','/O2','/MD','/DNOMINMAX','/DWIN32_LEAN_AND_MEAN',
        ('/I'+(Join-Path $taskRoot 'Shared/sdk')),('/I'+(Join-Path $taskRoot 'Client/sdk')),('/I'+$scratchRoot),
        (Join-Path $PSScriptRoot $fixtureSource)) + @($productionSources | ForEach-Object { Join-Path $taskRoot $_ }) +
        @(('/Fo'+$scratchRoot+[IO.Path]::DirectorySeparatorChar),('/Fd'+(Join-Path $scratchRoot 'fixture.pdb')),('/Fe'+$executable),'/link','bcrypt.lib')
    $record.result = [ordered]@{architecture='x64';environment=$environmentResult;compile=$null;test=$null;checks=0;
        pointerBits=$null;executable=$executable;executableSha256=$null}
    $record.result.compile = RunOwned $compiler $arguments $outputRoot 'compile' 60000
    if ($record.result.compile.exitCode -ne 0 -or $record.result.compile.timedOut) { throw 'Actual CModelInfo request SDK fixture compilation failed.' }
    $record.result.executableSha256 = Sha $executable
    $record.result.test = RunOwned $executable @() $outputRoot 'test' 10000
    $summary = [regex]::Match([IO.File]::ReadAllText($record.result.test.stdout).TrimEnd(),
        $(if ($Scope -eq 'Validity') {'(?:^|\r?\n)Passed (\d+) actual model validity SDK checks \((\d+)-bit\)$'} elseif ($Scope -eq 'Dependencies') {'(?:^|\r?\n)Passed (\d+) actual model dependency SDK checks \((\d+)-bit\)$'} else {'(?:^|\r?\n)Passed (\d+) actual model request SDK checks \((\d+)-bit\)$'}))
    if ($summary.Success) {
        $record.result.checks = [int]$summary.Groups[1].Value
        $record.result.pointerBits = [int]$summary.Groups[2].Value
    }
    $record.success = $record.result.test.exitCode -eq 0 -and -not $record.result.test.timedOut -and
        $record.result.checks -gt 0 -and $record.result.pointerBits -eq 64
    $stdout = [IO.File]::ReadAllText($record.result.test.stdout)
    $record.actualModelRequestSdkTested = $stdout.Contains('Production actual model request/reference SDK fixture completed')
    $record.actualModelValiditySdkTested = $stdout.Contains('Production actual model validity SDK fixture completed')
    $record.actualModelDependencySdkTested=$stdout.Contains('Production actual model dependency SDK fixture completed')
    if ($Scope -eq 'Dependencies') {
        $record.generatedFixture.defaultMissing505ReporterRefusalCovered=$stdout.Contains('Actual default Core505 reporter refusal covered')
        $record.generatedFixture.optionalAnimationOwnerDefaultRefusalCovered=$stdout.Contains('Actual optional animation owner default refusal covered')
        $record.success=$record.success -and $record.generatedFixture.defaultMissing505ReporterRefusalCovered -and $record.generatedFixture.optionalAnimationOwnerDefaultRefusalCovered
    }
    $selectedSdkTested = if ($Scope -eq 'Validity') {$record.actualModelValiditySdkTested} elseif ($Scope -eq 'Dependencies') {$record.actualModelDependencySdkTested} else {$record.actualModelRequestSdkTested}
    $record.success = $record.success -and $selectedSdkTested
    if (-not $record.success) { throw 'Actual model request/ref SDK checks failed or execution evidence was missing.' }

    }
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
if (-not $record.success) { throw "Actual model request/ref SDK checks failed; see $outputRoot" }
