# Hide only the named optional export in a caller-owned fixture DLL. Never load it.
. (Join-Path $PSScriptRoot '../../Client/definitive/NativeFrontendPackage.ps1')
function Hide-MtaCoreAdmissionExportFixture([string]$Path,[string]$OwnedRoot) {
    $repository=(Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../..')).Path
    $build=[IO.Path]::GetFullPath((Join-Path $repository 'Build')).TrimEnd('\','/')+[IO.Path]::DirectorySeparatorChar
    $owned=[IO.Path]::GetFullPath($OwnedRoot).TrimEnd('\','/')+[IO.Path]::DirectorySeparatorChar
    $full=[IO.Path]::GetFullPath($Path)
    if(-not $owned.StartsWith($build,[StringComparison]::OrdinalIgnoreCase) -or -not $full.StartsWith($owned,[StringComparison]::OrdinalIgnoreCase)){
        throw 'Export fixture escaped its owned workspace output.'
    }
    for($current=$full;$current;$current=[IO.Path]::GetDirectoryName($current)){
        if((Get-Item -LiteralPath $current -Force).Attributes -band [IO.FileAttributes]::ReparsePoint){throw 'Linked export fixture refused.'}
    }
    $bytes=[IO.File]::ReadAllBytes($full)
    if($bytes.Length -gt 64MB){throw 'Fixture Core size is out of bounds.'}
    $symbol='MTA_ReadDEFrontendRuntimeAdmissionV1'
    $exports=Get-MtaNativeFrontendExportsFromBytes $bytes $full
    if(-not $exports.Contains($symbol)){throw 'Fixture Core lacks the original optional admission export.'}
    $before=[Convert]::ToHexString([Security.Cryptography.SHA256]::HashData($bytes)).ToLowerInvariant()
    $pe=[int][BitConverter]::ToUInt32($bytes,0x3c)
    $optional=$pe+24
    $table=$optional+[BitConverter]::ToUInt16($bytes,$pe+20)
    $sections=@(for($index=0;$index -lt [BitConverter]::ToUInt16($bytes,$pe+6);++$index){
        $at=$table+40*$index
        @{rva=[uint64][BitConverter]::ToUInt32($bytes,$at+12);raw=[uint64][BitConverter]::ToUInt32($bytes,$at+20);size=[uint64][BitConverter]::ToUInt32($bytes,$at+16)}
    })
    $offset={param([uint64]$rva,[uint64]$size)
        foreach($section in $sections){
            if($rva -ge $section.rva -and $rva-$section.rva+$size -le $section.size -and $section.raw+$rva-$section.rva+$size -le $bytes.Length){return [int]($section.raw+$rva-$section.rva)}
        }
        throw 'Fixture export RVA is out of bounds.'
    }
    $directory=& $offset ([BitConverter]::ToUInt32($bytes,$optional+112)) 40
    $count=[BitConverter]::ToUInt32($bytes,$directory+24)
    $names=& $offset ([BitConverter]::ToUInt32($bytes,$directory+32)) (4*$count)
    $nameBytes=[Text.Encoding]::ASCII.GetBytes($symbol)
    $changed=-1
    for($index=0;$index -lt $count;++$index){
        $rva=[BitConverter]::ToUInt32($bytes,$names+4*$index)
        $at=& $offset $rva 1
        $length=0
        while($length -lt 512 -and $at+$length -lt $bytes.Length -and $bytes[$at+$length]){++$length}
        if($length -ne $nameBytes.Length){continue}
        & $offset $rva ($length+1)|Out-Null
        $match=$bytes[$at+$nameBytes.Length] -eq 0
        for($letter=0;$match -and $letter -lt $nameBytes.Length;++$letter){$match=$bytes[$at+$letter] -eq $nameBytes[$letter]}
        if($match){$bytes[$at]=[byte][char]'X';$changed=$at;break}
    }
    if($changed -lt 0 -or (Get-MtaNativeFrontendExportsFromBytes $bytes $full).Contains($symbol)){throw 'Optional export was not hidden exactly.'}
    [IO.File]::WriteAllBytes($full,$bytes)
    [ordered]@{path=$full;before=$before;after=[Convert]::ToHexString([Security.Cryptography.SHA256]::HashData($bytes)).ToLowerInvariant();changedByte=$changed;moduleExecuted=$false}
}
