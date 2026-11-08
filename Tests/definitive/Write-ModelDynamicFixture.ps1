# Preserve the actual model base and its new native predicate. Unrelated pure
# SDK operations abort so accidental generated overrides cannot hide dispatch.
function Write-ModelDynamicFixture([string]$taskRoot,[string]$OutputPath) {
    $text=[IO.File]::ReadAllText((Join-Path $taskRoot 'Client/sdk/game/CModelInfo.h'))
    $text=[regex]::Replace($text,'(?s)/\*.*?\*/|//[^\r\n]*','')
    $inherited=@('GetInterface','GetRwObject','GetModel','GetModelType','IsPlayerModel','IsVehicle','GetVehicleType','IsAllocatedInArchive',
        'IsBoat','IsCar','IsPlane','IsHeli','IsTrain','IsBike','IsBmx','IsTrailer','IsMonsterTruck','IsQuadBike','IsFakePlane','IsTowableBy','GetTime','IsDynamic')
    $provided=@();$rejectors=@()
    foreach($match in [regex]::Matches($text,'virtual\s+(?<signature>[^;{}]+);')) {
        $signature=[regex]::Replace($match.Groups['signature'].Value,'\s*=\s*0\s*$','').Trim()
        $signature=[regex]::Replace($signature,'\s*=\s*[^,)]+','')
        $signature=[regex]::Replace($signature,'\s+',' ')
        $name=[regex]::Match($signature,'(?<name>\w+)\s*\(').Groups['name'].Value
        if(-not $name){throw 'Unknown actual model SDK signature'}
        if($name -in $inherited){$provided+=$name}else{$rejectors+=($signature+' override {std::abort();}')}
    }
    if($provided.Count -ne 22 -or $rejectors.Count -ne 70 -or @($inherited|Where-Object{$_ -notin $provided}).Count){throw 'Actual dynamic model SDK partition changed'}
    $lines=@('#pragma once','#include <cstdlib>','#pragma warning(push)','#pragma warning(disable : 4100)',
        'class ModelDynamicObserver final : public mta::game_de::CModelInfoDE { public:',
        'ModelDynamicObserver(std::uint32_t model,mta::game_de::NativeModelClassificationAccess& owner) noexcept : CModelInfoDE(model,owner) {}')
    $lines+=$rejectors
    $lines+=@('};','static_assert(!std::is_abstract_v<ModelDynamicObserver>);',
        'static_assert(std::is_same_v<decltype(&ModelDynamicObserver::IsDynamic),decltype(&mta::game_de::CModelInfoDE::IsDynamic)>);','#pragma warning(pop)')
    [IO.File]::WriteAllText($OutputPath,($lines -join "`n")+"`n",[Text.UTF8Encoding]::new($false))
    [ordered]@{path=$OutputPath;sha256=(Get-FileHash -LiteralPath $OutputPath -Algorithm SHA256).Hash.ToLowerInvariant();
        inheritedModelMethods=22;rejectingModelMethods=70;inheritedNames=$provided;productionFactoryPublished=$false}
}
