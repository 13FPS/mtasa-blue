# Strict actual SDK receivers: camera4/35 and Cam5/13 inherited methods.
# Every unrelated hook aborts, so a factory/vector/setter shortcut fails.
function Get-CameraBasicsSdkMethods([string]$Root,[string]$RelativePath) {
    $text=[IO.File]::ReadAllText((Join-Path $Root $RelativePath))
    $text=[regex]::Replace($text,'(?s)/\*.*?\*/|//[^\r\n]*','')
    foreach($match in [regex]::Matches($text,'(?m)^\s*virtual\s+(?<signature>[^;]+);')) {
        $signature=[regex]::Replace($match.Groups['signature'].Value,'\s*=\s*0\s*$','').Trim()
        $name=[regex]::Match($signature,'(?<name>\w+)\s*\(').Groups['name'].Value
        if(-not $name -or $signature.Contains('{')){throw ('Unsupported actual camera SDK signature: '+$signature)}
        [ordered]@{name=$name;signature=([regex]::Replace($signature,'\s+',' '))}
    }
}
function Write-CameraBasicsFixture([string]$RepositoryRoot,[string]$OutputPath) {
    $source='Client/sdk/game/CCamera.h'
    $all=@(Get-CameraBasicsSdkMethods $RepositoryRoot $source)
    $provided=@($all|Where-Object {$_.name -in @('IsSphereVisible','GetActiveCam','Restore','RestoreWithJumpCut')})
    $rejected=@($all|Where-Object {$_.name -notin @('IsSphereVisible','GetActiveCam','Restore','RestoreWithJumpCut')})
    if($all.Count -ne 35 -or $provided.Count -ne 4 -or $rejected.Count -ne 31){throw 'Actual camera35-method partition changed'}
    $lines=@('#pragma once','#include <cstdlib>','#pragma warning(push)','#pragma warning(disable : 4100)',
        'class CameraBasicsObserver final : public mta::game_de::CCameraSphereVisibleDE {public:',
        'explicit CameraBasicsObserver(mta::game_de::NativeCameraSphereVisibleAccess& owner) noexcept : CCameraSphereVisibleDE(owner) {}')
    $lines+=@($rejected|ForEach-Object {$_.signature+' override {std::abort();}'})
    $lines+=@('};','#pragma warning(pop)')
    [IO.File]::WriteAllLines($OutputPath,$lines,[Text.UTF8Encoding]::new($false))
    [ordered]@{path=$OutputPath;sha256=(Get-FileHash -LiteralPath $OutputPath -Algorithm SHA256).Hash.ToLowerInvariant();rejectingMethods=$rejected.Count;
        inheritedProductionMethods=@($provided|ForEach-Object {$_.signature});source=$source;sourceSha256=(Get-FileHash -LiteralPath (Join-Path $RepositoryRoot $source) -Algorithm SHA256).Hash.ToLowerInvariant();
        productionCameraConcrete=$false;nativeFactoryClaimed=$false;sharedSdkEdited=$false;polymorphicBaseDeletionExecuted=$false}
}
function Write-CamFovFixture([string]$RepositoryRoot,[string]$OutputPath) {
    $source='Client/sdk/game/CCam.h'
    $all=@(Get-CameraBasicsSdkMethods $RepositoryRoot $source)
    $provided=@($all|Where-Object {$_.name -in @('GetFOV','SetFOV','GetMode','GetDirection','SetDirection')})
    $rejected=@($all|Where-Object {$_.name -notin @('GetFOV','SetFOV','GetMode','GetDirection','SetDirection')})
    if($all.Count -ne 13 -or $provided.Count -ne 5 -or $rejected.Count -ne 8){throw 'Actual Cam13-method partition changed'}
    $lines=@('#pragma once','#include <cstdlib>','#pragma warning(push)','#pragma warning(disable : 4100)',
        'class CamFovObserver final : public mta::game_de::CCamFovDE {public:',
        'CamFovObserver(std::uint32_t row,mta::game_de::NativeCameraSphereVisibleAccess& owner) noexcept : CCamFovDE(row,owner) {}')
    $lines+=@($rejected|ForEach-Object {$_.signature+' override {std::abort();}'})
    $lines+=@('};','#pragma warning(pop)')
    [IO.File]::WriteAllLines($OutputPath,$lines,[Text.UTF8Encoding]::new($false))
    [ordered]@{path=$OutputPath;sha256=(Get-FileHash -LiteralPath $OutputPath -Algorithm SHA256).Hash.ToLowerInvariant();rejectingMethods=$rejected.Count;
        inheritedProductionMethods=@($provided|ForEach-Object {$_.signature});source=$source;sourceSha256=(Get-FileHash -LiteralPath (Join-Path $RepositoryRoot $source) -Algorithm SHA256).Hash.ToLowerInvariant();
        productionCamConcrete=$false;nativeFactoryClaimed=$false;sharedSdkEdited=$false;polymorphicBaseDeletionExecuted=$false}
}

function Write-CamRowFixture([string]$RepositoryRoot,[string]$OutputPath) {
    $source='Client/sdk/game/CCam.h'
    $all=@(Get-CameraBasicsSdkMethods $RepositoryRoot $source)
    $names=@('GetFOV','SetFOV','GetMode','GetDirection','SetDirection','GetFront','GetUp','GetSource','GetFixedModeSource','GetFixedModeVector','GetTargetHistoryPos')
    $provided=@($all|Where-Object {$_.name -in $names})
    $rejected=@($all|Where-Object {$_.name -notin $names})
    if($all.Count -ne 13 -or $provided.Count -ne 11 -or $rejected.Count -ne 2){throw 'Actual retained Cam13-method partition changed'}
    $lines=@('#pragma once','#include <cstdlib>','#pragma warning(push)','#pragma warning(disable : 4100)',
        'class CamRowObserver final : public mta::game_de::CCamRowDE {public:',
        'CamRowObserver(std::uint32_t row,mta::game_de::NativeCameraSphereVisibleAccess& owner) : CCamRowDE(row,owner) {}')
    $lines+=@($rejected|ForEach-Object {$_.signature+' override {std::abort();}'})
    $lines+=@('};','#pragma warning(pop)')
    [IO.File]::WriteAllLines($OutputPath,$lines,[Text.UTF8Encoding]::new($false))
    [ordered]@{path=$OutputPath;sha256=(Get-FileHash -LiteralPath $OutputPath -Algorithm SHA256).Hash.ToLowerInvariant();rejectingMethods=$rejected.Count;
        inheritedProductionMethods=@($provided|ForEach-Object {$_.signature});source=$source;sourceSha256=(Get-FileHash -LiteralPath (Join-Path $RepositoryRoot $source) -Algorithm SHA256).Hash.ToLowerInvariant();
        productionCamConcrete=$false;nativeFactoryClaimed=$false;sharedSdkEdited=$false;polymorphicBaseDeletionExecuted=$false}
}

function Write-CamTargetFixture([string]$RepositoryRoot,[string]$OutputPath) {
    $source='Client/sdk/game/CCam.h'
    $all=@(Get-CameraBasicsSdkMethods $RepositoryRoot $source)
    $provided=@($all|Where-Object {$_.name -ne 'SetTargetEntity'})
    $rejected=@($all|Where-Object {$_.name -eq 'SetTargetEntity'})
    if($all.Count -ne 13 -or $provided.Count -ne 12 -or $rejected.Count -ne 1){throw 'Actual target Cam13-method partition changed'}
    $lines=@('#pragma once','#include <cstdlib>','#pragma warning(push)','#pragma warning(disable : 4100)',
        'class CamTargetObserver final : public mta::game_de::CCamTargetDE {public:',
        'CamTargetObserver(std::uint32_t row,mta::game_de::CPoolsDE& pools,mta::game_de::NativeGameTimingAccess& owner) : CCamTargetDE(row,pools,owner) {}')
    $lines+=@($rejected|ForEach-Object {$_.signature+' override {std::abort();}'})
    $lines+=@('};','#pragma warning(pop)')
    [IO.File]::WriteAllLines($OutputPath,$lines,[Text.UTF8Encoding]::new($false))
    [ordered]@{path=$OutputPath;sha256=(Get-FileHash -LiteralPath $OutputPath -Algorithm SHA256).Hash.ToLowerInvariant();rejectingMethods=$rejected.Count;
        inheritedProductionMethods=@($provided|ForEach-Object {$_.signature});source=$source;sourceSha256=(Get-FileHash -LiteralPath (Join-Path $RepositoryRoot $source) -Algorithm SHA256).Hash.ToLowerInvariant();
        productionCamConcrete=$false;nativeFactoryClaimed=$false;sharedSdkEdited=$false;polymorphicBaseDeletionExecuted=$false}
}

function Write-PedPooledFixture([string]$RepositoryRoot,[string]$OutputPath) {
    # Reuse the existing strict receiver generator, adding only actual pooled
    # GetInterface composition. Every unrelated hook remains a rejecting stub.
    $helper=Join-Path $RepositoryRoot 'Tests/definitive/Write-PedPoolRegistryFixture.ps1'
    . $helper
    $basePath=$OutputPath+'.base.h'
    $base=Write-PedPoolRegistryFixture $RepositoryRoot $basePath
    $text=[IO.File]::ReadAllText($basePath)
    $old='class Ped final : public Anchor, public mta::game_de::CPedVitalsDE { public:'
    if(-not $text.Contains($old)){throw 'Existing strict ped composition changed'}
    $text=$text.Replace('namespace pool_registry_fixture {','namespace camera_target_fixture {')
    $text=$text.Replace($old,'class Ped : public Anchor, public mta::game_de::CPedVitalsDE, public mta::game_de::CPedPooledDE { public:')
    # The deliberate virtual-CPed diamond inherits each real overrider from
    # its production slice. Do not add forwarding overrides to hide dominance.
    $text=$text.Replace('#pragma warning(disable : 4100)','#pragma warning(disable : 4100 4250)')
    $old='Ped(mta::game_de::NativeGameTimingAccess& owner,const mta::game_de::NativePedReferenceToken& token,std::function<void()> callback={}) noexcept : CPedVitalsDE(owner,token),destructionCallbackValue(std::move(callback)){}'
    if(-not $text.Contains($old)){throw 'Existing strict ped constructor changed'}
    $text=$text.Replace($old,'Ped(mta::game_de::NativeGameTimingAccess& owner,const mta::game_de::NativePedReferenceToken& token,mta::game_de::CPoolsDE& registry,std::function<void()> callback={}) noexcept : CPedVitalsDE(owner,token),CPedPooledDE(registry,owner),destructionCallbackValue(std::move(callback)){}')
    $pattern='(?m)^class CEntitySAInterface\* GetInterface\(\) override \{Unqualified\(\);\}\r?\n'
    if([regex]::Matches($text,$pattern).Count -ne 1){throw 'Expected exactly one rejecting GetInterface hook'}
    $text=[regex]::Replace($text,$pattern,'')
    [IO.File]::WriteAllText($OutputPath,$text,[Text.UTF8Encoding]::new($false))
    [ordered]@{path=$OutputPath;sha256=(Get-FileHash -LiteralPath $OutputPath -Algorithm SHA256).Hash.ToLowerInvariant();
        rejectingPedMethods=($base.rejectingPedMethods-1);rejectingPoolMethods=$base.rejectingPoolMethods;
        inheritedPooledMethods=@('CEntity::GetInterface');sharedVirtualPedBase=$true;nativeFactoryClaimed=$false;
        baseGenerator=$helper;baseGeneratorSha256=(Get-FileHash -LiteralPath $helper -Algorithm SHA256).Hash.ToLowerInvariant()}
}
