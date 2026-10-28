# Generate strict unsupported methods from the actual92-method SDK.
# The default partition stays unchanged for existing tests. The explicit
# player switch inherits the new real override instead of masking it with
# an aborting stub; neither mode simulates a native model factory.
function Write-ModelObjectFixture([string]$RepositoryRoot,[string]$OutputPath,[switch]$IncludePlayerIdentity) {
    $source='Client/sdk/game/CModelInfo.h'
    $text=[IO.File]::ReadAllText((Join-Path $RepositoryRoot $source))
    $text=[regex]::Replace($text,'(?s)/\*.*?\*/|//[^\r\n]*','')
    $body=[regex]::Match($text,'(?s)\bclass\s+CModelInfo\s*\{(?<body>.*)\};')
    if(-not $body.Success){throw 'Actual CModelInfo declaration not found'}
    $inherited=@('GetInterface','GetRwObject','GetModel','GetModelType','IsVehicle','IsAllocatedInArchive','IsBoat','IsCar','IsPlane','IsHeli')
    if($IncludePlayerIdentity){$inherited+='IsPlayerModel'}
    $provided=@()
    $methods=@(foreach($match in [regex]::Matches($body.Groups['body'].Value,'virtual\s+(?<signature>[^;{}]+);')) {
        $signature=[regex]::Replace($match.Groups['signature'].Value,'\s*=\s*0\s*$','').Trim()
        $name=[regex]::Match($signature,'(?<name>\w+)\s*\(').Groups['name'].Value
        if($name -in $inherited) {$provided+=$signature}
        else {[regex]::Replace($signature,'\s+',' ')}
    })
    $expectedProvided=if($IncludePlayerIdentity){11}else{10}
    $expectedRejected=92-$expectedProvided
    if($provided.Count -ne $expectedProvided -or $methods.Count -ne $expectedRejected){throw 'Actual model fixture partition changed'}
    $lines=@('#pragma once','#include <cstdlib>','#pragma warning(push)','#pragma warning(disable : 4100)',
        'class ModelObjectObserver final : public mta::game_de::CModelInfoDE { public:',
        'ModelObjectObserver(std::uint32_t model,mta::game_de::NativeModelClassificationAccess& owner) noexcept : CModelInfoDE(model,owner) {}')
    $lines+=@($methods|ForEach-Object {$_+' override {std::abort();}'})
    $lines+=@('};','#pragma warning(pop)')
    [IO.File]::WriteAllLines($OutputPath,$lines,[Text.UTF8Encoding]::new($false))
    [ordered]@{path=$OutputPath;sha256=(Get-FileHash -LiteralPath $OutputPath -Algorithm SHA256).Hash.ToLowerInvariant();rejectingMethods=$methods.Count;
        inheritedProductionMethods=$provided;source=$source;sourceSha256=(Get-FileHash -LiteralPath (Join-Path $RepositoryRoot $source) -Algorithm SHA256).Hash.ToLowerInvariant();
        productionModelConcrete=$false;nativeFactoryClaimed=$false;sharedSdkEdited=$false;polymorphicBaseDeletionExecuted=$false}
}
