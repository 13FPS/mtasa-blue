# Generate only non-ped fallback receivers around genuine SDK interfaces.
# Ped and native-owner definitions are reused without invoking old cases; the
# existing actual CPoolsDE client-association method stays inherited here.
function Write-ClientTargetedPedFixture([string]$taskRoot,[string]$OutputPath) {
    . (Join-Path $taskRoot 'Tests/definitive/Write-PedPoolRegistryFixture.ps1')
    $registryPath=Join-Path (Split-Path -Parent $OutputPath) 'PedPoolRegistryFixture.generated.h'
    $registry=Write-PedPoolRegistryFixture $taskRoot $registryPath
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



    $entities=@(InterfaceMethods 'Client/sdk/game/CEntity.h'|Where-Object {$_.name -ne 'GetInterface'})
    $pools=@(InterfaceMethods 'Client/sdk/game/CPools.h'|Where-Object {$_.name -notin @('GetPed','GetPedFromRef','GetPedClientEntity','GetPedCount','ResetPedPoolCount','InvalidateLocalPlayerClientEntity','GetClientEntity')})
    $lines=@('#pragma once','namespace client_targeted_ped_fixture {','class Entity final : public CEntity { public:',
        'explicit Entity(EntityState& state) noexcept : stateValue(state) {}',
        'CEntitySAInterface* GetInterface() override {++stateValue.calls;stateValue.trace.Record(2);if(stateValue.fail)throw RoutingFailure("entity-interface");return stateValue.identity;}')
    $lines+=@($entities|ForEach-Object {$_.signature+(RejectBody $_.signature ('Entity::'+$_.name))})
    $lines+=@('private:EntityState& stateValue;','};',
        'class Pools final : public mta::game_de::CPoolsDE { public:',
        'Pools(mta::game_de::NativeGameTimingAccess& owner,FallbackState& state) noexcept : CPoolsDE(owner),stateValue(state) {}',
        '~Pools() override=default;','using CPoolsDE::BindBorrowedPed;',
        'CClientEntity* GetClientEntity(DWORD* identity) override {++stateValue.calls;stateValue.received=identity;stateValue.trace.Record(3);if(stateValue.fail)throw RoutingFailure("entity-lookup");return stateValue.result;}')
    $lines+=@($pools|ForEach-Object {$_.signature+(RejectBody $_.signature ('Pools::'+$_.name))})
    $lines+=@('private:FallbackState& stateValue;','};','}')
    $lines|Set-Content -LiteralPath $OutputPath -Encoding utf8NoBOM
    [ordered]@{path=$OutputPath;sha256=(Get-FileHash -LiteralPath $OutputPath -Algorithm SHA256).Hash.ToLowerInvariant();
        rejectingEntityMethods=$entities.Count;rejectingPoolMethods=$pools.Count;registryFixture=$registry;
        actualPedClientEntityInherited=$true;oldRegistryCasesExecuted=$false;productionFactoriesPublished=$false}
}
