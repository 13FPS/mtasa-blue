# Keep the actual inherited entity/ped surface explicit. Only the seven
# production vitals/metadata/render methods stay inherited on this test leaf.
function Write-PedRenderFixture([string]$RepositoryRoot,[string]$OutputPath) {
    $provided=@('GetHealth','SetHealth','GetArmor','SetArmor','GetModelIndex','GetEntityType','GetRpClump')
    $declarations=[IO.File]::ReadAllText((Join-Path $RepositoryRoot 'Client/game_de/CPedVitalsDE.h'))+[IO.File]::ReadAllText((Join-Path $RepositoryRoot 'Client/game_de/CPedMetadataDE.h'))
    foreach($name in $provided){if($declarations -notmatch ('\b'+$name+'\s*\([^;{}]*\)\s*(?:const\s*)?override\s*;')){throw ('Missing inherited production ped method: '+$name)}}
    $methods=@(foreach($source in @('Client/sdk/game/CEntity.h','Client/sdk/game/CPhysical.h','Client/sdk/game/CPed.h')) {
        $text=[IO.File]::ReadAllText((Join-Path $RepositoryRoot $source))
        $text=[regex]::Replace($text,'(?s)/\*.*?\*/|//[^\r\n]*','')
        foreach($match in [regex]::Matches($text,'virtual\s+(?<signature>[^;{}]+);')) {
            $signature=[regex]::Replace($match.Groups['signature'].Value,'\s*=\s*0\s*$','').Trim()
            $name=[regex]::Match($signature,'(?<name>\w+)\s*\(').Groups['name'].Value
            if(-not $name){throw ('Cannot derive actual ped signature: '+$signature)}
            if($provided -contains $name){continue}
            $signature=[regex]::Replace($signature,'\s*=\s*[^,)]+','')
            [regex]::Replace($signature,'\s+',' ')
        }
    })
    $seen=@{}
    $methods=@($methods|Where-Object {$key=[regex]::Replace($_,'\s+','');if($seen.ContainsKey($key)){$false}else{$seen[$key]=$true;$true}})
    if($methods.Count -le 100 -or @($methods|Where-Object {$_ -match '\bGetRpClump\s*\('}).Count){throw 'Actual strict inherited ped partition changed'}
    $lines=@('#pragma once','#include <cstdlib>','#pragma warning(push)','#pragma warning(disable : 4100)','namespace ped_render_fixture {',
        ('inline constexpr unsigned RejectingMethods = {0};' -f $methods.Count),
        'class PedRenderFixture final : public mta::game_de::CPedMetadataDE { public:',
        'PedRenderFixture(mta::game_de::NativePedVitalsAccess& vitals,mta::game_de::NativePedMetadataAccess& metadata,const mta::game_de::NativePedReferenceToken& token) noexcept : CPedMetadataDE(vitals,metadata,token) {}',
        'const mta::game_de::NativePedReferenceToken* TokenAddressForTest() const noexcept {return &ReferenceToken();}')
    $lines+=@($methods|ForEach-Object {$_+' override {std::abort();}'})
    $lines+=@('};','}','#pragma warning(pop)')
    [IO.File]::WriteAllLines($OutputPath,$lines,[Text.UTF8Encoding]::new($false))
    [ordered]@{path=$OutputPath;sha256=(Get-FileHash -LiteralPath $OutputPath -Algorithm SHA256).Hash.ToLowerInvariant();rejectingMethods=$methods.Count;
        inheritedProductionMethods=$provided;historicalCasesExecuted=$false;nativeFactoryClaimed=$false;fullClumpSchemaClaimed=$false}
}
