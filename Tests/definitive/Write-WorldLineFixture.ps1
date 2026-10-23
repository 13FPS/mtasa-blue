# Generate strict test-only overrides from the actual CWorld pure SDK. Both
# Ground and Line methods stay inherited from production; the fixture cannot
# replace the boolean query it tests or publish a complete native world.
function Write-WorldLineFixture([string]$TaskRoot,[string]$OutputPath,[string]$WorldHeaderPath='',[string]$GroundHeaderPath='') {
    if(-not $WorldHeaderPath){$WorldHeaderPath=Join-Path $TaskRoot 'Client/game_de/CWorldLineDE.h'}
    if(-not $GroundHeaderPath){$GroundHeaderPath=Join-Path $TaskRoot 'Client/game_de/CWorldGroundDE.h'}
    $sdkPath=Join-Path $TaskRoot 'Client/sdk/game/CWorld.h'
    $sdk=[IO.File]::ReadAllText($sdkPath)
    $sdk=[regex]::Replace($sdk,'(?s)/\*.*?\*/|//[^\r\n]*','')
    $declaration=[regex]::Match($sdk,'(?s)class\s+CWorld\s*\{(?<body>.*?)\n\};')
    if(-not $declaration.Success){throw 'Actual CWorld declaration was not found'}
    $production=[IO.File]::ReadAllText($WorldHeaderPath)
    if($production -notmatch 'class\s+CWorldLineDE\s*:\s*public\s+CWorldGroundDE' -or
       $production -notmatch 'bool\s+IsLineOfSightClear\s*\([^;{}]+\)\s*override\s*;') {
        throw 'Actual abstract CWorld line-clear override is missing'
    }
    $ground=[IO.File]::ReadAllText($GroundHeaderPath)
    if($ground -notmatch 'float\s+FindGroundZFor3DPosition\s*\([^;{}]+\)\s*override\s*;'){throw 'Actual inherited Ground SDK override is missing'}
    $lines=@('#pragma once','namespace world_line_fixture {',
        'class WorldLineFixture final : public mta::game_de::CWorldLineDE { public:',
        'explicit WorldLineFixture(mta::game_de::NativeWorldGroundAccess& ground, mta::game_de::NativeWorldLineAccess& line) noexcept : CWorldLineDE(ground,line) {}')
    $count=0;$inherited=0;$names=@()
    foreach($match in [regex]::Matches($declaration.Groups['body'].Value,'virtual\s+(?<signature>[^;]+?)\s*=\s*0\s*;')) {
        $signature=$match.Groups['signature'].Value.Trim()
        $name=[regex]::Match($signature,'(?<name>\w+)\s*\(').Groups['name'].Value
        if(-not $name){throw "Cannot derive strict world fixture override: $signature"}
        if($name -in @('FindGroundZFor3DPosition','IsLineOfSightClear')){$inherited++;continue}
        $begin=$signature.IndexOf('(');$end=$signature.LastIndexOf(')')
        if($begin -lt 0 -or $end -lt $begin){throw "Unsupported CWorld signature: $signature"}
        $parameters=$signature.Substring($begin+1,$end-$begin-1)
        $parts=@();$part='';$depth=0
        foreach($character in $parameters.ToCharArray()) {
            if($character -eq ',' -and $depth -eq 0){$parts+=$part;$part='';continue}
            $part+=$character
            if($character -in @('<','(','[','{')){$depth++}
            elseif($character -in @('>',')',']','}')){$depth--}
        }
        if($part.Trim()){$parts+=$part}
        $clean=@();$discard=@()
        foreach($parameter in $parts) {
            $parameter=[regex]::Replace($parameter,'\s*=.*$','').Trim()
            $clean+=$parameter
            $argument=[regex]::Match($parameter,'(?<name>[A-Za-z_]\w*)\s*(?:\[[^\]]*\]\s*)*$')
            if(-not $argument.Success){throw "Unnamed/unsupported CWorld parameter: $parameter"}
            $discard+='(void)'+$argument.Groups['name'].Value+';'
        }
        $signature=$signature.Substring(0,$begin+1)+($clean -join ', ')+$signature.Substring($end)
        $signature=[regex]::Replace($signature,'\s+',' ')
        $signature+=' override'
        $lines+=$signature+' {'+($discard -join '')+'Unqualified("CWorld::'+$name+'");}'
        $count++;$names+=$name
    }
    if($inherited -ne 2 -or $count -ne 26){throw 'Ground/Line SDK inheritance or remaining pure-method coverage was incomplete'}
    $lines+='};';$lines+='}'
    [IO.File]::WriteAllLines($OutputPath,$lines,[Text.UTF8Encoding]::new($false))
    [ordered]@{path=$OutputPath;sha256=(Get-FileHash -LiteralPath $OutputPath -Algorithm SHA256).Hash.ToLowerInvariant();
        namespace='world_line_fixture';class='WorldLineFixture';rejectingOverrides=$count;rejectingNames=$names;
        productionMethodsNotOverridden=@('FindGroundZFor3DPosition','IsLineOfSightClear');nativeFactoryProvided=$false;warningsDisabled=$false}
}
