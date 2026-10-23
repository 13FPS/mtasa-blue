# Generate only test rejectors around the actual inherited ClearWeapon method.
# Balanced defaults preserve every real CPed/CPhysical/CEntity overload and the
# whole GiveWeapon ancestry; no fake native ped, weapon bank or factory appears.
function Write-PedClearWeaponFixture([string]$taskRoot,[string]$OutputPath,[string]$ClearHeaderPath='') {
    if(-not $ClearHeaderPath){$ClearHeaderPath=Join-Path $taskRoot 'Client/game_de/CPedClearWeaponDE.h'}
function InterfaceMethods([string]$RelativePath) {
    $text = [IO.File]::ReadAllText((Join-Path $taskRoot $RelativePath))
    $text = [regex]::Replace($text,'(?s)/\*.*?\*/|//[^\r\n]*','')
    # Braced defaults are parameter syntax, not inline method bodies. Keep
    # the complete declaration, then remove defaults per balanced parameter.
    foreach ($match in [regex]::Matches($text,'(?m)^\s*virtual\s+(?<signature>[^;]+);')) {
        $signature = [regex]::Replace($match.Groups['signature'].Value,'\s*=\s*0\s*$','').Trim()
        $begin = $signature.IndexOf('('); $end = $signature.LastIndexOf(')')
        if ($begin -lt 0 -or $end -lt $begin) { throw "Unsupported SDK signature: $signature" }
        if ($signature.Substring($end+1) -match '\{') { continue }
        $name = [regex]::Match($signature,'(?<name>\w+)\s*\(').Groups['name'].Value
        if (-not $name) { throw "Cannot derive rejecting fixture method from $signature" }
        $parameters = $signature.Substring($begin+1,$end-$begin-1)
        $parts=@(); $part=''; $depth=0
        foreach ($character in $parameters.ToCharArray()) {
            if ($character -eq ',' -and $depth -eq 0) { $parts+=$part; $part=''; continue }
            $part+=$character
            if ($character -in @('<','(','[','{')) { $depth++ }
            elseif ($character -in @('>',')',']','}')) { $depth-- }
        }
        if ($part.Trim()) { $parts+=$part }
        $clean=@($parts | ForEach-Object { [regex]::Replace($_,'\s*=.*$','').Trim() })
        $signature=$signature.Substring(0,$begin+1)+($clean -join ', ')+$signature.Substring($end)
        [ordered]@{name=$name;signature=([regex]::Replace($signature,'\s+',' '))}
    }
}


    function RejectBody([string]$signature,[string]$label) {
        $begin=$signature.IndexOf('(');$end=$signature.LastIndexOf(')')
        $parameters=$signature.Substring($begin+1,$end-$begin-1)
        $parts=@();$part='';$depth=0
        foreach($character in $parameters.ToCharArray()) {
            if($character -eq ',' -and $depth -eq 0){$parts+=$part;$part='';continue}
            $part+=$character
            if($character -in @('<','(','[')){$depth++}
            elseif($character -in @('>',')',']')){$depth--}
        }
        $parts+=$part
        $discard=@(foreach($parameter in $parts) {
            $trimmed=$parameter.Trim()
            $match=[regex]::Match($trimmed,'(?<name>[A-Za-z_]\w*)\s*(?:\[[^\]]*\]\s*)*$')
            if($match.Success) {
                $name=$match.Groups['name'].Value
                $prefix=$trimmed.Substring(0,$match.Index).Trim()
                if($prefix -and $prefix -notmatch '^(?:(?:const|volatile|struct|class|enum)\s*)+$' -and
                    $name -notin @('void','bool','char','short','int','long','float','double','unsigned','signed','DWORD','WORD','BYTE','uint32_t','int32_t','uint64_t','size_t')) {
                    '(void)'+$name+';'
                }
            } else {
                $functionPointer=[regex]::Match($trimmed,'\(\s*\*\s*(?<name>\w+)\s*\)')
                if($functionPointer.Success){'(void)'+$functionPointer.Groups['name'].Value+';'}
            }
        })
        ' override {'+($discard -join '')+'Unqualified("'+$label+'");}'
    }

    $provided = @('GetHealth','SetHealth','GetArmor','SetArmor','GetModelIndex','GetEntityType','SetMoveSpeed','SetTurnSpeed','GetStoredPointer','SetStoredPointer','GetPosition','GetMatrix','GetMoveSpeed','GetTurnSpeed','GetMass','SetMass','GetTurnMass','SetTurnMass','GetAirResistance','SetAirResistance','GetElasticity','SetElasticity','GetCenterOfMass','SetCenterOfMass','GetCurrentRotation','SetCurrentRotation','GetOxygenLevel','SetOxygenLevel','IsDucking','GetType','GetCustomMoveAnim','GetCantBeKnockedOffBike','SetCantBeKnockedOffBike','GetCanBeShotInVehicle','SetCanBeShotInVehicle','IsBleeding','SetBleeding','GetStayInSamePlace','SetStayInSamePlace','GetFightingStyle','SetFightingStyle','GetCurrentWeaponSlot','SetPosition','Teleport','GetCurrentWeaponStat','GetCurrentWeaponRange','SetInWaterFlags','IsInWater','SetOrientation','GetAreaCode','SetAreaCode','IsOnFire','SetUsesCollision','SetDucking','IsDoingGangDriveby','IsWearingGoggles','IsVisible','SetVisible','IsOnScreen','GiveWeapon','ClearWeapon')
    $headers=@('CPedVitalsDE','CPedMetadataDE','CPedMutationDE','CPedPhysicsDE','CPedRotationDE','CPedStateDE','CPedFlagsDE','CPedBehaviorDE','CPedCombatDE','CPedWeaponSelectionDE','CPedPoseDE','CPedWeaponStatsDE','CPedWaterDE','CPedOrientationDE','CPedAreaDE','CPedFireDE','CPedCollisionDE','CPedTasksDE','CPedGogglesDE','CPedVisibilityDE','CPedOnScreenDE','CPedGiveWeaponDE')
    $declaration=(@($headers|ForEach-Object {[IO.File]::ReadAllText((Join-Path $taskRoot ('Client/game_de/'+$_+'.h')))}) -join "`n")+[IO.File]::ReadAllText($ClearHeaderPath)
    foreach($method in $provided){
        if($declaration -notmatch ('\b'+$method+'\s*\([^;{}]*\)\s*(?:const\s*)?(?:noexcept\s*)?override\s*;')){throw "Actual inherited SDK override missing: $method"}
    }
    $seen=@{}
    $methods=@(foreach($header in @('Client/sdk/game/CEntity.h','Client/sdk/game/CPhysical.h','Client/sdk/game/CPed.h')){
        InterfaceMethods $header|Where-Object {
            $key=[regex]::Replace($_.signature,'\s+','')
            if($_.name -in $provided -or $seen.ContainsKey($key)){$false}else{$seen[$key]=$true;$true}
        }
    })
    $game=@(InterfaceMethods 'Client/sdk/game/CGame.h')
    $lines=@('#pragma once','#include <array>','#include <cstddef>','namespace ped_clear_weapon_fixture {',
        'struct TaskScenario { bool available=false; CPedIntelligence* intelligence=nullptr; std::array<int,16> events{}; std::size_t eventCount=0; void Record(int event) noexcept {if(eventCount==events.size())std::abort();events[eventCount++]=event;} void ResetEvents() noexcept {eventCount=0;} };',
        'struct WeaponLookup {std::array<CWeapon*,13> bank{};std::array<std::uint32_t,64> slots{};std::size_t count=0;void Record(std::uint32_t slot) noexcept {if(count==slots.size())std::abort();slots[count++]=slot;}};',
        'class GameBase : public CGame { public:')
    $lines+=@($game|ForEach-Object {$_.signature+(RejectBody $_.signature ('Game::'+$_.name))})
    $lines+=@('};','using Game=GameBase;','class PedClearWeaponFixture final : public mta::game_de::CPedClearWeaponDE { public:',
        'PedClearWeaponFixture(CGame& game,mta::game_de::NativePedVitalsAccess& vitals,mta::game_de::NativePedMetadataAccess& metadata,mta::game_de::NativePedMutationAccess& mutation,mta::game_de::NativePedObservationAccess& observation,mta::game_de::NativePedRotationAccess& rotation,mta::game_de::NativePedOxygenAccess& oxygen,mta::game_de::NativePedDuckingAccess& ducking,mta::game_de::NativePedBikeFlagsAccess& bike,mta::game_de::NativePedShotFlagsAccess& shot,mta::game_de::NativePedBleedingAccess& bleeding,mta::game_de::NativePedStayAccess& stay,mta::game_de::NativePedFightingStyleAccess& combat,mta::game_de::NativePedCurrentWeaponSlotAccess& selection,mta::game_de::NativePedPlacementAccess& placement,mta::game_de::NativePedWaterFlagsAccess& water,mta::game_de::NativePedWaterTaskAccess& waterTask,mta::game_de::NativePedOrientationAccess& orientation,mta::game_de::NativePedAreaAccess& area,mta::game_de::NativePedFireAccess& fire,mta::game_de::NativePedCollisionAccess& collision,mta::game_de::NativePedGogglesAccess& goggles,mta::game_de::NativePedVisibilityAccess& visibility,mta::game_de::NativePedOnScreenAccess& onScreen,mta::game_de::NativePedWeaponGrantAccess& grant,mta::game_de::NativePedWeaponClearAccess& clear,const mta::game_de::NativePedReferenceToken& token,std::uint32_t wrapperType,TaskScenario& scenario,WeaponLookup& lookup) noexcept : CPedClearWeaponDE(game,vitals,metadata,mutation,observation,rotation,oxygen,ducking,bike,shot,bleeding,stay,combat,selection,placement,water,waterTask,orientation,area,fire,collision,goggles,visibility,onScreen,grant,clear,token,wrapperType) {(void)scenario;(void)lookup;}',
        'const mta::game_de::NativePedReferenceToken* FixtureTokenAddress() const noexcept {return &ReferenceToken();}',
        'mta::game_de::ClockResult RepairLastGoodPhysicsState(const CMatrix& matrix) noexcept override {(void)matrix;Unqualified("Ped::RepairLastGoodPhysicsState");}')
    $lines+=@($methods|ForEach-Object {$_.signature+(RejectBody $_.signature ('Ped::'+$_.name))})
    $lines+=@('protected:bool HasPedIntelligence() const noexcept override {Unqualified("Ped::HasPedIntelligence");}','};','}')
    $lines|Set-Content -LiteralPath $OutputPath -Encoding utf8NoBOM
    [ordered]@{path=$OutputPath;sha256=(Get-FileHash -LiteralPath $OutputPath -Algorithm SHA256).Hash.ToLowerInvariant();
        rejectingPedOverrides=$methods.Count;rejectingGameOverrides=$game.Count;testedMethod='ClearWeapon';
        inheritedProductionMethods=$provided;actualInterfaces=@('CPed','CPhysical','CEntity','CGame');
        bothWeaponLookupsRemainRejectors=$true;productionFactoriesPublished=$false}
}
