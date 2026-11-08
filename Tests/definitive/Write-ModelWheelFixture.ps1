# Generate rejecting stubs from the real SDK. Only the three wheel overrides
# execute production behavior; GetModel is deliberately poisoned to prove that
# a virtual override cannot retarget their immutable CModelInfoDE key.
function Write-ModelWheelFixture([string]$RepositoryRoot,[string]$OutputPath) {
    $source='Client/sdk/game/CModelInfo.h'
    $text=[IO.File]::ReadAllText((Join-Path $RepositoryRoot $source))
    $text=[regex]::Replace($text,'(?s)/\*.*?\*/|//[^\r\n]*','')
    $body=[regex]::Match($text,'(?s)\bclass\s+CModelInfo\s*\{(?<body>.*)\};')
    if(-not $body.Success){throw 'Actual CModelInfo declaration not found'}
    $inherited=@('GetVehicleWheelSize','SetVehicleWheelSize','ResetVehicleWheelSizes')
    $provided=@();$methods=@()
    foreach($match in [regex]::Matches($body.Groups['body'].Value,'virtual\s+(?<signature>[^;{}]+);')) {
        $signature=[regex]::Replace($match.Groups['signature'].Value,'\s*=\s*0\s*$','').Trim()
        $signature=[regex]::Replace($signature,'\s+',' ')
        $name=[regex]::Match($signature,'(?<name>\w+)\s*\(').Groups['name'].Value
        if($name -in $inherited){$provided+=$signature}
        elseif($name -eq 'GetModel'){$methods+=($signature+' override {return reportedModel;}')}
        else{$methods+=($signature+' override {std::abort();}')}
    }
    if($provided.Count -ne 3 -or $methods.Count -ne 89){throw 'Actual model wheel fixture partition changed'}
    $lines=@('#pragma once','#include <cstdlib>','#pragma warning(push)','#pragma warning(disable : 4100)',
        'class ModelWheelObserver final : public mta::game_de::CModelInfoWheelDE { public:',
        'std::uint32_t reportedModel = 0xfedcba98;',
        'ModelWheelObserver(std::uint32_t model,mta::game_de::NativeModelClassificationAccess& classification,mta::game_de::NativeModelWheelAccess& wheel) : CModelInfoWheelDE(model,classification,wheel) {}')
    $lines+=$methods;$lines+=@('};','#pragma warning(pop)')
    [IO.File]::WriteAllLines($OutputPath,$lines,[Text.UTF8Encoding]::new($false))
    [ordered]@{path=$OutputPath;sha256=(Get-FileHash -LiteralPath $OutputPath).Hash.ToLowerInvariant();rejectingMethods=88;poisonedModelMethod=1;
        inheritedProductionMethods=$provided;source=$source;sourceSha256=(Get-FileHash -LiteralPath (Join-Path $RepositoryRoot $source)).Hash.ToLowerInvariant();
        productionModelConcrete=$false;nativeFactoryClaimed=$false}
}
