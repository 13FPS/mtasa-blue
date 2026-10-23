# Generate only test receivers; every unused actual SDK operation rejects by name.
function Write-WeaponFireBulletFixture([string]$taskRoot,[string]$OutputPath,[string]$FireBulletHeaderPath='') {
    if(-not $FireBulletHeaderPath){$FireBulletHeaderPath=Join-Path $taskRoot 'Client/game_de/CWeaponFireBulletDE.h'}
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


    if([IO.File]::ReadAllText($FireBulletHeaderPath) -notmatch 'bool\s+FireBullet\([^;{}]+\)\s+override\s*;'){throw 'Actual FireBullet SDK override missing'}
    function Methods([string[]]$headers,[string[]]$provided) {
        $seen=@{}
        @(foreach($header in $headers){InterfaceMethods $header|Where-Object {
            $key=[regex]::Replace($_.signature,'\s+','')
            if($_.name -in $provided -or $seen.ContainsKey($key)){$false}else{$seen[$key]=$true;$true}
        }})
    }
    $classes=@(
        [ordered]@{name='GameBase';base='CGame';headers=@('Client/sdk/game/CGame.h');provided=@('GetWorld','GetWeaponStatManager')},
        [ordered]@{name='WorldBase';base='CWorld';headers=@('Client/sdk/game/CWorld.h');provided=@('IgnoreEntity')},
        [ordered]@{name='ManagerBase';base='CWeaponStatManager';headers=@('Client/sdk/game/CWeaponStatManager.h');provided=@('GetWeaponStatsFromSkillLevel')},
        [ordered]@{name='StatBase';base='CWeaponStat';headers=@('Client/sdk/game/CWeaponStat.h');provided=@('GetFireOffset')},
        [ordered]@{name='Entity';base='CEntity';headers=@('Client/sdk/game/CEntity.h');provided=@()},
        [ordered]@{name='PlayerBase';base='CPlayerPed';headers=@('Client/sdk/game/CEntity.h','Client/sdk/game/CPhysical.h','Client/sdk/game/CPed.h','Client/sdk/game/CPlayerPed.h');provided=@('GetTransformedBonePosition','AddWeaponAudioEvent')}
    )
    $lines=@('#pragma once','namespace weapon_fire_bullet_fixture {')
    $counts=[ordered]@{}
    foreach($class in $classes) {
        $methods=@(Methods $class.headers $class.provided)
        $counts[$class.name]=$methods.Count
        $lines+='class '+$class.name+' : public '+$class.base+' { public:'
        $lines+=@($methods|ForEach-Object{$_.signature+(RejectBody $_.signature ($class.name+'::'+$_.name))})
        $lines+='};'
    }
    $provided=@('SetAsCurrentWeapon','GetPed','GetSlot','GetType','SetType','GetState','SetState','GetAmmoInClip','SetAmmoInClip','GetAmmoTotal','SetAmmoTotal','GetInfo','FireBullet','FireInstantHit')
    $weaponMethods=@(Methods @('Client/sdk/game/CWeapon.h') $provided)
    $counts['WeaponBase']=$weaponMethods.Count
    $lines+=@('class WeaponBase : public mta::game_de::CWeaponFireBulletDE { public:',
        'WeaponBase(CGame& game,mta::game_de::WeaponFireHandlerAccess& handlers,mta::game_de::NativeGameTimingAccess& access,const mta::game_de::NativePedReferenceToken& token,CPed* owner,eWeaponSlot slot) noexcept : CWeaponFireBulletDE(game,handlers,access,token,owner,slot) {}')
    $lines+=@($weaponMethods|ForEach-Object{$_.signature+(RejectBody $_.signature ('Weapon::'+$_.name))})
    $lines+=@('};','}')
    $lines|Set-Content -LiteralPath $OutputPath -Encoding utf8NoBOM
    [ordered]@{path=$OutputPath;sha256=(Get-FileHash -LiteralPath $OutputPath -Algorithm SHA256).Hash.ToLowerInvariant();rejectingOverrides=$counts;
        inheritedMethods=@('FireBullet','GetType');explicitPureDependency='FireInstantHit supplied only by test child';
        actualInterfaces=@('CWeapon','CEntity','CPlayerPed','CGame','CWorld','CWeaponStatManager','CWeaponStat');productionFactoriesPublished=$false}
}
