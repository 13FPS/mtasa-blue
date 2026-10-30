# Strict actual SDK receivers: camera4/35 and Cam1/13 inherited methods.
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
    $provided=@($all|Where-Object {$_.name -eq 'GetFOV'})
    $rejected=@($all|Where-Object {$_.name -ne 'GetFOV'})
    if($all.Count -ne 13 -or $provided.Count -ne 1 -or $rejected.Count -ne 12){throw 'Actual Cam13-method partition changed'}
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
