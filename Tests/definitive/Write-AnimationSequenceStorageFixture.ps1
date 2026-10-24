# Derive only the remaining mandatory manager methods from the real SDK.
# Implemented DE observations/factories/storage dispatch stay inherited.
function Write-AnimationSequenceStorageFixture([string]$RepositoryRoot,[string]$OutputPath) {
    $source='Client/sdk/game/CAnimManager.h'
    $text=[IO.File]::ReadAllText((Join-Path $RepositoryRoot $source))
    $text=[regex]::Replace($text,'(?s)/\*.*?\*/|//[^\r\n]*','')
    $implemented=@('GetAnimationBlock','GetAnimBlock','GetNumAnimations','GetNumAnimBlocks','GetAnimationBlockIndex',
        'GetGateWayBlockName','GetGateWayAnimationName','AllocateCustomAnimSequences','FreeCustomAnimSequences')
    $methods=@(foreach($match in [regex]::Matches($text,'virtual\s+(?<signature>[^;{}]+);')) {
        $signature=$match.Groups['signature'].Value.Trim()
        if($signature -notmatch '=\s*0\s*$'){continue}
        $signature=[regex]::Replace($signature,'\s*=\s*0\s*$','').Trim()
        $name=[regex]::Match($signature,'(?<name>\w+)\s*\(').Groups['name'].Value
        if(-not $name){throw "Cannot derive actual manager method from $signature"}
        if($name -notin $implemented) {
            $signature=[regex]::Replace($signature,'\s*=\s*[^,)]+','')
            [regex]::Replace($signature,'\s+',' ')
        }
    })
    if($methods.Count -lt 35){throw 'Remaining actual manager fixture extraction was incomplete'}
    $lines=@('#pragma once','#pragma warning(push)','#pragma warning(disable : 4100)',
        'class AnimationSequenceStorageManagerReceiver final : public mta::game_de::CAnimManagerDE { public:',
        'AnimationSequenceStorageManagerReceiver(mta::game_de::NativeAnimationManagerAccess& manager,mta::game_de::NativeAnimationSequenceStorageAccess& storage,std::uint64_t generation) : CAnimManagerDE(manager,storage,generation) {}',
        'AnimationSequenceStorageManagerReceiver(mta::game_de::NativeAnimationManagerAccess& manager,std::uint64_t generation) noexcept : CAnimManagerDE(manager,generation) {}')
    $lines+=@($methods|ForEach-Object{$_+' override {std::abort();}'})
    $lines+=@('};','#pragma warning(pop)')
    [IO.File]::WriteAllLines($OutputPath,$lines,[Text.UTF8Encoding]::new($false))
    [ordered]@{path=$OutputPath;sha256=(Get-FileHash -LiteralPath $OutputPath -Algorithm SHA256).Hash.ToLowerInvariant();
        rejectingMethods=$methods.Count;inheritedImplementedMethods=$implemented;source=$source;
        sourceSha256=(Get-FileHash -LiteralPath (Join-Path $RepositoryRoot $source) -Algorithm SHA256).Hash.ToLowerInvariant();
        completeGameSupplied=$false;productionManagerConcrete=$false}
}
