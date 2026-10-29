# Derive every rejecting method from the real common manager SDK. This receiver
# exposes only the two real static factories and their real group precheck.
function Write-StaticAssociationFactoryFixture([string]$RepositoryRoot,[string]$OutputPath) {
    $source='Client/sdk/game/CAnimManager.h'
    $text=[IO.File]::ReadAllText((Join-Path $RepositoryRoot $source))
    $text=[regex]::Replace($text,'(?s)/\*.*?\*/|//[^\r\n]*','')
    $provided=@()
    $methods=@(foreach($match in [regex]::Matches($text,'virtual\s+(?<signature>[^;{}]+);')) {
        $signature=$match.Groups['signature'].Value.Trim()
        if($signature -notmatch '=\s*0\s*$'){continue}
        $signature=[regex]::Replace($signature,'\s*=\s*0\s*$','').Trim()
        $name=[regex]::Match($signature,'(?<name>\w+)\s*\(').Groups['name'].Value
        if(-not $name){throw "Cannot derive actual static factory fixture from $signature"}
        if($name -in @('GetAnimStaticAssociation','IsValidGroup')) {$provided+=$signature}
        else {[regex]::Replace([regex]::Replace($signature,'\s*=\s*[^,)]+',''),'\s+',' ')}
    })
    if($provided.Count -ne 3 -or $methods.Count -ne 69 -or
       @($provided|Where-Object {$_ -match '\bGetAnimStaticAssociation\s*\('}).Count -ne 2) {
        throw 'Exact actual static factory SDK partition changed'
    }
    $lines=@('#pragma once','#include <cstdlib>','#pragma warning(push)','#pragma warning(disable : 4100)',
        'class StaticAssociationFactoryManagerReceiver final : public mta::game_de::CAnimManagerDE { public:',
        'using CAnimManagerDE::GetAnimStaticAssociation;',
        'StaticAssociationFactoryManagerReceiver(mta::game_de::NativeAnimationManagerAccess& owner,std::uint64_t generation) noexcept : CAnimManagerDE(owner,generation) {}')
    $lines+=@($methods|ForEach-Object {$_+' override {std::abort();}'})
    $lines+=@('};','#pragma warning(pop)')
    [IO.File]::WriteAllLines($OutputPath,$lines,[Text.UTF8Encoding]::new($false))
    [ordered]@{path=$OutputPath;sha256=(Get-FileHash -LiteralPath $OutputPath -Algorithm SHA256).Hash.ToLowerInvariant();
        source=$source;sourceSha256=(Get-FileHash -LiteralPath (Join-Path $RepositoryRoot $source) -Algorithm SHA256).Hash.ToLowerInvariant();
        rejectingMethods=$methods.Count;inheritedMethods=$provided;productionManagerConcrete=$false;completeGameSupplied=$false}
}
