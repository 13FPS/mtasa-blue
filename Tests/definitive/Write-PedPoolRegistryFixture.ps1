# This helper emits rejecting test-only concrete subclasses of the actual SDK
# slices. It supplies no native pool/factory behavior and executes no compiler.
function Write-PedPoolRegistryFixture([string]$RepositoryRoot,[string]$OutputPath) {
    function InterfaceMethods([string]$RelativePath) {
        $text=[IO.File]::ReadAllText((Join-Path $RepositoryRoot $RelativePath))
        $text=[regex]::Replace($text,'(?s)/\*.*?\*/|//[^\r\n]*','')
        foreach($match in [regex]::Matches($text,'virtual\s+(?<signature>[^;{}]+);')) {
            $signature=[regex]::Replace($match.Groups['signature'].Value,'\s*=\s*0\s*$','').Trim()
            $name=[regex]::Match($signature,'(?<name>\w+)\s*\(').Groups['name'].Value
            if(-not $name){throw "Cannot derive rejecting fixture method from $signature"}
            $signature=[regex]::Replace($signature,'\s*=\s*[^,)]+','')
            [ordered]@{name=$name;signature=([regex]::Replace($signature,'\s+',' '))}
        }
    }
    $seen=@{}
    $pedMethods=@(foreach($path in @('Client/sdk/game/CEntity.h','Client/sdk/game/CPhysical.h','Client/sdk/game/CPed.h')){
        InterfaceMethods $path | Where-Object {
            if($_.name -in @('GetHealth','SetHealth','GetArmor','SetArmor')){$false}
            else{
                $key=[regex]::Replace($_.signature,'\s+','')
                if($seen.ContainsKey($key)){$false}else{$seen[$key]=$true;$true}
            }
        }
    })
    $poolMethods=@(InterfaceMethods 'Client/sdk/game/CPools.h' | Where-Object {
        $_.name -notin @('GetPed','GetPedFromRef','GetPedCount','ResetPedPoolCount','InvalidateLocalPlayerClientEntity')
    })
    $lines=@('#pragma once','#pragma warning(push)','#pragma warning(disable : 4100)',
        'namespace pool_registry_fixture {',
        '[[noreturn]] inline void Unqualified() noexcept {std::abort();}',
        'struct Anchor { virtual ~Anchor()=default;std::array<unsigned char,17> padding{}; };',
        'class Ped final : public Anchor, public mta::game_de::CPedVitalsDE { public:',
        'Ped(mta::game_de::NativeGameTimingAccess& owner,const mta::game_de::NativePedReferenceToken& token,std::function<void()> callback={}) noexcept : CPedVitalsDE(owner,token),destructionCallbackValue(std::move(callback)){}',
        '~Ped() override {if(destructionCallbackValue)destructionCallbackValue();}')
    $lines+=@($pedMethods | ForEach-Object {$_.signature+' override {Unqualified();}'})
    $lines+=@('private:std::function<void()> destructionCallbackValue;','};',
        'class Pools final : public mta::game_de::CPoolsDE { public:',
        'explicit Pools(mta::game_de::NativeGameTimingAccess& owner) noexcept : CPoolsDE(owner){}',
        '~Pools() override=default;',
        'using CPoolsDE::BindBorrowedPed;')
    $lines+=@($poolMethods | ForEach-Object {$_.signature+' override {Unqualified();}'})
    $lines+=@('};','}','#pragma warning(pop)')
    [IO.File]::WriteAllLines($OutputPath,$lines,[Text.UTF8Encoding]::new($false))
    [ordered]@{path=$OutputPath;sha256=(Get-FileHash -LiteralPath $OutputPath -Algorithm SHA256).Hash.ToLowerInvariant();
        rejectingPedMethods=$pedMethods.Count;rejectingPoolMethods=$poolMethods.Count}
}
