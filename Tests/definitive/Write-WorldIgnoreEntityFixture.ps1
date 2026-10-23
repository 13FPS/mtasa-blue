# Test-only concrete receivers preserve actual controlled lookup and world dispatch.
function Write-WorldIgnoreEntityFixture([string]$taskRoot,[string]$OutputPath,[string]$WorldHeaderPath='') {
    if(-not $WorldHeaderPath){$WorldHeaderPath=Join-Path $taskRoot 'Client/game_de/CWorldIgnoreEntityDE.h'}
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



    if([IO.File]::ReadAllText($WorldHeaderPath) -notmatch 'void\s+IgnoreEntity\([^;{}]+\)\s+override\s*;'){throw 'Actual IgnoreEntity SDK override missing'}
    function Methods([string[]]$headers,[string[]]$provided) {
        $seen=@{}
        @(foreach($header in $headers){InterfaceMethods $header|Where-Object {
            $key=[regex]::Replace($_.signature,'\s+','')
            if($_.name -in $provided -or $seen.ContainsKey($key)){$false}else{$seen[$key]=$true;$true}
        }})
    }
    $counts=[ordered]@{}
    $lines=@('#pragma once','#include <array>','namespace world_ignore_entity_fixture {',
        'struct Anchor {virtual ~Anchor()=default;std::array<unsigned char,17> padding{};};',
        'class Ped final : public Anchor,public mta::game_de::CPedVitalsDE { public:',
        'Ped(mta::game_de::NativeGameTimingAccess& native,const mta::game_de::NativePedReferenceToken& token) noexcept : CPedVitalsDE(native,token) {}')
    $methods=@(Methods @('Client/sdk/game/CEntity.h','Client/sdk/game/CPhysical.h','Client/sdk/game/CPed.h') @('GetHealth','SetHealth','GetArmor','SetArmor'))
    $counts['Ped']=$methods.Count
    $lines+=@($methods|ForEach-Object{$_.signature+(RejectBody $_.signature ('Ped::'+$_.name))})
    $lines+=@('};','class ForeignEntity final : public CEntity { public:')
    $methods=@(Methods @('Client/sdk/game/CEntity.h') @())
    $counts['ForeignEntity']=$methods.Count
    $lines+=@($methods|ForEach-Object{$_.signature+(RejectBody $_.signature ('ForeignEntity::'+$_.name))})
    $lines+=@('};','class Pools final : public mta::game_de::CPoolsDE { public:',
        'explicit Pools(mta::game_de::NativeGameTimingAccess& native) noexcept : CPoolsDE(native) {}',
        '~Pools() override=default;using CPoolsDE::BindBorrowedPed;')
    $methods=@(Methods @('Client/sdk/game/CPools.h') @('GetPed','GetPedFromRef','GetPedClientEntity','GetPedCount','ResetPedPoolCount','InvalidateLocalPlayerClientEntity'))
    $counts['Pools']=$methods.Count
    $lines+=@($methods|ForEach-Object{$_.signature+(RejectBody $_.signature ('Pools::'+$_.name))})
    $lines+=@('};','class World final : public mta::game_de::CWorldIgnoreEntityDE { public:',
        'World(mta::game_de::NativeWorldGroundAccess& ground,mta::game_de::NativeWorldLineAccess& line,mta::game_de::NativeWorldAreaAccess& area,mta::game_de::NativeWorldJetpackHeightAccess& jetpack,mta::game_de::CPoolsDE& pools,mta::game_de::NativeWorldIgnoreEntityAccess& ignore) noexcept : CWorldIgnoreEntityDE(ground,line,area,jetpack,pools,ignore) {}')
    $methods=@(Methods @('Client/sdk/game/CWorld.h') @('FindGroundZFor3DPosition','IsLineOfSightClear','GetCurrentArea','GetJetpackMaxHeight','IgnoreEntity'))
    $counts['World']=$methods.Count
    $lines+=@($methods|ForEach-Object{$_.signature+(RejectBody $_.signature ('World::'+$_.name))})
    $lines+=@('};','using WorldIgnoreEntityFixture = World;','}')
    $lines|Set-Content -LiteralPath $OutputPath -Encoding utf8NoBOM
    [ordered]@{path=$OutputPath;sha256=(Get-FileHash -LiteralPath $OutputPath -Algorithm SHA256).Hash.ToLowerInvariant();rejectingOverrides=$counts;
        inheritedMethods=@('IgnoreEntity','ReadBorrowedPedReference','BindBorrowedPed');
        foreignEntityAndNativeInterfaceMethodsReject=$true;productionFactoriesPublished=$false}
}
