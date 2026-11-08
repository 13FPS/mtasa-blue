# Generate rejecting stubs from the real SDK. Only the thirteen wheel/group/dummy overrides
# execute production behavior; GetModel is deliberately poisoned to prove that
# a virtual override cannot retarget their immutable CModelInfoDE key.
function Write-ModelDummiesFixture([string]$RepositoryRoot,[string]$OutputPath) {
    $source='Client/sdk/game/CModelInfo.h'
    $text=[IO.File]::ReadAllText((Join-Path $RepositoryRoot $source))
    $text=[regex]::Replace($text,'(?s)/\*.*?\*/|//[^\r\n]*','')
    $body=[regex]::Match($text,'(?s)\bclass\s+CModelInfo\s*\{(?<body>.*)\};')
    if(-not $body.Success){throw 'Actual CModelInfo declaration not found'}
    $inherited=@('GetVehicleWheelSize','SetVehicleWheelSize','ResetVehicleWheelSizes','GetObjectPropertiesGroup','SetObjectPropertiesGroup','RestoreObjectPropertiesGroup','GetVehicleExhaustFumesPosition','SetVehicleExhaustFumesPosition','GetVehicleDummyDefaultPosition','GetVehicleDummyPosition','GetVehicleDummyPositions','SetVehicleDummyPosition','ResetVehicleDummies')
    $provided=@();$methods=@()
    foreach($match in [regex]::Matches($body.Groups['body'].Value,'virtual\s+(?<signature>[^;{}]+);')) {
        $signature=[regex]::Replace($match.Groups['signature'].Value,'\s*=\s*0\s*$','').Trim()
        $signature=[regex]::Replace($signature,'\s+',' ')
        $name=[regex]::Match($signature,'(?<name>\w+)\s*\(').Groups['name'].Value
        if($name -in $inherited){$provided+=$signature}
        elseif($name -eq 'GetModel'){$methods+=($signature+' override {return reportedModel;}')}
        elseif($name -eq 'IsVehicle'){$methods+=($signature+' override {return vehicle;}')}
        elseif($name -eq 'IsLoaded'){$methods+=($signature+' override {effects.emplace_back("loaded");return loaded;}')}
        elseif($name -eq 'Request'){$methods+=($signature+' override {if(requestType!=BLOCKING)std::abort();effects.emplace_back(std::string("request:")+szTag);loaded=true;}')}
        elseif($name -eq 'ModelAddRef'){$methods+=($signature+' override {if(requestType!=BLOCKING)std::abort();effects.emplace_back(std::string("add:")+szTag);++references;loaded=true;}')}
        elseif($name -eq 'RemoveRef'){$methods+=($signature+' override {effects.emplace_back("remove");--references;}')}
        else{$methods+=($signature+' override {std::abort();}')}
    }
    if($provided.Count -ne 13 -or $methods.Count -ne 79){throw 'Actual model dummies fixture partition changed'}
    $lines=@('#pragma once','#include <cstdlib>','#include <vector>','#include <string>','#pragma warning(push)','#pragma warning(disable : 4100)',
        'class ModelDummiesObserver final : public mta::game_de::CModelInfoDummiesDE { public:',
        'std::uint32_t reportedModel = 0xfedcba98; bool vehicle=true, loaded=true; unsigned references=0; std::vector<std::string> effects;',
        'ModelDummiesObserver(std::uint32_t model,mta::game_de::NativeModelClassificationAccess& classification,mta::game_de::NativeModelWheelAccess& wheel,mta::game_de::NativeModelObjectGroupAccess& groups,mta::game_de::NativeModelDummiesAccess& dummies) : CModelInfoDummiesDE(model,classification,wheel,groups,dummies) {}')
    $lines+=$methods;$lines+=@('};','#pragma warning(pop)')
    [IO.File]::WriteAllLines($OutputPath,$lines,[Text.UTF8Encoding]::new($false))
    [ordered]@{path=$OutputPath;sha256=(Get-FileHash -LiteralPath $OutputPath).Hash.ToLowerInvariant();rejectingMethods=73;poisonedModelMethod=1;sourcePolicyDoubles=5;
        inheritedProductionMethods=$provided;source=$source;sourceSha256=(Get-FileHash -LiteralPath (Join-Path $RepositoryRoot $source)).Hash.ToLowerInvariant();
        productionModelConcrete=$false;nativeFactoryClaimed=$false}
}
