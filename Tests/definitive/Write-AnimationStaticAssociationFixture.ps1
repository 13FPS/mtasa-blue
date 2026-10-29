# All eleven real methods stay inherited from the complete production receiver.
# This helper only exposes its protected CPU-facade constructor to fixtures.
function Write-AnimationStaticAssociationFixture([string]$RepositoryRoot,[string]$OutputPath) {
    $source='Client/sdk/game/CAnimBlendStaticAssociation.h'
    $text=[IO.File]::ReadAllText((Join-Path $RepositoryRoot $source))
    $text=[regex]::Replace($text,'(?s)/\*.*?\*/|//[^\r\n]*','')
    $provided=@()
    $methods=@(foreach($match in [regex]::Matches($text,'virtual\s+(?<signature>[^;{}]+);')) {
        $signature=$match.Groups['signature'].Value.Trim()
        if($signature -notmatch '=\s*0\s*$'){continue}
        $signature=[regex]::Replace($signature,'\s*=\s*0\s*$','').Trim()
        $name=[regex]::Match($signature,'(?<name>\w+)\s*\(').Groups['name'].Value
        if($name -in @('SetNumBlendNodes','SetAnimID','SetAnimGroup','SetFlags','GetNumBlendNodes','GetAnimID','GetAnimGroup','GetFlags','GetAnimHierarchyInterface','GetInterface','Initialize')) {$provided+=$signature}
        else {[regex]::Replace($signature,'\s+',' ')}
    })
    if($provided.Count -ne 11 -or $methods.Count -ne 0){throw 'Actual complete static-association eleven-method partition changed'}
    $lines=@('#pragma once','#include <cstdlib>','#pragma warning(push)','#pragma warning(disable : 4100)',
        'class AnimationStaticAssociationReceiver final : public mta::game_de::CAnimBlendStaticAssociationDE { public:',
        'AnimationStaticAssociationReceiver(mta::game_de::NativeAnimationStaticAssociationAccess& owner,const mta::game_de::NativeAnimationStaticAssociationReferenceToken& token) noexcept : CAnimBlendStaticAssociationDE(owner,token) {}')
    $lines+=@($methods|ForEach-Object {$_+' override {std::abort();}'})
    $lines+=@('};','#pragma warning(pop)')
    [IO.File]::WriteAllLines($OutputPath,$lines,[Text.UTF8Encoding]::new($false))
    [ordered]@{path=$OutputPath;sha256=(Get-FileHash -LiteralPath $OutputPath -Algorithm SHA256).Hash.ToLowerInvariant();
        rejectingMethods=$methods.Count;inheritedMethods=$provided;inheritedReadMethods=@($provided|Where-Object {$_ -match '\bGet\w+\s*\('});inheritedWriteMethods=@($provided|Where-Object {$_ -match '\bSet\w+\s*\('});source=$source;
        sourceSha256=(Get-FileHash -LiteralPath (Join-Path $RepositoryRoot $source) -Algorithm SHA256).Hash.ToLowerInvariant();
        productionStaticAssociationConcrete=$true;nativeFactoryClaimed=$false;polymorphicBaseDeletionExecuted=$false}
}
