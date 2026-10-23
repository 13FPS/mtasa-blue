# Rejecting test-only implementations keep every unqualified SDK operation
# explicit while both the SDK and actual owner fixture call production pose.
function Write-PedWaterFixture([string]$taskRoot,[string]$OutputPath) {
function InterfaceMethods([string]$RelativePath) {
    $text = [IO.File]::ReadAllText((Join-Path $taskRoot $RelativePath))
    $text = [regex]::Replace($text,'(?s)/\*.*?\*/|//[^\r\n]*','')
    foreach ($match in [regex]::Matches($text,'virtual\s+(?<signature>[^;{}]+);')) {
        $signature = [regex]::Replace($match.Groups['signature'].Value,'\s*=\s*0\s*$','').Trim()
        $name = [regex]::Match($signature,'(?<name>\w+)\s*\(').Groups['name'].Value
        if (-not $name) { throw "Cannot derive rejecting fixture method from $signature" }
        # Defaults are caller syntax, not part of an overriding signature.
        $signature = [regex]::Replace($signature,'\s*=\s*[^,)]+','')
        [ordered]@{name=$name;signature=([regex]::Replace($signature,'\s+',' '))}
    }
}

    $provided = @('GetHealth','SetHealth','GetArmor','SetArmor','GetModelIndex','GetEntityType','SetMoveSpeed','SetTurnSpeed','GetStoredPointer','SetStoredPointer','GetPosition','GetMatrix','GetMoveSpeed','GetTurnSpeed','GetMass','SetMass','GetTurnMass','SetTurnMass','GetAirResistance','SetAirResistance','GetElasticity','SetElasticity','GetCenterOfMass','SetCenterOfMass','GetCurrentRotation','SetCurrentRotation','GetOxygenLevel','SetOxygenLevel','IsDucking','GetType','GetCustomMoveAnim','GetCantBeKnockedOffBike','SetCantBeKnockedOffBike','GetCanBeShotInVehicle','SetCanBeShotInVehicle','IsBleeding','SetBleeding','GetStayInSamePlace','SetStayInSamePlace','GetFightingStyle','SetFightingStyle','GetCurrentWeaponSlot','SetPosition','Teleport','GetCurrentWeaponStat','GetCurrentWeaponRange','SetInWaterFlags','IsInWater')
    $declaration = [IO.File]::ReadAllText((Join-Path $taskRoot 'Client/game_de/CPedVitalsDE.h')) + [IO.File]::ReadAllText((Join-Path $taskRoot 'Client/game_de/CPedMetadataDE.h')) + [IO.File]::ReadAllText((Join-Path $taskRoot 'Client/game_de/CPedMutationDE.h')) + [IO.File]::ReadAllText((Join-Path $taskRoot 'Client/game_de/CPedPhysicsDE.h')) + [IO.File]::ReadAllText((Join-Path $taskRoot 'Client/game_de/CPedRotationDE.h')) + [IO.File]::ReadAllText((Join-Path $taskRoot 'Client/game_de/CPedStateDE.h')) + [IO.File]::ReadAllText((Join-Path $taskRoot 'Client/game_de/CPedFlagsDE.h')) + [IO.File]::ReadAllText((Join-Path $taskRoot 'Client/game_de/CPedBehaviorDE.h')) + [IO.File]::ReadAllText((Join-Path $taskRoot 'Client/game_de/CPedCombatDE.h')) + [IO.File]::ReadAllText((Join-Path $taskRoot 'Client/game_de/CPedWeaponSelectionDE.h')) + [IO.File]::ReadAllText((Join-Path $taskRoot 'Client/game_de/CPedPoseDE.h')) + [IO.File]::ReadAllText((Join-Path $taskRoot 'Client/game_de/CPedWeaponStatsDE.h')) + [IO.File]::ReadAllText((Join-Path $taskRoot 'Client/game_de/CPedWaterDE.h'))
    foreach ($method in $provided) {
        if ($declaration -notmatch ('\b'+$method+'\s*\([^;{}]*\)\s*(?:const\s*)?(?:noexcept\s*)?override\s*;')) {
            throw "Production CPed SDK override is missing: $method"
        }
    }
    $allMethods = @(foreach ($header in @('Client/sdk/game/CEntity.h','Client/sdk/game/CPhysical.h','Client/sdk/game/CPed.h')) {
        InterfaceMethods $header | Where-Object { $provided -notcontains $_.name }
    })
    # CPed repeats some inherited pure declarations. Emit one matching override
    # per signature while retaining every overload and const/noexcept qualifier.
    $seen = @{}
    $pedMethods = @($allMethods | Where-Object {
        $key = [regex]::Replace($_.signature,'\s+','')
        if ($seen.ContainsKey($key)) { $false } else { $seen[$key] = $true; $true }
    })
    $generated = $OutputPath
    $lines = @('#pragma once','#pragma warning(push)','#pragma warning(disable : 4100)','namespace ped_water_fixture {',
        ('inline constexpr unsigned RejectingMethods = {0};' -f $pedMethods.Count),
        'class Game final : public CGame { public:')
    # A rejected virtual must identify itself in the owned child's log; an
    # anonymous abort cannot distinguish an accidental SDK call from a fault.
    $lines += @(InterfaceMethods 'Client/sdk/game/CGame.h' | ForEach-Object { $_.signature + ' override { Unqualified("Game::' + $_.name + '"); }' })
    $lines += @('};','class PedWaterFixture final : public mta::game_de::CPedWaterDE { public:',
        'PedWaterFixture(CGame& game,mta::game_de::NativePedVitalsAccess& vitals,mta::game_de::NativePedMetadataAccess& metadata,mta::game_de::NativePedMutationAccess& mutation,mta::game_de::NativePedObservationAccess& observation,mta::game_de::NativePedRotationAccess& rotation,mta::game_de::NativePedOxygenAccess& oxygen,mta::game_de::NativePedDuckingAccess& ducking,mta::game_de::NativePedBikeFlagsAccess& bike,mta::game_de::NativePedShotFlagsAccess& shot,mta::game_de::NativePedBleedingAccess& bleeding,mta::game_de::NativePedStayAccess& stay,mta::game_de::NativePedFightingStyleAccess& combat,mta::game_de::NativePedCurrentWeaponSlotAccess& selection,mta::game_de::NativePedPlacementAccess& placement,mta::game_de::NativePedWaterFlagsAccess& water,mta::game_de::NativePedWaterTaskAccess& waterTask,const mta::game_de::NativePedReferenceToken& token,std::uint32_t wrapperType) noexcept : CPedWaterDE(game,vitals,metadata,mutation,observation,rotation,oxygen,ducking,bike,shot,bleeding,stay,combat,selection,placement,water,waterTask,token,wrapperType) {}',
        'CMatrix repairRequested; unsigned repairCalls=0; std::function<void(const CMatrix&)> repairCallback;',
        'mta::game_de::ClockResult RepairLastGoodPhysicsState(const CMatrix& matrix) noexcept override {repairRequested=matrix;++repairCalls;if(repairCallback)repairCallback(matrix);return mta::game_de::ClockResult::Ok;}')
    $lines += @($pedMethods | ForEach-Object { $_.signature + ' override { Unqualified("Ped::' + $_.name + '"); }' })
    $lines += @('};','}','#pragma warning(pop)')
    $lines | Set-Content -LiteralPath $generated -Encoding utf8NoBOM
    [ordered]@{path=$OutputPath;sha256=(Get-FileHash -LiteralPath $OutputPath -Algorithm SHA256).Hash.ToLowerInvariant();
        rejectingOverrides=$pedMethods.Count;interfaces=@('CPed','CPhysical','CEntity');productionMethodsNotOverridden=$provided;
        testedMethods=@('SetInWaterFlags(bool)','IsInWater() const');gameReceiver='Complete rejecting test-only CGame; unused by this storage operation, not a production factory'}
}
