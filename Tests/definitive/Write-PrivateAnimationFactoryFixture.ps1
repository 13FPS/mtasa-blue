# Derive unsupported operations from the real common SDK. Only actual current
# private/bank factories and owner-receipt operations remain inherited; raw sequence
# interface delete still rejects and no complete Game/provider is supplied.
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
        $bankOrdinal = $name -eq 'GetAnimation' -and $signature -match '\bGetAnimation\s*\(\s*int\s+\w+\s*\)\s*$'
        if($bankOrdinal -or $name -eq 'GetAnimBlendHierarchy' -or $name -in @('GetCustomAnimBlendHierarchy','DeleteCustomAnimHierarchyInterface','GetCustomAnimBlendSequenceAt','BeginCustomAnimationCleanup','HasPendingCustomAnimationCleanup','GetCustomAnimBlendSequence','BeginCustomAnimationSequenceCleanup','BeginCustomAnimationSequenceTransfer','BeginCustomAnimationFrameAttachment')) {
            $provided+= $signature
        } else {
            $signature=[regex]::Replace($signature,'\s*=\s*[^,)]+','')
            [regex]::Replace($signature,'\s+',' ')
        }
    })
    if($provided.Count -ne 13 -or $methods.Count -ne 59 -or
       @($provided|Where-Object{$_ -match '\bGetCustomAnimBlendHierarchy\s*\('}).Count -ne 2) {
        throw 'Exact actual private factory and standalone operation contracts changed'
    }
    # The runtime cleanup fixture inherits these actual SDK methods; an
    # abortor here would mask the production ticket or teardown query.
    foreach($required in @('BeginCustomAnimationCleanup','HasPendingCustomAnimationCleanup')) {
        if(@($provided|Where-Object {$_ -match ('\b'+$required+'\s*\(')}).Count -ne 1) {
            throw ('Missing actual inherited cleanup operation: '+$required)
        }
    }
    # The next owner scope inherits actual standalone operations. An abortor
    # here would hide the real typed ticket/provider path under test.
    foreach($required in @('BeginCustomAnimationSequenceCleanup','BeginCustomAnimationSequenceTransfer','BeginCustomAnimationFrameAttachment')) {
        if(@($provided|Where-Object {$_ -match ('\b'+$required+'\s*\(')}).Count -ne 1) {
            throw ('Missing actual inherited standalone operation: '+$required)
        }
    }
    if(@($provided|Where-Object{$_ -match '\bGetCustomAnimBlendSequence\s*\('}).Count -ne 2 -or
       @($methods|Where-Object{$_ -match '\bDeleteCustomAnimSequenceInterface\s*\('}).Count -ne 1) {
        throw 'Standalone new/borrow factories must inherit while raw interface-only delete stays rejecting'
    }
    # Only the integer ordinal overload is implemented in this bank slice.
    # Retaining the other two abortors must not hide the inherited overload.
    if(@($provided|Where-Object{$_ -match '\bGetAnimation\s*\('}).Count -ne 1 -or
       @($provided|Where-Object{$_ -match '\bGetAnimBlendHierarchy\s*\('}).Count -ne 1 -or
       @($methods|Where-Object{$_ -match '\bGetAnimation\s*\('}).Count -ne 2) {
        throw 'Bank factories must preserve both unsupported name/block overloads'
    }
    $lines=@('#pragma once','#include <cstdlib>','#pragma warning(push)','#pragma warning(disable : 4100)',
        'class PrivateAnimationFactoryManagerReceiver final : public mta::game_de::CAnimManagerDE { public:',
        'using CAnimManagerDE::GetAnimation;',
        'PrivateAnimationFactoryManagerReceiver(mta::game_de::NativeAnimationManagerAccess& manager,mta::game_de::NativeAnimationHierarchyAccess& hierarchy,mta::game_de::NativeAnimationSequenceAccess& sequence,mta::game_de::NativeAnimationSequenceStorageAccess& storage,std::uint64_t generation) : CAnimManagerDE(manager,hierarchy,sequence,storage,generation) {}',
        'PrivateAnimationFactoryManagerReceiver(mta::game_de::NativeAnimationManagerAccess& manager,std::uint64_t generation) noexcept : CAnimManagerDE(manager,generation) {}')
    $lines+=@($methods|ForEach-Object{$_+' override {std::abort();}'})
    $lines+=@('};','#pragma warning(pop)')
    [IO.File]::WriteAllLines($OutputPath,$lines,[Text.UTF8Encoding]::new($false))
    [ordered]@{path=$OutputPath;sha256=(Get-FileHash -LiteralPath $OutputPath -Algorithm SHA256).Hash.ToLowerInvariant();
        rejectingMethods=$methods.Count;inheritedFactoryMethods=@($provided|Where-Object {$_ -notmatch '\b(?:BeginCustomAnimationCleanup|HasPendingCustomAnimationCleanup|BeginCustomAnimationSequenceCleanup|BeginCustomAnimationSequenceTransfer|BeginCustomAnimationFrameAttachment)\s*\('});inheritedCleanupMethods=@($provided|Where-Object {$_ -match '\b(?:BeginCustomAnimationCleanup|HasPendingCustomAnimationCleanup|BeginCustomAnimationSequenceCleanup|BeginCustomAnimationSequenceTransfer|BeginCustomAnimationFrameAttachment)\s*\('});source=$source;
        sourceSha256=(Get-FileHash -LiteralPath (Join-Path $RepositoryRoot $source) -Algorithm SHA256).Hash.ToLowerInvariant();
        productionManagerConcrete=$false;completeGameSupplied=$false;parserReadyClaimed=$false}
}
