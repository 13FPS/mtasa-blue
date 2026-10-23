# Only the real const sphere query is inherited; every other camera hook rejects.
function Write-CameraSphereVisibleFixture([string]$taskRoot,[string]$OutputPath,[string]$CameraHeaderPath='') {
    if(-not $CameraHeaderPath){$CameraHeaderPath=Join-Path $taskRoot 'Client/game_de/CCameraSphereVisibleDE.h'}
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



    if([IO.File]::ReadAllText($CameraHeaderPath) -notmatch 'bool\s+IsSphereVisible\([^;{}]+\)\s+const\s+override\s*;'){throw 'Actual const camera SDK override missing'}
    $methods=@(InterfaceMethods 'Client/sdk/game/CCamera.h'|Where-Object{$_.name -ne 'IsSphereVisible'})
    $lines=@('#pragma once','namespace camera_sphere_visible_fixture {',
        'class CameraSphereVisibleFixture final : public mta::game_de::CCameraSphereVisibleDE {public:',
        'explicit CameraSphereVisibleFixture(mta::game_de::NativeCameraSphereVisibleAccess& access) noexcept : CCameraSphereVisibleDE(access) {}')
    $lines+=@($methods|ForEach-Object{$_.signature+(RejectBody $_.signature ('CCamera::'+$_.name))})
    $lines+=@('};','}')
    $lines|Set-Content -LiteralPath $OutputPath -Encoding utf8NoBOM
    [ordered]@{path=$OutputPath;sha256=(Get-FileHash -LiteralPath $OutputPath -Algorithm SHA256).Hash.ToLowerInvariant();rejectingOverrides=$methods.Count;
        inheritedMethods=@('IsSphereVisible');actualInterface='CCamera';constQuery=$true;nativeFactoryProvided=$false}
}
