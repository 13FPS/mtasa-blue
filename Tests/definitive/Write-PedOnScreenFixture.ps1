# Generate a rejecting test-only receiver around the real on-screen SDK layer.
# Inherited task availability/getter hooks remain distinct and observable, so
# this property cannot silently create intelligence or query tasks for identity.
function Write-PedOnScreenFixture([string]$taskRoot,[string]$OutputPath,[string]$AreaHeaderPath='',[string]$FireHeaderPath='',[string]$VitalsHeaderPath='',[string]$CollisionHeaderPath='',[string]$TasksHeaderPath='',[string]$StateHeaderPath='',[string]$GogglesHeaderPath='',[string]$VisibilityHeaderPath='',[string]$OnScreenHeaderPath='') {
    if(-not $OnScreenHeaderPath){$OnScreenHeaderPath=Join-Path $taskRoot 'Client/game_de/CPedOnScreenDE.h'}
    if(-not $GogglesHeaderPath){$GogglesHeaderPath=Join-Path $taskRoot 'Client/game_de/CPedGogglesDE.h'}
    if(-not $VisibilityHeaderPath){$VisibilityHeaderPath=Join-Path $taskRoot 'Client/game_de/CPedVisibilityDE.h'}
    if(-not $TasksHeaderPath){$TasksHeaderPath=Join-Path $taskRoot 'Client/game_de/CPedTasksDE.h'}
    if(-not $StateHeaderPath){$StateHeaderPath=Join-Path $taskRoot 'Client/game_de/CPedStateDE.h'}
    if(-not $VitalsHeaderPath){$VitalsHeaderPath=Join-Path $taskRoot 'Client/game_de/CPedVitalsDE.h'}
    if(-not $FireHeaderPath){$FireHeaderPath=Join-Path $taskRoot 'Client/game_de/CPedFireDE.h'}
    if(-not $CollisionHeaderPath){$CollisionHeaderPath=Join-Path $taskRoot 'Client/game_de/CPedCollisionDE.h'}
    if(-not $AreaHeaderPath){$AreaHeaderPath=Join-Path $taskRoot 'Client/game_de/CPedAreaDE.h'}
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

    $provided = @('GetHealth','SetHealth','GetArmor','SetArmor','GetModelIndex','GetEntityType','SetMoveSpeed','SetTurnSpeed','GetStoredPointer','SetStoredPointer','GetPosition','GetMatrix','GetMoveSpeed','GetTurnSpeed','GetMass','SetMass','GetTurnMass','SetTurnMass','GetAirResistance','SetAirResistance','GetElasticity','SetElasticity','GetCenterOfMass','SetCenterOfMass','GetCurrentRotation','SetCurrentRotation','GetOxygenLevel','SetOxygenLevel','IsDucking','GetType','GetCustomMoveAnim','GetCantBeKnockedOffBike','SetCantBeKnockedOffBike','GetCanBeShotInVehicle','SetCanBeShotInVehicle','IsBleeding','SetBleeding','GetStayInSamePlace','SetStayInSamePlace','GetFightingStyle','SetFightingStyle','GetCurrentWeaponSlot','SetPosition','Teleport','GetCurrentWeaponStat','GetCurrentWeaponRange','SetInWaterFlags','IsInWater','SetOrientation','GetAreaCode','SetAreaCode','IsOnFire','SetUsesCollision','SetDucking','IsDoingGangDriveby','IsWearingGoggles','IsVisible','SetVisible','IsOnScreen')
    $declaration = [IO.File]::ReadAllText($VitalsHeaderPath) + [IO.File]::ReadAllText((Join-Path $taskRoot 'Client/game_de/CPedMetadataDE.h')) + [IO.File]::ReadAllText((Join-Path $taskRoot 'Client/game_de/CPedMutationDE.h')) + [IO.File]::ReadAllText((Join-Path $taskRoot 'Client/game_de/CPedPhysicsDE.h')) + [IO.File]::ReadAllText((Join-Path $taskRoot 'Client/game_de/CPedRotationDE.h')) + [IO.File]::ReadAllText($StateHeaderPath) + [IO.File]::ReadAllText((Join-Path $taskRoot 'Client/game_de/CPedFlagsDE.h')) + [IO.File]::ReadAllText((Join-Path $taskRoot 'Client/game_de/CPedBehaviorDE.h')) + [IO.File]::ReadAllText((Join-Path $taskRoot 'Client/game_de/CPedCombatDE.h')) + [IO.File]::ReadAllText((Join-Path $taskRoot 'Client/game_de/CPedWeaponSelectionDE.h')) + [IO.File]::ReadAllText((Join-Path $taskRoot 'Client/game_de/CPedPoseDE.h')) + [IO.File]::ReadAllText((Join-Path $taskRoot 'Client/game_de/CPedWeaponStatsDE.h')) + [IO.File]::ReadAllText((Join-Path $taskRoot 'Client/game_de/CPedWaterDE.h')) + [IO.File]::ReadAllText((Join-Path $taskRoot 'Client/game_de/CPedOrientationDE.h')) + [IO.File]::ReadAllText($AreaHeaderPath) + [IO.File]::ReadAllText($FireHeaderPath) + [IO.File]::ReadAllText($CollisionHeaderPath) + [IO.File]::ReadAllText($TasksHeaderPath) + [IO.File]::ReadAllText($GogglesHeaderPath) + [IO.File]::ReadAllText($VisibilityHeaderPath) + [IO.File]::ReadAllText($OnScreenHeaderPath)
    foreach ($method in $provided) {
        if ($declaration -notmatch ('\b'+$method+'\s*\([^;{}]*\)\s*(?:const\s*)?(?:noexcept\s*)?override\s*;')) {
            throw "Production CPed SDK override is missing: $method"
        }
    }
    if ([IO.File]::ReadAllText($VitalsHeaderPath) -notmatch 'bool\s+IsOnFire\(\)\s+override\s*;' -or
        [IO.File]::ReadAllText($FireHeaderPath) + [IO.File]::ReadAllText($CollisionHeaderPath) -notmatch 'bool\s+IsOnFire\(\)\s+const\s+override\s*;') {
        throw 'The separate inherited nonconst and direct const fire declarations must both be production methods'
    }
    $allMethods = @(foreach ($header in @('Client/sdk/game/CEntity.h','Client/sdk/game/CPhysical.h','Client/sdk/game/CPed.h')) {
        InterfaceMethods $header | Where-Object { $provided -notcontains $_.name -and $_.name -ne 'GetPedIntelligence' }
    })
    # CPed repeats some inherited pure declarations. Emit one matching override
    # per signature while retaining every overload and const/noexcept qualifier.
    $seen = @{}
    $pedMethods = @($allMethods | Where-Object {
        $key = [regex]::Replace($_.signature,'\s+','')
        if ($seen.ContainsKey($key)) { $false } else { $seen[$key] = $true; $true }
    })
    $generated = $OutputPath
    $lines = @('#pragma once','#include <array>','#include <cstddef>','namespace ped_on_screen_fixture {',
        'struct TaskScenario { bool available=false; CPedIntelligence* intelligence=nullptr; std::array<int,16> events{}; std::size_t eventCount=0; void Record(int event) noexcept {if(eventCount==events.size())std::abort();events[eventCount++]=event;} void ResetEvents() noexcept {eventCount=0;} };',
        ('inline constexpr unsigned RejectingMethods = {0};' -f $pedMethods.Count),
        'class Game final : public CGame { public:')
    # A rejected virtual must identify itself in the owned child's log; an
    # anonymous abort cannot distinguish an accidental SDK call from a fault.
    $lines += @(InterfaceMethods 'Client/sdk/game/CGame.h' | ForEach-Object { $_.signature + (RejectBody $_.signature ('Game::'+$_.name)) })
    $lines += @('};','class PedOnScreenFixture final : public mta::game_de::CPedOnScreenDE { public:',
        'PedOnScreenFixture(CGame& game,mta::game_de::NativePedVitalsAccess& vitals,mta::game_de::NativePedMetadataAccess& metadata,mta::game_de::NativePedMutationAccess& mutation,mta::game_de::NativePedObservationAccess& observation,mta::game_de::NativePedRotationAccess& rotation,mta::game_de::NativePedOxygenAccess& oxygen,mta::game_de::NativePedDuckingAccess& ducking,mta::game_de::NativePedBikeFlagsAccess& bike,mta::game_de::NativePedShotFlagsAccess& shot,mta::game_de::NativePedBleedingAccess& bleeding,mta::game_de::NativePedStayAccess& stay,mta::game_de::NativePedFightingStyleAccess& combat,mta::game_de::NativePedCurrentWeaponSlotAccess& selection,mta::game_de::NativePedPlacementAccess& placement,mta::game_de::NativePedWaterFlagsAccess& water,mta::game_de::NativePedWaterTaskAccess& waterTask,mta::game_de::NativePedOrientationAccess& orientation,mta::game_de::NativePedAreaAccess& area,mta::game_de::NativePedFireAccess& fire,mta::game_de::NativePedCollisionAccess& collision,mta::game_de::NativePedGogglesAccess& goggles,mta::game_de::NativePedVisibilityAccess& visibility,mta::game_de::NativePedOnScreenAccess& onScreen,const mta::game_de::NativePedReferenceToken& token,std::uint32_t wrapperType,TaskScenario& scenario) noexcept : CPedOnScreenDE(game,vitals,metadata,mutation,observation,rotation,oxygen,ducking,bike,shot,bleeding,stay,combat,selection,placement,water,waterTask,orientation,area,fire,collision,goggles,visibility,onScreen,token,wrapperType),taskScenarioValue(scenario) {}',
        'CPedIntelligence* GetPedIntelligence() const noexcept override {taskScenarioValue.Record(2);return taskScenarioValue.intelligence;}',
        'mta::game_de::ClockResult RepairLastGoodPhysicsState(const CMatrix& matrix) noexcept override {(void)matrix;Unqualified("Ped::RepairLastGoodPhysicsState");}')
    $lines += @($pedMethods | ForEach-Object { $_.signature + (RejectBody $_.signature ('Ped::'+$_.name)) })
    $lines += @('protected:bool HasPedIntelligence() const noexcept override {taskScenarioValue.Record(1);return taskScenarioValue.available;}','private:TaskScenario& taskScenarioValue;','};','}')
    $lines | Set-Content -LiteralPath $generated -Encoding utf8NoBOM
    [ordered]@{path=$OutputPath;sha256=(Get-FileHash -LiteralPath $OutputPath -Algorithm SHA256).Hash.ToLowerInvariant();
        rejectingOverrides=$pedMethods.Count;interfaces=@('CPed','CPhysical','CEntity');productionMethodsNotOverridden=$provided;
        testedMethods=@('IsOnScreen');separateStoredAvailabilityBeforeVirtualGetter=$true;gameReceiver='Complete rejecting test-only CGame; no constructor/query game access or native factory'}
}
