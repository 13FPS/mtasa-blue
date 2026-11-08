# Generate strict concrete test receivers from the actual SDK. Only the
# inherited timed-model methods and the named game reset are exercised.
function Write-ModelTimeMutationFixture([string]$RepositoryRoot,[string]$OutputPath) {
    $source='Client/sdk/game/CModelInfo.h'
    $text=[IO.File]::ReadAllText((Join-Path $RepositoryRoot $source))
    $text=[regex]::Replace($text,'(?s)/\*.*?\*/|//[^\r\n]*','')
    $body=[regex]::Match($text,'(?s)\bclass\s+CModelInfo\s*\{(?<body>.*)\};')
    if(-not $body.Success){throw 'Actual CModelInfo declaration not found'}
    $inherited=@('GetInterface','GetRwObject','GetModelType','IsVehicle','IsAllocatedInArchive','IsBoat','IsCar','IsPlane','IsHeli','IsTrain','IsBike','IsBmx','IsTrailer','IsMonsterTruck','IsQuadBike','IsFakePlane','GetTime',
        'GetVehicleWheelSize','SetVehicleWheelSize','ResetVehicleWheelSizes','GetObjectPropertiesGroup','SetObjectPropertiesGroup','RestoreObjectPropertiesGroup',
        'GetVehicleExhaustFumesPosition','SetVehicleExhaustFumesPosition','GetVehicleDummyDefaultPosition','GetVehicleDummyPosition','GetVehicleDummyPositions','SetVehicleDummyPosition','ResetVehicleDummies','SetTime')
    $provided=@();$methods=@()
    foreach($match in [regex]::Matches($body.Groups['body'].Value,'virtual\s+(?<signature>[^;{}]+);')) {
        $signature=[regex]::Replace($match.Groups['signature'].Value,'\s*=\s*0\s*$','').Trim()
        $signature=[regex]::Replace($signature,'\s+',' ')
        $name=[regex]::Match($signature,'(?<name>\w+)\s*\(').Groups['name'].Value
        if($name -in $inherited){$provided+=$signature}
        elseif($name -eq 'GetModel'){$methods+=($signature+' override {return 0xfedcba98u;}')}
        else{$methods+=($signature+' override {std::abort();}')}
    }
    if($provided.Count -ne 31 -or $methods.Count -ne 61){throw 'Actual time mutation SDK partition changed'}
    $lines=@('#pragma once','#include <cstdlib>','#pragma warning(push)','#pragma warning(disable : 4100)',
        'class ModelTimeMutationObserver final : public mta::game_de::CModelInfoTimeDE { public:',
        'ModelTimeMutationObserver(std::uint32_t model,mta::game_de::NativeGameTimingAccess& classification,mta::game_de::NativeModelTimeAccess& time) : CModelInfoTimeDE(model,classification,time,classification,classification,classification) {}')
    $lines+=$methods;$lines+=@('};','#pragma warning(pop)')
    . (Join-Path $RepositoryRoot 'Tests/definitive/Write-StartupLifecycleFixture.ps1')
    $parentPath=Join-Path (Split-Path -Parent $OutputPath) 'ModelTimeLifecycleParent.generated.h'
    $parent=Write-StartupLifecycleFixture $RepositoryRoot $parentPath
    $parentText=[IO.File]::ReadAllText($parentPath)
    $marker=$parentText.IndexOf('class AllHooksProbe')
    if($marker -lt 0){throw 'Strict lifecycle prefix boundary missing'}
    $prefix=$parentText.Substring(0,$marker).Replace('namespace lifecycle_fixture {','namespace model_time_lifecycle_fixture {')
    $remove='(?m)^.*\bResetModelTimes\([^\r\n]+\r?\n'
    if([regex]::Matches($prefix,$remove).Count -ne 1){throw 'The named game reset rejector is not unique'}
    $prefix=[regex]::Replace($prefix,$remove,'')
    # The old omission probes target the old abstract lifecycle and are not
    # copied: removing this stub is intentional only for the new real base.
    [IO.File]::WriteAllText($OutputPath,($lines -join "`n")+"`n"+$prefix+"`n}`n",[Text.UTF8Encoding]::new($false))
    [ordered]@{path=$OutputPath;sha256=(Get-FileHash -LiteralPath $OutputPath -Algorithm SHA256).Hash.ToLowerInvariant();
        inheritedProductionMethods=$provided;inheritedModelMethods=31;rejectingModelMethods=60;poisonedModelMethod=1;
        removedNamedGameResetRejector=1;parent=$parent;productionFactoryPublished=$false;historicalTestsExecuted=$false}
}
