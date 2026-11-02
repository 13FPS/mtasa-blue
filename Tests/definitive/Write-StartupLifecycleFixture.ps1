# Strict linear lifecycle receiver. Native hooks stay pure for genuine owner admission.
function Write-StartupLifecycleFixture([string]$taskRoot,[string]$OutputPath,[switch]$BuildingDefault) {
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




    $provided=@('Initialize','Reset','RemoveGameWorld','RestoreGameWorld','SetBuildingPoolSize','GetGarages','GetStreaming','DisableVSync')
    foreach($header in @('Client/game_de/CGameDE.h','Client/game_de/CGameTimingDE.h','Client/game_de/CGameWeaponInfoDE.h','Client/game_de/CGameInitializationDE.h')) {
        $text=[IO.File]::ReadAllText((Join-Path $taskRoot $header))
        $provided+=@([regex]::Matches($text,'(?<name>\w+)\s*\([^;{}]*\)\s*(?:const\s*)?(?:noexcept\s*)?override\b')|ForEach-Object{$_.Groups['name'].Value})
    }
    $methods=@(InterfaceMethods 'Client/sdk/game/CGame.h'|Where-Object{$provided -notcontains $_.name})
    $hooks=@()
    foreach($header in @('Client/game_de/CGameResetDE.h','Client/game_de/CGameWorldDE.h')) {
        $text=[IO.File]::ReadAllText((Join-Path $taskRoot $header))
        foreach($match in [regex]::Matches($text,'virtual\s+(?<signature>[^;{}]+?)\s*=\s*0\s*;')) {
            $signature=[regex]::Replace($match.Groups['signature'].Value.Trim(),'\s+',' ')
            $name=[regex]::Match($signature,'(?<name>\w+)\s*\(').Groups['name'].Value
            $hooks+=@{name=$name;signature=$signature}
        }
    }
    if($hooks.Count -ne 14){throw 'Actual lifecycle must retain exactly14 unresolved native hooks'}
    $lines=@('#pragma once','#include <utility>','#include <type_traits>','#include <cstdio>','#include <cstdlib>',
        'namespace lifecycle_fixture {',
        '[[noreturn]] inline void Unqualified(const char* method) noexcept {std::fprintf(stderr,"Unqualified lifecycle fixture call: %s\n",method);std::abort();}',
        'template<class Base> class GameBase : public Base { protected:',
        'template<class... Args> explicit GameBase(Args&&... args) : Base(std::forward<Args>(args)...) {}',
        'public:')
    $lines+=@($methods|ForEach-Object{$_.signature+(RejectBody $_.signature ('Game::'+$_.name))})
    $lines+=@('};','template<class Base> class NoPresentationGameBase : public GameBase<Base> { protected:',
        'using GameBase<Base>::GameBase;','public: void DisableVSync() override {Unqualified("DisableVSync");}','};')
    # Omit each real signature in turn. The all-hooks leaf being concrete
    # proves that no unrelated missing method can hide a lost pure hook.
    foreach($missing in -1..13) {
        $name=if($missing -lt 0){'AllHooksProbe'}else{'MissingHook'+$missing}
        $lines+=('class '+$name+' : public NoPresentationGameBase<mta::game_de::CGameStartupLifecycleDE> { protected:')
        for($index=0;$index -lt $hooks.Count;$index++) {
            if($index -ne $missing){$lines+=($hooks[$index].signature+(RejectBody $hooks[$index].signature ('NativeHook::'+$hooks[$index].name)))}
        }
        $lines+='};'
        if($missing -lt 0){$lines+='static_assert(!std::is_abstract_v<AllHooksProbe>);'}else{$lines+=('static_assert(std::is_abstract_v<'+$name+'>);')}
    }
    # The base fourteen omission probes stay unchanged for existing owners.
    # This derived surface supplies only thirteen guards: implementing the
    # production capacity hook here would accidentally hide the new SDK path.
    if($BuildingDefault) {
        $defaultHooks=@($hooks|Where-Object{$_.name -ne 'GetResetBuildingPoolCapacity'})
        if($defaultHooks.Count -ne 13){throw 'Building default must leave exactly13 unresolved native hooks'}
        foreach($missing in -1..12) {
            $name=if($missing -lt 0){'DefaultAllHooksProbe'}else{'DefaultMissingHook'+$missing}
            $lines+=('class '+$name+' : public NoPresentationGameBase<mta::game_de::CGameBuildingPoolDefaultDE> { protected:')
            for($index=0;$index -lt $defaultHooks.Count;$index++) {
                if($index -ne $missing){$lines+=($defaultHooks[$index].signature+(RejectBody $defaultHooks[$index].signature ('NativeHook::'+$defaultHooks[$index].name)))}
            }
            $lines+='};'
            if($missing -lt 0){$lines+='static_assert(!std::is_abstract_v<DefaultAllHooksProbe>);'}else{$lines+=('static_assert(std::is_abstract_v<'+$name+'>);')}
        }
    }
    $counts=[ordered]@{game=$methods.Count}
    foreach($definition in @(
        @{name='Fire';header='Client/sdk/game/CFireManager.h';base='CFireManager';provided=@('ExtinguishAllFires')},
        @{name='Camera';header='Client/sdk/game/CCamera.h';base='CCamera';provided=@('Restore','SetFadeColor','Fade')},
        @{name='Hud';header='Client/sdk/game/CHud.h';base='CHud';provided=@('Disable','SetComponentVisible')},
        @{name='Buildings';header='Client/sdk/game/CBuildingsPool.h';base='CBuildingsPool';provided=@('GetSize','Resize','RemoveAllWithBackup','RestoreBackup')},
        @{name='Dummies';header='Client/sdk/game/CDummyPool.h';base='CDummyPool';provided=@('RemoveAllWithBackup','RestoreBackup')},
        @{name='Pools';header='Client/sdk/game/CPools.h';base='CPools';provided=@('GetBuildingsPool','GetDummyPool')}
    )) {
        $rejectors=@(InterfaceMethods $definition.header|Where-Object{$definition.provided -notcontains $_.name})
        $lines+=('class '+$definition.name+'Base : public '+$definition.base+' { public:')
        $lines+=@($rejectors|ForEach-Object{$_.signature+(RejectBody $_.signature ($definition.name+'::'+$_.name))})
        $lines+='};';$counts[$definition.name]=$rejectors.Count
    }
    $lines+='}'
    $lines|Set-Content -LiteralPath $OutputPath -Encoding utf8NoBOM
    [ordered]@{path=$OutputPath;sha256=(Get-FileHash -LiteralPath $OutputPath -Algorithm SHA256).Hash.ToLowerInvariant();
        namespace='lifecycle_fixture';template='GameBase<Base>';nonPresentationTemplate='NoPresentationGameBase<Base>';
        rejectingOverrides=$counts;inheritedProductionMethods=$provided|Sort-Object -Unique;pureNativeHooks=$hooks;
        omittedHookProbes=14;defaultOmittedHookProbes=$(if($BuildingDefault){13}else{0});defaultHookInherited=[bool]$BuildingDefault;allHooksProbeConcrete=$true;nativeHooksImplemented=$false;completeFactoryProvided=$false}
}
