# Generate strict test-only CWorld overrides. Ground, Line, Area and the
# JetpackHeight getter stay inherited from production; the unqualified setter
# remains a test rejector, never a production implementation or publication.
function Write-WorldJetpackHeightFixture([string]$TaskRoot,[string]$OutputPath,[string]$WorldHeaderPath='',[string]$GroundHeaderPath='',[string]$LineHeaderPath='',[string]$AreaHeaderPath='') {
    if(-not $WorldHeaderPath){$WorldHeaderPath=Join-Path $TaskRoot 'Client/game_de/CWorldJetpackHeightDE.h'}
    if(-not $AreaHeaderPath){$AreaHeaderPath=Join-Path $TaskRoot 'Client/game_de/CWorldAreaDE.h'}
    if(-not $LineHeaderPath){$LineHeaderPath=Join-Path $TaskRoot 'Client/game_de/CWorldLineDE.h'}
    if(-not $GroundHeaderPath){$GroundHeaderPath=Join-Path $TaskRoot 'Client/game_de/CWorldGroundDE.h'}
    $sdkPath=Join-Path $TaskRoot 'Client/sdk/game/CWorld.h'
    $sdk=[IO.File]::ReadAllText($sdkPath)
    $sdk=[regex]::Replace($sdk,'(?s)/\*.*?\*/|//[^\r\n]*','')
    $declaration=[regex]::Match($sdk,'(?s)class\s+CWorld\s*\{(?<body>.*?)\n\};')
    if(-not $declaration.Success){throw 'Actual CWorld declaration was not found'}
    $production=[IO.File]::ReadAllText($WorldHeaderPath)
    if($production -notmatch 'class\s+CWorldJetpackHeightDE\s*:\s*public\s+CWorldAreaDE' -or
       $production -notmatch 'float\s+GetJetpackMaxHeight\s*\(\s*\)\s*override\s*;') {
        throw 'Actual abstract CWorld jetpack-height getter is missing'
    }
    $ground=[IO.File]::ReadAllText($GroundHeaderPath)
    if($ground -notmatch 'float\s+FindGroundZFor3DPosition\s*\([^;{}]+\)\s*override\s*;'){throw 'Actual inherited Ground SDK override is missing'}
    $line=[IO.File]::ReadAllText($LineHeaderPath)
    if($line -notmatch 'bool\s+IsLineOfSightClear\s*\([^;{}]+\)\s*override\s*;'){throw 'Actual inherited Line SDK override is missing'}
    $area=[IO.File]::ReadAllText($AreaHeaderPath)
    if($area -notmatch 'DWORD\s+GetCurrentArea\s*\(\s*\)\s*override\s*;'){throw 'Actual inherited Area SDK getter is missing'}
    $lines=@('#pragma once','namespace world_jetpack_height_fixture {',
        'class WorldJetpackHeightFixture final : public mta::game_de::CWorldJetpackHeightDE { public:',
        'explicit WorldJetpackHeightFixture(mta::game_de::NativeWorldGroundAccess& ground, mta::game_de::NativeWorldLineAccess& line, mta::game_de::NativeWorldAreaAccess& area, mta::game_de::NativeWorldJetpackHeightAccess& jetpack) noexcept : CWorldJetpackHeightDE(ground,line,area,jetpack) {}')
    $count=0;$inherited=0;$names=@()
    foreach($match in [regex]::Matches($declaration.Groups['body'].Value,'virtual\s+(?<signature>[^;]+?)\s*=\s*0\s*;')) {
        $signature=$match.Groups['signature'].Value.Trim()
        $name=[regex]::Match($signature,'(?<name>\w+)\s*\(').Groups['name'].Value
        if(-not $name){throw "Cannot derive strict world fixture override: $signature"}
        if($name -in @('FindGroundZFor3DPosition','IsLineOfSightClear','GetCurrentArea','GetJetpackMaxHeight')){$inherited++;continue}
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
    if($inherited -ne 4 -or $count -ne 24){throw 'Ground/Line/Area/Jetpack SDK inheritance or remaining pure-method coverage was incomplete'}
    $lines+='};';$lines+='}'
    [IO.File]::WriteAllLines($OutputPath,$lines,[Text.UTF8Encoding]::new($false))
    [ordered]@{path=$OutputPath;sha256=(Get-FileHash -LiteralPath $OutputPath -Algorithm SHA256).Hash.ToLowerInvariant();
        namespace='world_jetpack_height_fixture';class='WorldJetpackHeightFixture';rejectingOverrides=$count;rejectingNames=$names;
        productionMethodsNotOverridden=@('FindGroundZFor3DPosition','IsLineOfSightClear','GetCurrentArea','GetJetpackMaxHeight');nativeFactoryProvided=$false;warningsDisabled=$false}
}
