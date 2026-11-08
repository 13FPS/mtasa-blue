# Create strict receivers from the real SDK; every implemented model method is
# inherited from the new composition so a stub cannot hide a broken base chain.
function Write-ModelStartupCompositionFixture([string]$taskRoot,[string]$OutputPath) {
    function Methods([string]$relative) {
        $text=[IO.File]::ReadAllText((Join-Path $taskRoot $relative))
        $text=[regex]::Replace($text,'(?s)/\*.*?\*/|//[^\r\n]*','')
        foreach($match in [regex]::Matches($text,'virtual\s+(?<signature>[^;{}]+);')) {
            $signature=[regex]::Replace($match.Groups['signature'].Value,'\s*=\s*0\s*$','').Trim()
            $signature=[regex]::Replace($signature,'\s*=\s*[^,)]+','')
            $signature=[regex]::Replace($signature,'\s+',' ')
            $name=[regex]::Match($signature,'(?<name>\w+)\s*\(').Groups['name'].Value
            if(-not $name){throw "Unrecognized actual SDK method: $signature"}
            [ordered]@{name=$name;signature=$signature}
        }
    }
    $modelProvided=@('GetInterface','GetRwObject','GetModel','GetModelType','IsPlayerModel','IsVehicle','GetVehicleType','IsAllocatedInArchive',
        'IsBoat','IsCar','IsPlane','IsHeli','IsTrain','IsBike','IsBmx','IsTrailer','IsMonsterTruck','IsQuadBike','IsFakePlane','IsTowableBy','GetTime',
        'Request','IsLoaded','ModelAddRef','RemoveRef','GetRefCount','IsValid','MakePedModel',
        'SetCustomModel','RestoreOriginalModel','SetColModel','RestoreColModel','MakeCustomModel','UnloadUnused',
        'GetVehicleWheelSize','SetVehicleWheelSize','ResetVehicleWheelSizes','GetObjectPropertiesGroup','SetObjectPropertiesGroup','RestoreObjectPropertiesGroup',
        'GetVehicleExhaustFumesPosition','SetVehicleExhaustFumesPosition','GetVehicleDummyDefaultPosition','GetVehicleDummyPosition','GetVehicleDummyPositions',
        'SetVehicleDummyPosition','ResetVehicleDummies','SetTime','GetLODDistance','GetOriginalLODDistance','SetLODDistance')
    $streamingProvided=@('RequestSpecialModel','LoadSceneCollision','GetModelStreamingFlags','RequestModel','RemoveModel','HasModelLoaded',
        'LoadAllRequestedModels','LoadScene','GetNumPedsLoaded','IsModelInLoadedPedGroup','GetNumLoadedVehicles','IsModelInLoadedVehicleGroup')
    $modelAll=@(Methods 'Client/sdk/game/CModelInfo.h')
    $modelInherited=@($modelAll|Where-Object{$_.name -in $modelProvided})
    $modelMethods=@($modelAll|Where-Object{$_.name -notin $modelProvided})
    if($modelProvided.Count -ne 51 -or $modelInherited.Count -ne 51 -or $modelMethods.Count -ne 41 -or
        @($modelProvided|Where-Object{$_ -notin $modelInherited.name}).Count){throw 'Actual startup model SDK partition changed; expected51 inherited/41 rejecting'}
    $streamingAll=@(Methods 'Client/sdk/game/CStreaming.h')
    $streamingMethods=@($streamingAll|Where-Object{$_.name -notin $streamingProvided})
    $gameMethods=@(Methods 'Client/sdk/game/CGame.h'|Where-Object{$_.name -notin @('GetStreaming','GetBaseIDforTXD','GetBaseIDforIFP','GetModelInfo','IsASyncLoadingEnabled')})
    $coreMethods=@(Methods 'Client/sdk/core/CCoreInterface.h'|Where-Object{$_.name -ne 'LogEvent'})
    $lines=@('#pragma once','#pragma warning(push)','#pragma warning(disable : 4100)','namespace model_startup_fixture {',
        'class CoreFixtureBase : public CCoreInterface { public:')
    $lines+=@($coreMethods|ForEach-Object{$_.signature+' override {Unqualified();}'})
    $lines+=@('};','class GameFixtureBase : public CGame { public:')
    $lines+=@($gameMethods|ForEach-Object{$_.signature+' override {Unqualified();}'})
    $lines+=@('};','class ModelFixtureBase : public mta::game_de::CModelInfoStartupDE { protected:',
        'using CModelInfoStartupDE::CModelInfoStartupDE;','public:')
    $lines+=@($modelMethods|ForEach-Object{$_.signature+' override {Unqualified();}'})
    $lines+=@('};','class StreamingFixtureBase : public mta::game_de::CStreamingDE { public:',
        'explicit StreamingFixtureBase(mta::game_de::NativeStreamingModelAccess& owner) noexcept : CStreamingDE(owner) {}')
    $lines+=@($streamingMethods|ForEach-Object{$_.signature+' override {Unqualified();}'})
    $lines+=@('};','}','#pragma warning(pop)')
    [IO.File]::WriteAllText($OutputPath,($lines -join "`n")+"`n",[Text.UTF8Encoding]::new($false))
    [ordered]@{path=$OutputPath;sha256=(Get-FileHash -LiteralPath $OutputPath -Algorithm SHA256).Hash.ToLowerInvariant();
        inheritedModelMethods=$modelInherited.Count;rejectingModelMethods=$modelMethods.Count;modelInterfaceMethods=$modelAll.Count;
        inheritedStreamingMethods=$streamingProvided.Count;rejectingStreamingMethods=$streamingMethods.Count;streamingInterfaceMethods=$streamingAll.Count;
        rejectingGameMethods=$gameMethods.Count;rejectingCoreMethods=$coreMethods.Count;modelInheritedNames=$modelProvided;
        productionFactoryPublished=$false;reportingMethodsMasked=$false;requestReferenceMethodsMasked=$false}
}
