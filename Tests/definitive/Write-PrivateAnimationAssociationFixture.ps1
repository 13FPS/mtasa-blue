# Derive all22actual SDK operations. Only the seventeen implemented operations
# remain inherited; no clump/noexcept/factory behavior is faked by this leaf.
function Write-PrivateAnimationAssociationFixture([string]$RepositoryRoot,[string]$OutputPath) {
    $source='Client/sdk/game/CAnimBlendAssociation.h'
    $text=[IO.File]::ReadAllText((Join-Path $RepositoryRoot $source))
    $text=[regex]::Replace($text,'(?s)/\*.*?\*/|//[^\r\n]*','')
    $provided=@()
    $methods=@(foreach($match in [regex]::Matches($text,'virtual\s+(?<signature>[^;{}]+);')) {
        $signature=[regex]::Replace($match.Groups['signature'].Value,'\s*=\s*0\s*$','').Trim()
        $name=[regex]::Match($signature,'(?<name>\w+)\s*\(').Groups['name'].Value
        $staticCtor=$name -eq 'Constructor' -and $signature -match 'CAnimBlendStaticAssociationSAInterface\s*&'
        if($staticCtor -or $name -in @('GetInterface','GetAnimGroup','GetAnimID','GetBlendAmount','SetBlendAmount','IsPartial','SetCurrentSpeed','SetAnimID','SetAnimGroup','SetFlags','GetAnimHierarchy','RestrictToBones','RestrictToBonesOf','GetCurrentProgress','GetCurrentSpeed','GetLength')) {$provided+=$signature}
        else {[regex]::Replace($signature,'\s+',' ')}
    })
    if($provided.Count -ne 17 -or $methods.Count -ne 5){throw 'Actual private association17/5partition changed'}
    foreach($pure in @('GetCurrentProgress','GetCurrentSpeed','GetLength')) {
        if(@($provided|Where-Object {$_ -match ('\b'+$pure+'\s*\(') -and $_ -notmatch '\bnoexcept\b'}).Count -ne 1){throw ('Missing actual throwing-contract getter: '+$pure)}
    }
    $lines=@('#pragma once','#include <windows.h>','#include <game/CAnimBlendHierarchy.h>','#include <cstdlib>','#pragma warning(push)','#pragma warning(disable : 4100)',
        'class PrivateAnimationAssociationReceiver final : public mta::game_de::CAnimBlendAssociationDE { public:',
        'using CAnimBlendAssociationDE::Constructor;',
        'PrivateAnimationAssociationReceiver(mta::game_de::NativeAnimationAssociationAccess& owner,const mta::game_de::NativeAnimationAssociationReferenceToken& token) noexcept : CAnimBlendAssociationDE(owner,token) {}')
    $lines+=@($methods|ForEach-Object {$_+' override {std::abort();}'})
    $lines+=@('};','#pragma warning(pop)')
    [IO.File]::WriteAllLines($OutputPath,$lines,[Text.UTF8Encoding]::new($false))
    [ordered]@{path=$OutputPath;sha256=(Get-FileHash -LiteralPath $OutputPath -Algorithm SHA256).Hash.ToLowerInvariant();rejectingMethods=$methods.Count;
        inheritedProductionMethods=$provided;source=$source;sourceSha256=(Get-FileHash -LiteralPath (Join-Path $RepositoryRoot $source) -Algorithm SHA256).Hash.ToLowerInvariant();
        productionAssociationConcrete=$false;nativeFactoryClaimed=$false;sharedSdkEdited=$false;polymorphicBaseDeletionExecuted=$false}
}
