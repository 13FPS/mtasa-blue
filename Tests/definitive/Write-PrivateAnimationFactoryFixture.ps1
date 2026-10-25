# Derive unsupported operations from the real common SDK. The four factories and two
# current private factory and cleanup operations remain inherited; no complete Game or
# success-shaped substitute manager is supplied by this constructor shell.
function Write-PrivateAnimationFactoryFixture([string]$RepositoryRoot,[string]$OutputPath) {
    $source='Client/sdk/game/CAnimManager.h'
    $text=[IO.File]::ReadAllText((Join-Path $RepositoryRoot $source))
    $text=[regex]::Replace($text,'(?s)/\*.*?\*/|//[^\r\n]*','')
    $provided=@()
    $methods=@(foreach($match in [regex]::Matches($text,'virtual\s+(?<signature>[^;{}]+);')) {
        $signature=$match.Groups['signature'].Value.Trim()
        if($signature -notmatch '=\s*0\s*$'){continue}
        $signature=[regex]::Replace($signature,'\s*=\s*0\s*$','').Trim()
        $name=[regex]::Match($signature,'(?<name>\w+)\s*\(').Groups['name'].Value
        if(-not $name){throw "Cannot derive private manager fixture from $signature"}
        if($name -in @('GetCustomAnimBlendHierarchy','DeleteCustomAnimHierarchyInterface','GetCustomAnimBlendSequenceAt','BeginCustomAnimationCleanup','HasPendingCustomAnimationCleanup')) {
            $provided+= $signature
        } else {
            $signature=[regex]::Replace($signature,'\s*=\s*[^,)]+','')
            [regex]::Replace($signature,'\s+',' ')
        }
    })
    if($provided.Count -ne 6 -or $methods.Count -ne 63 -or
       @($provided|Where-Object{$_ -match '\bGetCustomAnimBlendHierarchy\s*\('}).Count -ne 2) {
        throw 'Exact four factory and two cleanup manager contracts changed'
    }
    # The runtime cleanup fixture inherits these actual SDK methods; an
    # abortor here would mask the production ticket or teardown query.
    foreach($required in @('BeginCustomAnimationCleanup','HasPendingCustomAnimationCleanup')) {
        if(@($provided|Where-Object {$_ -match ('\b'+$required+'\s*\(')}).Count -ne 1) {
            throw ('Missing actual inherited cleanup operation: '+$required)
        }
    }
    $lines=@('#pragma once','#include <cstdlib>','#pragma warning(push)','#pragma warning(disable : 4100)',
        'class PrivateAnimationFactoryManagerReceiver final : public mta::game_de::CAnimManagerDE { public:',
        'PrivateAnimationFactoryManagerReceiver(mta::game_de::NativeAnimationManagerAccess& manager,mta::game_de::NativeAnimationHierarchyAccess& hierarchy,mta::game_de::NativeAnimationSequenceAccess& sequence,mta::game_de::NativeAnimationSequenceStorageAccess& storage,std::uint64_t generation) : CAnimManagerDE(manager,hierarchy,sequence,storage,generation) {}',
        'PrivateAnimationFactoryManagerReceiver(mta::game_de::NativeAnimationManagerAccess& manager,std::uint64_t generation) noexcept : CAnimManagerDE(manager,generation) {}')
    $lines+=@($methods|ForEach-Object{$_+' override {std::abort();}'})
    $lines+=@('};','#pragma warning(pop)')
    [IO.File]::WriteAllLines($OutputPath,$lines,[Text.UTF8Encoding]::new($false))
    [ordered]@{path=$OutputPath;sha256=(Get-FileHash -LiteralPath $OutputPath -Algorithm SHA256).Hash.ToLowerInvariant();
        rejectingMethods=$methods.Count;inheritedFactoryMethods=@($provided|Where-Object {$_ -notmatch '\b(?:BeginCustomAnimationCleanup|HasPendingCustomAnimationCleanup)\s*\('});inheritedCleanupMethods=@($provided|Where-Object {$_ -match '\b(?:BeginCustomAnimationCleanup|HasPendingCustomAnimationCleanup)\s*\('});source=$source;
        sourceSha256=(Get-FileHash -LiteralPath (Join-Path $RepositoryRoot $source) -Algorithm SHA256).Hash.ToLowerInvariant();
        productionManagerConcrete=$false;completeGameSupplied=$false;parserReadyClaimed=$false}
}
