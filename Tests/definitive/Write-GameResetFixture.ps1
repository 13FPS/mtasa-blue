# Generate strictly rejecting test-only receivers from genuine common SDKs.
# No production Reset/service override is replaced by a successful fixture stub.
function Write-GameResetFixture([string]$taskRoot,[string]$OutputPath,[string]$ResetHeaderPath='') {
    if(-not $ResetHeaderPath){$ResetHeaderPath=Join-Path $taskRoot 'Client/game_de/CGameResetDE.h'}
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


    $provided=@()
    foreach($header in @('Client/game_de/CGameInitializationDE.h','Client/game_de/CGameDE.h','Client/game_de/CGameTimingDE.h','Client/game_de/CGameWeaponInfoDE.h')) {
        $text=[IO.File]::ReadAllText((Join-Path $taskRoot $header))
        $provided+=@([regex]::Matches($text,'(?<name>\w+)\s*\([^;{}]*\)\s*(?:const\s*)?(?:noexcept\s*)?override\s*;')|ForEach-Object {$_.Groups['name'].Value})
    }
    if([IO.File]::ReadAllText($ResetHeaderPath) -notmatch 'void\s+Reset\(\)\s+override\s*;'){throw 'Actual Reset SDK override missing'}
    $provided+='Reset'
    $testedGame=@('GetSystemState','GetFireManager','GetCamera','GetHud','GetPools','Pause','RestoreGameWorld','SetBuildingPoolSize')
    $game=@(InterfaceMethods 'Client/sdk/game/CGame.h'|Where-Object {$provided -notcontains $_.name -and $testedGame -notcontains $_.name})
    $lines=@('#pragma once','namespace reset_fixture {','class GameResetFixtureBase : public mta::game_de::CGameResetDE { public:',
        'GameResetFixtureBase(const mta::game_de::NativeGameTimingReader& reader,mta::game_de::CClockDE& clock,mta::game_de::NativeGameTimingAccess& timing,const mta::game_de::NativeStreamingFileIdsReader& streaming,const mta::game_de::NativeGameStateReader& state) noexcept : CGameResetDE(reader,clock,timing,streaming,state) {}',
        'using ConstructorServices=mta::game_de::CGameResetDE::ConstructorServices;',
        'GameResetFixtureBase(const mta::game_de::NativeGameTimingReader& reader,mta::game_de::CClockDE& clock,mta::game_de::NativeGameTimingAccess& timing,const mta::game_de::NativeStreamingFileIdsReader& streaming,const mta::game_de::NativeGameStateReader& state,ConstructorServices services) : CGameResetDE(reader,clock,timing,streaming,state,services) {}')
    $lines+=@($game|ForEach-Object {$_.signature+(RejectBody $_.signature ('Game::'+$_.name))})
    $lines+='};'
    $counts=[ordered]@{game=$game.Count}
    foreach($definition in @(
        @{name='Fire';header='Client/sdk/game/CFireManager.h';base='CFireManager';provided=@('ExtinguishAllFires')},
        @{name='Camera';header='Client/sdk/game/CCamera.h';base='CCamera';provided=@('Restore','SetFadeColor','Fade')},
        @{name='Hud';header='Client/sdk/game/CHud.h';base='CHud';provided=@('Disable','SetComponentVisible')},
        @{name='Buildings';header='Client/sdk/game/CBuildingsPool.h';base='CBuildingsPool';provided=@('GetSize')},
        @{name='Pools';header='Client/sdk/game/CPools.h';base='CPools';provided=@('GetBuildingsPool')}
    )) {
        $methods=@(InterfaceMethods $definition.header|Where-Object {$definition.provided -notcontains $_.name})
        $lines+=('class '+$definition.name+'FixtureBase : public '+$definition.base+' { public:')
        $lines+=@($methods|ForEach-Object {$_.signature+(RejectBody $_.signature ($definition.name+'::'+$_.name))})
        $lines+='};';$counts[$definition.name]=$methods.Count
    }
    $lines+='}'
    $lines|Set-Content -LiteralPath $OutputPath -Encoding utf8NoBOM
    [ordered]@{path=$OutputPath;sha256=(Get-FileHash -LiteralPath $OutputPath -Algorithm SHA256).Hash.ToLowerInvariant();
        rejectingOverrides=$counts;productionGameMethodsNotOverridden=$provided;testedGameReceivers=$testedGame;
        nativeFactoryProvided=$false;requiredResetAndInitializationHooksRemainPure=$true;warningsDisabled=$false}
}
