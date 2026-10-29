# Derive every remaining mandatory group operation from the actual SDK.
# Inherit nine completed production operations, including the owner-command
# empty initializer. The remaining two SDK methods are fixture-only rejectors.
function Write-AnimationAssociationGroupFixture([string]$RepositoryRoot,[string]$OutputPath) {
    $source='Client/sdk/game/CAnimBlendAssocGroup.h'
    $text=[IO.File]::ReadAllText((Join-Path $RepositoryRoot $source))
    $text=[regex]::Replace($text,'(?s)/\*.*?\*/|//[^\r\n]*','')
    $provided=@()
    $methods=@(foreach($match in [regex]::Matches($text,'virtual\s+(?<signature>[^;{}]+);')) {
        $signature=$match.Groups['signature'].Value.Trim()
        if($signature -notmatch '=\s*0\s*$'){continue}
        $signature=[regex]::Replace($signature,'\s*=\s*0\s*$','').Trim()
        $name=[regex]::Match($signature,'(?<name>\w+)\s*\(').Groups['name'].Value
        if($name -in @('IsCreated','GetNumAnimations','GetGroupID','IsLoaded','GetInterface','SetIDOffset','GetAnimBlock','GetAnimation','InitEmptyAssociations')) {$provided+=$signature}
        else {[regex]::Replace($signature,'\s+',' ')}
    })
    if($provided.Count -ne 9 -or $methods.Count -ne 2){throw 'Actual group empty-initialization/pure method partition changed'}
    foreach($name in @('IsCreated','GetNumAnimations','GetGroupID','IsLoaded','GetInterface','SetIDOffset','GetAnimBlock','GetAnimation','InitEmptyAssociations')) {
        if(@($provided|Where-Object {$_ -match ('\b'+$name+'\s*\(')}).Count -ne 1){throw ('Missing inherited actual group method: '+$name)}
    }
    $lines=@('#pragma once','#include <cstdlib>','#pragma warning(push)','#pragma warning(disable : 4100)',
        'class AnimationAssociationGroupReceiver final : public mta::game_de::CAnimBlendAssocGroupDE { public:',
        'AnimationAssociationGroupReceiver(mta::game_de::NativeAnimationAssociationGroupAccess& owner,const mta::game_de::NativeAnimationAssociationGroupReferenceToken& token) noexcept : CAnimBlendAssocGroupDE(owner,token) {}',
        'AnimationAssociationGroupReceiver(mta::game_de::NativeAnimationAssociationGroupAccess& owner,const mta::game_de::NativeAnimationAssociationGroupReferenceToken& token,CGame& game) noexcept : CAnimBlendAssocGroupDE(owner,token,game) {}')
    $lines+=@($methods|ForEach-Object {$_+' override {std::abort();}'})
    $lines+=@('};','#pragma warning(pop)')
    [IO.File]::WriteAllLines($OutputPath,$lines,[Text.UTF8Encoding]::new($false))
    [ordered]@{path=$OutputPath;sha256=(Get-FileHash -LiteralPath $OutputPath -Algorithm SHA256).Hash.ToLowerInvariant();
        rejectingMethods=$methods.Count;inheritedMethods=$provided;
        inheritedReadMethods=@($provided|Where-Object {$_ -notmatch '\b(?:SetIDOffset|GetAnimBlock|GetAnimation|InitEmptyAssociations)\s*\('});
        inheritedFactoryMethods=@($provided|Where-Object {$_ -match '\b(?:GetAnimBlock|GetAnimation)\s*\('});
        inheritedWriteMethods=@($provided|Where-Object {$_ -match '\bSetIDOffset\s*\('});
        inheritedInitializationMethods=@($provided|Where-Object {$_ -match '\bInitEmptyAssociations\s*\('});source=$source;
        sourceSha256=(Get-FileHash -LiteralPath (Join-Path $RepositoryRoot $source) -Algorithm SHA256).Hash.ToLowerInvariant();
        productionGroupConcrete=$false;nativeFactoryClaimed=$false;polymorphicBaseDeletionExecuted=$false}
}
