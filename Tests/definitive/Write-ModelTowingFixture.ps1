# Strict isolated portable policy fixture: two actual inherited methods,
# five explicit virtual-input spies, and85 source-derived abortors.
function Write-ModelTowingFixture([string]$RepositoryRoot,[string]$OutputPath) {
    $source='Client/sdk/game/CModelInfo.h'
    $text=[IO.File]::ReadAllText((Join-Path $RepositoryRoot $source))
    $text=[regex]::Replace($text,'(?s)/\*.*?\*/|//[^\r\n]*','')
    $body=[regex]::Match($text,'(?s)\bclass\s+CModelInfo\s*\{(?<body>.*)\};')
    if(-not $body.Success){throw 'Actual CModelInfo declaration not found'}
    $provided=@()
    $methods=@(foreach($match in [regex]::Matches($body.Groups['body'].Value,'virtual\s+(?<signature>[^;{}]+);')) {
        $signature=[regex]::Replace($match.Groups['signature'].Value,'\s*=\s*0\s*$','').Trim()
        $name=[regex]::Match($signature,'(?<name>\w+)\s*\(').Groups['name'].Value
        if($name -in @('IsTowableBy','IsBoat','GetModel','IsTrain','IsBike','IsBmx','IsTrailer')) {$provided+=$name}
        else {[regex]::Replace($signature,'\s+',' ')}
    })
    if($provided.Count -ne 7 -or $methods.Count -ne 85){throw 'Actual isolated towing policy7/85 partition changed'}
    $lines=@('#pragma once','#include <cstdlib>','#pragma warning(push)','#pragma warning(disable : 4100)',
        'class ModelTowingFixtureBase : public mta::game_de::CModelInfoDE { public:',
        'ModelTowingFixtureBase(std::uint32_t modelKey,mta::game_de::NativeModelClassificationAccess& owner) noexcept : CModelInfoDE(modelKey,owner) {}')
    $lines+=@($methods|ForEach-Object {$_+' override {std::abort();}'})
    $lines+=@('};','#pragma warning(pop)')
    [IO.File]::WriteAllLines($OutputPath,$lines,[Text.UTF8Encoding]::new($false))
    [ordered]@{path=$OutputPath;sha256=(Get-FileHash -LiteralPath $OutputPath -Algorithm SHA256).Hash.ToLowerInvariant();rejectingMethods=$methods.Count;
        inheritedProductionMethods=@('IsTowableBy','IsBoat');controlledVirtualInputs=@('GetModel','IsTrain','IsBike','IsBmx','IsTrailer');
        source=$source;sourceSha256=(Get-FileHash -LiteralPath (Join-Path $RepositoryRoot $source) -Algorithm SHA256).Hash.ToLowerInvariant();
        productionModelConcrete=$false;nativeFactoryClaimed=$false;sharedSdkEdited=$false;polymorphicBaseDeletionExecuted=$false}
}
