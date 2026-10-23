# Strict test-only CGame receiver around the actual prepared constructors.
# Production service methods remain inherited; unresolved methods reject loudly.
function Write-GameConstructorServicesFixture([string]$taskRoot,[string]$OutputPath,[string]$GameHeaderPath='') {
    if(-not $GameHeaderPath){$GameHeaderPath=Join-Path $taskRoot 'Client/game_de/CGameDE.h'}
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
    foreach($path in @($GameHeaderPath,(Join-Path $taskRoot 'Client/game_de/CGameTimingDE.h'),(Join-Path $taskRoot 'Client/game_de/CGameWeaponInfoDE.h'))) {
        $text=[IO.File]::ReadAllText($path)
        $provided+=@([regex]::Matches($text,'(?<name>\w+)\s*\([^;{}]*\)\s*(?:const\s*)?(?:noexcept\s*)?override\s*;')|ForEach-Object {$_.Groups['name'].Value})
    }
    $declaration=[IO.File]::ReadAllText($GameHeaderPath)
    if($declaration -notmatch 'const\s+NativeGameConstructorServices\s*&'){throw 'Actual typed constructor service-bundle route missing'}
    $methods=@(InterfaceMethods 'Client/sdk/game/CGame.h'|Where-Object {$provided -notcontains $_.name})
    $lines=@('#pragma once','namespace constructor_services_fixture {','class GameConstructorFixtureBase : public mta::game_de::CGameDE { public:',
        'GameConstructorFixtureBase(const mta::game_de::NativeGameTimingReader& reader,mta::game_de::CClockDE& clock,mta::game_de::NativeGameTimingAccess& timing,const mta::game_de::NativeStreamingFileIdsReader& streaming,const mta::game_de::NativeGameStateReader& state) noexcept : CGameDE(reader,clock,timing,streaming,state) {}',
        'using ConstructorServices=mta::game_de::CGameDE::ConstructorServices;',
        'GameConstructorFixtureBase(const mta::game_de::NativeGameTimingReader& reader,mta::game_de::CClockDE& clock,mta::game_de::NativeGameTimingAccess& timing,const mta::game_de::NativeStreamingFileIdsReader& streaming,const mta::game_de::NativeGameStateReader& state,ConstructorServices services) : CGameDE(reader,clock,timing,streaming,state,services) {}',
        'GameConstructorFixtureBase(const mta::game_de::NativeGameTimingReader& reader,mta::game_de::CClockDE& clock,mta::game_de::NativeGameTimingAccess& timing,const mta::game_de::NativeStreamingFileIdsReader& streaming,const mta::game_de::NativeGameStateReader& state,const mta::game_de::NativeGameConstructorServices& services) : CGameDE(reader,clock,timing,streaming,state,services) {}')
    $lines+=@($methods|ForEach-Object {$_.signature+(RejectBody $_.signature ('Game::'+$_.name))})
    $lines+=@('};','}')
    $lines|Set-Content -LiteralPath $OutputPath -Encoding utf8NoBOM
    [ordered]@{path=$OutputPath;sha256=(Get-FileHash -LiteralPath $OutputPath -Algorithm SHA256).Hash.ToLowerInvariant();
        rejectingOverrides=$methods.Count;productionMethodsNotOverridden=$provided;
        constructors=@('Historical partial5','PreparedWeapons6','NativeGameConstructorServices6');
        nativeFactoryProvided=$false;warningsDisabled=$false;namespace='constructor_services_fixture';class='GameConstructorFixtureBase'}
}
